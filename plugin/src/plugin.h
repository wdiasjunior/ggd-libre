#ifndef GGD_PLUGIN_H
#define GGD_PLUGIN_H

#include <clap/clap.h>
#include <stdatomic.h>
#include "types.h"
#include "library.h"
#include "audio_engine.h"

// GUI preview hits travel to the audio thread through a single-producer,
// single-consumer ring, so the GUI never touches voices directly.
#define PREVIEW_QUEUE_SIZE 32
typedef struct {
    int   lib;
    int   note;
    float velocity;
} PreviewHit;

typedef struct {
    PreviewHit  items[PREVIEW_QUEUE_SIZE];
    atomic_uint head;   // written by the main thread
    atomic_uint tail;   // written by the audio thread
} PreviewQueue;

typedef struct ggd_plugin {
    clap_plugin_t  plugin;
    const clap_host_t *host;
    const clap_host_params_t          *host_params;
    const clap_host_state_t           *host_state;
    const clap_host_log_t             *host_log;
    const clap_host_timer_support_t   *host_timer;
    const clap_host_posix_fd_support_t *host_posix_fd;
    const clap_host_audio_ports_t     *host_audio_ports;
    const clap_host_note_name_t       *host_note_name;

    AudioEngine  engine;
    LibMix       mix[LIB_COUNT];
    int          midi_map_mode;

    // Library discovery (main thread)
    char base_path[1024];                  // the plugin's folder
    char lib_root[LIB_COUNT][1024];
    bool lib_available[LIB_COUNT];

    // Library hand-off. The main thread loads a runtime and publishes it in
    // `pending`; the audio thread fades out, swaps it into `active` and hands
    // the old one back through `retired`, which the main thread frees.
    _Atomic(LibraryRuntime *) active;
    _Atomic(LibraryRuntime *) pending;
    _Atomic(LibraryRuntime *) retired;
    atomic_bool  swap_done;                // audio -> main: refresh host names
    int          selected_lib;             // main thread: last requested library, -1 none

    PreviewQueue preview;

    // GUI (defined in gui.h as PluginGui)
    void *gui;

    bool activated;
} ggd_plugin_t;

clap_plugin_t *ggd_plugin_create(const clap_host_t *host);

// Main thread. Loads the library and makes it the one that plays. Returns
// false if it is not extracted or fails to load.
bool plugin_select_library(ggd_plugin_t *plug, int lib);
// Main thread. Re-scan the disk for libraries (e.g. after extracting one).
void plugin_probe_libraries(ggd_plugin_t *plug);
// True while a selected library is still waiting for the audio thread.
bool plugin_swap_pending(const ggd_plugin_t *plug);
// Main thread. Queue a preview hit on the selected library.
void plugin_preview_note(ggd_plugin_t *plug, int note, float velocity);

// Parameter access by id, from any thread the CLAP params API allows.
bool plugin_get_param(const ggd_plugin_t *plug, clap_id id, double *out);
void plugin_apply_param(ggd_plugin_t *plug, clap_id id, double value);

#endif
