#ifndef GGD_AUDIO_ENGINE_H
#define GGD_AUDIO_ENGINE_H

#include "types.h"
#include "sample_bank.h"
#include "midi_map.h"
#include <math.h>

typedef struct {
    float gain_db;
    float pan;        // -1 L, 0 center, +1 R
    bool  mute;
    bool  solo;
    bool  phase_invert;
    bool  stereo_mode;
    // Cached linear values
    float gain_linear;
    float pan_l, pan_r;
} ChannelParams;

typedef struct {
    bool     active;
    int      articulation_index;
    DrumType drum_type;
    int      velocity_layer;
    int      round_robin;
    uint32_t playback_pos;
    float    velocity_gain;
    ChokeGroup choke_group;
    bool     fading_out;
    float    fade_gain;
    uint32_t max_frame_count;
} Voice;

typedef struct {
    Voice        voices[MAX_VOICES];
    int          rr_counters[MAX_MIDI_NOTES];
    ChannelParams channels[MIXER_CHANNEL_COUNT];
    float        master_gain_db;
    float        master_gain_linear;
    float        tab_master_db[TAB_COUNT];
    float        tab_master_linear[TAB_COUNT];
    bool         any_solo;
    float        sample_rate;
    float        choke_fade_samples;
} AudioEngine;

void engine_init(AudioEngine *engine, float sample_rate);
void engine_note_on(AudioEngine *engine, const SampleBank *bank, const MidiMap *map,
                    int midi_note, float velocity);
void engine_note_off(AudioEngine *engine, int midi_note);
void engine_choke(AudioEngine *engine, ChokeGroup group);
void engine_render(AudioEngine *engine, const SampleBank *bank,
                   float *out_l, float *out_r, uint32_t num_frames);
void engine_update_channel(AudioEngine *engine, MixerChannel ch);
void engine_update_master(AudioEngine *engine);
void engine_update_tab_master(AudioEngine *engine, GuiTab tab);
void engine_update_solo_state(AudioEngine *engine);

#endif
