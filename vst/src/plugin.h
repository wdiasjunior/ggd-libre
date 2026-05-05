#ifndef GGD_PLUGIN_H
#define GGD_PLUGIN_H

#include <clap/clap.h>
#include "types.h"
#include "sample_bank.h"
#include "midi_map.h"
#include "audio_engine.h"

typedef struct ggd_plugin {
    clap_plugin_t  plugin;
    const clap_host_t *host;
    const clap_host_params_t          *host_params;
    const clap_host_state_t           *host_state;
    const clap_host_log_t             *host_log;
    const clap_host_timer_support_t   *host_timer;
    const clap_host_posix_fd_support_t *host_posix_fd;

    SampleBank   bank;
    MidiMap      midi_map;
    AudioEngine  engine;

    // Variant selectors
    int kick_size;
    int snare_type;
    int tom_head[4];
    int china_size;
    int stack_type;

    // Path to samples (resolved at init)
    char samples_path[1024];

    // GUI (defined in gui.h as PluginGui)
    void *gui;

    bool activated;
} ggd_plugin_t;

clap_plugin_t *ggd_plugin_create(const clap_host_t *host);

#endif
