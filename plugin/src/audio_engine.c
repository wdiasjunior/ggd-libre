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

void engine_reset_voices(AudioEngine *engine) {
    for (int i = 0; i < MAX_VOICES; i++) {
        Voice *v = &engine->voices[i];
        v->active = false;
        v->fading_out = false;
        v->steal_pending = false;
        v->fade_gain = 1.0f;
        v->fade_step = 0.0f;
        v->playback_pos = 0.0;
    }
    memset(engine->rr_counters, 0, sizeof(engine->rr_counters));
}

void engine_init(AudioEngine *engine, float sample_rate) {
    memset(engine, 0, sizeof(*engine));
    engine->master_gain_db = 0.0f;
    engine->master_gain_linear = 1.0f;
    engine->output_trim_linear = db_to_linear(OUTPUT_TRIM_DB);
    engine_set_sample_rate(engine, sample_rate);
    engine_reset_voices(engine);
}

void engine_update_master(AudioEngine *engine) {
    engine->master_gain_linear = db_to_linear(engine->master_gain_db);
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

// The recorded layers already carry the dynamics — a soft hit is intrinsically
// 15-27 dB quieter than a hard one — so velocity must not scale volume a
// second time. All that is left is a small tilt centred on the recorded level,
// which smooths the step at each layer boundary while leaving the mean level of
// every layer exactly as recorded.
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

void engine_note_on(AudioEngine *engine, const LibraryRuntime *rt, const LibMix *mix,
                    int midi_note, float velocity) {
    if (!rt || midi_note < 0 || midi_note >= MAX_MIDI_NOTES) return;

    const MidiNoteSlot *slot = &rt->map.slots[midi_note];
    const ArtList *arts = midi_map_resolve(&rt->map, midi_note, mix->selector);
    if (arts->count == 0) return;
    const SampleBank *bank = &rt->bank;

    if (slot->is_choke_trigger && slot->choke_group != CHOKE_NONE)
        engine_choke(engine, slot->choke_group);

    int rr_base = engine->rr_counters[midi_note];
    // 5040 is divisible by every round-robin count from 1 to 10 (and 12), so the
    // sequence never jumps when the counter wraps.
    engine->rr_counters[midi_note] = (rr_base + 1) % 5040;

    // Derive the intra-layer tilt once per note, from the first articulation,
    // so multi-articulation notes (the Halpern kick fires close + room) stay
    // level-coherent even if the two have different layer counts.
    float tilt_t = 0.5f;
    {
        const ArticulationSamples *fa = &bank->articulations[arts->arts[0]];
        if (fa->num_velocity_layers > 0)
            velocity_to_layer(velocity, fa->num_velocity_layers, &tilt_t);
    }
    float vel_gain = velocity_layer_gain(tilt_t);

    for (int ai = 0; ai < arts->count; ai++) {
        int art_idx = arts->arts[ai];
        const ArticulationSamples *art = &bank->articulations[art_idx];
        if (art->num_velocity_layers <= 0 || art->num_round_robins <= 0) continue;

        float t_unused;
        int layer = velocity_to_layer(velocity, art->num_velocity_layers, &t_unused);
        int rr = rr_base % art->num_round_robins;

        uint32_t max_frames = 0;
        for (int m = 0; m < bank->num_mics; m++) {
            if (!art->mics[m].available) continue;
            const SampleBuffer *buf = mic_buffer(&art->mics[m], layer, rr);
            if (buf && buf->wav.num_frames > max_frames)
                max_frames = buf->wav.num_frames;
        }
        if (max_frames == 0) continue;

        uint32_t src_rate = art->sample_rate ? art->sample_rate : 48000;
        uint32_t tail = (uint32_t)((MIC_TAIL_FADE_MS / 1000.0f) * (float)src_rate);

        VoiceProgram prog;
        prog.articulation_index = art_idx;
        prog.drum_type          = slot->drum_type;
        prog.velocity_layer     = layer;
        prog.round_robin        = rr;
        prog.velocity_gain      = vel_gain;
        prog.choke_group        = slot->choke_group;
        prog.max_frame_count    = max_frames;
        prog.step               = engine->sample_rate > 0.0f
                                ? (double)src_rate / (double)engine->sample_rate : 1.0;
        prog.tail_fade_frames   = tail ? tail : 1;

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
            v->playback_pos  = 0.0;
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

void engine_fade_all(AudioEngine *engine) {
    float step = engine->choke_fade_samples > 0.0f
               ? 1.0f / engine->choke_fade_samples : 1.0f;
    for (int i = 0; i < MAX_VOICES; i++) {
        Voice *v = &engine->voices[i];
        if (!v->active) continue;
        v->steal_pending = false;   // a pending retrigger would outlive the fade
        if (!v->fading_out) v->fade_gain = 1.0f;
        v->fading_out = true;
        if (v->fade_step < step) v->fade_step = step;
    }
}

bool engine_any_active(const AudioEngine *engine) {
    for (int i = 0; i < MAX_VOICES; i++)
        if (engine->voices[i].active) return true;
    return false;
}

// One frame of a mic, as a stereo pair. Mono sources yield the same value on
// both sides. Off-grid positions use 4-point Hermite interpolation; positions
// past the end read as silence.
static inline void frame_at(const WavFile *w, int64_t idx, float *l, float *r) {
    if (idx < 0) idx = 0;
    if (idx >= (int64_t)w->num_frames) { *l = *r = 0.0f; return; }
    wav_frame_lr(w, (uint32_t)idx, l, r);
}

static inline float hermite(float y0, float y1, float y2, float y3, float f) {
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

static inline void read_interp(const WavFile *w, uint32_t ipos, float frac,
                               float *out_l, float *out_r) {
    float l0, r0, l1, r1, l2, r2, l3, r3;
    frame_at(w, (int64_t)ipos - 1, &l0, &r0);
    frame_at(w, (int64_t)ipos,     &l1, &r1);
    frame_at(w, (int64_t)ipos + 1, &l2, &r2);
    frame_at(w, (int64_t)ipos + 2, &l3, &r3);
    *out_l = hermite(l0, l1, l2, l3, frac);
    *out_r = hermite(r0, r1, r2, r3, frac);
}

void engine_render(AudioEngine *engine, const LibraryRuntime *rt, const LibMix *mix,
                   StereoOut *outs, uint32_t num_ports, uint32_t num_frames) {
    // Clear all output buffers
    for (uint32_t p = 0; p < num_ports; p++) {
        if (outs[p].l) memset(outs[p].l, 0, num_frames * sizeof(float));
        if (outs[p].r) memset(outs[p].r, 0, num_frames * sizeof(float));
    }
    if (!rt) return;

    const SampleBank *bank = &rt->bank;
    const int num_mics = bank->num_mics;
    const bool have_master = (num_ports > 0 && outs[0].l && outs[0].r);
    const float trim = engine->output_trim_linear;

    for (int vi = 0; vi < MAX_VOICES; vi++) {
        Voice *v = &engine->voices[vi];
        if (!v->active) continue;

        const ArticulationSamples *art = &bank->articulations[v->cur.articulation_index];
        float tab_gain = mix->tab_master_linear[drum_type_tab(v->cur.drum_type)];
        double pos = v->playback_pos;

        for (uint32_t i = 0; i < num_frames; i++) {
            // Resolve a completed fade before rendering this frame.
            if (v->fading_out && v->fade_gain <= 0.0f) {
                if (v->steal_pending) {
                    v->cur           = v->pending;
                    v->steal_pending = false;
                    v->fading_out    = false;
                    v->fade_gain     = 1.0f;
                    v->fade_step     = 0.0f;
                    pos              = 0.0;
                    art      = &bank->articulations[v->cur.articulation_index];
                    tab_gain = mix->tab_master_linear[drum_type_tab(v->cur.drum_type)];
                } else {
                    v->active = false;
                    break;
                }
            }

            if (pos >= (double)v->cur.max_frame_count) {
                v->active = false;
                break;
            }

            float voice_gain = v->cur.velocity_gain * trim;
            if (v->fading_out) {
                voice_gain *= v->fade_gain;
                v->fade_gain -= v->fade_step;
            }

            const uint32_t ipos = (uint32_t)pos;
            const float frac = (float)(pos - (double)ipos);
            const float tail = (float)v->cur.tail_fade_frames;
            const int8_t *route = rt->route[v->cur.drum_type];

            for (int m = 0; m < num_mics; m++) {
                if (!art->mics[m].available) continue;
                const SampleBuffer *buf = mic_buffer(&art->mics[m], v->cur.velocity_layer,
                                                     v->cur.round_robin);
                if (!buf || ipos >= buf->wav.num_frames) continue;

                int mix_ch = route[m];
                if (mix_ch < 0) continue;

                const ChannelParams *ch = &mix->channels[mix_ch];
                if (ch->mute) continue;
                if (mix->any_solo && !ch->solo) continue;

                // Mics have different lengths; the shorter ones would otherwise
                // step straight to zero mid-hit. Ramp each one out at its own end.
                float remaining = (float)((double)buf->wav.num_frames - pos);
                float g = voice_gain;
                if (remaining < tail) g *= remaining / tail;
                if (ch->phase_invert) g = -g;
                g *= ch->gain_linear;

                // Overhead and room mics are stereo, spot mics are mono. Keep
                // recorded stereo intact unless the channel is switched to mono.
                float ml, mr;
                if (frac == 0.0f) {
                    if (buf->wav.num_channels >= 2) wav_frame_lr(&buf->wav, ipos, &ml, &mr);
                    else ml = mr = wav_sample_at(&buf->wav, ipos);
                } else {
                    read_interp(&buf->wav, ipos, frac, &ml, &mr);
                }

                float sl, sr;
                if (buf->wav.num_channels >= 2) {
                    if (ch->stereo_mode) {
                        sl = ml * g * ch->bal_l;
                        sr = mr * g * ch->bal_r;
                    } else {
                        float mono = (ml + mr) * 0.5f * g;
                        sl = mono * ch->pan_l;
                        sr = mono * ch->pan_r;
                    }
                } else {
                    float mono = ml * g;
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

            pos += v->cur.step;
        }

        v->playback_pos = pos;
        if (v->active && pos >= (double)v->cur.max_frame_count)
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
