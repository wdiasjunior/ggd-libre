#include "gui.h"
#include "gui_widgets.h"
#include "plugin.h"
#include "params.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __linux__

// ---------- Layout Constants ----------
#define TAB_BAR_H     35
#define TAB_W         120
#define STRIP_TOP     70
#define STRIP_W       80
#define FADER_H       220
#define BTN_H         22
#define BTN_W         30
#define BTN_GAP       4
#define KNOB_R        14
#define MASTER_X      720
#define SELECTOR_H    35
#define SELECTOR_W    120

// ---------- Channel Strip Layout ----------

typedef struct {
    DrumChannel channel;
    const char *label;
} StripInfo;

static const StripInfo s_kick_strips[] = {
    { DRUM_KICK, "KICK" },
};

static const StripInfo s_snare_strips[] = {
    { DRUM_SNARE, "SNARE" },
};

static const StripInfo s_tom_strips[] = {
    { DRUM_RACK1,  "RACK 1" },
    { DRUM_RACK2,  "RACK 2" },
    { DRUM_FLOOR1, "FLOOR 1" },
    { DRUM_FLOOR2, "FLOOR 2" },
};

static const StripInfo s_cymbal_strips[] = {
    { DRUM_HIHAT,  "HI-HAT" },
    { DRUM_LCRASH, "L CRASH" },
    { DRUM_RCRASH, "R CRASH" },
    { DRUM_RIDE,   "RIDE" },
    { DRUM_CHINA,  "CHINA" },
    { DRUM_STACK,  "STACK" },
    { DRUM_SPLASH, "SPLASH" },
    { DRUM_ACCENT, "ACCENT" },
};

static const char *tab_names[TAB_COUNT] = { "KICK", "SNARE", "TOMS", "CYMBALS" };

static const StripInfo *get_strips(GuiTab tab, int *count) {
    switch (tab) {
    case TAB_KICK:    *count = 1; return s_kick_strips;
    case TAB_SNARE:   *count = 1; return s_snare_strips;
    case TAB_TOMS:    *count = 4; return s_tom_strips;
    case TAB_CYMBALS: *count = 8; return s_cymbal_strips;
    default:          *count = 0; return NULL;
    }
}

// ---------- Helper: gain dB to fader 0..1 ----------
static float db_to_fader(float db) {
    if (db <= -80.0f) return 0.0f;
    return (db + 80.0f) / 92.0f; // -80..+12 -> 0..1
}

static float fader_to_db(float v) {
    if (v <= 0.001f) return -80.0f;
    return v * 92.0f - 80.0f;
}

// ---------- Drawing ----------

static void draw_channel_strip(cairo_t *cr, ggd_plugin_t *plug,
                                int x, const StripInfo *info) {
    ChannelParams *ch = &plug->engine.channels[info->channel];

    // Gain fader
    float fv = db_to_fader(ch->gain_db);
    char val_text[32];
    if (ch->gain_db <= -80.0f)
        snprintf(val_text, sizeof(val_text), "-inf");
    else
        snprintf(val_text, sizeof(val_text), "%.1f dB", ch->gain_db);

    widget_draw_fader(cr, x, STRIP_TOP, STRIP_W, FADER_H,
                      fv, info->label, val_text);

    // Pan knob below fader
    int knob_y = STRIP_TOP + FADER_H + 10;
    widget_draw_pan_knob(cr, x + STRIP_W / 2, knob_y, KNOB_R, ch->pan);

    // Buttons row below knob
    int btn_y = knob_y + KNOB_R + 12;
    int btn_x = x + (STRIP_W - (BTN_W * 2 + BTN_GAP)) / 2;

    widget_draw_toggle(cr, btn_x, btn_y, BTN_W, BTN_H,
                       ch->mute, "M", COL_MUTE_R, COL_MUTE_G, COL_MUTE_B);
    widget_draw_toggle(cr, btn_x + BTN_W + BTN_GAP, btn_y, BTN_W, BTN_H,
                       ch->solo, "S", COL_SOLO_R, COL_SOLO_G, COL_SOLO_B);

    // Second row: Phase and Stereo/Mono
    btn_y += BTN_H + BTN_GAP;
    widget_draw_toggle(cr, btn_x, btn_y, BTN_W, BTN_H,
                       ch->phase_invert, "\xCE\xA6",  // Greek capital phi (UTF-8)
                       COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
    widget_draw_toggle(cr, btn_x + BTN_W + BTN_GAP, btn_y, BTN_W, BTN_H,
                       ch->stereo_mode, ch->stereo_mode ? "ST" : "MO",
                       COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
}

static void draw_variant_selectors(cairo_t *cr, ggd_plugin_t *plug, GuiTab tab) {
    int sel_x = 10;
    int sel_y = GUI_HEIGHT - SELECTOR_H - 10;

    switch (tab) {
    case TAB_KICK: {
        const char *sizes[] = {"22x16", "22x20"};
        widget_draw_selector(cr, sel_x, sel_y, SELECTOR_W, SELECTOR_H,
                             "Kick Size", sizes[plug->kick_size % 2]);
        break;
    }
    case TAB_SNARE: {
        const char *types[] = {"High", "Med", "Low", "13\"", "BFSD"};
        widget_draw_selector(cr, sel_x, sel_y, SELECTOR_W, SELECTOR_H,
                             "Snare Type", types[plug->snare_type % 5]);
        break;
    }
    case TAB_TOMS: {
        const char *heads[] = {"Clear", "Coated"};
        for (int i = 0; i < 4; i++) {
            char label[32];
            snprintf(label, sizeof(label), "Tom %d", i + 1);
            widget_draw_selector(cr, sel_x + i * (SELECTOR_W + 10), sel_y,
                                 SELECTOR_W, SELECTOR_H,
                                 label, heads[plug->tom_head[i] % 2]);
        }
        break;
    }
    case TAB_CYMBALS: {
        const char *china[] = {"Default", "18\""};
        const char *stack[] = {"Default", "Mini"};
        widget_draw_selector(cr, sel_x, sel_y, SELECTOR_W, SELECTOR_H,
                             "China", china[plug->china_size % 2]);
        widget_draw_selector(cr, sel_x + SELECTOR_W + 10, sel_y, SELECTOR_W, SELECTOR_H,
                             "Stack", stack[plug->stack_type % 2]);
        break;
    }
    default: break;
    }
}

static void draw_gui(PluginGui *gui) {
    cairo_t *cr = gui->cr;
    ggd_plugin_t *plug = gui->plug;

    // Background
    cairo_set_source_rgb(cr, COL_BG_R, COL_BG_G, COL_BG_B);
    cairo_paint(cr);

    // Tab bar
    for (int i = 0; i < TAB_COUNT; i++) {
        widget_draw_tab(cr, i * TAB_W, 0, TAB_W, TAB_BAR_H,
                        gui->active_tab == i, tab_names[i]);
    }

    // Separator line under tabs
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_set_line_width(cr, 1);
    cairo_move_to(cr, 0, TAB_BAR_H);
    cairo_line_to(cr, GUI_WIDTH, TAB_BAR_H);
    cairo_stroke(cr);

    // Title / logo area
    cairo_set_source_rgb(cr, COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    widget_draw_text(cr, 15, STRIP_TOP - 10, "GGD LIBRE", 16);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

    // Channel strips
    int strip_count;
    const StripInfo *strips = get_strips(gui->active_tab, &strip_count);
    int strip_start_x = 100;
    for (int i = 0; i < strip_count; i++) {
        draw_channel_strip(cr, plug, strip_start_x + i * STRIP_W, &strips[i]);
    }

    // Master fader
    float master_fv = db_to_fader(plug->engine.master_gain_db);
    char master_text[32];
    if (plug->engine.master_gain_db <= -80.0f)
        snprintf(master_text, sizeof(master_text), "-inf");
    else
        snprintf(master_text, sizeof(master_text), "%.1f dB", plug->engine.master_gain_db);
    widget_draw_fader(cr, MASTER_X, STRIP_TOP, STRIP_W, FADER_H,
                      master_fv, "MASTER", master_text);

    // Variant selectors
    draw_variant_selectors(cr, plug, gui->active_tab);

    cairo_surface_flush(gui->surface);
    XFlush(gui->display);
}

// ---------- Hit Testing ----------

typedef enum {
    HIT_NONE = 0,
    HIT_TAB,
    HIT_FADER,
    HIT_MUTE,
    HIT_SOLO,
    HIT_PHASE,
    HIT_STEREO,
    HIT_PAN_KNOB,
    HIT_MASTER_FADER,
    HIT_SELECTOR,
} HitType;

typedef struct {
    HitType type;
    int     strip_index;  // which strip (or tab index)
    int     selector_idx; // for selectors
} HitResult;

static HitResult hit_test(PluginGui *gui, int mx, int my) {
    HitResult r = {HIT_NONE, 0, 0};

    // Tab bar
    if (my < TAB_BAR_H) {
        int tab = mx / TAB_W;
        if (tab >= 0 && tab < TAB_COUNT) {
            r.type = HIT_TAB;
            r.strip_index = tab;
        }
        return r;
    }

    // Master fader
    if (mx >= MASTER_X && mx < MASTER_X + STRIP_W &&
        my >= STRIP_TOP + 20 && my < STRIP_TOP + FADER_H - 30) {
        r.type = HIT_MASTER_FADER;
        return r;
    }

    // Channel strips
    int strip_count;
    const StripInfo *strips = get_strips(gui->active_tab, &strip_count);
    int strip_start_x = 100;

    for (int i = 0; i < strip_count; i++) {
        int sx = strip_start_x + i * STRIP_W;
        if (mx < sx || mx >= sx + STRIP_W) continue;

        // Fader area
        int fader_y = STRIP_TOP + 20;
        int fader_h = FADER_H - 50;
        if (my >= fader_y && my < fader_y + fader_h) {
            r.type = HIT_FADER;
            r.strip_index = i;
            return r;
        }

        // Buttons
        int knob_y = STRIP_TOP + FADER_H + 10;
        int btn_y = knob_y + KNOB_R + 12;
        int btn_x = sx + (STRIP_W - (BTN_W * 2 + BTN_GAP)) / 2;

        // Mute
        if (mx >= btn_x && mx < btn_x + BTN_W &&
            my >= btn_y && my < btn_y + BTN_H) {
            r.type = HIT_MUTE;
            r.strip_index = i;
            return r;
        }
        // Solo
        if (mx >= btn_x + BTN_W + BTN_GAP && mx < btn_x + BTN_W * 2 + BTN_GAP &&
            my >= btn_y && my < btn_y + BTN_H) {
            r.type = HIT_SOLO;
            r.strip_index = i;
            return r;
        }

        btn_y += BTN_H + BTN_GAP;
        // Phase
        if (mx >= btn_x && mx < btn_x + BTN_W &&
            my >= btn_y && my < btn_y + BTN_H) {
            r.type = HIT_PHASE;
            r.strip_index = i;
            return r;
        }
        // Stereo
        if (mx >= btn_x + BTN_W + BTN_GAP && mx < btn_x + BTN_W * 2 + BTN_GAP &&
            my >= btn_y && my < btn_y + BTN_H) {
            r.type = HIT_STEREO;
            r.strip_index = i;
            return r;
        }

        // Pan knob
        int kcx = sx + STRIP_W / 2;
        int kcy = knob_y;
        int dx = mx - kcx, dy = my - kcy;
        if (dx * dx + dy * dy <= KNOB_R * KNOB_R) {
            r.type = HIT_PAN_KNOB;
            r.strip_index = i;
            return r;
        }
    }

    // Variant selectors
    int sel_y = GUI_HEIGHT - SELECTOR_H - 10;
    if (my >= sel_y && my < sel_y + SELECTOR_H) {
        int num_selectors = 0;
        switch (gui->active_tab) {
        case TAB_KICK:    num_selectors = 1; break;
        case TAB_SNARE:   num_selectors = 1; break;
        case TAB_TOMS:    num_selectors = 4; break;
        case TAB_CYMBALS: num_selectors = 2; break;
        default: break;
        }
        for (int i = 0; i < num_selectors; i++) {
            int sx = 10 + i * (SELECTOR_W + 10);
            if (mx >= sx && mx < sx + SELECTOR_W) {
                r.type = HIT_SELECTOR;
                r.selector_idx = i;
                return r;
            }
        }
    }

    return r;
}

// ---------- Event -> Parameter Changes ----------

static void send_param_change(ggd_plugin_t *plug, clap_id param_id, double value) {
    // Apply locally
    // The host will pick this up through the output events in process()
    // For now, apply directly and request a state rescan
    if (plug->host_params)
        plug->host_params->request_flush(plug->host);
}

static void toggle_param(ggd_plugin_t *plug, clap_id param_id) {
    DrumChannel ch;
    int offset;
    if (param_is_channel(param_id, &ch, &offset)) {
        ChannelParams *cp = &plug->engine.channels[ch];
        switch (offset) {
        case PARAM_CH_MUTE:   cp->mute = !cp->mute; break;
        case PARAM_CH_SOLO:   cp->solo = !cp->solo; engine_update_solo_state(&plug->engine); break;
        case PARAM_CH_PHASE:  cp->phase_invert = !cp->phase_invert; break;
        case PARAM_CH_STEREO: cp->stereo_mode = !cp->stereo_mode; break;
        }
    }
    send_param_change(plug, param_id, 0);
}

static void cycle_selector(ggd_plugin_t *plug, GuiTab tab, int sel_idx) {
    switch (tab) {
    case TAB_KICK:
        plug->kick_size = (plug->kick_size + 1) % 2;
        break;
    case TAB_SNARE:
        plug->snare_type = (plug->snare_type + 1) % 5;
        break;
    case TAB_TOMS:
        if (sel_idx >= 0 && sel_idx < 4)
            plug->tom_head[sel_idx] = (plug->tom_head[sel_idx] + 1) % 2;
        break;
    case TAB_CYMBALS:
        if (sel_idx == 0)
            plug->china_size = (plug->china_size + 1) % 2;
        else if (sel_idx == 1)
            plug->stack_type = (plug->stack_type + 1) % 2;
        break;
    default: break;
    }
    midi_map_update_variants(&plug->midi_map, &plug->bank,
                             plug->kick_size, plug->snare_type,
                             plug->tom_head, plug->china_size, plug->stack_type);
    send_param_change(plug, 0, 0);
}

// ---------- X11 Event Handling ----------

static void handle_x11_events(PluginGui *gui) {
    while (XPending(gui->display)) {
        XEvent ev;
        XNextEvent(gui->display, &ev);

        switch (ev.type) {
        case Expose:
            if (ev.xexpose.count == 0)
                draw_gui(gui);
            break;

        case ButtonPress: {
            int mx = ev.xbutton.x, my = ev.xbutton.y;
            HitResult hit = hit_test(gui, mx, my);

            int strip_count;
            const StripInfo *strips = get_strips(gui->active_tab, &strip_count);

            switch (hit.type) {
            case HIT_TAB:
                gui->active_tab = (GuiTab)hit.strip_index;
                draw_gui(gui);
                break;

            case HIT_FADER:
                if (hit.strip_index < strip_count) {
                    gui->dragging = true;
                    gui->drag_param_id = param_channel_id(strips[hit.strip_index].channel, PARAM_CH_GAIN);
                    gui->drag_start_y = my;
                    gui->drag_start_value = db_to_fader(
                        gui->plug->engine.channels[strips[hit.strip_index].channel].gain_db);

                    // Immediate update
                    float v = fader_y_to_value(my, STRIP_TOP + 20, FADER_H - 50);
                    float db = fader_to_db(v);
                    gui->plug->engine.channels[strips[hit.strip_index].channel].gain_db = db;
                    engine_update_channel(&gui->plug->engine, strips[hit.strip_index].channel);
                    draw_gui(gui);
                }
                break;

            case HIT_MASTER_FADER: {
                gui->dragging = true;
                gui->drag_param_id = PARAM_MASTER_GAIN;
                gui->drag_start_y = my;
                gui->drag_start_value = db_to_fader(gui->plug->engine.master_gain_db);

                float v = fader_y_to_value(my, STRIP_TOP + 20, FADER_H - 50);
                float db = fader_to_db(v);
                gui->plug->engine.master_gain_db = db;
                engine_update_master(&gui->plug->engine);
                draw_gui(gui);
                break;
            }

            case HIT_MUTE:
                if (hit.strip_index < strip_count)
                    toggle_param(gui->plug,
                                 param_channel_id(strips[hit.strip_index].channel, PARAM_CH_MUTE));
                draw_gui(gui);
                break;

            case HIT_SOLO:
                if (hit.strip_index < strip_count)
                    toggle_param(gui->plug,
                                 param_channel_id(strips[hit.strip_index].channel, PARAM_CH_SOLO));
                draw_gui(gui);
                break;

            case HIT_PHASE:
                if (hit.strip_index < strip_count)
                    toggle_param(gui->plug,
                                 param_channel_id(strips[hit.strip_index].channel, PARAM_CH_PHASE));
                draw_gui(gui);
                break;

            case HIT_STEREO:
                if (hit.strip_index < strip_count)
                    toggle_param(gui->plug,
                                 param_channel_id(strips[hit.strip_index].channel, PARAM_CH_STEREO));
                draw_gui(gui);
                break;

            case HIT_PAN_KNOB:
                if (hit.strip_index < strip_count) {
                    gui->dragging = true;
                    gui->drag_param_id = param_channel_id(strips[hit.strip_index].channel, PARAM_CH_PAN);
                    gui->drag_start_y = my;
                    gui->drag_start_value = gui->plug->engine.channels[strips[hit.strip_index].channel].pan;
                }
                break;

            case HIT_SELECTOR:
                cycle_selector(gui->plug, gui->active_tab, hit.selector_idx);
                draw_gui(gui);
                break;

            default: break;
            }
            break;
        }

        case ButtonRelease:
            gui->dragging = false;
            gui->drag_param_id = -1;
            break;

        case MotionNotify:
            if (gui->dragging) {
                int my = ev.xmotion.y;
                DrumChannel ch;
                int offset;

                if (gui->drag_param_id == PARAM_MASTER_GAIN) {
                    float v = fader_y_to_value(my, STRIP_TOP + 20, FADER_H - 50);
                    gui->plug->engine.master_gain_db = fader_to_db(v);
                    engine_update_master(&gui->plug->engine);
                } else if (param_is_channel(gui->drag_param_id, &ch, &offset)) {
                    if (offset == PARAM_CH_GAIN) {
                        float v = fader_y_to_value(my, STRIP_TOP + 20, FADER_H - 50);
                        gui->plug->engine.channels[ch].gain_db = fader_to_db(v);
                        engine_update_channel(&gui->plug->engine, ch);
                    } else if (offset == PARAM_CH_PAN) {
                        float delta = (gui->drag_start_y - my) / 100.0f;
                        float new_pan = gui->drag_start_value + delta;
                        if (new_pan < -1.0f) new_pan = -1.0f;
                        if (new_pan > 1.0f) new_pan = 1.0f;
                        gui->plug->engine.channels[ch].pan = new_pan;
                        engine_update_channel(&gui->plug->engine, ch);
                    }
                }
                draw_gui(gui);
            }
            break;

        case ClientMessage:
            // Window close
            break;
        }
    }
}

// ---------- CLAP GUI Extension ----------

static bool gui_is_api_supported(const clap_plugin_t *plugin, const char *api, bool is_floating) {
    return !strcmp(api, CLAP_WINDOW_API_X11);
}

static bool gui_get_preferred_api(const clap_plugin_t *plugin, const char **api, bool *is_floating) {
    *api = CLAP_WINDOW_API_X11;
    *is_floating = false;
    return true;
}

static bool gui_create(const clap_plugin_t *plugin, const char *api, bool is_floating) {
    if (strcmp(api, CLAP_WINDOW_API_X11) != 0) return false;

    ggd_plugin_t *plug = plugin->plugin_data;
    PluginGui *gui = calloc(1, sizeof(PluginGui));
    if (!gui) return false;

    gui->plug = plug;
    gui->width = GUI_WIDTH;
    gui->height = GUI_HEIGHT;
    gui->active_tab = TAB_KICK;
    gui->drag_param_id = -1;

    gui->display = XOpenDisplay(NULL);
    if (!gui->display) {
        free(gui);
        return false;
    }
    gui->owns_display = true;
    gui->screen = DefaultScreen(gui->display);
    gui->visual = DefaultVisual(gui->display, gui->screen);
    gui->created = true;

    // Store gui pointer in plugin (we'll add this field)
    plug->gui = gui;
    return true;
}

static void gui_destroy(const clap_plugin_t *plugin) {
    ggd_plugin_t *plug = plugin->plugin_data;
    PluginGui *gui = plug->gui;
    if (!gui) return;

    if (gui->cr) cairo_destroy(gui->cr);
    if (gui->surface) cairo_surface_destroy(gui->surface);
    if (gui->window) XDestroyWindow(gui->display, gui->window);
    if (gui->owns_display && gui->display) XCloseDisplay(gui->display);

    free(gui);
    plug->gui = NULL;
}

static bool gui_set_scale(const clap_plugin_t *plugin, double scale) {
    return false; // Fixed scale for now
}

static bool gui_get_size(const clap_plugin_t *plugin, uint32_t *width, uint32_t *height) {
    *width = GUI_WIDTH;
    *height = GUI_HEIGHT;
    return true;
}

static bool gui_can_resize(const clap_plugin_t *plugin) { return false; }

static bool gui_get_resize_hints(const clap_plugin_t *plugin, clap_gui_resize_hints_t *hints) {
    return false;
}

static bool gui_adjust_size(const clap_plugin_t *plugin, uint32_t *w, uint32_t *h) {
    *w = GUI_WIDTH;
    *h = GUI_HEIGHT;
    return true;
}

static bool gui_set_size(const clap_plugin_t *plugin, uint32_t w, uint32_t h) {
    return w == GUI_WIDTH && h == GUI_HEIGHT;
}

static bool gui_set_parent(const clap_plugin_t *plugin, const clap_window_t *window) {
    ggd_plugin_t *plug = plugin->plugin_data;
    PluginGui *gui = plug->gui;
    if (!gui || !gui->display) return false;

    gui->parent_window = (Window)window->x11;

    // Create child window
    gui->window = XCreateSimpleWindow(gui->display, gui->parent_window,
                                       0, 0, GUI_WIDTH, GUI_HEIGHT, 0,
                                       BlackPixel(gui->display, gui->screen),
                                       BlackPixel(gui->display, gui->screen));

    XSelectInput(gui->display, gui->window,
                 ExposureMask | ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask | StructureNotifyMask);

    // Create Cairo surface
    gui->surface = cairo_xlib_surface_create(gui->display, gui->window,
                                              gui->visual, GUI_WIDTH, GUI_HEIGHT);
    gui->cr = cairo_create(gui->surface);

    XMapWindow(gui->display, gui->window);
    XFlush(gui->display);

    return true;
}

static bool gui_set_transient(const clap_plugin_t *plugin, const clap_window_t *window) {
    return false;
}

static void gui_suggest_title(const clap_plugin_t *plugin, const char *title) {}

static bool gui_show(const clap_plugin_t *plugin) {
    ggd_plugin_t *plug = plugin->plugin_data;
    PluginGui *gui = plug->gui;
    if (!gui || !gui->window) return false;

    XMapWindow(gui->display, gui->window);
    gui->visible = true;
    draw_gui(gui);
    return true;
}

static bool gui_hide(const clap_plugin_t *plugin) {
    ggd_plugin_t *plug = plugin->plugin_data;
    PluginGui *gui = plug->gui;
    if (!gui || !gui->window) return false;

    XUnmapWindow(gui->display, gui->window);
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

// ---------- Timer Support ----------

static void timer_on_timer(const clap_plugin_t *plugin, clap_id timer_id) {
    ggd_plugin_t *plug = plugin->plugin_data;
    PluginGui *gui = plug->gui;
    if (!gui || !gui->visible) return;

    handle_x11_events(gui);
    draw_gui(gui);
}

const clap_plugin_timer_support_t ggd_timer_ext = {
    .on_timer = timer_on_timer,
};

// ---------- POSIX FD Support ----------

static void posix_fd_on_fd(const clap_plugin_t *plugin, int fd, clap_posix_fd_flags_t flags) {
    ggd_plugin_t *plug = plugin->plugin_data;
    PluginGui *gui = plug->gui;
    if (!gui) return;

    handle_x11_events(gui);
}

const clap_plugin_posix_fd_support_t ggd_posix_fd_ext = {
    .on_fd = posix_fd_on_fd,
};

#endif // __linux__
