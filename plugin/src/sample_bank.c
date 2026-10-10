#include "sample_bank.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <sys/mman.h>
#include <pthread.h>
#endif

int sample_bank_find(const SampleBank *bank, const char *id) {
    for (int i = 0; i < bank->num_articulations; i++) {
        if (strcmp(bank->articulations[i].id, id) == 0)
            return i;
    }
    return -1;
}

static int mic_index(const char *name, const char *const *mic_ids, int num_mics) {
    for (int m = 0; m < num_mics; m++)
        if (strcmp(mic_ids[m], name) == 0) return m;
    return -1;
}

// Load one mic's [layer][rr] grid of relative paths.
static int load_mic(ArticulationSamples *art, MicSampleSet *ms, const char *wav_dir,
                    const cJSON *layers) {
    int n_layers = cJSON_GetArraySize(layers);
    int n_rr = 0;
    const cJSON *layer;
    cJSON_ArrayForEach(layer, layers) {
        int n = cJSON_GetArraySize(layer);
        if (n > n_rr) n_rr = n;
    }
    if (n_layers == 0 || n_rr == 0) return 0;

    ms->buffers = calloc((size_t)n_layers * n_rr, sizeof(SampleBuffer));
    if (!ms->buffers) return 0;
    ms->num_velocity_layers = n_layers;
    ms->num_round_robins = n_rr;

    int loaded = 0, l = 0;
    cJSON_ArrayForEach(layer, layers) {
        int r = 0;
        const cJSON *path;
        cJSON_ArrayForEach(path, layer) {
            if (cJSON_IsString(path)) {
                char filepath[2048];
                snprintf(filepath, sizeof(filepath), "%s/%s", wav_dir, path->valuestring);
                SampleBuffer *buf = &ms->buffers[l * n_rr + r];
                if (wav_open(filepath, &buf->wav)) {
                    buf->loaded = true;
                    if (art->sample_rate == 0) art->sample_rate = buf->wav.sample_rate;
                    loaded++;
                } else {
                    fprintf(stderr, "ggd-libre: failed to mmap %s\n", filepath);
                }
            }
            r++;
        }
        l++;
    }
    if (loaded > 0) {
        ms->available = true;
        if (n_layers > art->num_velocity_layers) art->num_velocity_layers = n_layers;
        if (n_rr > art->num_round_robins) art->num_round_robins = n_rr;
    }
    return loaded;
}

bool sample_bank_load(SampleBank *bank, const char *wav_dir, const cJSON *articulations,
                      const char *const *mic_ids, int num_mics) {
    memset(bank, 0, sizeof(*bank));
    atomic_init(&bank->prefetch_cancel, false);
    strncpy(bank->base_path, wav_dir, sizeof(bank->base_path) - 1);
    bank->num_mics = num_mics;

    int n_arts = cJSON_GetArraySize(articulations);
    if (n_arts <= 0) return false;
    bank->articulations = calloc((size_t)n_arts, sizeof(ArticulationSamples));
    if (!bank->articulations) return false;

    int total = 0;
    const cJSON *art_json;
    cJSON_ArrayForEach(art_json, articulations) {
        ArticulationSamples *art = &bank->articulations[bank->num_articulations];
        strncpy(art->id, art_json->string, sizeof(art->id) - 1);

        const cJSON *mic_json;
        cJSON_ArrayForEach(mic_json, art_json) {
            int m = mic_index(mic_json->string, mic_ids, num_mics);
            if (m < 0) {
                fprintf(stderr, "ggd-libre: %s: unknown mic '%s'\n", art->id, mic_json->string);
                continue;
            }
            total += load_mic(art, &art->mics[m], wav_dir, mic_json);
        }
        if (art->num_velocity_layers > 0)
            bank->num_articulations++;
        else
            memset(art, 0, sizeof(*art));
    }

    fprintf(stderr, "ggd-libre: mapped %d samples, %d articulations from %s\n",
            total, bank->num_articulations, wav_dir);
    return bank->num_articulations > 0;
}

// ---------- Background prefetch ----------

#define FOR_EACH_BUFFER(bank, buf)                                              \
    for (int a_ = 0; a_ < (bank)->num_articulations; a_++)                      \
        for (int m_ = 0; m_ < MAX_LIB_MICS; m_++)                               \
            for (int i_ = 0, n_ = (bank)->articulations[a_].mics[m_].num_velocity_layers * \
                                  (bank)->articulations[a_].mics[m_].num_round_robins;      \
                 i_ < n_; i_++)                                                 \
                for (SampleBuffer *buf = &(bank)->articulations[a_].mics[m_].buffers[i_]; \
                     buf; buf = NULL)

static void stop_prefetch(SampleBank *bank) {
    if (!bank->prefetch_running) return;
    atomic_store(&bank->prefetch_cancel, true);
#ifdef _WIN32
    WaitForSingleObject((HANDLE)bank->prefetch_thread, INFINITE);
    CloseHandle((HANDLE)bank->prefetch_thread);
#else
    pthread_join((pthread_t)bank->prefetch_thread, NULL);
#endif
    bank->prefetch_running = false;
}

void sample_bank_free(SampleBank *bank) {
    stop_prefetch(bank);
    if (!bank->articulations) return;
    for (int a = 0; a < bank->num_articulations; a++) {
        for (int m = 0; m < MAX_LIB_MICS; m++) {
            MicSampleSet *ms = &bank->articulations[a].mics[m];
            int n = ms->num_velocity_layers * ms->num_round_robins;
            for (int i = 0; i < n; i++)
                if (ms->buffers[i].loaded) wav_close(&ms->buffers[i].wav);
            free(ms->buffers);
        }
    }
    free(bank->articulations);
    bank->articulations = NULL;
    bank->num_articulations = 0;
}

// Touch every page in a mapped region to warm the OS page cache.
// volatile prevents the compiler from optimizing out the reads.
static void prefetch_region(SampleBank *bank, const void *base, size_t size) {
    const volatile unsigned char *p = (const volatile unsigned char *)base;
    volatile unsigned char sink = 0;
    for (size_t off = 0; off < size; off += 4096) {
        if ((off & 0xFFFFF) == 0 && atomic_load(&bank->prefetch_cancel)) return;
        sink += p[off];
    }
    (void)sink;
}

static void prefetch_all_samples(SampleBank *bank) {
    int count = 0;
    FOR_EACH_BUFFER(bank, buf) {
        if (atomic_load(&bank->prefetch_cancel)) return;
        if (buf->loaded && buf->wav.map_base) {
            prefetch_region(bank, buf->wav.map_base, buf->wav.map_size);
            count++;
        }
    }
    fprintf(stderr, "ggd-libre: prefetched %d sample files into page cache\n", count);
}

#ifdef _WIN32
static unsigned __stdcall prefetch_thread_func(void *arg) {
    prefetch_all_samples((SampleBank *)arg);
    return 0;
}
#else
static void *prefetch_thread_func(void *arg) {
    prefetch_all_samples((SampleBank *)arg);
    return NULL;
}
#endif

void sample_bank_prefetch(SampleBank *bank) {
    if (!bank->articulations || bank->num_articulations == 0 || bank->prefetch_running) return;

#ifdef __linux__
    // Hint the kernel to read ahead all mapped files
    FOR_EACH_BUFFER(bank, buf) {
        if (buf->loaded && buf->wav.map_base)
            madvise(buf->wav.map_base, buf->wav.map_size, MADV_WILLNEED);
    }
#endif

    // Also touch every page from a background thread (works on both platforms,
    // and ensures pages are actually faulted in on Windows where there's no madvise)
    atomic_store(&bank->prefetch_cancel, false);
#ifdef _WIN32
    uintptr_t h = _beginthreadex(NULL, 0, prefetch_thread_func, bank, 0, NULL);
    if (h) { bank->prefetch_thread = (void *)h; bank->prefetch_running = true; }
#else
    pthread_t thread;
    if (pthread_create(&thread, NULL, prefetch_thread_func, bank) == 0) {
        bank->prefetch_thread = (unsigned long)thread;
        bank->prefetch_running = true;
    }
#endif
}
