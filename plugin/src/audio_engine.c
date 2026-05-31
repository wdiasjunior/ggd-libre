#include "audio_engine.h"
#include <string.h>
#include <math.h>

static float db_to_linear(float db) {
    if (db <= -80.0f) return 0.0f;
    return powf(10.0f, db / 20.0f);
}

void engine_init(AudioEngine *engine, float sample_rate) {
    memset(engine, 0, sizeof(*engine));
    engine->sample_rate = sample_rate;
    engine->choke_fade_samples = (CHOKE_FADE_MS / 1000.0f) * sample_rate;

    for (int i = 0; i < MIXER_CHANNEL_COUNT; i++) {
        engine->channels[i].gain_db = 0.0f;
        engine->channels[i].pan = 0.0f;
        engine->channels[i].gain_linear = 1.0f;
        engine->channels[i].pan_l = 0.707f;
        engine->channels[i].pan_r = 0.707f;
    }
    engine->master_gain_db = 0.0f;
    engine->master_gain_linear = 1.0f;
    for (int t = 0; t < TAB_COUNT; t++) {
        engine->tab_master_db[t] = 0.0f;
        engine->tab_master_linear[t] = 1.0f;
    }
}

void engine_update_channel(AudioEngine *engine, MixerChannel ch) {
    ChannelParams *p = &engine->channels[ch];
    p->gain_linear = db_to_linear(p->gain_db);
    float pan_norm = (p->pan + 1.0f) * 0.5f;
    p->pan_l = cosf(pan_norm * 1.5707963f);
    p->pan_r = sinf(pan_norm * 1.5707963f);
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

static int velocity_to_layer(float velocity, int num_layers) {
    int v = (int)(velocity * 127.0f);
    if (v < 1) v = 1;
    float step = 127.0f / num_layers;
    int layer = (int)((v - 1) / step);
    if (layer >= num_layers) layer = num_layers - 1;
    return layer;
}

static int find_free_voice(AudioEngine *engine) {
    uint32_t oldest_pos = 0;
    int oldest_idx = 0;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!engine->voices[i].active) return i;
        if (engine->voices[i].playback_pos > oldest_pos) {
            oldest_pos = engine->voices[i].playback_pos;
            oldest_idx = i;
        }
    }
    return oldest_idx; // steal oldest
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
    engine->rr_counters[midi_note]++;

    // Trigger a voice for each active variant (usually 1, kick needs 2: close + room)
    for (int ai = 0; ai < slot->num_active; ai++) {
        const NoteVariant *var = &slot->variants[slot->active_indices[ai]];
        const ArticulationSamples *art = &bank->articulations[var->articulation_index];

        int layer = velocity_to_layer(velocity, art->num_velocity_layers);
        int rr = rr_base % art->num_round_robins;

        uint32_t max_frames = 0;
        for (int m = 0; m < MIC_COUNT; m++) {
            if (!art->mics[m].available) continue;
            const SampleBuffer *buf = &art->mics[m].buffers[layer][rr];
            if (buf->loaded && buf->wav.num_frames > max_frames)
                max_frames = buf->wav.num_frames;
        }
        if (max_frames == 0) continue;

        int voice_idx = find_free_voice(engine);
        Voice *v = &engine->voices[voice_idx];
        v->active = true;
        v->articulation_index = var->articulation_index;
        v->drum_type = var->drum_type;
        v->velocity_layer = layer;
        v->round_robin = rr;
        v->playback_pos = 0;
        v->velocity_gain = velocity;
        v->choke_group = var->choke_group;
        v->fading_out = false;
        v->fade_gain = 1.0f;
        v->max_frame_count = max_frames;
    }
}

void engine_note_off(AudioEngine *engine, int midi_note) {
    (void)engine; (void)midi_note;
}

void engine_choke(AudioEngine *engine, ChokeGroup group) {
    for (int i = 0; i < MAX_VOICES; i++) {
        Voice *v = &engine->voices[i];
        if (v->active && v->choke_group == group && !v->fading_out) {
            v->fading_out = true;
            v->fade_gain = 1.0f;
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

    float fade_decrement = 0.0f;
    if (engine->choke_fade_samples > 0)
        fade_decrement = 1.0f / engine->choke_fade_samples;

    for (int vi = 0; vi < MAX_VOICES; vi++) {
        Voice *v = &engine->voices[vi];
        if (!v->active) continue;

        const ArticulationSamples *art = &bank->articulations[v->articulation_index];
        GuiTab tab = drum_type_tab(v->drum_type);
        float tab_gain = engine->tab_master_linear[tab];

        for (uint32_t i = 0; i < num_frames; i++) {
            uint32_t pos = v->playback_pos + i;
            if (pos >= v->max_frame_count) {
                v->active = false;
                break;
            }

            float voice_fade = v->velocity_gain;
            if (v->fading_out) {
                voice_fade *= v->fade_gain;
                v->fade_gain -= fade_decrement;
                if (v->fade_gain <= 0.0f) {
                    v->active = false;
                    break;
                }
            }

            for (int m = 0; m < MIC_COUNT; m++) {
                if (!art->mics[m].available) continue;
                const SampleBuffer *buf = &art->mics[m].buffers[v->velocity_layer][v->round_robin];
                if (!buf->loaded || pos >= buf->wav.num_frames) continue;

                int mix_ch = get_mixer_channel(v->drum_type, (MicPosition)m);
                if (mix_ch < 0) continue;

                const ChannelParams *ch = &engine->channels[mix_ch];
                if (ch->mute) continue;
                if (engine->any_solo && !ch->solo) continue;

                float sample = wav_sample_at(&buf->wav, pos);
                float phase = ch->phase_invert ? -1.0f : 1.0f;
                float ch_sample = sample * voice_fade * ch->gain_linear * phase;
                float sl = ch_sample * ch->pan_l;
                float sr = ch_sample * ch->pan_r;

                // Write to per-channel output port
                uint32_t ch_port = (uint32_t)mix_ch + 1;
                if (ch_port < num_ports && outs[ch_port].l) {
                    outs[ch_port].l[i] += sl;
                    outs[ch_port].r[i] += sr;
                }

                // Crashes also route through the cymbal room channel for that mic
                // so the cymbal OH/Near/Far faders affect crashes too
                if (v->drum_type == DRUM_LCRASH || v->drum_type == DRUM_RCRASH) {
                    int room_ch = -1;
                    if (m == MIC_OH)        room_ch = CH_CYMBALS_OH;
                    else if (m == MIC_NEAR_ROOM) room_ch = CH_CYMBALS_NEAR;
                    else if (m == MIC_FAR_ROOM)  room_ch = CH_CYMBALS_FAR;

                    if (room_ch >= 0) {
                        const ChannelParams *rch = &engine->channels[room_ch];
                        if (!rch->mute && !(engine->any_solo && !rch->solo)) {
                            uint32_t rport = (uint32_t)room_ch + 1;
                            float room_sample = sample * voice_fade * rch->gain_linear
                                                * (rch->phase_invert ? -1.0f : 1.0f);
                            if (rport < num_ports && outs[rport].l) {
                                outs[rport].l[i] += room_sample * rch->pan_l;
                                outs[rport].r[i] += room_sample * rch->pan_r;
                            }
                        }
                    }
                }

                // Accumulate to master (port 0) with tab + global master gains
                if (outs[0].l) {
                    float master_sample = ch_sample * tab_gain;
                    outs[0].l[i] += master_sample * ch->pan_l;
                    outs[0].r[i] += master_sample * ch->pan_r;
                }
            }
        }

        if (v->active) {
            v->playback_pos += num_frames;
            if (v->playback_pos >= v->max_frame_count)
                v->active = false;
        }
    }

    // Apply global master gain to port 0 only
    if (outs[0].l) {
        for (uint32_t i = 0; i < num_frames; i++) {
            outs[0].l[i] *= engine->master_gain_linear;
            outs[0].r[i] *= engine->master_gain_linear;
        }
    }
}
