#include "state.h"
#include "plugin.h"
#include "params.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STATE_MAGIC 0x47474431  // "GGD1"
#define STATE_VERSION 7

// v7: header, the selected library's slug, then every parameter as an
// (id, value) pair. Unknown ids are skipped on load, so adding parameters or
// libraries later does not need another format change.
typedef struct {
    uint32_t magic;
    uint32_t version;
    char     library[32];
    uint32_t num_params;
    uint32_t reserved;
} StateHeader;

typedef struct {
    uint32_t id;
    uint32_t reserved;
    double   value;
} StateParam;

// v6 (single-library, Halpern only), kept for loading old sessions.
#define V6_CHANNELS 27
typedef struct {
    uint32_t magic;
    uint32_t version;
    float    master_gain_db;
    float    tab_master_db[TAB_COUNT];
    int32_t  kick_size;
    int32_t  snare_type;
    int32_t  tom_head[4];
    int32_t  china_size;
    int32_t  stack_type;
    int32_t  lcrash_size;
    int32_t  rcrash_size;
    int32_t  midi_map_mode;
    struct {
        float gain_db;
        float pan;
        uint8_t mute;
        uint8_t solo;
        uint8_t phase_invert;
        uint8_t stereo_mode;
    } channels[V6_CHANNELS];
} StateDataV6;

static int64_t stream_write_all(const clap_ostream_t *stream, const void *buf, uint64_t size) {
    const uint8_t *p = buf;
    uint64_t written = 0;
    while (written < size) {
        int64_t n = stream->write(stream, p + written, size - written);
        if (n <= 0) return -1;
        written += n;
    }
    return written;
}

static int64_t stream_read_all(const clap_istream_t *stream, void *buf, uint64_t size) {
    uint8_t *p = buf;
    uint64_t total = 0;
    while (total < size) {
        int64_t n = stream->read(stream, p + total, size - total);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

bool state_save(const struct ggd_plugin *plug, const clap_ostream_t *stream) {
    uint32_t count = params_count();
    StateParam *vals = calloc(count, sizeof(StateParam));
    if (!vals) return false;
    uint32_t n = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t id = params_index_to_id(i);
        double v;
        if (plugin_get_param(plug, id, &v))
            vals[n++] = (StateParam){ .id = id, .value = v };
    }

    StateHeader h;
    memset(&h, 0, sizeof(h));
    h.magic = STATE_MAGIC;
    h.version = STATE_VERSION;
    if (plug->selected_lib >= 0)
        snprintf(h.library, sizeof(h.library), "%s", library_def(plug->selected_lib)->slug);
    h.num_params = n;

    bool ok = stream_write_all(stream, &h, sizeof(h)) > 0 &&
              stream_write_all(stream, vals, (uint64_t)n * sizeof(StateParam)) >= 0;
    free(vals);
    return ok;
}

static void apply(struct ggd_plugin *plug, uint32_t id, double value) {
    plugin_apply_param(plug, id, value);
}

// Halpern v6 field -> current param id.
static bool load_v6(struct ggd_plugin *plug, const clap_istream_t *stream, uint32_t version) {
    StateDataV6 s;
    s.magic = STATE_MAGIC;
    s.version = version;
    // The two header words are already consumed.
    if (stream_read_all(stream, (uint8_t *)&s + 8, sizeof(s) - 8) < 0)
        return false;

    const LibraryDef *d = &LIB_DEF_HALPERN;
    apply(plug, PARAM_MASTER_GAIN, s.master_gain_db);
    apply(plug, PARAM_MIDI_MAP_MODE, s.midi_map_mode);
    for (int t = 0; t < TAB_COUNT; t++)
        apply(plug, lib_param_tab_master(d, t), s.tab_master_db[t]);

    const int32_t sel[10] = {
        s.kick_size, s.snare_type, s.tom_head[0], s.tom_head[1], s.tom_head[2], s.tom_head[3],
        s.china_size, s.stack_type, s.lcrash_size, s.rcrash_size,
    };
    for (int i = 0; i < 10 && i < d->num_selectors; i++)
        apply(plug, d->selectors[i].param_id, sel[i]);

    for (int c = 0; c < V6_CHANNELS && c < d->num_channels; c++) {
        apply(plug, lib_param_channel(d, c, PARAM_CH_GAIN),   s.channels[c].gain_db);
        apply(plug, lib_param_channel(d, c, PARAM_CH_PAN),    s.channels[c].pan);
        apply(plug, lib_param_channel(d, c, PARAM_CH_MUTE),   s.channels[c].mute);
        apply(plug, lib_param_channel(d, c, PARAM_CH_SOLO),   s.channels[c].solo);
        apply(plug, lib_param_channel(d, c, PARAM_CH_PHASE),  s.channels[c].phase_invert);
        apply(plug, lib_param_channel(d, c, PARAM_CH_STEREO), s.channels[c].stereo_mode);
    }
    plugin_select_library(plug, LIB_HALPERN);
    return true;
}

bool state_load(struct ggd_plugin *plug, const clap_istream_t *stream) {
    uint32_t head[2];
    if (stream_read_all(stream, head, sizeof(head)) < 0 || head[0] != STATE_MAGIC)
        return false;
    if (head[1] == 6)
        return load_v6(plug, stream, head[1]);
    if (head[1] != STATE_VERSION)
        return false;

    StateHeader h;
    h.magic = head[0];
    h.version = head[1];
    if (stream_read_all(stream, (uint8_t *)&h + 8, sizeof(h) - 8) < 0)
        return false;
    h.library[sizeof(h.library) - 1] = '\0';

    for (uint32_t i = 0; i < h.num_params; i++) {
        StateParam p;
        if (stream_read_all(stream, &p, sizeof(p)) < 0)
            return false;
        apply(plug, p.id, p.value);
    }

    // Keep the current library if the saved one isn't extracted here.
    for (int l = 0; l < LIB_COUNT; l++) {
        if (!strcmp(library_def(l)->slug, h.library)) {
            if (!plugin_select_library(plug, l))
                fprintf(stderr, "ggd-libre: saved library '%s' is not available\n", h.library);
            break;
        }
    }
    return true;
}
