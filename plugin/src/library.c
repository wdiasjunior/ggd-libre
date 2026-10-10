#include "library.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
static bool file_exists(const char *path) {
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
static void resolve_absolute(const char *path, char *out, size_t out_size) {
    if (!_fullpath(out, path, (int)out_size))
        snprintf(out, out_size, "%s", path);
}
#else
#include <sys/stat.h>
static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}
static void resolve_absolute(const char *path, char *out, size_t out_size) {
    char tmp[4096];
    if (realpath(path, tmp)) snprintf(out, out_size, "%s", tmp);
    else snprintf(out, out_size, "%s", path);
}
#endif

const LibraryDef *library_def(int lib) {
    switch (lib) {
    case LIB_HALPERN: return &LIB_DEF_HALPERN;
    case LIB_OKW:     return &LIB_DEF_OKW;
    case LIB_PV:      return &LIB_DEF_PV;
    default:          return NULL;
    }
}

// ---------- Mixer state ----------

static float db_to_linear(float db) {
    if (db <= -80.0f) return 0.0f;
    return powf(10.0f, db / 20.0f);
}

void libmix_update_channel(LibMix *mix, int ch) {
    ChannelParams *p = &mix->channels[ch];
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

void libmix_update_tab_master(LibMix *mix, GuiTab tab) {
    mix->tab_master_linear[tab] = db_to_linear(mix->tab_master_db[tab]);
}

void libmix_update_solo(LibMix *mix, int num_channels) {
    mix->any_solo = false;
    for (int i = 0; i < num_channels; i++) {
        if (mix->channels[i].solo) {
            mix->any_solo = true;
            return;
        }
    }
}

void libmix_init(LibMix *mix, const LibraryDef *def) {
    memset(mix, 0, sizeof(*mix));
    for (int i = 0; i < MAX_LIB_CHANNELS; i++) {
        mix->channels[i].stereo_mode = true;   // matches the param default
        libmix_update_channel(mix, i);
    }
    for (int t = 0; t < TAB_COUNT; t++) {
        mix->tab_master_db[t] = 0.0f;
        libmix_update_tab_master(mix, (GuiTab)t);
    }
    for (int s = 0; s < def->num_selectors; s++)
        mix->selector[s] = def->selectors[s].default_option;
}

// ---------- Loading ----------

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)size + 1);
    if (buf && fread(buf, 1, (size_t)size, f) == (size_t)size) {
        buf[size] = '\0';
    } else {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    return buf;
}

LibraryRuntime *library_load(int lib, const char *root) {
    const LibraryDef *def = library_def(lib);
    if (!def || !root || !root[0]) return NULL;

    char path[2048];
    snprintf(path, sizeof(path), "%s/library_index.json", root);
    char *text = read_file(path);
    if (!text) {
        fprintf(stderr, "ggd-libre: cannot read %s\n", path);
        return NULL;
    }
    cJSON *index = cJSON_Parse(text);
    free(text);
    if (!index) {
        fprintf(stderr, "ggd-libre: JSON parse error in %s\n", path);
        return NULL;
    }

    LibraryRuntime *rt = calloc(1, sizeof(*rt));
    if (!rt) { cJSON_Delete(index); return NULL; }
    rt->lib = lib;
    rt->def = def;

    char wav_dir[2048];
    snprintf(wav_dir, sizeof(wav_dir), "%s/wav", root);
    bool ok = sample_bank_load(&rt->bank, wav_dir, cJSON_GetObjectItem(index, "articulations"),
                               def->mics, def->num_mics);
    if (ok) {
        const char *keys[MAX_SELECTORS];
        for (int s = 0; s < def->num_selectors; s++) keys[s] = def->selectors[s].key;
        ok = midi_map_load(&rt->map, &rt->bank, cJSON_GetObjectItem(index, "notes"),
                           cJSON_GetObjectItem(index, "selectors"), keys, def->num_selectors);
    }
    cJSON_Delete(index);
    if (!ok) {
        library_free(rt);
        return NULL;
    }

    for (int d = 0; d < DRUM_TYPE_COUNT; d++)
        for (int m = 0; m < MAX_LIB_MICS; m++) {
            int ch = (m < def->num_mics) ? def->route((DrumType)d, m) : -1;
            rt->route[d][m] = (int8_t)((ch >= 0 && ch < def->num_channels) ? ch : -1);
        }

    sample_bank_prefetch(&rt->bank);
    return rt;
}

void library_free(LibraryRuntime *rt) {
    if (!rt) return;
    sample_bank_free(&rt->bank);
    free(rt);
}

// ---------- Locating libraries ----------

static bool try_root(const char *dir, char *out_root, size_t out_size) {
    char index[2048];
    snprintf(index, sizeof(index), "%s/library_index.json", dir);
    if (!file_exists(index)) return false;
    resolve_absolute(dir, out_root, out_size);
    return true;
}

bool library_probe(int lib, const char *base, char *out_root, size_t out_size) {
    const LibraryDef *def = library_def(lib);
    if (!def) return false;
    char dir[2048];

    // GGD_SAMPLES_PATH is the legacy Halpern override: it names the wav/ folder.
    if (lib == LIB_HALPERN) {
        const char *env = getenv("GGD_SAMPLES_PATH");
        if (env && env[0]) {
            snprintf(dir, sizeof(dir), "%s/..", env);
            if (try_root(dir, out_root, out_size)) return true;
        }
    }
    const char *root_env = getenv("GGD_LIBRE_ROOT");
    if (root_env && root_env[0]) {
        snprintf(dir, sizeof(dir), "%s/%s", root_env, def->slug);
        if (try_root(dir, out_root, out_size)) return true;
    }
    if (!base || !base[0]) return false;

    // The standard install: ggd-libre-data/<slug>/ next to the .clap file.
    snprintf(dir, sizeof(dir), "%s/" LIBRARY_DATA_DIR "/%s", base, def->slug);
    if (try_root(dir, out_root, out_size)) return true;

    for (int i = 0; i < 3 && def->install_dirs[i]; i++) {
        snprintf(dir, sizeof(dir), "%s/%s", base, def->install_dirs[i]);
        if (try_root(dir, out_root, out_size)) return true;
    }
    // Development: the repo's output/ folder, next to or above the .clap file.
    snprintf(dir, sizeof(dir), "%s/output/%s", base, def->slug);
    if (try_root(dir, out_root, out_size)) return true;
    snprintf(dir, sizeof(dir), "%s/../output/%s", base, def->slug);
    if (try_root(dir, out_root, out_size)) return true;
    return false;
}
