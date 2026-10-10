#ifndef GGD_SAMPLE_BANK_H
#define GGD_SAMPLE_BANK_H

#include "types.h"
#include "wav_reader.h"
#include <stdatomic.h>

typedef struct cJSON cJSON;

typedef struct {
    WavFile  wav;       // memory-mapped WAV file
    bool     loaded;
} SampleBuffer;

// One mic's samples for an articulation, [layer][round robin], row-major.
typedef struct {
    SampleBuffer *buffers;
    int num_velocity_layers;
    int num_round_robins;
    bool available;
} MicSampleSet;

static inline const SampleBuffer *mic_buffer(const MicSampleSet *ms, int layer, int rr) {
    if (layer >= ms->num_velocity_layers || rr >= ms->num_round_robins) return NULL;
    const SampleBuffer *b = &ms->buffers[layer * ms->num_round_robins + rr];
    return b->loaded ? b : NULL;
}

typedef struct {
    char id[128];
    MicSampleSet mics[MAX_LIB_MICS];
    int num_velocity_layers;   // max over mics
    int num_round_robins;      // max over mics
    uint32_t sample_rate;      // rate of this articulation's files
} ArticulationSamples;

typedef struct {
    ArticulationSamples *articulations;
    int                  num_articulations;
    int                  num_mics;
    char                 base_path[1024];   // the library's wav/ folder

    // Background prefetch thread; joined by sample_bank_free.
    atomic_bool          prefetch_cancel;
    bool                 prefetch_running;
#ifdef _WIN32
    void                *prefetch_thread;
#else
    unsigned long        prefetch_thread;
#endif
} SampleBank;

// Map every sample listed in the index's "articulations" object. mic_ids
// gives the library's mic order: index mic names are matched against it.
bool sample_bank_load(SampleBank *bank, const char *wav_dir, const cJSON *articulations,
                      const char *const *mic_ids, int num_mics);
void sample_bank_free(SampleBank *bank);
int  sample_bank_find(const SampleBank *bank, const char *id);

// Start background prefetch of all mapped samples into the OS page cache.
// Non-blocking; sample_bank_free stops and joins the thread.
void sample_bank_prefetch(SampleBank *bank);

#endif
