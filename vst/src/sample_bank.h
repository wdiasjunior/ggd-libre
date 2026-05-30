#ifndef GGD_SAMPLE_BANK_H
#define GGD_SAMPLE_BANK_H

#include "types.h"
#include "wav_reader.h"

typedef struct {
    WavFile  wav;       // memory-mapped WAV file
    bool     loaded;
} SampleBuffer;

typedef struct {
    SampleBuffer buffers[MAX_VELOCITY_LAYERS][MAX_ROUND_ROBINS];
    int num_velocity_layers;
    int num_round_robins;
    bool available;
} MicSampleSet;

typedef struct {
    char prefix[128];
    MicSampleSet mics[MIC_COUNT];
    int num_velocity_layers;
    int num_round_robins;
} ArticulationSamples;

typedef struct {
    ArticulationSamples *articulations;
    int                  num_articulations;
    uint32_t             sample_rate;
    char                 base_path[512];
} SampleBank;

bool sample_bank_load(SampleBank *bank, const char *wav_base_dir);
void sample_bank_free(SampleBank *bank);
int  sample_bank_find(const SampleBank *bank, const char *prefix);

// Start background prefetch of all mapped samples into the OS page cache.
// Non-blocking — returns immediately, prefetching happens in a background thread.
void sample_bank_prefetch(SampleBank *bank);

#endif
