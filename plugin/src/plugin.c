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
    .version = "0.1.0",
    .description = "GGD Matt Halpern drum sampler",
    .features = (const char *[]){
        CLAP_PLUGIN_FEATURE_INSTRUMENT,
        CLAP_PLUGIN_FEATURE_STEREO,
        NULL
    },
};

// ---------- Audio Ports ----------

static uint32_t audio_ports_count(const clap_plugin_t *plugin, bool is_input) {
    (void)plugin;
    return is_input ? 0 : NUM_OUTPUT_PORTS;
}

static bool audio_ports_get(const clap_plugin_t *plugin, uint32_t index,
                            bool is_input, clap_audio_port_info_t *info) {
    (void)plugin;
    if (is_input || index >= NUM_OUTPUT_PORTS) return false;

    info->id = index;
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = CLAP_INVALID_ID;

    if (index == 0) {
        snprintf(info->name, sizeof(info->name), "Master");
        info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    } else {
        snprintf(info->name, sizeof(info->name), "%s", MIXER_CHANNEL_NAMES[index - 1]);
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
    return is_input ? 1 : 0;
}

static bool note_ports_get(const clap_plugin_t *plugin, uint32_t index,
                           bool is_input, clap_note_port_info_t *info) {
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

static uint32_t note_name_count(const clap_plugin_t *plugin) {
    ggd_plugin_t *plug = plugin->plugin_data;
    uint32_t count = 0;
    for (int i = 0; i < MAX_MIDI_NOTES; i++) {
        if (plug->midi_map.slots[i].num_variants > 0 && plug->midi_map.slots[i].name[0])
            count++;
    }
    return count;
}

static bool note_name_get(const clap_plugin_t *plugin, uint32_t index,
                           clap_note_name_t *note_name) {
    ggd_plugin_t *plug = plugin->plugin_data;
    uint32_t count = 0;
    for (int i = 0; i < MAX_MIDI_NOTES; i++) {
        if (plug->midi_map.slots[i].num_variants > 0 && plug->midi_map.slots[i].name[0]) {
            if (count == index) {
                strncpy(note_name->name, plug->midi_map.slots[i].name, CLAP_NAME_SIZE - 1);
                note_name->name[CLAP_NAME_SIZE - 1] = '\0';
                note_name->port = -1;
                note_name->key = (int16_t)i;
                note_name->channel = -1;
                return true;
            }
            count++;
        }
    }
    return false;
}

static const clap_plugin_note_name_t s_note_name = {
    .count = note_name_count,
    .get = note_name_get,
};

// ---------- Params ----------

static uint32_t plug_params_count(const clap_plugin_t *plugin) {
    return params_count();
}

static bool plug_params_get_info(const clap_plugin_t *plugin, uint32_t index,
                                 clap_param_info_t *info) {
    return params_get_info(index, info);
}

static bool plug_params_get_value(const clap_plugin_t *plugin, clap_id param_id,
                                  double *out) {
    ggd_plugin_t *plug = plugin->plugin_data;

    if (param_id == PARAM_MASTER_GAIN) {
        *out = plug->engine.master_gain_db;
        return true;
    }

    if (param_id >= PARAM_TAB_MASTER_BASE && param_id < PARAM_TAB_MASTER_BASE + TAB_COUNT) {
        *out = plug->engine.tab_master_db[param_id - PARAM_TAB_MASTER_BASE];
        return true;
    }

    MixerChannel ch;
    int offset;
    if (param_is_channel(param_id, &ch, &offset)) {
        const ChannelParams *cp = &plug->engine.channels[ch];
        switch (offset) {
        case PARAM_CH_GAIN:   *out = cp->gain_db; return true;
        case PARAM_CH_PAN:    *out = cp->pan; return true;
        case PARAM_CH_MUTE:   *out = cp->mute ? 1.0 : 0.0; return true;
        case PARAM_CH_SOLO:   *out = cp->solo ? 1.0 : 0.0; return true;
        case PARAM_CH_PHASE:  *out = cp->phase_invert ? 1.0 : 0.0; return true;
        case PARAM_CH_STEREO: *out = cp->stereo_mode ? 1.0 : 0.0; return true;
        }
    }

    switch (param_id) {
    case PARAM_VAR_KICK_SIZE:  *out = plug->kick_size; return true;
    case PARAM_VAR_SNARE_TYPE: *out = plug->snare_type; return true;
    case PARAM_VAR_TOM1_HEAD:  *out = plug->tom_head[0]; return true;
    case PARAM_VAR_TOM2_HEAD:  *out = plug->tom_head[1]; return true;
    case PARAM_VAR_TOM3_HEAD:  *out = plug->tom_head[2]; return true;
    case PARAM_VAR_TOM4_HEAD:  *out = plug->tom_head[3]; return true;
    case PARAM_VAR_CHINA_SIZE: *out = plug->china_size; return true;
    case PARAM_VAR_STACK_TYPE:  *out = plug->stack_type; return true;
    case PARAM_VAR_LCRASH_SIZE: *out = plug->lcrash_size; return true;
    case PARAM_VAR_RCRASH_SIZE:   *out = plug->rcrash_size; return true;
    case PARAM_VAR_MIDI_MAP_MODE: *out = plug->midi_map_mode; return true;
    }

    return false;
}

static bool plug_params_value_to_text(const clap_plugin_t *plugin, clap_id param_id,
                                      double value, char *buf, uint32_t buf_size) {
    return params_value_to_text(param_id, value, buf, buf_size);
}

static bool plug_params_text_to_value(const clap_plugin_t *plugin, clap_id param_id,
                                      const char *text, double *out) {
    return params_text_to_value(param_id, text, out);
}

static void apply_param_value(ggd_plugin_t *plug, clap_id param_id, double value) {
    if (param_id == PARAM_MASTER_GAIN) {
        plug->engine.master_gain_db = (float)value;
        engine_update_master(&plug->engine);
        return;
    }

    if (param_id >= PARAM_TAB_MASTER_BASE && param_id < PARAM_TAB_MASTER_BASE + TAB_COUNT) {
        int t = param_id - PARAM_TAB_MASTER_BASE;
        plug->engine.tab_master_db[t] = (float)value;
        engine_update_tab_master(&plug->engine, (GuiTab)t);
        return;
    }

    MixerChannel ch;
    int offset;
    if (param_is_channel(param_id, &ch, &offset)) {
        ChannelParams *cp = &plug->engine.channels[ch];
        switch (offset) {
        case PARAM_CH_GAIN:   cp->gain_db = (float)value; break;
        case PARAM_CH_PAN:    cp->pan = (float)value; break;
        case PARAM_CH_MUTE:   cp->mute = value > 0.5; break;
        case PARAM_CH_SOLO:   cp->solo = value > 0.5; break;
        case PARAM_CH_PHASE:  cp->phase_invert = value > 0.5; break;
        case PARAM_CH_STEREO: cp->stereo_mode = value > 0.5; break;
        }
        engine_update_channel(&plug->engine, ch);
        if (offset == PARAM_CH_SOLO)
            engine_update_solo_state(&plug->engine);
        return;
    }

    bool variant_changed = false;
    int vi = (int)(value + 0.5);
    switch (param_id) {
    case PARAM_VAR_KICK_SIZE:  plug->kick_size = vi; variant_changed = true; break;
    case PARAM_VAR_SNARE_TYPE: plug->snare_type = vi; variant_changed = true; break;
    case PARAM_VAR_TOM1_HEAD:  plug->tom_head[0] = vi; variant_changed = true; break;
    case PARAM_VAR_TOM2_HEAD:  plug->tom_head[1] = vi; variant_changed = true; break;
    case PARAM_VAR_TOM3_HEAD:  plug->tom_head[2] = vi; variant_changed = true; break;
    case PARAM_VAR_TOM4_HEAD:  plug->tom_head[3] = vi; variant_changed = true; break;
    case PARAM_VAR_CHINA_SIZE: plug->china_size = vi; variant_changed = true; break;
    case PARAM_VAR_STACK_TYPE:  plug->stack_type = vi; variant_changed = true; break;
    case PARAM_VAR_LCRASH_SIZE: plug->lcrash_size = vi; variant_changed = true; break;
    case PARAM_VAR_RCRASH_SIZE:   plug->rcrash_size = vi; variant_changed = true; break;
    case PARAM_VAR_MIDI_MAP_MODE: plug->midi_map_mode = vi; break;
    }

    if (variant_changed) {
        midi_map_update_variants(&plug->midi_map, &plug->bank,
                                 plug->kick_size, plug->snare_type,
                                 plug->tom_head, plug->china_size, plug->stack_type,
                             plug->lcrash_size, plug->rcrash_size);
    }
}

static void plug_params_flush(const clap_plugin_t *plugin,
                               const clap_input_events_t *in,
                               const clap_output_events_t *out) {
    ggd_plugin_t *plug = plugin->plugin_data;
    uint32_t count = in->size(in);
    for (uint32_t i = 0; i < count; i++) {
        const clap_event_header_t *hdr = in->get(in, i);
        if (hdr->space_id != CLAP_CORE_EVENT_SPACE_ID) continue;
        if (hdr->type == CLAP_EVENT_PARAM_VALUE) {
            const clap_event_param_value_t *ev = (const clap_event_param_value_t *)hdr;
            apply_param_value(plug, ev->param_id, ev->value);
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

// ---------- Plugin Lifecycle ----------

static void resolve_samples_path(ggd_plugin_t *plug, const char *plugin_path) {
    // Try to find samples relative to plugin location
    // Expected: plugin is at <project>/vst/ggd-libre.clap
    // Samples at: <project>/output/halpern/wav/
    if (plugin_path && strlen(plugin_path) > 0) {
        // Walk up from plugin_path to find output/halpern/wav
        char base[1024];
        strncpy(base, plugin_path, sizeof(base) - 1);

        // Try stripping filename and /vst/ or just look relative
        char *last_slash = strrchr(base, '/');
        if (last_slash) {
            *last_slash = '\0'; // now base = dir containing plugin
            // Try <dir>/output/halpern/wav
            snprintf(plug->samples_path, sizeof(plug->samples_path),
                     "%s/output/halpern/wav", base);

            FILE *test = fopen(plug->samples_path, "r");
            if (!test) {
                // Try going up one more level: <dir>/../output/halpern/wav
                last_slash = strrchr(base, '/');
                if (last_slash) {
                    *last_slash = '\0';
                    snprintf(plug->samples_path, sizeof(plug->samples_path),
                             "%s/output/halpern/wav", base);
                }
            } else {
                fclose(test);
            }
        }
    }

    // Fallback: check environment variable
    if (plug->samples_path[0] == '\0') {
        const char *env = getenv("GGD_SAMPLES_PATH");
        if (env) strncpy(plug->samples_path, env, sizeof(plug->samples_path) - 1);
    }
}

static bool plug_init(const struct clap_plugin *plugin) {
    ggd_plugin_t *plug = plugin->plugin_data;

    plug->host_params = (const clap_host_params_t *)
        plug->host->get_extension(plug->host, CLAP_EXT_PARAMS);
    plug->host_state = (const clap_host_state_t *)
        plug->host->get_extension(plug->host, CLAP_EXT_STATE);
    plug->host_log = (const clap_host_log_t *)
        plug->host->get_extension(plug->host, CLAP_EXT_LOG);
    plug->host_timer = (const clap_host_timer_support_t *)
        plug->host->get_extension(plug->host, CLAP_EXT_TIMER_SUPPORT);
    plug->host_posix_fd = (const clap_host_posix_fd_support_t *)
        plug->host->get_extension(plug->host, CLAP_EXT_POSIX_FD_SUPPORT);

    // Bring the mixer up before anything else: the plugin struct is calloc'd, so
    // without this every gain_linear is 0 (silence), and CLAP permits
    // state.load() to arrive before activate().
    engine_init(&plug->engine, 48000.0f);

    // Load samples
    if (plug->samples_path[0] == '\0') {
        fprintf(stderr, "ggd-libre: no samples path configured\n");
        return false;
    }

    fprintf(stderr, "ggd-libre: loading samples from %s\n", plug->samples_path);
    if (!sample_bank_load(&plug->bank, plug->samples_path)) {
        fprintf(stderr, "ggd-libre: failed to load samples\n");
        return false;
    }
    engine_set_source_rate(&plug->engine, plug->bank.sample_rate);

    // Load MIDI map
    char json_path[1024];
    // midi_map.json is one level up from the wav/ directory
    char *wav_pos = strstr(plug->samples_path, "/wav");
    if (wav_pos) {
        size_t base_len = wav_pos - plug->samples_path;
        snprintf(json_path, sizeof(json_path), "%.*s/midi_map.json",
                 (int)base_len, plug->samples_path);
    } else {
        snprintf(json_path, sizeof(json_path), "%s/../midi_map.json",
                 plug->samples_path);
    }

    if (!midi_map_load(&plug->midi_map, &plug->bank, json_path)) {
        fprintf(stderr, "ggd-libre: failed to load MIDI map from %s\n", json_path);
        sample_bank_free(&plug->bank);
        return false;
    }

    // Set default variants
    midi_map_update_variants(&plug->midi_map, &plug->bank,
                             plug->kick_size, plug->snare_type,
                             plug->tom_head, plug->china_size, plug->stack_type,
                             plug->lcrash_size, plug->rcrash_size);

    // Start background prefetch of sample data into OS page cache
    sample_bank_prefetch(&plug->bank);

    return true;
}

static void plug_destroy(const struct clap_plugin *plugin) {
    ggd_plugin_t *plug = plugin->plugin_data;
    sample_bank_free(&plug->bank);
    free(plug);
}

static bool plug_activate(const struct clap_plugin *plugin, double sample_rate,
                           uint32_t min_frames, uint32_t max_frames) {
    ggd_plugin_t *plug = plugin->plugin_data;
    // Only refresh rate-dependent caches — a full engine_init here would wipe
    // the mixer and any preset the host already restored via state.load().
    engine_set_sample_rate(&plug->engine, (float)sample_rate);
    engine_reset_voices(&plug->engine);
    plug->activated = true;
    return true;
}

static void plug_deactivate(const struct clap_plugin *plugin) {
    ggd_plugin_t *plug = plugin->plugin_data;
    plug->activated = false;
}

static bool plug_start_processing(const struct clap_plugin *plugin) { return true; }
static void plug_stop_processing(const struct clap_plugin *plugin) {}

static void plug_reset(const struct clap_plugin *plugin) {
    ggd_plugin_t *plug = plugin->plugin_data;
    engine_reset_voices(&plug->engine);
}

static void process_event(ggd_plugin_t *plug, const clap_event_header_t *hdr) {
    if (hdr->space_id != CLAP_CORE_EVENT_SPACE_ID) return;

    switch (hdr->type) {
    case CLAP_EVENT_NOTE_ON: {
        const clap_event_note_t *ev = (const clap_event_note_t *)hdr;
        // No logging here: process_event runs on the audio thread, where a
        // blocking write to stderr is a real-time violation and can glitch.
        int key = ev->key;
        key = midi_remap_note(key, plug->midi_map_mode);
        engine_note_on(&plug->engine, &plug->bank, &plug->midi_map,
                       key, (float)ev->velocity);
        break;
    }
    case CLAP_EVENT_NOTE_OFF: {
        const clap_event_note_t *ev = (const clap_event_note_t *)hdr;
        int key = ev->key;
        key = midi_remap_note(key, plug->midi_map_mode);
        engine_note_off(&plug->engine, key);
        break;
    }
    case CLAP_EVENT_NOTE_CHOKE: {
        const clap_event_note_t *ev = (const clap_event_note_t *)hdr;
        // Kill all voices for this note
        float step = plug->engine.choke_fade_samples > 0.0f
                   ? 1.0f / plug->engine.choke_fade_samples : 1.0f;
        for (int i = 0; i < MAX_VOICES; i++) {
            if (plug->engine.voices[i].active) {
                plug->engine.voices[i].fading_out = true;
                plug->engine.voices[i].fade_gain = 1.0f;
                plug->engine.voices[i].fade_step = step;
            }
        }
        break;
    }
    case CLAP_EVENT_PARAM_VALUE: {
        const clap_event_param_value_t *ev = (const clap_event_param_value_t *)hdr;
        apply_param_value(plug, ev->param_id, ev->value);
        break;
    }
    case CLAP_EVENT_MIDI: {
        const clap_event_midi_t *ev = (const clap_event_midi_t *)hdr;
        uint8_t status = ev->data[0] & 0xF0;
        if (status == 0x90 && ev->data[2] > 0) {
            float vel = ev->data[2] / 127.0f;
            int key = ev->data[1];
            key = midi_remap_note(key, plug->midi_map_mode);
            engine_note_on(&plug->engine, &plug->bank, &plug->midi_map, key, vel);
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
            process_event(plug, hdr);
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

        engine_render(&plug->engine, &plug->bank,
                      block_outs, NUM_OUTPUT_PORTS, block_size);
        i = next_ev_frame;
    }

    // Check if any voices are still active
    for (int vi = 0; vi < MAX_VOICES; vi++) {
        if (plug->engine.voices[vi].active)
            return CLAP_PROCESS_CONTINUE;
    }
    return CLAP_PROCESS_SLEEP;
}

static const void *plug_get_extension(const struct clap_plugin *plugin, const char *id) {
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
    return NULL;
}

static void plug_on_main_thread(const struct clap_plugin *plugin) {}

const clap_plugin_descriptor_t *ggd_get_descriptor(void) {
    return &s_descriptor;
}

clap_plugin_t *ggd_plugin_create(const clap_host_t *host) {
    ggd_plugin_t *p = calloc(1, sizeof(*p));
    if (!p) return NULL;

    p->host = host;
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
