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
    bool  stereo_mode; // true: keep stereo sources in stereo; false: fold to mono
    // Cached linear values
    float gain_linear;
    float pan_l, pan_r; // equal-power pan, for mono sources
    float bal_l, bal_r; // balance law (unity at centre), for stereo sources
} ChannelParams;

// Everything needed to play one articulation. Split out from Voice so a voice
// being stolen can fade out and then retrigger with the new program, instead of
// being overwritten mid-sample.
typedef struct {
    int        articulation_index;
    DrumType   drum_type;
    int        velocity_layer;
    int        round_robin;
    float      velocity_gain;
    ChokeGroup choke_group;
    uint32_t   max_frame_count;
} VoiceProgram;

typedef struct {
    bool         active;
    bool         fading_out;
    bool         steal_pending; // fade out, then start `pending`
    float        fade_gain;
    float        fade_step;     // per-sample decrement; choke and steal differ
    uint32_t     playback_pos;
    VoiceProgram cur;
    VoiceProgram pending;
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
    float        sample_rate;        // host rate
    uint32_t     src_rate;           // sample bank rate, for source-frame maths
    float        choke_fade_samples;
    float        steal_fade_samples;
    uint32_t     tail_fade_frames;   // in SOURCE frames
    float        output_trim_linear;
} AudioEngine;

// Full reset, including all mixer state. Call once from plug_init.
void engine_init(AudioEngine *engine, float sample_rate);
// Update rate-dependent caches only — does NOT touch mixer state, so an
// activate/deactivate cycle cannot wipe a restored preset.
void engine_set_sample_rate(AudioEngine *engine, float sample_rate);
// Tell the engine the sample bank's rate, once the bank is loaded.
void engine_set_source_rate(AudioEngine *engine, uint32_t src_rate);
// Silence all voices without disturbing the mixer.
void engine_reset_voices(AudioEngine *engine);
void engine_note_on(AudioEngine *engine, const SampleBank *bank, const MidiMap *map,
                    int midi_note, float velocity);
void engine_note_off(AudioEngine *engine, int midi_note);
void engine_choke(AudioEngine *engine, ChokeGroup group);
// Output buffer pair (left + right) for one port
typedef struct {
    float *l;
    float *r;
} StereoOut;

// Render to multiple output ports.
// outs[0] = master (full mix), outs[1..N] = per mixer channel.
// num_ports may be less than NUM_OUTPUT_PORTS if host doesn't provide all.
void engine_render(AudioEngine *engine, const SampleBank *bank,
                   StereoOut *outs, uint32_t num_ports, uint32_t num_frames);
void engine_update_channel(AudioEngine *engine, MixerChannel ch);
void engine_update_master(AudioEngine *engine);
void engine_update_tab_master(AudioEngine *engine, GuiTab tab);
void engine_update_solo_state(AudioEngine *engine);

#endif
