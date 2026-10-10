#ifndef GGD_AUDIO_ENGINE_H
#define GGD_AUDIO_ENGINE_H

#include "types.h"
#include "library.h"
#include <math.h>

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
    uint32_t   max_frame_count;   // in SOURCE frames
    double     step;              // source frames per host frame
    uint32_t   tail_fade_frames;  // in SOURCE frames
} VoiceProgram;

typedef struct {
    bool         active;
    bool         fading_out;
    bool         steal_pending; // fade out, then start `pending`
    float        fade_gain;
    float        fade_step;     // per-sample decrement; choke and steal differ
    double       playback_pos;  // in source frames; fractional when resampling
    VoiceProgram cur;
    VoiceProgram pending;
} Voice;

typedef struct {
    Voice        voices[MAX_VOICES];
    int          rr_counters[MAX_MIDI_NOTES];
    float        master_gain_db;
    float        master_gain_linear;
    float        sample_rate;        // host rate
    float        choke_fade_samples;
    float        steal_fade_samples;
    float        output_trim_linear;
} AudioEngine;

// Full reset. Call once from plug_init.
void engine_init(AudioEngine *engine, float sample_rate);
// Update rate-dependent caches only.
void engine_set_sample_rate(AudioEngine *engine, float sample_rate);
// Silence all voices.
void engine_reset_voices(AudioEngine *engine);
void engine_note_on(AudioEngine *engine, const LibraryRuntime *rt, const LibMix *mix,
                    int midi_note, float velocity);
void engine_note_off(AudioEngine *engine, int midi_note);
void engine_choke(AudioEngine *engine, ChokeGroup group);
// Start a short fade on every voice (library switch, CLAP note choke).
void engine_fade_all(AudioEngine *engine);
bool engine_any_active(const AudioEngine *engine);
void engine_update_master(AudioEngine *engine);

// Output buffer pair (left + right) for one port
typedef struct {
    float *l;
    float *r;
} StereoOut;

// Render to multiple output ports.
// outs[0] = master (full mix), outs[1..N] = per mixer channel.
// rt may be NULL (no library loaded): outputs are cleared.
void engine_render(AudioEngine *engine, const LibraryRuntime *rt, const LibMix *mix,
                   StereoOut *outs, uint32_t num_ports, uint32_t num_frames);

#endif
