#include "gui.h"
#include "gui_platform.h"
#include "gui_widgets.h"
#include "plugin.h"
#include "params.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
static uint32_t get_time_ms(void) { return GetTickCount(); }
#else
#include <time.h>
static uint32_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#endif

#define DOUBLE_CLICK_MS 400

// ---------- Layout Constants ----------
#define LIB_BAR_H     30
#define LIB_BTN_W     170
#define TAB_BAR_Y     LIB_BAR_H
#define TAB_BAR_H     35
#define TAB_W         120
#define CONTENT_Y     (TAB_BAR_Y + TAB_BAR_H)
#define STRIP_TOP     (CONTENT_Y + 35)
#define STRIP_W       80
#define FADER_H       220
#define BTN_H         22
#define BTN_W         30
#define BTN_GAP       4
#define KNOB_R        14
#define STRIP_START_X 10
#define SEPARATOR_GAP 15
#define SELECTOR_H    35
#define SELECTOR_W    120
#define PREVIEW_BTN_H 20
#define MAX_STRIPS    MAX_LIB_CHANNELS

// Bottom area starts after fader strips + knobs + buttons
#define BOTTOM_AREA_Y  (STRIP_TOP + FADER_H + 10 + KNOB_R*2 + 12 + BTN_H*2 + BTN_GAP + 15)

static const char *tab_names[TAB_COUNT] = { "KICK", "SNARE", "TOMS", "CYMBALS" };

// ---------- Library helpers ----------

static const LibraryDef *gui_def(const PluginGui *gui) {
    int l = gui->plug->selected_lib;
    return l >= 0 ? library_def(l) : NULL;
}

static LibMix *gui_mix(const PluginGui *gui) {
    int l = gui->plug->selected_lib;
    return l >= 0 ? &gui->plug->mix[l] : NULL;
}

// The selected library's channels that belong to a tab, in descriptor order.
static int get_strips(const LibraryDef *d, GuiTab tab, int *channels) {
    int n = 0;
    if (!d) return 0;
    for (int ch = 0; ch < d->num_channels && n < MAX_STRIPS; ch++)
        if (d->channels[ch].tab == tab) channels[n++] = ch;
    return n;
}

// ---------- Layout helpers ----------

static int tab_master_x(int strip_count) {
    return STRIP_START_X + strip_count * STRIP_W + SEPARATOR_GAP;
}

static int global_master_x(int strip_count) {
    return tab_master_x(strip_count) + STRIP_W + SEPARATOR_GAP;
}

// ---------- Helpers ----------

static float db_to_fader(float db) {
    if (db <= -80.0f) return 0.0f;
    return (db + 80.0f) / 92.0f;
}

static float fader_to_db(float v) {
    if (v <= 0.001f) return -80.0f;
    return v * 92.0f - 80.0f;
}

static void db_text(char *buf, size_t size, float db) {
    if (db <= -80.0f) snprintf(buf, size, "-inf");
    else snprintf(buf, size, "%.1f dB", db);
}

// ---------- Drawing ----------

static void draw_library_bar(cairo_t *cr, PluginGui *gui) {
    ggd_plugin_t *plug = gui->plug;
    for (int l = 0; l < LIB_COUNT; l++) {
        const LibraryDef *d = library_def(l);
        int x = l * LIB_BTN_W;
        bool selected = (plug->selected_lib == l);

        if (plug->lib_available[l] || selected) {
            widget_draw_tab(cr, x, 0, LIB_BTN_W, LIB_BAR_H, selected, d->name);
            if (selected) {
                // Accent underline marks the library that plays.
                cairo_set_source_rgb(cr, COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
                cairo_rectangle(cr, x + 6, LIB_BAR_H - 4, LIB_BTN_W - 12, 3);
                cairo_fill(cr);
            }
            continue;
        }

        // Not extracted: dimmed, hatched, badged. Clicking re-scans the disk.
        cairo_set_source_rgb(cr, 0.11, 0.11, 0.12);
        cairo_rectangle(cr, x + 1, 1, LIB_BTN_W - 2, LIB_BAR_H - 2);
        cairo_fill(cr);
        cairo_save(cr);
        cairo_rectangle(cr, x + 1, 1, LIB_BTN_W - 2, LIB_BAR_H - 2);
        cairo_clip(cr);
        cairo_set_source_rgba(cr, 0.9, 0.3, 0.2, 0.18);
        cairo_set_line_width(cr, 2);
        for (int hx = x - LIB_BAR_H; hx < x + LIB_BTN_W; hx += 10) {
            cairo_move_to(cr, hx, LIB_BAR_H);
            cairo_line_to(cr, hx + LIB_BAR_H, 0);
        }
        cairo_stroke(cr);
        cairo_restore(cr);

        cairo_set_source_rgb(cr, 0.45, 0.45, 0.47);
        widget_draw_text_centered(cr, x, 14, LIB_BTN_W, d->name, 11);
        cairo_set_source_rgb(cr, COL_MUTE_R, COL_MUTE_G, COL_MUTE_B);
        widget_draw_text_centered(cr, x, 26, LIB_BTN_W, "NOT EXTRACTED", 8);
    }

    // Title, right-aligned in the library bar
    cairo_set_source_rgb(cr, COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    widget_draw_text(cr, GUI_WIDTH - 110, 20, "GGD LIBRE", 16);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
}

static void draw_channel_strip(cairo_t *cr, const ChannelParams *ch, int x, const char *label) {
    float fv = db_to_fader(ch->gain_db);
    char val_text[32];
    db_text(val_text, sizeof(val_text), ch->gain_db);

    widget_draw_fader(cr, x, STRIP_TOP, STRIP_W, FADER_H, fv, label, val_text);

    int knob_y = STRIP_TOP + FADER_H + 10;
    widget_draw_pan_knob(cr, x + STRIP_W / 2, knob_y, KNOB_R, ch->pan);

    int btn_y = knob_y + KNOB_R + 12;
    int btn_x = x + (STRIP_W - (BTN_W * 2 + BTN_GAP)) / 2;

    widget_draw_toggle(cr, btn_x, btn_y, BTN_W, BTN_H,
                       ch->mute, "M", COL_MUTE_R, COL_MUTE_G, COL_MUTE_B);
    widget_draw_toggle(cr, btn_x + BTN_W + BTN_GAP, btn_y, BTN_W, BTN_H,
                       ch->solo, "S", COL_SOLO_R, COL_SOLO_G, COL_SOLO_B);

    btn_y += BTN_H + BTN_GAP;
    widget_draw_toggle(cr, btn_x, btn_y, BTN_W, BTN_H,
                       ch->phase_invert, "\xCE\xA6",
                       COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
    widget_draw_toggle(cr, btn_x + BTN_W + BTN_GAP, btn_y, BTN_W, BTN_H,
                       ch->stereo_mode, ch->stereo_mode ? "ST" : "MO",
                       COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
}

static void draw_preview_button(cairo_t *cr, int x, int y, int w, int h) {
    // Draw button background
    widget_draw_toggle(cr, x, y, w, h, false, "",
                       COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
    // Draw a play triangle manually
    double cx = x + w / 2.0;
    double cy = y + h / 2.0;
    double sz = h * 0.3;
    cairo_set_source_rgb(cr, COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);
    cairo_move_to(cr, cx - sz * 0.6, cy - sz);
    cairo_line_to(cr, cx + sz * 0.8, cy);
    cairo_line_to(cr, cx - sz * 0.6, cy + sz);
    cairo_close_path(cr);
    cairo_fill(cr);
}

// The row under the faders: per item, a preview button over a selector or a
// fixed caption naming the piece.
static void draw_bottom_items(cairo_t *cr, const LibraryDef *d, const LibMix *mix, GuiTab tab) {
    int info_y = BOTTOM_AREA_Y + PREVIEW_BTN_H + 3;
    for (int i = 0; i < d->num_items[tab]; i++) {
        const LibBottomItem *it = &d->items[tab][i];
        int ix = 10 + i * (SELECTOR_W + 10);
        if (it->preview_note >= 0)
            draw_preview_button(cr, ix, BOTTOM_AREA_Y, SELECTOR_W, PREVIEW_BTN_H);

        const char *value = it->info ? it->info : "";
        if (it->selector >= 0) {
            const LibSelector *s = &d->selectors[it->selector];
            int o = mix->selector[it->selector];
            if (o < 0 || o >= s->num_options) o = 0;
            value = s->options[o];
        }
        widget_draw_selector(cr, ix, info_y, SELECTOR_W, SELECTOR_H, it->label, value);
    }
}

static void draw_message(cairo_t *cr, const char *line1, const char *line2) {
    cairo_set_source_rgb(cr, COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);
    widget_draw_text_centered(cr, 0, CONTENT_Y + 150, GUI_WIDTH, line1, 16);
    cairo_set_source_rgb(cr, 0.6, 0.6, 0.62);
    widget_draw_text_centered(cr, 0, CONTENT_Y + 180, GUI_WIDTH, line2, 11);
}

void gui_draw(PluginGui *gui) {
    if (!gui->cr) return;
    cairo_t *cr = gui->cr;
    ggd_plugin_t *plug = gui->plug;
    const LibraryDef *d = gui_def(gui);
    LibMix *mix = gui_mix(gui);

    // Background
    cairo_set_source_rgb(cr, COL_BG_R, COL_BG_G, COL_BG_B);
    cairo_paint(cr);

    draw_library_bar(cr, gui);

    // Drum tab bar
    for (int i = 0; i < TAB_COUNT; i++) {
        widget_draw_tab(cr, i * TAB_W, TAB_BAR_Y, TAB_W, TAB_BAR_H,
                        gui->active_tab == (GuiTab)i, tab_names[i]);
    }

    // Separators
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_set_line_width(cr, 1);
    cairo_move_to(cr, 0, LIB_BAR_H);
    cairo_line_to(cr, GUI_WIDTH, LIB_BAR_H);
    cairo_move_to(cr, 0, CONTENT_Y);
    cairo_line_to(cr, GUI_WIDTH, CONTENT_Y);
    cairo_stroke(cr);

    // MIDI map mode selector (top right)
    {
        int mm_x = GUI_WIDTH - SELECTOR_W - 10;
        int mm_y = CONTENT_Y + 5;
        int mm = plug->midi_map_mode;
        if (mm < 0 || mm >= MIDIMAP_MODE_COUNT) mm = 0;
        widget_draw_selector(cr, mm_x, mm_y, SELECTOR_W, 28, "MIDI Map", MIDI_MAP_MODE_NAMES[mm]);
    }

    if (gui->loading_lib >= 0) {
        char msg[96];
        snprintf(msg, sizeof(msg), "Loading %s...", library_def(gui->loading_lib)->name);
        draw_message(cr, msg, "Mapping sample files");
    } else if (!d || !mix) {
        draw_message(cr, "No extracted library found",
                     "Extract a library and run tools/build_index.py, then click its tab above");
    } else {
        if (plugin_swap_pending(plug)) {
            cairo_set_source_rgb(cr, 0.85, 0.65, 0.25);
            widget_draw_text(cr, TAB_COUNT * TAB_W + 15, TAB_BAR_Y + 22, "switching...", 11);
        }

        // Channel strips (aligned left)
        int strips[MAX_STRIPS];
        int strip_count = get_strips(d, gui->active_tab, strips);
        for (int i = 0; i < strip_count; i++) {
            draw_channel_strip(cr, &mix->channels[strips[i]], STRIP_START_X + i * STRIP_W,
                               d->channels[strips[i]].strip_label);
        }

        // Vertical separator after channel strips
        int sep_x = tab_master_x(strip_count) - SEPARATOR_GAP / 2;
        cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
        cairo_set_line_width(cr, 1);
        cairo_move_to(cr, sep_x, STRIP_TOP);
        cairo_line_to(cr, sep_x, STRIP_TOP + FADER_H);
        cairo_stroke(cr);

        // Tab master fader (amber)
        {
            int tmx = tab_master_x(strip_count);
            float db = mix->tab_master_db[gui->active_tab];
            char text[32];
            db_text(text, sizeof(text), db);
            widget_draw_fader_colored(cr, tmx, STRIP_TOP, STRIP_W, FADER_H,
                                      db_to_fader(db), tab_names[gui->active_tab], text,
                                      0.85, 0.65, 0.25);
        }

        // Vertical separator before global master
        sep_x = global_master_x(strip_count) - SEPARATOR_GAP / 2;
        cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
        cairo_move_to(cr, sep_x, STRIP_TOP);
        cairo_line_to(cr, sep_x, STRIP_TOP + FADER_H);
        cairo_stroke(cr);

        // Global master fader (green)
        {
            int gmx = global_master_x(strip_count);
            char text[32];
            db_text(text, sizeof(text), plug->engine.master_gain_db);
            widget_draw_fader_colored(cr, gmx, STRIP_TOP, STRIP_W, FADER_H,
                                      db_to_fader(plug->engine.master_gain_db), "MASTER", text,
                                      0.30, 0.80, 0.35);
        }

        draw_bottom_items(cr, d, mix, gui->active_tab);
    }

    cairo_surface_flush(gui->surface);
    gui_platform_flush(gui);
}

// ---------- Hit Testing ----------

typedef enum {
    HIT_NONE = 0, HIT_LIBRARY, HIT_TAB, HIT_FADER, HIT_MUTE, HIT_SOLO,
    HIT_PHASE, HIT_STEREO, HIT_PAN_KNOB,
    HIT_TAB_MASTER_FADER, HIT_GLOBAL_MASTER_FADER,
    HIT_SELECTOR, HIT_PREVIEW, HIT_MIDI_MAP,
} HitType;

typedef struct {
    HitType type;
    int index;   // library, tab, strip channel or bottom item
} HitResult;

static HitResult hit_test(PluginGui *gui, int mx, int my) {
    HitResult r = {HIT_NONE, 0};

    if (my < LIB_BAR_H) {
        int l = mx / LIB_BTN_W;
        if (l >= 0 && l < LIB_COUNT) { r.type = HIT_LIBRARY; r.index = l; }
        return r;
    }
    if (my < CONTENT_Y) {
        int tab = mx / TAB_W;
        if (tab >= 0 && tab < TAB_COUNT) { r.type = HIT_TAB; r.index = tab; }
        return r;
    }

    // MIDI map selector (top right)
    {
        int mm_x = GUI_WIDTH - SELECTOR_W - 10;
        int mm_y = CONTENT_Y + 5;
        if (mx >= mm_x && mx < mm_x + SELECTOR_W && my >= mm_y && my < mm_y + 28) {
            r.type = HIT_MIDI_MAP;
            return r;
        }
    }

    const LibraryDef *d = gui_def(gui);
    if (!d || gui->loading_lib >= 0) return r;

    int strips[MAX_STRIPS];
    int strip_count = get_strips(d, gui->active_tab, strips);

    int tmx = tab_master_x(strip_count);
    if (mx >= tmx && mx < tmx + STRIP_W &&
        my >= STRIP_TOP + 20 && my < STRIP_TOP + FADER_H - 30) {
        r.type = HIT_TAB_MASTER_FADER;
        return r;
    }

    int gmx = global_master_x(strip_count);
    if (mx >= gmx && mx < gmx + STRIP_W &&
        my >= STRIP_TOP + 20 && my < STRIP_TOP + FADER_H - 30) {
        r.type = HIT_GLOBAL_MASTER_FADER;
        return r;
    }

    for (int i = 0; i < strip_count; i++) {
        int sx = STRIP_START_X + i * STRIP_W;
        if (mx < sx || mx >= sx + STRIP_W) continue;
        r.index = strips[i];

        int fader_y = STRIP_TOP + 20, fader_h = FADER_H - 50;
        if (my >= fader_y && my < fader_y + fader_h) { r.type = HIT_FADER; return r; }

        int knob_y = STRIP_TOP + FADER_H + 10;
        int btn_y = knob_y + KNOB_R + 12;
        int btn_x = sx + (STRIP_W - (BTN_W * 2 + BTN_GAP)) / 2;

        if (mx >= btn_x && mx < btn_x + BTN_W && my >= btn_y && my < btn_y + BTN_H)
            { r.type = HIT_MUTE; return r; }
        if (mx >= btn_x + BTN_W + BTN_GAP && mx < btn_x + BTN_W * 2 + BTN_GAP &&
            my >= btn_y && my < btn_y + BTN_H)
            { r.type = HIT_SOLO; return r; }

        btn_y += BTN_H + BTN_GAP;
        if (mx >= btn_x && mx < btn_x + BTN_W && my >= btn_y && my < btn_y + BTN_H)
            { r.type = HIT_PHASE; return r; }
        if (mx >= btn_x + BTN_W + BTN_GAP && mx < btn_x + BTN_W * 2 + BTN_GAP &&
            my >= btn_y && my < btn_y + BTN_H)
            { r.type = HIT_STEREO; return r; }

        int kcx = sx + STRIP_W / 2, kcy = knob_y;
        int dx = mx - kcx, dy = my - kcy;
        if (dx * dx + dy * dy <= KNOB_R * KNOB_R)
            { r.type = HIT_PAN_KNOB; return r; }
        r.index = 0;
    }

    // Preview buttons and selectors
    int info_y = BOTTOM_AREA_Y + PREVIEW_BTN_H + 3;
    for (int i = 0; i < d->num_items[gui->active_tab]; i++) {
        const LibBottomItem *it = &d->items[gui->active_tab][i];
        int ix = 10 + i * (SELECTOR_W + 10);
        if (mx < ix || mx >= ix + SELECTOR_W) continue;
        r.index = i;
        if (my >= BOTTOM_AREA_Y && my < BOTTOM_AREA_Y + PREVIEW_BTN_H && it->preview_note >= 0)
            { r.type = HIT_PREVIEW; return r; }
        if (my >= info_y && my < info_y + SELECTOR_H && it->selector >= 0)
            { r.type = HIT_SELECTOR; return r; }
    }

    r.index = 0;
    return r;
}

// ---------- Param helpers ----------

static void set_param(ggd_plugin_t *plug, clap_id id, double value) {
    plugin_apply_param(plug, id, value);
    if (plug->host_params)
        plug->host_params->request_flush(plug->host);
}

static void toggle_param(ggd_plugin_t *plug, clap_id id) {
    double v = 0.0;
    plugin_get_param(plug, id, &v);
    set_param(plug, id, v > 0.5 ? 0.0 : 1.0);
}

// Fader drag: start, or reset to 0 dB on double-click.
static void begin_fader_drag(PluginGui *gui, clap_id id, int my, uint32_t now) {
    if (gui->last_click_hit == (int)id && (now - gui->last_click_time_ms) < DOUBLE_CLICK_MS) {
        set_param(gui->plug, id, 0.0);
        gui->last_click_hit = -1;
        return;
    }
    gui->last_click_hit = (int)id;
    gui->last_click_time_ms = now;

    double db = 0.0;
    plugin_get_param(gui->plug, id, &db);
    gui->dragging = true;
    gui->drag_param_id = (int)id;
    gui->drag_start_y = my;
    gui->drag_start_value = db_to_fader((float)db);
    set_param(gui->plug, id, fader_to_db(fader_y_to_value(my, STRIP_TOP + 20, FADER_H - 50)));
}

static void select_library(PluginGui *gui, int lib) {
    ggd_plugin_t *plug = gui->plug;
    if (lib == plug->selected_lib) return;
    if (!plug->lib_available[lib]) {
        plugin_probe_libraries(plug);
        if (!plug->lib_available[lib]) return;   // still missing: stays badged
    }
    // Mapping thousands of files takes a moment: show it before blocking.
    gui->loading_lib = lib;
    gui_draw(gui);
    if (!plugin_select_library(plug, lib))
        fprintf(stderr, "ggd-libre: failed to load library %s\n", library_def(lib)->slug);
    gui->loading_lib = -1;
}

// ---------- Mouse event handlers (called by platform backends) ----------

void gui_handle_mouse_down(PluginGui *gui, int mx, int my) {
    HitResult hit = hit_test(gui, mx, my);
    ggd_plugin_t *plug = gui->plug;
    const LibraryDef *d = gui_def(gui);
    uint32_t now = get_time_ms();

    switch (hit.type) {
    case HIT_LIBRARY:
        select_library(gui, hit.index);
        break;

    case HIT_TAB:
        gui->active_tab = (GuiTab)hit.index;
        break;

    case HIT_FADER:
        begin_fader_drag(gui, lib_param_channel(d, hit.index, PARAM_CH_GAIN), my, now);
        break;

    case HIT_TAB_MASTER_FADER:
        begin_fader_drag(gui, lib_param_tab_master(d, gui->active_tab), my, now);
        break;

    case HIT_GLOBAL_MASTER_FADER:
        begin_fader_drag(gui, PARAM_MASTER_GAIN, my, now);
        break;

    case HIT_MUTE:
        toggle_param(plug, lib_param_channel(d, hit.index, PARAM_CH_MUTE));
        break;
    case HIT_SOLO:
        toggle_param(plug, lib_param_channel(d, hit.index, PARAM_CH_SOLO));
        break;
    case HIT_PHASE:
        toggle_param(plug, lib_param_channel(d, hit.index, PARAM_CH_PHASE));
        break;
    case HIT_STEREO:
        toggle_param(plug, lib_param_channel(d, hit.index, PARAM_CH_STEREO));
        break;

    case HIT_PAN_KNOB: {
        clap_id pid = lib_param_channel(d, hit.index, PARAM_CH_PAN);
        if (gui->last_click_hit == (int)pid && (now - gui->last_click_time_ms) < DOUBLE_CLICK_MS) {
            set_param(plug, pid, 0.0);
            gui->last_click_hit = -1;
            break;
        }
        gui->last_click_hit = (int)pid;
        gui->last_click_time_ms = now;

        double pan = 0.0;
        plugin_get_param(plug, pid, &pan);
        gui->dragging = true;
        gui->drag_param_id = (int)pid;
        gui->drag_start_y = my;
        gui->drag_start_value = (float)pan;
        break;
    }

    case HIT_SELECTOR: {
        const LibBottomItem *it = &d->items[gui->active_tab][hit.index];
        const LibSelector *s = &d->selectors[it->selector];
        int next = (gui_mix(gui)->selector[it->selector] + 1) % s->num_options;
        set_param(plug, s->param_id, next);
        break;
    }

    case HIT_PREVIEW:
        plugin_preview_note(plug, d->items[gui->active_tab][hit.index].preview_note, 0.8f);
        break;

    case HIT_MIDI_MAP:
        set_param(plug, PARAM_MIDI_MAP_MODE, (plug->midi_map_mode + 1) % MIDIMAP_MODE_COUNT);
        break;

    default: break;
    }

    gui_draw(gui);
}

void gui_handle_mouse_up(PluginGui *gui) {
    gui->dragging = false;
    gui->drag_param_id = -1;
}

void gui_handle_mouse_move(PluginGui *gui, int mx, int my) {
    (void)mx;
    if (!gui->dragging || gui->drag_param_id < 0) return;

    ParamRef ref;
    clap_id id = (clap_id)gui->drag_param_id;
    if (!params_resolve(id, &ref)) return;

    if (ref.kind == PK_CHANNEL && ref.offset == PARAM_CH_PAN) {
        float new_pan = gui->drag_start_value + (gui->drag_start_y - my) / 100.0f;
        if (new_pan < -1.0f) new_pan = -1.0f;
        if (new_pan > 1.0f) new_pan = 1.0f;
        set_param(gui->plug, id, new_pan);
    } else {
        set_param(gui->plug, id, fader_to_db(fader_y_to_value(my, STRIP_TOP + 20, FADER_H - 50)));
    }
    gui_draw(gui);
}

// ---------- CLAP GUI Extension (shared callbacks) ----------

static bool gui_is_api_supported(const clap_plugin_t *plugin, const char *api, bool is_floating) {
    (void)plugin; (void)is_floating;
    bool supported;
#ifdef _WIN32
    supported = !strcmp(api, CLAP_WINDOW_API_WIN32);
#else
    supported = !strcmp(api, CLAP_WINDOW_API_X11);
#endif
    fprintf(stderr, "ggd-libre: gui_is_api_supported api='%s' floating=%d -> %d\n",
            api, is_floating, supported);
    return supported;
}

static bool gui_get_preferred_api(const clap_plugin_t *plugin, const char **api, bool *is_floating) {
    (void)plugin;
#ifdef _WIN32
    *api = CLAP_WINDOW_API_WIN32;
#else
    *api = CLAP_WINDOW_API_X11;
#endif
    *is_floating = false;
    return true;
}

static bool gui_create(const clap_plugin_t *plugin, const char *api, bool is_floating) {
    (void)is_floating;
#ifdef _WIN32
    if (strcmp(api, CLAP_WINDOW_API_WIN32) != 0) return false;
#else
    if (strcmp(api, CLAP_WINDOW_API_X11) != 0) return false;
#endif

    ggd_plugin_t *plug = plugin->plugin_data;
    PluginGui *gui = calloc(1, sizeof(PluginGui));
    if (!gui) return false;

    gui->plug = plug;
    gui->width = GUI_WIDTH;
    gui->height = GUI_HEIGHT;
    gui->active_tab = TAB_KICK;
    gui->loading_lib = -1;
    gui->drag_param_id = -1;

    fprintf(stderr, "ggd-libre: gui_create api='%s'\n", api);

    if (!gui_platform_create(gui)) {
        fprintf(stderr, "ggd-libre: gui_platform_create failed\n");
        free(gui);
        return false;
    }

    gui->created = true;
    plug->gui = gui;
    fprintf(stderr, "ggd-libre: gui_create OK\n");
    return true;
}

static void gui_unregister_callbacks(ggd_plugin_t *plug, PluginGui *gui) {
    if (gui->timer_registered && plug->host_timer) {
        plug->host_timer->unregister_timer(plug->host, gui->timer_id);
        gui->timer_registered = false;
    }
    gui_platform_unregister_fd(gui);
}

static void gui_register_callbacks(ggd_plugin_t *plug, PluginGui *gui) {
    if (!gui->timer_registered && plug->host_timer) {
        if (plug->host_timer->register_timer(plug->host, 33, &gui->timer_id))
            gui->timer_registered = true;
    }
    gui_platform_register_fd(gui);
}

static void gui_destroy(const clap_plugin_t *plugin) {
    ggd_plugin_t *plug = plugin->plugin_data;
    PluginGui *gui = plug->gui;
    if (!gui) return;

    gui_unregister_callbacks(plug, gui);
    if (gui->cr) cairo_destroy(gui->cr);
    if (gui->surface) cairo_surface_destroy(gui->surface);
    gui_platform_destroy(gui);
    free(gui);
    plug->gui = NULL;
}

static bool gui_set_scale(const clap_plugin_t *p, double s) { (void)p; (void)s; return false; }
static bool gui_get_size(const clap_plugin_t *p, uint32_t *w, uint32_t *h) {
    (void)p; *w = GUI_WIDTH; *h = GUI_HEIGHT; return true;
}
static bool gui_can_resize(const clap_plugin_t *p) { (void)p; return false; }
static bool gui_get_resize_hints(const clap_plugin_t *p, clap_gui_resize_hints_t *h) {
    (void)p; (void)h; return false;
}
static bool gui_adjust_size(const clap_plugin_t *p, uint32_t *w, uint32_t *h) {
    (void)p; *w = GUI_WIDTH; *h = GUI_HEIGHT; return true;
}
static bool gui_set_size(const clap_plugin_t *p, uint32_t w, uint32_t h) {
    (void)p; return w == GUI_WIDTH && h == GUI_HEIGHT;
}

static bool gui_set_parent(const clap_plugin_t *plugin, const clap_window_t *window) {
    ggd_plugin_t *plug = plugin->plugin_data;
    PluginGui *gui = plug->gui;
    if (!gui) { fprintf(stderr, "ggd-libre: gui_set_parent: no gui\n"); return false; }
    fprintf(stderr, "ggd-libre: gui_set_parent called\n");
    bool ok = gui_platform_set_parent(gui, window);
    fprintf(stderr, "ggd-libre: gui_set_parent %s (surface=%p cr=%p)\n",
            ok ? "OK" : "FAILED", (void*)gui->surface, (void*)gui->cr);
    return ok;
}

static bool gui_set_transient(const clap_plugin_t *p, const clap_window_t *w) {
    (void)p; (void)w; return false;
}
static void gui_suggest_title(const clap_plugin_t *p, const char *t) { (void)p; (void)t; }

static bool gui_show(const clap_plugin_t *plugin) {
    ggd_plugin_t *plug = plugin->plugin_data;
    PluginGui *gui = plug->gui;
    if (!gui) { fprintf(stderr, "ggd-libre: gui_show: no gui\n"); return false; }
    fprintf(stderr, "ggd-libre: gui_show called\n");

    if (!gui_platform_show(gui)) {
        fprintf(stderr, "ggd-libre: gui_platform_show failed\n");
        return false;
    }
    gui->visible = true;
    gui_register_callbacks(plug, gui);
    fprintf(stderr, "ggd-libre: gui_show: timer_registered=%d\n", gui->timer_registered);
    gui_draw(gui);
    fprintf(stderr, "ggd-libre: gui_show OK\n");
    return true;
}

static bool gui_hide(const clap_plugin_t *plugin) {
    ggd_plugin_t *plug = plugin->plugin_data;
    PluginGui *gui = plug->gui;
    if (!gui) return false;

    gui_unregister_callbacks(plug, gui);
    gui_platform_hide(gui);
    gui->visible = false;
    return true;
}

const clap_plugin_gui_t ggd_gui_ext = {
    .is_api_supported = gui_is_api_supported,
    .get_preferred_api = gui_get_preferred_api,
    .create = gui_create,
    .destroy = gui_destroy,
    .set_scale = gui_set_scale,
    .get_size = gui_get_size,
    .can_resize = gui_can_resize,
    .get_resize_hints = gui_get_resize_hints,
    .adjust_size = gui_adjust_size,
    .set_size = gui_set_size,
    .set_parent = gui_set_parent,
    .set_transient = gui_set_transient,
    .suggest_title = gui_suggest_title,
    .show = gui_show,
    .hide = gui_hide,
};

// ---------- Timer ----------

static void timer_on_timer(const clap_plugin_t *plugin, clap_id timer_id) {
    (void)timer_id;
    ggd_plugin_t *plug = plugin->plugin_data;
    PluginGui *gui = plug->gui;
    if (!gui || !gui->visible) return;

    gui_platform_process_events(gui);
    gui_draw(gui);
}

const clap_plugin_timer_support_t ggd_timer_ext = {
    .on_timer = timer_on_timer,
};
