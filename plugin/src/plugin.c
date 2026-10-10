#include "plugin.h"
#include "params.h"
#include "state.h"
#include "gui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations for extension structs
static const clap_plugin_note_name_t   s_note_name;
static const clap_plugin_audio_ports_t s_audio_ports;
static const clap_plugin_note_ports_t  s_note_ports;
static const clap_plugin_params_t      s_params;
static const clap_plugin_state_t       s_state;

static const clap_plugin_descriptor_t s_descriptor = {
    .clap_version = CLAP_VERSION_INIT,
    .id = "com.ggd-libre.drumsampler",
    .name = "GGD Libre",
    .vendor = "GGD Libre",
    .url = "",
    .manual_url = "",
    .support_url = "",
    .version = "0.2.0",
    .description = "Drum sampler for extracted GGD libraries",
    .features = (const char *[]){
        CLAP_PLUGIN_FEATURE_INSTRUMENT,
        CLAP_PLUGIN_FEATURE_STEREO,
        NULL
    },
};

static const LibraryDef *selected_def(const ggd_plugin_t *plug) {
    return plug->selected_lib >= 0 ? library_def(plug->selected_lib) : NULL;
}

// ---------- Audio Ports ----------

static uint32_t audio_ports_count(const clap_plugin_t *plugin, bool is_input) {
    (void)plugin;
    return is_input ? 0 : NUM_OUTPUT_PORTS;
}

static bool audio_ports_get(const clap_plugin_t *plugin, uint32_t index,
                            bool is_input, clap_audio_port_info_t *info) {
    ggd_plugin_t *plug = plugin->plugin_data;
    if (is_input || index >= NUM_OUTPUT_PORTS) return false;

    info->id = index;
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = CLAP_INVALID_ID;

    // The port count never changes; names follow the selected library.
    const LibraryDef *d = selected_def(plug);
    if (index == 0) {
        snprintf(info->name, sizeof(info->name), "Master");
        info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    } else if (d && (int)index - 1 < d->num_channels) {
        snprintf(info->name, sizeof(info->name), "%s", d->channels[index - 1].name);
        info->flags = 0;
    } else {
        snprintf(info->name, sizeof(info->name), "Stem %u", index);
        info->flags = 0;
    }
    return true;
}

static const clap_plugin_audio_ports_t s_audio_ports = {
    .count = audio_ports_count,
    .get = audio_ports_get,
};

// ---------- Note Ports ----------

static uint32_t note_ports_count(const clap_plugin_t *plugin, bool is_input) {
    (void)plugin;
    return is_input ? 1 : 0;
}

static bool note_ports_get(const clap_plugin_t *plugin, uint32_t index,
                           bool is_input, clap_note_port_info_t *info) {
    (void)plugin;
    if (!is_input || index > 0) return false;
    info->id = 0;
    snprintf(info->name, sizeof(info->name), "MIDI In");
    info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
    info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
    return true;
}

static const clap_plugin_note_ports_t s_note_ports = {
    .count = note_ports_count,
    .get = note_ports_get,
};

// ---------- Note Names ----------

// Main thread only. The main thread is the only one that frees runtimes, so
// the active pointer stays valid for the duration of the call.
static const MidiMap *main_thread_map(const ggd_plugin_t *plug) {
    LibraryRuntime *rt = atomic_load(&((ggd_plugin_t *)plug)->pending);
    if (!rt) rt = atomic_load(&((ggd_plugin_t *)plug)->active);
    return rt ? &rt->map : NULL;
}

static uint32_t note_name_count(const clap_plugin_t *plugin) {
    const MidiMap *map = main_thread_map(plugin->plugin_data);
    if (!map) return 0;
    uint32_t count = 0;
    for (int i = 0; i < MAX_MIDI_NOTES; i++)
        if (map->slots[i].name[0]) count++;
    return count;
}

static bool note_name_get(const clap_plugin_t *plugin, uint32_t index,
                          clap_note_name_t *note_name) {
    const MidiMap *map = main_thread_map(plugin->plugin_data);
    if (!map) return false;
    uint32_t count = 0;
    for (int i = 0; i < MAX_MIDI_NOTES; i++) {
        if (!map->slots[i].name[0]) continue;
        if (count == index) {
            snprintf(note_name->name, CLAP_NAME_SIZE, "%s", map->slots[i].name);
            note_name->port = -1;
            note_name->key = (int16_t)i;
            note_name->channel = -1;
            return true;
        }
        count++;
    }
    return false;
}

static const clap_plugin_note_name_t s_note_name = {
    .count = note_name_count,
    .get = note_name_get,
};

// ---------- Params ----------

bool plugin_get_param(const ggd_plugin_t *plug, clap_id id, double *out) {
    ParamRef ref;
    if (!params_resolve(id, &ref)) return false;
    const LibMix *mix = &plug->mix[ref.lib];

    switch (ref.kind) {
    case PK_MASTER:     *out = plug->engine.master_gain_db; return true;
    case PK_MIDI_MAP:   *out = plug->midi_map_mode; return true;
    case PK_TAB_MASTER: *out = mix->tab_master_db[ref.index]; return true;
    case PK_SELECTOR:   *out = mix->selector[ref.index]; return true;
    case PK_CHANNEL: {
        const ChannelParams *cp = &mix->channels[ref.index];
        switch (ref.offset) {
        case PARAM_CH_GAIN:   *out = cp->gain_db; return true;
        case PARAM_CH_PAN:    *out = cp->pan; return true;
        case PARAM_CH_MUTE:   *out = cp->mute ? 1.0 : 0.0; return true;
        case PARAM_CH_SOLO:   *out = cp->solo ? 1.0 : 0.0; return true;
        case PARAM_CH_PHASE:  *out = cp->phase_invert ? 1.0 : 0.0; return true;
        case PARAM_CH_STEREO: *out = cp->stereo_mode ? 1.0 : 0.0; return true;
        }
        return false;
    }
    default: return false;
    }
}

void plugin_apply_param(ggd_plugin_t *plug, clap_id id, double value) {
    ParamRef ref;
    if (!params_resolve(id, &ref)) return;
    LibMix *mix = &plug->mix[ref.lib];
    const LibraryDef *d = library_def(ref.lib);
    int vi = (int)(value + 0.5);

    switch (ref.kind) {
    case PK_MASTER:
        plug->engine.master_gain_db = (float)value;
        engine_update_master(&plug->engine);
        break;
    case PK_MIDI_MAP:
        if (vi >= 0 && vi < MIDIMAP_MODE_COUNT) plug->midi_map_mode = vi;
        break;
    case PK_TAB_MASTER:
        mix->tab_master_db[ref.index] = (float)value;
        libmix_update_tab_master(mix, (GuiTab)ref.index);
        break;
    case PK_SELECTOR:
        if (vi < 0) vi = 0;
        if (vi >= d->selectors[ref.index].num_options) vi = d->selectors[ref.index].num_options - 1;
        mix->selector[ref.index] = vi;
        break;
    case PK_CHANNEL: {
        ChannelParams *cp = &mix->channels[ref.index];
        switch (ref.offset) {
        case PARAM_CH_GAIN:   cp->gain_db = (float)value; break;
        case PARAM_CH_PAN:    cp->pan = (float)value; break;
        case PARAM_CH_MUTE:   cp->mute = value > 0.5; break;
        case PARAM_CH_SOLO:   cp->solo = value > 0.5; break;
        case PARAM_CH_PHASE:  cp->phase_invert = value > 0.5; break;
        case PARAM_CH_STEREO: cp->stereo_mode = value > 0.5; break;
        }
        libmix_update_channel(mix, ref.index);
        if (ref.offset == PARAM_CH_SOLO)
            libmix_update_solo(mix, d->num_channels);
        break;
    }
    default: break;
    }
}

static uint32_t plug_params_count(const clap_plugin_t *plugin) {
    (void)plugin;
    return params_count();
}

static bool plug_params_get_info(const clap_plugin_t *plugin, uint32_t index,
                                 clap_param_info_t *info) {
    (void)plugin;
    return params_get_info(index, info);
}

static bool plug_params_get_value(const clap_plugin_t *plugin, clap_id param_id,
                                  double *out) {
    return plugin_get_param(plugin->plugin_data, param_id, out);
}

static bool plug_params_value_to_text(const clap_plugin_t *plugin, clap_id param_id,
                                      double value, char *buf, uint32_t buf_size) {
    (void)plugin;
    return params_value_to_text(param_id, value, buf, buf_size);
}

static bool plug_params_text_to_value(const clap_plugin_t *plugin, clap_id param_id,
                                      const char *text, double *out) {
    (void)plugin;
    return params_text_to_value(param_id, text, out);
}

static void plug_params_flush(const clap_plugin_t *plugin,
                              const clap_input_events_t *in,
                              const clap_output_events_t *out) {
    (void)out;
    ggd_plugin_t *plug = plugin->plugin_data;
    uint32_t count = in->size(in);
    for (uint32_t i = 0; i < count; i++) {
        const clap_event_header_t *hdr = in->get(in, i);
        if (hdr->space_id != CLAP_CORE_EVENT_SPACE_ID) continue;
        if (hdr->type == CLAP_EVENT_PARAM_VALUE) {
            const clap_event_param_value_t *ev = (const clap_event_param_value_t *)hdr;
            plugin_apply_param(plug, ev->param_id, ev->value);
        }
    }
}

static const clap_plugin_params_t s_params = {
    .count = plug_params_count,
    .get_info = plug_params_get_info,
    .get_value = plug_params_get_value,
    .value_to_text = plug_params_value_to_text,
    .text_to_value = plug_params_text_to_value,
    .flush = plug_params_flush,
};

// ---------- State ----------

static bool plug_state_save(const clap_plugin_t *plugin, const clap_ostream_t *stream) {
    return state_save(plugin->plugin_data, stream);
}

static bool plug_state_load(const clap_plugin_t *plugin, const clap_istream_t *stream) {
    return state_load(plugin->plugin_data, stream);
}

static const clap_plugin_state_t s_state = {
    .save = plug_state_save,
    .load = plug_state_load,
};

// ---------- Library switching ----------

void plugin_probe_libraries(ggd_plugin_t *plug) {
    for (int l = 0; l < LIB_COUNT; l++) {
        plug->lib_available[l] = library_probe(l, plug->base_path, plug->lib_root[l],
                                               sizeof(plug->lib_root[l]));
        fprintf(stderr, "ggd-libre: library %s: %s\n", library_def(l)->slug,
                plug->lib_available[l] ? plug->lib_root[l] : "not found");
    }
}

static void notify_library_changed(ggd_plugin_t *plug) {
    if (plug->host_note_name)
        plug->host_note_name->changed(plug->host);
    if (plug->host_audio_ports &&
        plug->host_audio_ports->is_rescan_flag_supported(plug->host, CLAP_AUDIO_PORTS_RESCAN_NAMES))
        plug->host_audio_ports->rescan(plug->host, CLAP_AUDIO_PORTS_RESCAN_NAMES);
}

bool plugin_select_library(ggd_plugin_t *plug, int lib) {
    if (lib < 0 || lib >= LIB_COUNT) return false;
    if (lib == plug->selected_lib) return true;
    if (!plug->lib_available[lib]) {
        plugin_probe_libraries(plug);
        if (!plug->lib_available[lib]) return false;
    }

    LibraryRuntime *rt = library_load(lib, plug->lib_root[lib]);
    if (!rt) {
        plug->lib_available[lib] = false;
        return false;
    }
    plug->selected_lib = lib;

    if (!plug->activated) {
        // No audio thread: swap directly.
        library_free(atomic_exchange(&plug->pending, NULL));
        library_free(atomic_exchange(&plug->active, rt));
        engine_reset_voices(&plug->engine);
        notify_library_changed(plug);
    } else {
        // A runtime still pending was never seen by the audio thread.
        library_free(atomic_exchange(&plug->pending, rt));
        if (plug->host) plug->host->request_process(plug->host);
    }
    return true;
}

bool plugin_swap_pending(const ggd_plugin_t *plug) {
    return atomic_load(&((ggd_plugin_t *)plug)->pending) != NULL;
}

// Audio thread, start of each block. Fades out what is playing, then swaps.
static void process_library_swap(ggd_plugin_t *plug) {
    if (!atomic_load(&plug->pending)) return;
    if (engine_any_active(&plug->engine)) {
        engine_fade_all(&plug->engine);
        return;
    }
    if (atomic_load(&plug->retired)) return;   // main thread hasn't freed the last one

    LibraryRuntime *rt = atomic_exchange(&plug->pending, NULL);
    if (!rt) return;
    LibraryRuntime *old = atomic_exchange(&plug->active, rt);
    atomic_store(&plug->retired, old);
    engine_reset_voices(&plug->engine);
    atomic_store(&plug->swap_done, true);
    plug->host->request_callback(plug->host);
}

// Main thread: finish what the audio thread handed back.
static void collect_swap(ggd_plugin_t *plug) {
    library_free(atomic_exchange(&plug->retired, NULL));
    if (atomic_exchange(&plug->swap_done, false))
        notify_library_changed(plug);
}

// ---------- Preview queue ----------

void plugin_preview_note(ggd_plugin_t *plug, int note, float velocity) {
    unsigned head = atomic_load_explicit(&plug->preview.head, memory_order_relaxed);
    unsigned tail = atomic_load_explicit(&plug->preview.tail, memory_order_acquire);
    if (head - tail >= PREVIEW_QUEUE_SIZE) return;   // full: drop
    plug->preview.items[head % PREVIEW_QUEUE_SIZE] =
        (PreviewHit){ plug->selected_lib, note, velocity };
    atomic_store_explicit(&plug->preview.head, head + 1, memory_order_release);
    if (plug->host) plug->host->request_process(plug->host);
}

static void drain_previews(ggd_plugin_t *plug, const LibraryRuntime *rt, bool play) {
    unsigned tail = atomic_load_explicit(&plug->preview.tail, memory_order_relaxed);
    unsigned head = atomic_load_explicit(&plug->preview.head, memory_order_acquire);
    for (; tail != head; tail++) {
        PreviewHit hit = plug->preview.items[tail % PREVIEW_QUEUE_SIZE];
        if (play && rt && hit.lib == rt->lib)
            engine_note_on(&plug->engine, rt, &plug->mix[rt->lib], hit.note, hit.velocity);
    }
    atomic_store_explicit(&plug->preview.tail, tail, memory_order_release);
}

// ---------- Plugin Lifecycle ----------

static bool plug_init(const struct clap_plugin *plugin) {
    ggd_plugin_t *plug = plugin->plugin_data;

    plug->host_params = plug->host->get_extension(plug->host, CLAP_EXT_PARAMS);
    plug->host_state = plug->host->get_extension(plug->host, CLAP_EXT_STATE);
    plug->host_log = plug->host->get_extension(plug->host, CLAP_EXT_LOG);
    plug->host_timer = plug->host->get_extension(plug->host, CLAP_EXT_TIMER_SUPPORT);
    plug->host_posix_fd = plug->host->get_extension(plug->host, CLAP_EXT_POSIX_FD_SUPPORT);
    plug->host_audio_ports = plug->host->get_extension(plug->host, CLAP_EXT_AUDIO_PORTS);
    plug->host_note_name = plug->host->get_extension(plug->host, CLAP_EXT_NOTE_NAME);

    params_init();

    // Bring the mixer up before anything else: CLAP permits state.load() to
    // arrive before activate().
    engine_init(&plug->engine, 48000.0f);
    for (int l = 0; l < LIB_COUNT; l++)
        libmix_init(&plug->mix[l], library_def(l));

    plugin_probe_libraries(plug);

    // Start on the first extracted library; GGD_LIBRE_LIBRARY=<slug> picks one.
    // Missing samples are not fatal: the GUI shows which libraries to extract.
    const char *want = getenv("GGD_LIBRE_LIBRARY");
    if (want && want[0]) {
        for (int l = 0; l < LIB_COUNT; l++)
            if (!strcmp(library_def(l)->slug, want) && plugin_select_library(plug, l))
                return true;
    }
    for (int l = 0; l < LIB_COUNT; l++)
        if (plug->lib_available[l] && plugin_select_library(plug, l))
            return true;

    fprintf(stderr, "ggd-libre: no extracted library found\n");
    return true;
}

static void plug_destroy(const struct clap_plugin *plugin) {
    ggd_plugin_t *plug = plugin->plugin_data;
    library_free(atomic_exchange(&plug->pending, NULL));
    library_free(atomic_exchange(&plug->retired, NULL));
    library_free(atomic_exchange(&plug->active, NULL));
    free(plug);
}

static bool plug_activate(const struct clap_plugin *plugin, double sample_rate,
                          uint32_t min_frames, uint32_t max_frames) {
    (void)min_frames; (void)max_frames;
    ggd_plugin_t *plug = plugin->plugin_data;
    // Only refresh rate-dependent caches — a full engine_init here would wipe
    // the master gain the host already restored via state.load().
    engine_set_sample_rate(&plug->engine, (float)sample_rate);
    engine_reset_voices(&plug->engine);
    plug->activated = true;
    return true;
}

static void plug_deactivate(const struct clap_plugin *plugin) {
    ggd_plugin_t *plug = plugin->plugin_data;
    plug->activated = false;
    // The audio thread is stopped: complete any hand-off directly.
    collect_swap(plug);
    LibraryRuntime *rt = atomic_exchange(&plug->pending, NULL);
    if (rt) {
        library_free(atomic_exchange(&plug->active, rt));
        engine_reset_voices(&plug->engine);
        notify_library_changed(plug);
    }
}

static bool plug_start_processing(const struct clap_plugin *plugin) { (void)plugin; return true; }
static void plug_stop_processing(const struct clap_plugin *plugin) { (void)plugin; }

static void plug_reset(const struct clap_plugin *plugin) {
    ggd_plugin_t *plug = plugin->plugin_data;
    engine_reset_voices(&plug->engine);
}

static void process_event(ggd_plugin_t *plug, const LibraryRuntime *rt, bool accept_notes,
                          const clap_event_header_t *hdr) {
    if (hdr->space_id != CLAP_CORE_EVENT_SPACE_ID) return;
    const LibMix *mix = rt ? &plug->mix[rt->lib] : NULL;

    switch (hdr->type) {
    case CLAP_EVENT_NOTE_ON: {
        const clap_event_note_t *ev = (const clap_event_note_t *)hdr;
        // No logging here: process_event runs on the audio thread, where a
        // blocking write to stderr is a real-time violation and can glitch.
        if (!accept_notes || !rt) break;
        int key = midi_remap_note(ev->key, plug->midi_map_mode);
        engine_note_on(&plug->engine, rt, mix, key, (float)ev->velocity);
        break;
    }
    case CLAP_EVENT_NOTE_OFF: {
        const clap_event_note_t *ev = (const clap_event_note_t *)hdr;
        engine_note_off(&plug->engine, midi_remap_note(ev->key, plug->midi_map_mode));
        break;
    }
    case CLAP_EVENT_NOTE_CHOKE:
        engine_fade_all(&plug->engine);
        break;
    case CLAP_EVENT_PARAM_VALUE: {
        const clap_event_param_value_t *ev = (const clap_event_param_value_t *)hdr;
        plugin_apply_param(plug, ev->param_id, ev->value);
        break;
    }
    case CLAP_EVENT_MIDI: {
        const clap_event_midi_t *ev = (const clap_event_midi_t *)hdr;
        uint8_t status = ev->data[0] & 0xF0;
        if (status == 0x90 && ev->data[2] > 0) {
            if (!accept_notes || !rt) break;
            int key = midi_remap_note(ev->data[1], plug->midi_map_mode);
            engine_note_on(&plug->engine, rt, mix, key, ev->data[2] / 127.0f);
        } else if (status == 0x80 || (status == 0x90 && ev->data[2] == 0)) {
            engine_note_off(&plug->engine, ev->data[1]);
        }
        break;
    }
    }
}

static clap_process_status plug_process(const struct clap_plugin *plugin,
                                        const clap_process_t *process) {
    ggd_plugin_t *plug = plugin->plugin_data;
    const uint32_t nframes = process->frames_count;
    const uint32_t nev = process->in_events->size(process->in_events);
    uint32_t ev_index = 0;
    uint32_t next_ev_frame = nev > 0 ? 0 : nframes;

    process_library_swap(plug);
    const LibraryRuntime *rt = atomic_load(&plug->active);
    // While a new library is waiting, let the old one fade instead of
    // starting new hits on it.
    const bool accept_notes = !atomic_load(&plug->pending);
    const LibMix *mix = rt ? &plug->mix[rt->lib] : &plug->mix[0];
    drain_previews(plug, rt, accept_notes);

    // Build output buffer array from host-provided ports
    uint32_t num_ports = process->audio_outputs_count;
    if (num_ports > NUM_OUTPUT_PORTS) num_ports = NUM_OUTPUT_PORTS;

    StereoOut outs[NUM_OUTPUT_PORTS];
    for (uint32_t p = 0; p < NUM_OUTPUT_PORTS; p++) {
        if (p < num_ports && process->audio_outputs[p].data32) {
            outs[p].l = process->audio_outputs[p].data32[0];
            outs[p].r = process->audio_outputs[p].data32[1];
        } else {
            outs[p].l = NULL;
            outs[p].r = NULL;
        }
    }

    for (uint32_t i = 0; i < nframes;) {
        while (ev_index < nev && next_ev_frame == i) {
            const clap_event_header_t *hdr =
                process->in_events->get(process->in_events, ev_index);
            if (hdr->time != i) {
                next_ev_frame = hdr->time;
                break;
            }
            process_event(plug, rt, accept_notes, hdr);
            ++ev_index;
            if (ev_index == nev) {
                next_ev_frame = nframes;
                break;
            }
        }

        uint32_t block_size = next_ev_frame - i;

        // Offset all output buffers for this block
        StereoOut block_outs[NUM_OUTPUT_PORTS];
        for (uint32_t p = 0; p < NUM_OUTPUT_PORTS; p++) {
            block_outs[p].l = outs[p].l ? outs[p].l + i : NULL;
            block_outs[p].r = outs[p].r ? outs[p].r + i : NULL;
        }

        engine_render(&plug->engine, rt, mix, block_outs, num_ports, block_size);
        i = next_ev_frame;
    }

    if (engine_any_active(&plug->engine) || atomic_load(&plug->pending))
        return CLAP_PROCESS_CONTINUE;
    return CLAP_PROCESS_SLEEP;
}

// ---------- Test extension (headless render host) ----------

typedef struct {
    bool (*select_library)(const clap_plugin_t *plugin, const char *slug);
    bool (*swap_pending)(const clap_plugin_t *plugin);
} ggd_test_ext_t;

static bool test_select_library(const clap_plugin_t *plugin, const char *slug) {
    for (int l = 0; l < LIB_COUNT; l++)
        if (!strcmp(library_def(l)->slug, slug))
            return plugin_select_library(plugin->plugin_data, l);
    return false;
}

static bool test_swap_pending(const clap_plugin_t *plugin) {
    return plugin_swap_pending(plugin->plugin_data);
}

static const ggd_test_ext_t s_test_ext = {
    .select_library = test_select_library,
    .swap_pending = test_swap_pending,
};

static const void *plug_get_extension(const struct clap_plugin *plugin, const char *id) {
    (void)plugin;
    if (!strcmp(id, CLAP_EXT_AUDIO_PORTS))       return &s_audio_ports;
    if (!strcmp(id, CLAP_EXT_NOTE_PORTS))        return &s_note_ports;
    if (!strcmp(id, CLAP_EXT_NOTE_NAME))         return &s_note_name;
    if (!strcmp(id, CLAP_EXT_PARAMS))            return &s_params;
    if (!strcmp(id, CLAP_EXT_STATE))             return &s_state;
    if (!strcmp(id, CLAP_EXT_GUI))               return &ggd_gui_ext;
    if (!strcmp(id, CLAP_EXT_TIMER_SUPPORT))     return &ggd_timer_ext;
#ifdef __linux__
    if (!strcmp(id, CLAP_EXT_POSIX_FD_SUPPORT))  return &ggd_posix_fd_ext;
#endif
    if (!strcmp(id, "com.ggd-libre.test"))       return &s_test_ext;
    return NULL;
}

static void plug_on_main_thread(const struct clap_plugin *plugin) {
    collect_swap(plugin->plugin_data);
}

const clap_plugin_descriptor_t *ggd_get_descriptor(void) {
    return &s_descriptor;
}

clap_plugin_t *ggd_plugin_create(const clap_host_t *host) {
    ggd_plugin_t *p = calloc(1, sizeof(*p));
    if (!p) return NULL;

    p->host = host;
    p->selected_lib = -1;
    atomic_init(&p->active, NULL);
    atomic_init(&p->pending, NULL);
    atomic_init(&p->retired, NULL);
    atomic_init(&p->swap_done, false);
    atomic_init(&p->preview.head, 0);
    atomic_init(&p->preview.tail, 0);

    p->plugin.desc = &s_descriptor;
    p->plugin.plugin_data = p;
    p->plugin.init = plug_init;
    p->plugin.destroy = plug_destroy;
    p->plugin.activate = plug_activate;
    p->plugin.deactivate = plug_deactivate;
    p->plugin.start_processing = plug_start_processing;
    p->plugin.stop_processing = plug_stop_processing;
    p->plugin.reset = plug_reset;
    p->plugin.process = plug_process;
    p->plugin.get_extension = plug_get_extension;
    p->plugin.on_main_thread = plug_on_main_thread;

    return &p->plugin;
}
