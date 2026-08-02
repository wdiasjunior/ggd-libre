#include "audio_engine.h"
#include <string.h>
#include <math.h>

static float db_to_linear(float db) {
    if (db <= -80.0f) return 0.0f;
    return powf(10.0f, db / 20.0f);
}

// Safety saturation on the master bus. With the output trim in place this
// should rarely engage; it exists so cranked faders saturate rather than
// hard-clip.
static inline float soft_clip(float x) {
    const float t = 0.85f;
    if (x >  t) return   t + (1.0f - t) * tanhf(( x - t) / (1.0f - t));
    if (x < -t) return -(t + (1.0f - t) * tanhf((-x - t) / (1.0f - t)));
    return x;
}

void engine_set_sample_rate(AudioEngine *engine, float sample_rate) {
    engine->sample_rate = sample_rate;
    engine->choke_fade_samples = (CHOKE_FADE_MS / 1000.0f) * sample_rate;
    engine->steal_fade_samples = (STEAL_FADE_MS / 1000.0f) * sample_rate;
}

void engine_set_source_rate(AudioEngine *engine, uint32_t src_rate) {
    if (src_rate == 0) src_rate = 48000;
    engine->src_rate = src_rate;
    // Compared against WavFile::num_frames, which counts source frames.
    engine->tail_fade_frames = (uint32_t)((MIC_TAIL_FADE_MS / 1000.0f) * (float)src_rate);
    if (engine->tail_fade_frames == 0) engine->tail_fade_frames = 1;
}

void engine_reset_voices(AudioEngine *engine) {
    for (int i = 0; i < MAX_VOICES; i++) {
        Voice *v = &engine->voices[i];
        v->active = false;
        v->fading_out = false;
        v->steal_pending = false;
        v->fade_gain = 1.0f;
        v->fade_step = 0.0f;
        v->playback_pos = 0;
    }
    memset(engine->rr_counters, 0, sizeof(engine->rr_counters));
}

void engine_init(AudioEngine *engine, float sample_rate) {
    memset(engine, 0, sizeof(*engine));

    for (int i = 0; i < MIXER_CHANNEL_COUNT; i++) {
        ChannelParams *p = &engine->channels[i];
        p->gain_db = 0.0f;
        p->pan = 0.0f;
        p->stereo_mode = true;   // matches the param default in params.c
        engine_update_channel(engine, (MixerChannel)i);
    }
    engine->master_gain_db = 0.0f;
    engine->master_gain_linear = 1.0f;
    for (int t = 0; t < TAB_COUNT; t++) {
        engine->tab_master_db[t] = 0.0f;
        engine->tab_master_linear[t] = 1.0f;
    }
    engine->output_trim_linear = db_to_linear(OUTPUT_TRIM_DB);

    engine_set_sample_rate(engine, sample_rate);
    engine_set_source_rate(engine, 48000);
    engine_reset_voices(engine);
}

void engine_update_channel(AudioEngine *engine, MixerChannel ch) {
    ChannelParams *p = &engine->channels[ch];
    p->gain_linear = db_to_linear(p->gain_db);

    // Mono sources: equal-power pan (0.707 either side at centre).
    float pan_norm = (p->pan + 1.0f) * 0.5f;
    p->pan_l = cosf(pan_norm * 1.5707963f);
    p->pan_r = sinf(pan_norm * 1.5707963f);

    // Stereo sources: balance law, unity both sides at centre so the recorded
    // stereo image passes through untouched.
    p->bal_l = (p->pan <= 0.0f) ? 1.0f : (1.0f - p->pan);
    p->bal_r = (p->pan >= 0.0f) ? 1.0f : (1.0f + p->pan);
}

void engine_update_master(AudioEngine *engine) {
    engine->master_gain_linear = db_to_linear(engine->master_gain_db);
}

void engine_update_tab_master(AudioEngine *engine, GuiTab tab) {
    engine->tab_master_linear[tab] = db_to_linear(engine->tab_master_db[tab]);
}

void engine_update_solo_state(AudioEngine *engine) {
    engine->any_solo = false;
    for (int i = 0; i < MIXER_CHANNEL_COUNT; i++) {
        if (engine->channels[i].solo) {
            engine->any_solo = true;
            return;
        }
    }
}

// Pick the velocity layer, and report where within that layer's band the
// velocity fell (0..1) so the caller can smooth the boundary step.
static int velocity_to_layer(float velocity, int num_layers, float *out_t) {
    float v = velocity * 127.0f;
    if (v < 1.0f) v = 1.0f;
    else if (v > 127.0f) v = 127.0f;

    float step = 127.0f / (float)num_layers;
    float fpos = (v - 1.0f) / step;
    int layer = (int)fpos;
    if (layer < 0) layer = 0;
    if (layer >= num_layers) layer = num_layers - 1;

    float t = fpos - (float)layer;
    if (t < 0.0f) t = 0.0f;
    else if (t > 1.0f) t = 1.0f;
    *out_t = t;
    return layer;
}

// The recorded layers already carry the dynamics — a dyn1 hit is intrinsically
// 15-27 dB quieter than dyn10 in this library — so velocity must not scale
// volume a second time. All that is left is a small tilt centred on the
// recorded level, which smooths the step at each layer boundary while leaving
// the mean level of every layer exactly as recorded.
static float velocity_layer_gain(float t) {
    return db_to_linear(VEL_TILT_DB * (t - 0.5f));
}

// Prefer a free slot. Otherwise steal by normalized progress, biased away from
// loud hits, so a nearly-finished kick dies before a crash that just started.
static int engine_alloc_voice(AudioEngine *engine) {
    for (int i = 0; i < MAX_VOICES; i++)
        if (!engine->voices[i].active) return i;

    int best = -1;
    float best_score = -1.0f;
    for (int i = 0; i < MAX_VOICES; i++) {
        Voice *v = &engine->voices[i];
        if (v->steal_pending) continue; // already reserved this block

        float progress = v->cur.max_frame_count
            ? (float)v->playback_pos / (float)v->cur.max_frame_count
            : 1.0f;
        float score = progress / (v->cur.velocity_gain + 0.05f);
        if (v->fading_out) score += 10.0f; // already dying, take it first

        if (score > best_score) { best_score = score; best = i; }
    }
    return best >= 0 ? best : 0;
}

void engine_note_on(AudioEngine *engine, const SampleBank *bank, const MidiMap *map,
                    int midi_note, float velocity) {
    if (midi_note < 0 || midi_note >= MAX_MIDI_NOTES) return;

    const MidiNoteSlot *slot = &map->slots[midi_note];
    if (slot->num_active == 0) return;

    // Handle choke from the first active variant
    const NoteVariant *first_var = &slot->variants[slot->active_indices[0]];
    if (first_var->is_choke_trigger && first_var->choke_group != CHOKE_NONE)
        engine_choke(engine, first_var->choke_group);

    int rr_base = engine->rr_counters[midi_note];
    // 5040 = LCM(1..7), so every round-robin cycle length divides it and the
    // sequence never jumps when the counter wraps.
    engine->rr_counters[midi_note] = (rr_base + 1) % 5040;

    // Derive the intra-layer tilt once per note, from the first active variant,
    // so multi-variant notes (the kick fires close + room) stay level-coherent
    // even if the two articulations have different layer counts.
    float tilt_t = 0.5f;
    {
        const NoteVariant *fv = &slot->variants[slot->active_indices[0]];
        const ArticulationSamples *fa = &bank->articulations[fv->articulation_index];
        if (fa->num_velocity_layers > 0)
            velocity_to_layer(velocity, fa->num_velocity_layers, &tilt_t);
    }
    float vel_gain = velocity_layer_gain(tilt_t);

    // Trigger a voice for each active variant (usually 1, kick needs 2: close + room)
    for (int ai = 0; ai < slot->num_active; ai++) {
        const NoteVariant *var = &slot->variants[slot->active_indices[ai]];
        const ArticulationSamples *art = &bank->articulations[var->articulation_index];
        if (art->num_velocity_layers <= 0 || art->num_round_robins <= 0) continue;

        float t_unused;
        int layer = velocity_to_layer(velocity, art->num_velocity_layers, &t_unused);
        int rr = rr_base % art->num_round_robins;

        uint32_t max_frames = 0;
        for (int m = 0; m < MIC_COUNT; m++) {
            if (!art->mics[m].available) continue;
            const SampleBuffer *buf = &art->mics[m].buffers[layer][rr];
            if (buf->loaded && buf->wav.num_frames > max_frames)
                max_frames = buf->wav.num_frames;
        }
        if (max_frames == 0) continue;

        VoiceProgram prog;
        prog.articulation_index = var->articulation_index;
        prog.drum_type          = var->drum_type;
        prog.velocity_layer     = layer;
        prog.round_robin        = rr;
        prog.velocity_gain      = vel_gain;
        prog.choke_group        = var->choke_group;
        prog.max_frame_count    = max_frames;

        Voice *v = &engine->voices[engine_alloc_voice(engine)];
        if (v->active) {
            // Slot is sounding: fade it out first, then retrigger in place.
            // Never overwrite a voice mid-sample — that is an instant step.
            v->pending       = prog;
            v->steal_pending = true;
            v->fading_out    = true;
            v->fade_step     = engine->steal_fade_samples > 0.0f
                             ? 1.0f / engine->steal_fade_samples : 1.0f;
        } else {
            v->cur           = prog;
            v->active        = true;
            v->fading_out    = false;
            v->steal_pending = false;
            v->fade_gain     = 1.0f;
            v->fade_step     = 0.0f;
            v->playback_pos  = 0;
        }
    }
}

void engine_note_off(AudioEngine *engine, int midi_note) {
    (void)engine; (void)midi_note;
}

void engine_choke(AudioEngine *engine, ChokeGroup group) {
    float step = engine->choke_fade_samples > 0.0f
               ? 1.0f / engine->choke_fade_samples : 1.0f;
    for (int i = 0; i < MAX_VOICES; i++) {
        Voice *v = &engine->voices[i];
        if (v->active && v->cur.choke_group == group && !v->fading_out) {
            v->fading_out = true;
            v->fade_gain = 1.0f;
            v->fade_step = step;
        }
    }
}

void engine_render(AudioEngine *engine, const SampleBank *bank,
                   StereoOut *outs, uint32_t num_ports, uint32_t num_frames) {
    // Clear all output buffers
    for (uint32_t p = 0; p < num_ports; p++) {
        if (outs[p].l) memset(outs[p].l, 0, num_frames * sizeof(float));
        if (outs[p].r) memset(outs[p].r, 0, num_frames * sizeof(float));
    }

    const bool have_master = (num_ports > 0 && outs[0].l && outs[0].r);
    const float trim = engine->output_trim_linear;
    const uint32_t tail = engine->tail_fade_frames;

    for (int vi = 0; vi < MAX_VOICES; vi++) {
        Voice *v = &engine->voices[vi];
        if (!v->active) continue;

        const ArticulationSamples *art = &bank->articulations[v->cur.articulation_index];
        float tab_gain = engine->tab_master_linear[drum_type_tab(v->cur.drum_type)];
        uint32_t pos = v->playback_pos;

        for (uint32_t i = 0; i < num_frames; i++) {
            // Resolve a completed fade before rendering this frame.
            if (v->fading_out && v->fade_gain <= 0.0f) {
                if (v->steal_pending) {
                    v->cur           = v->pending;
                    v->steal_pending = false;
                    v->fading_out    = false;
                    v->fade_gain     = 1.0f;
                    v->fade_step     = 0.0f;
                    pos              = 0;
                    art      = &bank->articulations[v->cur.articulation_index];
                    tab_gain = engine->tab_master_linear[drum_type_tab(v->cur.drum_type)];
                } else {
                    v->active = false;
                    break;
                }
            }

            if (pos >= v->cur.max_frame_count) {
                v->active = false;
                break;
            }

            float voice_gain = v->cur.velocity_gain * trim;
            if (v->fading_out) {
                voice_gain *= v->fade_gain;
                v->fade_gain -= v->fade_step;
            }

            for (int m = 0; m < MIC_COUNT; m++) {
                if (!art->mics[m].available) continue;
                const SampleBuffer *buf =
                    &art->mics[m].buffers[v->cur.velocity_layer][v->cur.round_robin];
                if (!buf->loaded || pos >= buf->wav.num_frames) continue;

                int mix_ch = get_mixer_channel(v->cur.drum_type, (MicPosition)m);
                if (mix_ch < 0) continue;

                const ChannelParams *ch = &engine->channels[mix_ch];
                if (ch->mute) continue;
                if (engine->any_solo && !ch->solo) continue;

                // Mics have different lengths; the shorter ones would otherwise
                // step straight to zero mid-hit. Ramp each one out at its own end.
                uint32_t remaining = buf->wav.num_frames - pos;
                float g = voice_gain;
                if (remaining < tail) g *= (float)remaining / (float)tail;
                if (ch->phase_invert) g = -g;
                g *= ch->gain_linear;

                // The overhead and room mics in this library are stereo; the
                // spot mics are mono. Keep recorded stereo intact unless the
                // channel is switched to mono.
                float sl, sr;
                if (buf->wav.num_channels >= 2) {
                    float ml, mr;
                    wav_frame_lr(&buf->wav, pos, &ml, &mr);
                    if (ch->stereo_mode) {
                        sl = ml * g * ch->bal_l;
                        sr = mr * g * ch->bal_r;
                    } else {
                        float mono = (ml + mr) * 0.5f * g;
                        sl = mono * ch->pan_l;
                        sr = mono * ch->pan_r;
                    }
                } else {
                    float mono = wav_sample_at(&buf->wav, pos) * g;
                    sl = mono * ch->pan_l;
                    sr = mono * ch->pan_r;
                }

                uint32_t ch_port = (uint32_t)mix_ch + 1;
                if (ch_port < num_ports && outs[ch_port].l) {
                    outs[ch_port].l[i] += sl;
                    outs[ch_port].r[i] += sr;
                }

                if (have_master) {
                    outs[0].l[i] += sl * tab_gain;
                    outs[0].r[i] += sr * tab_gain;
                }
            }

            pos++;
        }

        v->playback_pos = pos;
        if (v->active && pos >= v->cur.max_frame_count)
            v->active = false;
    }

    // Global master gain and safety saturation, port 0 only. The per-channel
    // stems stay clean so they can be summed externally.
    if (have_master) {
        float mg = engine->master_gain_linear;
        for (uint32_t i = 0; i < num_frames; i++) {
            outs[0].l[i] = soft_clip(outs[0].l[i] * mg);
            outs[0].r[i] = soft_clip(outs[0].r[i] * mg);
        }
    }
}
