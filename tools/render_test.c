// Headless CLAP render host for ggd-libre regression tests (dev only).
//
// Loads the plugin, plays every note 24..83 at three velocities, and writes the
// master bus (and optionally every stem) as 32-bit float WAV.
//
// Build:  gcc -O2 -I../../plugin/clap-sdk/include -o render_test render_test.c -ldl -lpthread
// Usage:  render_test <plugin.clap> <out.wav> [--rate N] [--block N] [--lib SLUG]
//                     [--stems DIR] [--notes a,b,c] [--switch SLUG@SECONDS]
//
// --lib and --switch use the plugin's private "com.ggd-libre.test" extension when
// it is present; older builds ignore them.

#include <clap/clap.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

typedef struct {
    bool (*select_library)(const clap_plugin_t *plugin, const char *slug);
    bool (*swap_pending)(const clap_plugin_t *plugin);
} ggd_test_ext_t;

static const clap_plugin_t *g_plugin;
static bool g_callback_requested;

static const void *host_get_extension(const clap_host_t *h, const char *id) {
    (void)h; (void)id;
    return NULL;
}
static void host_request_restart(const clap_host_t *h) { (void)h; }
static void host_request_process(const clap_host_t *h) { (void)h; }
static void host_request_callback(const clap_host_t *h) { (void)h; g_callback_requested = true; }

static const clap_host_t s_host = {
    .clap_version = CLAP_VERSION_INIT,
    .host_data = NULL,
    .name = "ggd-render-test",
    .vendor = "ggd-libre",
    .url = "",
    .version = "1.0",
    .get_extension = host_get_extension,
    .request_restart = host_request_restart,
    .request_process = host_request_process,
    .request_callback = host_request_callback,
};

// ---- event list ----
#define MAX_EVENTS 64
typedef struct { clap_event_note_t ev[MAX_EVENTS]; uint32_t n; } EventList;
static uint32_t in_size(const clap_input_events_t *l) { return ((EventList *)l->ctx)->n; }
static const clap_event_header_t *in_get(const clap_input_events_t *l, uint32_t i) {
    return &((EventList *)l->ctx)->ev[i].header;
}
static bool out_try_push(const clap_output_events_t *l, const clap_event_header_t *e) {
    (void)l; (void)e; return true;
}

// ---- WAV writer (32-bit float stereo) ----
static void write_wav(const char *path, const float *interleaved, uint32_t frames, uint32_t rate) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    uint32_t data_bytes = frames * 2 * 4;
    uint32_t riff = 36 + data_bytes;
    uint16_t fmt_tag = 3, ch = 2, bits = 32, align = 8;
    uint32_t fmt_len = 16, byte_rate = rate * 8;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); fwrite(&fmt_len, 4, 1, f); fwrite(&fmt_tag, 2, 1, f);
    fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&byte_rate, 4, 1, f);
    fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data_bytes, 4, 1, f);
    fwrite(interleaved, 4, (size_t)frames * 2, f);
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <plugin.clap> <out.wav> [--rate N] [--block N] [--lib SLUG] "
                        "[--stems DIR] [--notes a,b,c] [--switch SLUG@SECONDS]\n", argv[0]);
        return 2;
    }
    const char *clap_path = argv[1], *out_path = argv[2];
    uint32_t rate = 48000, block = 256;
    const char *lib = NULL, *stems_dir = NULL, *notes_arg = NULL;
    const char *switch_lib = NULL; double switch_at = -1.0;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--rate") && i + 1 < argc) rate = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--block") && i + 1 < argc) block = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--lib") && i + 1 < argc) lib = argv[++i];
        else if (!strcmp(argv[i], "--stems") && i + 1 < argc) stems_dir = argv[++i];
        else if (!strcmp(argv[i], "--notes") && i + 1 < argc) notes_arg = argv[++i];
        else if (!strcmp(argv[i], "--switch") && i + 1 < argc) {
            static char buf[64];
            snprintf(buf, sizeof(buf), "%s", argv[++i]);
            char *at = strchr(buf, '@');
            if (!at) { fprintf(stderr, "--switch needs SLUG@SECONDS\n"); return 2; }
            *at = '\0'; switch_lib = buf; switch_at = atof(at + 1);
        }
    }

    // Score: (note, velocity) hits spaced 0.4 s apart.
    int notes[128], n_notes = 0;
    if (notes_arg) {
        char tmp[1024]; snprintf(tmp, sizeof(tmp), "%s", notes_arg);
        for (char *t = strtok(tmp, ","); t && n_notes < 128; t = strtok(NULL, ","))
            notes[n_notes++] = atoi(t);
    } else {
        for (int n = 24; n <= 83; n++) notes[n_notes++] = n;
    }
    const double vels[3] = {40 / 127.0, 90 / 127.0, 1.0};
    const uint32_t spacing = (uint32_t)(0.4 * rate);
    const uint32_t n_hits = (uint32_t)n_notes * 3;
    const uint32_t total = n_hits * spacing + 2 * rate;

    void *so = dlopen(clap_path, RTLD_NOW | RTLD_LOCAL);
    if (!so) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    const clap_plugin_entry_t *entry = dlsym(so, "clap_entry");
    if (!entry || !entry->init(clap_path)) { fprintf(stderr, "bad clap_entry\n"); return 1; }
    const clap_plugin_factory_t *fac = entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    const clap_plugin_descriptor_t *desc = fac->get_plugin_descriptor(fac, 0);
    g_plugin = fac->create_plugin(fac, &s_host, desc->id);
    if (!g_plugin || !g_plugin->init(g_plugin)) { fprintf(stderr, "plugin init failed\n"); return 1; }

    const ggd_test_ext_t *test = g_plugin->get_extension(g_plugin, "com.ggd-libre.test");
    if (lib) {
        if (!test) fprintf(stderr, "warning: plugin has no test extension, --lib ignored\n");
        else if (!test->select_library(g_plugin, lib)) { fprintf(stderr, "select_library(%s) failed\n", lib); return 1; }
    }

    const clap_plugin_audio_ports_t *ports = g_plugin->get_extension(g_plugin, CLAP_EXT_AUDIO_PORTS);
    uint32_t n_ports = ports->count(g_plugin, false);

    if (!g_plugin->activate(g_plugin, rate, 1, block)) { fprintf(stderr, "activate failed\n"); return 1; }
    g_plugin->start_processing(g_plugin);

    // Let a library selected before activation finish swapping in.
    for (int spin = 0; test && test->swap_pending(g_plugin) && spin < 1000; spin++) {
        clap_audio_buffer_t dummy_bufs[64];
        float *dummy[64][2];
        static float scratch[64][2][4096];
        for (uint32_t p = 0; p < n_ports; p++) {
            dummy[p][0] = scratch[p][0]; dummy[p][1] = scratch[p][1];
            dummy_bufs[p] = (clap_audio_buffer_t){ .data32 = dummy[p], .channel_count = 2 };
        }
        EventList none = {.n = 0};
        clap_input_events_t in = {.ctx = &none, .size = in_size, .get = in_get};
        clap_output_events_t out = {.ctx = NULL, .try_push = out_try_push};
        clap_process_t pr = {.steady_time = -1, .frames_count = block, .audio_outputs = dummy_bufs,
                             .audio_outputs_count = n_ports, .in_events = &in, .out_events = &out};
        g_plugin->process(g_plugin, &pr);
        if (g_callback_requested) { g_callback_requested = false; g_plugin->on_main_thread(g_plugin); }
        usleep(1000);
    }

    float **port_l = calloc(n_ports, sizeof(float *)), **port_r = calloc(n_ports, sizeof(float *));
    for (uint32_t p = 0; p < n_ports; p++) {
        port_l[p] = calloc(total, sizeof(float));
        port_r[p] = calloc(total, sizeof(float));
    }
    clap_audio_buffer_t *bufs = calloc(n_ports, sizeof(*bufs));
    float *(*chans)[2] = calloc(n_ports, sizeof(*chans));

    uint32_t next_hit = 0;
    bool switched = false;
    for (uint32_t pos = 0; pos < total; pos += block) {
        uint32_t frames = (total - pos < block) ? total - pos : block;
        EventList evs = {.n = 0};
        while (next_hit < n_hits && (uint64_t)next_hit * spacing < (uint64_t)pos + frames) {
            uint32_t t = next_hit * spacing - pos;
            clap_event_note_t *e = &evs.ev[evs.n++];
            *e = (clap_event_note_t){
                .header = {.size = sizeof(*e), .time = t, .space_id = CLAP_CORE_EVENT_SPACE_ID,
                           .type = CLAP_EVENT_NOTE_ON, .flags = 0},
                .note_id = -1, .port_index = 0, .channel = 0,
                .key = (int16_t)notes[next_hit / 3], .velocity = vels[next_hit % 3]};
            next_hit++;
        }
        if (switch_lib && !switched && pos >= switch_at * rate) {
            switched = true;
            if (!test || !test->select_library(g_plugin, switch_lib))
                fprintf(stderr, "warning: switch to %s failed\n", switch_lib);
        }
        for (uint32_t p = 0; p < n_ports; p++) {
            chans[p][0] = port_l[p] + pos;
            chans[p][1] = port_r[p] + pos;
            bufs[p] = (clap_audio_buffer_t){.data32 = chans[p], .channel_count = 2};
        }
        clap_input_events_t in = {.ctx = &evs, .size = in_size, .get = in_get};
        clap_output_events_t out = {.ctx = NULL, .try_push = out_try_push};
        clap_process_t pr = {.steady_time = pos, .frames_count = frames, .audio_outputs = bufs,
                             .audio_outputs_count = n_ports, .in_events = &in, .out_events = &out};
        g_plugin->process(g_plugin, &pr);
        if (g_callback_requested) { g_callback_requested = false; g_plugin->on_main_thread(g_plugin); }
    }

    g_plugin->stop_processing(g_plugin);
    g_plugin->deactivate(g_plugin);

    float *inter = malloc((size_t)total * 2 * sizeof(float));
    for (uint32_t p = 0; p < n_ports; p++) {
        if (p > 0 && !stems_dir) break;
        for (uint32_t i = 0; i < total; i++) { inter[2 * i] = port_l[p][i]; inter[2 * i + 1] = port_r[p][i]; }
        if (p == 0) {
            write_wav(out_path, inter, total, rate);
        } else {
            clap_audio_port_info_t info;
            ports->get(g_plugin, p, false, &info);
            char path[1200];
            snprintf(path, sizeof(path), "%s/%02u_%s.wav", stems_dir, p, info.name);
            for (char *c = path + strlen(stems_dir) + 1; *c; c++) if (*c == ' ' || *c == '/') *c = '_';
            write_wav(path, inter, total, rate);
        }
    }

    g_plugin->destroy(g_plugin);
    entry->deinit();
    fprintf(stderr, "rendered %u frames @ %u Hz, %u hits, %u ports\n", total, rate, n_hits, n_ports);
    return 0;
}
