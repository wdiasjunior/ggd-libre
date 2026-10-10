#ifndef GGD_LIBRARY_H
#define GGD_LIBRARY_H

// A library is one extracted GGD kit. Everything that differs between kits —
// mic positions, mixer channels, routing, variant selectors, GUI labels and
// parameter ids — lives in its LibraryDef (lib_*.c). The sample data and the
// note -> articulation mapping come from <root>/library_index.json, written by
// tools/build_index.py.

#include "types.h"
#include "sample_bank.h"
#include "midi_map.h"

// Folder next to the .clap file that holds one subfolder per library slug.
#define LIBRARY_DATA_DIR  "ggd-libre-data"

#define MAX_SELECTORS     12
#define MAX_SEL_OPTIONS   8
#define MAX_BOTTOM_ITEMS  8

typedef enum {
    LIB_HALPERN = 0,
    LIB_OKW,
    LIB_PV,
    LIB_COUNT
} LibraryId;

typedef struct {
    const char *name;         // port / param name, e.g. "Kick Close"
    GuiTab      tab;
    const char *strip_label;  // fader caption, e.g. "CLOSE"
} LibChannel;

typedef struct {
    const char *key;          // matches "selectors" in library_index.json
    const char *name;         // param name, e.g. "Kick Size"
    uint32_t    param_id;
    int         default_option;
    int         num_options;
    const char *options[MAX_SEL_OPTIONS];
} LibSelector;

// One box in the row under the faders: a preview button plus either a
// selector (selector >= 0) or a fixed info caption.
typedef struct {
    const char *label;
    int         preview_note; // -1: no preview
    int         selector;     // index into selectors, or -1
    const char *info;         // shown when selector < 0
} LibBottomItem;

typedef struct LibraryDef {
    const char *slug;               // folder name under output/, e.g. "okw_metal"
    const char *name;               // library bar caption
    const char *param_prefix;       // prepended to param/port names ("" for Halpern)
    const char *install_dirs[3];    // legacy folder names under the plugin dir, NULL-terminated
    uint32_t    param_base;

    int         num_mics;
    const char *mics[MAX_LIB_MICS]; // mic ids, as in library_index.json "mics"

    int         num_channels;
    LibChannel  channels[MAX_LIB_CHANNELS];

    // Mixer channel for a drum type + mic index, or -1 if that mic is not used.
    int (*route)(DrumType drum, int mic);

    int         num_selectors;
    LibSelector selectors[MAX_SELECTORS];

    int           num_items[TAB_COUNT];
    LibBottomItem items[TAB_COUNT][MAX_BOTTOM_ITEMS];
} LibraryDef;

extern const LibraryDef LIB_DEF_HALPERN;
extern const LibraryDef LIB_DEF_OKW;
extern const LibraryDef LIB_DEF_PV;

const LibraryDef *library_def(int lib);

// Per-library mixer and selector state. One per library, so switching kits
// keeps each kit's mix, and automation for an inactive kit lands in its own store.
typedef struct {
    ChannelParams channels[MAX_LIB_CHANNELS];
    float         tab_master_db[TAB_COUNT];
    float         tab_master_linear[TAB_COUNT];
    bool          any_solo;
    int           selector[MAX_SELECTORS];
} LibMix;

void libmix_init(LibMix *mix, const LibraryDef *def);
void libmix_update_channel(LibMix *mix, int ch);
void libmix_update_tab_master(LibMix *mix, GuiTab tab);
void libmix_update_solo(LibMix *mix, int num_channels);

// A loaded library: what the audio thread plays from.
typedef struct LibraryRuntime {
    int               lib;
    const LibraryDef *def;
    SampleBank        bank;
    MidiMap           map;
    int8_t            route[DRUM_TYPE_COUNT][MAX_LIB_MICS];
} LibraryRuntime;

// Load <root>/library_index.json and map every sample. NULL on failure.
LibraryRuntime *library_load(int lib, const char *root);
// Stops and joins the prefetch thread, unmaps everything.
void library_free(LibraryRuntime *rt);

// Find the library's root folder (one containing library_index.json).
// base is the plugin's directory. Searched in order:
//   $GGD_SAMPLES_PATH/.. (Halpern only), $GGD_LIBRE_ROOT/<slug>,
//   <base>/ggd-libre-data/<slug>, <base>/<legacy install dir>,
//   <base>/output/<slug>, <base>/../output/<slug>
// Returns false if not found.
bool library_probe(int lib, const char *base, char *out_root, size_t out_size);

// Param id helpers
static inline uint32_t lib_param_tab_master(const LibraryDef *d, int tab) {
    return d->param_base + PARAM_TAB_MASTER_OFFSET + (uint32_t)tab;
}
static inline uint32_t lib_param_channel(const LibraryDef *d, int ch, int offset) {
    return d->param_base + PARAM_CHANNEL_OFFSET + (uint32_t)ch * 10 + (uint32_t)offset;
}

#endif
