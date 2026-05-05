#include "sample_bank.h"
#include "wav_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

int sample_bank_find(const SampleBank *bank, const char *prefix) {
    for (int i = 0; i < bank->num_articulations; i++) {
        if (strcmp(bank->articulations[i].prefix, prefix) == 0)
            return i;
    }
    return -1;
}

static int find_or_create_articulation(SampleBank *bank, const char *prefix) {
    int idx = sample_bank_find(bank, prefix);
    if (idx >= 0) return idx;
    if (bank->num_articulations >= MAX_ARTICULATIONS) return -1;

    idx = bank->num_articulations++;
    ArticulationSamples *art = &bank->articulations[idx];
    memset(art, 0, sizeof(*art));
    strncpy(art->prefix, prefix, sizeof(art->prefix) - 1);
    return idx;
}

// Parse filename: {MicDir}_{DrumName}_dyn{N}_rr{M}.wav
// The mic dir prefix is stripped since we know which mic dir we're scanning.
// So we parse: {ArticulationPrefix}_dyn{N}_rr{M}.wav
static bool parse_sample_filename(const char *mic_dir_name, const char *filename,
                                  char *out_prefix, int *out_dyn, int *out_rr) {
    // filename starts with mic_dir_name + "_"
    size_t mic_len = strlen(mic_dir_name);
    if (strncmp(filename, mic_dir_name, mic_len) != 0 || filename[mic_len] != '_')
        return false;

    const char *rest = filename + mic_len + 1;

    // Find _dyn{N}_rr{M}.wav from the end
    const char *dyn_ptr = strstr(rest, "_dyn");
    if (!dyn_ptr) return false;

    // Copy prefix (everything before _dyn)
    size_t prefix_len = dyn_ptr - rest;
    if (prefix_len >= 128) return false;
    memcpy(out_prefix, rest, prefix_len);
    out_prefix[prefix_len] = '\0';

    // Parse _dyn{N}_rr{M}.wav
    int dyn, rr;
    if (sscanf(dyn_ptr, "_dyn%d_rr%d", &dyn, &rr) != 2)
        return false;

    *out_dyn = dyn;
    *out_rr = rr;
    return true;
}

static bool load_mic_directory(SampleBank *bank, MicPosition mic, const char *dir_path) {
    const char *mic_name = MIC_DIR_NAMES[mic];
    DIR *d = opendir(dir_path);
    if (!d) return false;

    struct dirent *ent;
    int loaded = 0;

    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        size_t namelen = strlen(ent->d_name);
        if (namelen < 5 || strcmp(ent->d_name + namelen - 4, ".wav") != 0)
            continue;

        char prefix[128];
        int dyn, rr;
        if (!parse_sample_filename(mic_name, ent->d_name, prefix, &dyn, &rr))
            continue;

        int art_idx = find_or_create_articulation(bank, prefix);
        if (art_idx < 0) continue;

        ArticulationSamples *art = &bank->articulations[art_idx];
        MicSampleSet *ms = &art->mics[mic];

        if (dyn < 1 || dyn > MAX_VELOCITY_LAYERS || rr < 1 || rr > MAX_ROUND_ROBINS)
            continue;

        // Build full path
        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, ent->d_name);

        WavFile wav;
        if (!wav_read(filepath, &wav)) {
            fprintf(stderr, "ggd-libre: failed to read %s\n", filepath);
            continue;
        }

        SampleBuffer *buf = &ms->buffers[dyn - 1][rr - 1];
        buf->data = wav.samples;
        buf->frame_count = wav.num_frames;

        if (bank->sample_rate == 0)
            bank->sample_rate = wav.sample_rate;

        if (dyn > ms->num_velocity_layers) ms->num_velocity_layers = dyn;
        if (rr > ms->num_round_robins) ms->num_round_robins = rr;
        ms->available = true;

        if (dyn > art->num_velocity_layers) art->num_velocity_layers = dyn;
        if (rr > art->num_round_robins) art->num_round_robins = rr;

        loaded++;
    }

    closedir(d);
    fprintf(stderr, "ggd-libre: loaded %d samples from %s\n", loaded, mic_name);
    return true;
}

bool sample_bank_load(SampleBank *bank, const char *wav_base_dir) {
    memset(bank, 0, sizeof(*bank));
    strncpy(bank->base_path, wav_base_dir, sizeof(bank->base_path) - 1);

    bank->articulations = calloc(MAX_ARTICULATIONS, sizeof(ArticulationSamples));
    if (!bank->articulations) return false;

    for (int mic = 0; mic < MIC_COUNT; mic++) {
        char dir_path[1024];
        snprintf(dir_path, sizeof(dir_path), "%s/%s", wav_base_dir, MIC_DIR_NAMES[mic]);
        load_mic_directory(bank, (MicPosition)mic, dir_path);
    }

    fprintf(stderr, "ggd-libre: %d articulations loaded\n", bank->num_articulations);
    return bank->num_articulations > 0;
}

void sample_bank_free(SampleBank *bank) {
    if (!bank->articulations) return;
    for (int a = 0; a < bank->num_articulations; a++) {
        for (int m = 0; m < MIC_COUNT; m++) {
            MicSampleSet *ms = &bank->articulations[a].mics[m];
            for (int v = 0; v < MAX_VELOCITY_LAYERS; v++) {
                for (int r = 0; r < MAX_ROUND_ROBINS; r++) {
                    free(ms->buffers[v][r].data);
                }
            }
        }
    }
    free(bank->articulations);
    bank->articulations = NULL;
    bank->num_articulations = 0;
}
