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
#define TAB_BAR_H     35
#define TAB_W         120
#define STRIP_TOP     70
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

// ---------- Channel Strip Layout ----------

typedef struct {
    MixerChannel channel;
    const char *label;
} StripInfo;

static const StripInfo s_kick_strips[] = {
    { CH_KICK_CLOSE, "CLOSE" },
    { CH_KICK_OH,    "OH" },
    { CH_KICK_NEAR,  "NEAR ROOM" },
    { CH_KICK_FAR,   "FAR ROOM" },
};

static const StripInfo s_snare_strips[] = {
    { CH_SNARE_TOP1,   "TOP MIC 1" },
    { CH_SNARE_TOP2,   "TOP MIC 2" },
    { CH_SNARE_BOTTOM, "BOTTOM" },
    { CH_SNARE_OH,     "OH" },
    { CH_SNARE_NEAR,   "NEAR ROOM" },
    { CH_SNARE_FAR,    "FAR ROOM" },
};

static const StripInfo s_tom_strips[] = {
    { CH_RACK1,      "RACK 1" },
    { CH_RACK2,      "RACK 2" },
    { CH_FLOOR1,     "FLOOR 1" },
    { CH_FLOOR2,     "FLOOR 2" },
    { CH_TOMS_OH,    "OH" },
    { CH_TOMS_NEAR,  "NEAR ROOM" },
    { CH_TOMS_FAR,   "FAR ROOM" },
};

static const StripInfo s_cymbal_strips[] = {
    { CH_HIHAT,       "HI-HAT" },
    { CH_LCRASH,      "L CRASH" },
    { CH_RCRASH,      "R CRASH" },
    { CH_RIDE,        "RIDE" },
    { CH_STACK,       "STACK" },
    { CH_SPLASH,      "SPLASH" },
    { CH_CHINA,       "CHINA" },
    { CH_CYMBALS_OH,  "OH" },
    { CH_CYMBALS_NEAR,"NEAR ROOM" },
    { CH_CYMBALS_FAR, "FAR ROOM" },
};

static const char *tab_names[TAB_COUNT] = { "KICK", "SNARE", "TOMS", "CYMBALS" };

static const StripInfo *get_strips(GuiTab tab, int *count) {
    switch (tab) {
    case TAB_KICK:    *count = 4; return s_kick_strips;
    case TAB_SNARE:   *count = 6; return s_snare_strips;
    case TAB_TOMS:    *count = 7; return s_tom_strips;
    case TAB_CYMBALS: *count = 10; return s_cymbal_strips;
    default:          *count = 0; return NULL;
    }
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

// ---------- Drawing ----------

static void draw_channel_strip(cairo_t *cr, ggd_plugin_t *plug,
                                int x, const StripInfo *info) {
    ChannelParams *ch = &plug->engine.channels[info->channel];

    float fv = db_to_fader(ch->gain_db);
    char val_text[32];
    if (ch->gain_db <= -80.0f)
        snprintf(val_text, sizeof(val_text), "-inf");
    else
        snprintf(val_text, sizeof(val_text), "%.1f dB", ch->gain_db);

    widget_draw_fader(cr, x, STRIP_TOP, STRIP_W, FADER_H,
                      fv, info->label, val_text);

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

static void draw_selector_with_preview(cairo_t *cr, int x, int *y,
                                       const char *label, const char *value) {
    draw_preview_button(cr, x, *y, SELECTOR_W, PREVIEW_BTN_H);
    *y += PREVIEW_BTN_H + 3;
    widget_draw_selector(cr, x, *y, SELECTOR_W, SELECTOR_H, label, value);
}

// Bottom area starts after fader strips + knobs + buttons
#define BOTTOM_AREA_Y  (STRIP_TOP + FADER_H + 10 + KNOB_R*2 + 12 + BTN_H*2 + BTN_GAP + 15)

static void draw_variant_selectors(cairo_t *cr, ggd_plugin_t *plug, GuiTab tab) {
    int sel_x = 10;
    int sel_y = BOTTOM_AREA_Y;

    switch (tab) {
    case TAB_KICK: {
        const char *sizes[] = {"22x16", "22x20"};
        int y = sel_y;
        draw_selector_with_preview(cr, sel_x, &y,
                                   "Kick Size", sizes[plug->kick_size % 2]);
        break;
    }
    case TAB_SNARE: {
        const char *types[] = {"High", "Med", "Low", "13\"", "BFSD"};
        int y = sel_y;
        draw_selector_with_preview(cr, sel_x, &y,
                                   "Snare Type", types[plug->snare_type % 5]);
        break;
    }
    case TAB_TOMS: {
        const char *heads[] = {"Clear", "Coated"};
        for (int i = 0; i < 4; i++) {
            char label[32];
            snprintf(label, sizeof(label), "Tom %d", i + 1);
            int y = sel_y;
            draw_selector_with_preview(cr, sel_x + i * (SELECTOR_W + 10), &y,
                                       label, heads[plug->tom_head[i] % 2]);
        }
        break;
    }
    case TAB_CYMBALS: {
        // 7 cymbal pieces, each with preview button + info/selector box
        // Order matches strip faders: Hi-Hat, L Crash, R Crash, Ride, Stack, Splash, China
        static const char *cymbal_names[] = {
            "Hi-Hat", "L Crash", "R Crash", "Ride", "Stack", "Splash", "China"
        };
        const char *lcrash[] = {"17\" Byz Thin", "18\" Med Byz"};
        const char *rcrash[] = {"20\" Byz Thin", "19\" Med Byz"};
        const char *china[] = {"China", "18\" China"};
        const char *stack_v[] = {"Stack", "Mini Stack"};

        int item_w = SELECTOR_W;
        int item_gap = 10;
        int info_y = sel_y + PREVIEW_BTN_H + 3;

        for (int i = 0; i < 7; i++) {
            int ix = sel_x + i * (item_w + item_gap);

            // Preview button
            draw_preview_button(cr, ix, sel_y, item_w, PREVIEW_BTN_H);

            // Info/selector box below
            const char *value;
            switch (i) {
            case 0: value = "14\" Paiste 2002"; break;                    // Hi-Hat
            case 1: value = lcrash[plug->lcrash_size % 2]; break;         // L Crash
            case 2: value = rcrash[plug->rcrash_size % 2]; break;         // R Crash
            case 3: value = "22\" Paiste Ride"; break;                    // Ride
            case 4: value = stack_v[plug->stack_type % 2]; break;         // Stack
            case 5: value = "Paiste 2002 Splash"; break;                  // Splash
            default: value = china[plug->china_size % 2]; break;          // China
            }
            widget_draw_selector(cr, ix, info_y, item_w, SELECTOR_H,
                                 cymbal_names[i], value);
        }
        break;
    }
    default: break;
    }
}

void gui_draw(PluginGui *gui) {
    if (!gui->cr) return;
    cairo_t *cr = gui->cr;
    ggd_plugin_t *plug = gui->plug;

    // Background
    cairo_set_source_rgb(cr, COL_BG_R, COL_BG_G, COL_BG_B);
    cairo_paint(cr);

    // Tab bar
    for (int i = 0; i < TAB_COUNT; i++) {
        widget_draw_tab(cr, i * TAB_W, 0, TAB_W, TAB_BAR_H,
                        gui->active_tab == (GuiTab)i, tab_names[i]);
    }

    // Separator
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_set_line_width(cr, 1);
    cairo_move_to(cr, 0, TAB_BAR_H);
    cairo_line_to(cr, GUI_WIDTH, TAB_BAR_H);
    cairo_stroke(cr);

    // Title
    cairo_set_source_rgb(cr, COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    widget_draw_text(cr, 15, STRIP_TOP - 10, "GGD LIBRE", 16);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

    // MIDI map mode selector (top right)
    {
        int mm_x = GUI_WIDTH - SELECTOR_W - 10;
        int mm_y = TAB_BAR_H + 5;
        const char *mode = plug->midi_map_mode == 0 ? "GGD" : "General MIDI";
        widget_draw_selector(cr, mm_x, mm_y, SELECTOR_W, 28, "MIDI Map", mode);
    }

    // Channel strips (aligned left)
    int strip_count;
    const StripInfo *strips = get_strips(gui->active_tab, &strip_count);
    for (int i = 0; i < strip_count; i++) {
        draw_channel_strip(cr, plug, STRIP_START_X + i * STRIP_W, &strips[i]);
    }

    // Vertical separator after channel strips
    int sep_x = tab_master_x(strip_count) - SEPARATOR_GAP / 2;
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_set_line_width(cr, 1);
    cairo_move_to(cr, sep_x, STRIP_TOP);
    cairo_line_to(cr, sep_x, STRIP_TOP + FADER_H);
    cairo_stroke(cr);

    // Tab master fader (amber/orange color)
    {
        static const char *tab_master_labels[] = {"KICK", "SNARE", "TOMS", "CYMBALS"};
        int tmx = tab_master_x(strip_count);
        float tab_fv = db_to_fader(plug->engine.tab_master_db[gui->active_tab]);
        char tab_text[32];
        if (plug->engine.tab_master_db[gui->active_tab] <= -80.0f)
            snprintf(tab_text, sizeof(tab_text), "-inf");
        else
            snprintf(tab_text, sizeof(tab_text), "%.1f dB",
                     plug->engine.tab_master_db[gui->active_tab]);
        widget_draw_fader_colored(cr, tmx, STRIP_TOP, STRIP_W, FADER_H,
                                  tab_fv, tab_master_labels[gui->active_tab], tab_text,
                                  0.85, 0.65, 0.25);  // amber
    }

    // Vertical separator before global master
    sep_x = global_master_x(strip_count) - SEPARATOR_GAP / 2;
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_move_to(cr, sep_x, STRIP_TOP);
    cairo_line_to(cr, sep_x, STRIP_TOP + FADER_H);
    cairo_stroke(cr);

    // Global master fader (green color)
    {
        int gmx = global_master_x(strip_count);
        float master_fv = db_to_fader(plug->engine.master_gain_db);
        char master_text[32];
        if (plug->engine.master_gain_db <= -80.0f)
            snprintf(master_text, sizeof(master_text), "-inf");
        else
            snprintf(master_text, sizeof(master_text), "%.1f dB", plug->engine.master_gain_db);
        widget_draw_fader_colored(cr, gmx, STRIP_TOP, STRIP_W, FADER_H,
                                  master_fv, "MASTER", master_text,
                                  0.30, 0.80, 0.35);  // green
    }

    // Variant selectors
    draw_variant_selectors(cr, plug, gui->active_tab);

    cairo_surface_flush(gui->surface);
    gui_platform_flush(gui);
}

// ---------- Hit Testing ----------

typedef enum {
    HIT_NONE = 0, HIT_TAB, HIT_FADER, HIT_MUTE, HIT_SOLO,
    HIT_PHASE, HIT_STEREO, HIT_PAN_KNOB,
    HIT_TAB_MASTER_FADER, HIT_GLOBAL_MASTER_FADER,
    HIT_SELECTOR, HIT_PREVIEW, HIT_MIDI_MAP,
} HitType;

typedef struct {
    HitType type;
    int strip_index;
    int selector_idx;
} HitResult;

static HitResult hit_test(PluginGui *gui, int mx, int my) {
    HitResult r = {HIT_NONE, 0, 0};

    if (my < TAB_BAR_H) {
        int tab = mx / TAB_W;
        if (tab >= 0 && tab < TAB_COUNT) { r.type = HIT_TAB; r.strip_index = tab; }
        return r;
    }

    // MIDI map selector (top right)
    {
        int mm_x = GUI_WIDTH - SELECTOR_W - 10;
        int mm_y = TAB_BAR_H + 5;
        if (mx >= mm_x && mx < mm_x + SELECTOR_W && my >= mm_y && my < mm_y + 28) {
            r.type = HIT_MIDI_MAP;
            return r;
        }
    }

    int strip_count;
    const StripInfo *strips = get_strips(gui->active_tab, &strip_count);

    // Tab master fader
    int tmx = tab_master_x(strip_count);
    if (mx >= tmx && mx < tmx + STRIP_W &&
        my >= STRIP_TOP + 20 && my < STRIP_TOP + FADER_H - 30) {
        r.type = HIT_TAB_MASTER_FADER;
        return r;
    }

    // Global master fader
    int gmx = global_master_x(strip_count);
    if (mx >= gmx && mx < gmx + STRIP_W &&
        my >= STRIP_TOP + 20 && my < STRIP_TOP + FADER_H - 30) {
        r.type = HIT_GLOBAL_MASTER_FADER;
        return r;
    }

    for (int i = 0; i < strip_count; i++) {
        int sx = STRIP_START_X + i * STRIP_W;
        if (mx < sx || mx >= sx + STRIP_W) continue;

        int fader_y = STRIP_TOP + 20, fader_h = FADER_H - 50;
        if (my >= fader_y && my < fader_y + fader_h) {
            r.type = HIT_FADER; r.strip_index = i; return r;
        }

        int knob_y = STRIP_TOP + FADER_H + 10;
        int btn_y = knob_y + KNOB_R + 12;
        int btn_x = sx + (STRIP_W - (BTN_W * 2 + BTN_GAP)) / 2;

        if (mx >= btn_x && mx < btn_x + BTN_W && my >= btn_y && my < btn_y + BTN_H)
            { r.type = HIT_MUTE; r.strip_index = i; return r; }
        if (mx >= btn_x + BTN_W + BTN_GAP && mx < btn_x + BTN_W * 2 + BTN_GAP &&
            my >= btn_y && my < btn_y + BTN_H)
            { r.type = HIT_SOLO; r.strip_index = i; return r; }

        btn_y += BTN_H + BTN_GAP;
        if (mx >= btn_x && mx < btn_x + BTN_W && my >= btn_y && my < btn_y + BTN_H)
            { r.type = HIT_PHASE; r.strip_index = i; return r; }
        if (mx >= btn_x + BTN_W + BTN_GAP && mx < btn_x + BTN_W * 2 + BTN_GAP &&
            my >= btn_y && my < btn_y + BTN_H)
            { r.type = HIT_STEREO; r.strip_index = i; return r; }

        int kcx = sx + STRIP_W / 2, kcy = knob_y;
        int dx = mx - kcx, dy = my - kcy;
        if (dx * dx + dy * dy <= KNOB_R * KNOB_R)
            { r.type = HIT_PAN_KNOB; r.strip_index = i; return r; }
    }

    // Preview buttons and selectors
    int preview_y = BOTTOM_AREA_Y;
    int sel_bottom_y = preview_y + PREVIEW_BTN_H + 3;

    if (gui->active_tab == TAB_CYMBALS) {
        // Cymbals: 7 items, each with preview button + selector/info box
        int item_w = SELECTOR_W, item_gap = 10;
        int info_y = preview_y + PREVIEW_BTN_H + 3;

        for (int i = 0; i < 7; i++) {
            int ix = 10 + i * (item_w + item_gap);
            if (mx >= ix && mx < ix + item_w) {
                if (my >= preview_y && my < preview_y + PREVIEW_BTN_H) {
                    r.type = HIT_PREVIEW;
                    r.selector_idx = i;
                    return r;
                }
                if (my >= info_y && my < info_y + SELECTOR_H) {
                    // Only L Crash(1), R Crash(2), Stack(4), China(6) have variants
                    if (i == 1 || i == 2 || i == 4 || i == 6) {
                        r.type = HIT_SELECTOR;
                        r.selector_idx = i;
                        return r;
                    }
                }
            }
        }
    } else {
        // Other tabs: preview + selector paired
        int num = 0;
        switch (gui->active_tab) {
        case TAB_KICK: num = 1; break; case TAB_SNARE: num = 1; break;
        case TAB_TOMS: num = 4; break; default: break;
        }
        if (my >= preview_y && my < sel_bottom_y + SELECTOR_H) {
            for (int i = 0; i < num; i++) {
                int sx = 10 + i * (SELECTOR_W + 10);
                if (mx >= sx && mx < sx + SELECTOR_W) {
                    if (my < sel_bottom_y) {
                        r.type = HIT_PREVIEW;
                        r.selector_idx = i;
                    } else {
                        r.type = HIT_SELECTOR;
                        r.selector_idx = i;
                    }
                    return r;
                }
            }
        }
    }

    return r;
}

// ---------- Param helpers ----------

static void send_param_change(ggd_plugin_t *plug) {
    if (plug->host_params)
        plug->host_params->request_flush(plug->host);
}

static void toggle_param(ggd_plugin_t *plug, clap_id param_id) {
    MixerChannel ch;
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
    send_param_change(plug);
}

// Map (tab, selector_index) to a MIDI note for preview playback
static int preview_midi_note(GuiTab tab, int sel_idx) {
    switch (tab) {
    case TAB_KICK:   return 24;  // Kick Main Hit
    case TAB_SNARE:  return 26;  // Snare Hit
    case TAB_TOMS:
        switch (sel_idx) {
        case 0: return 33;  // Hi Tom
        case 1: return 35;  // Mid Tom 1
        case 2: return 37;  // Mid Tom 2
        case 3: return 39;  // Floor Tom
        }
        return -1;
    case TAB_CYMBALS:
        switch (sel_idx) {
        case 0: return 47;  // Hi-Hat Tip Closed
        case 1: return 62;  // L Crash Hit
        case 2: return 67;  // R Crash Hit
        case 3: return 72;  // Ride Tip
        case 4: return 81;  // Stack Tight Hit
        case 5: return 83;  // Splash Hit
        case 6: return 76;  // China Main Hit
        }
        return -1;
    default: return -1;
    }
}

static void play_preview(ggd_plugin_t *plug, GuiTab tab, int sel_idx) {
    int note = preview_midi_note(tab, sel_idx);
    if (note >= 0)
        engine_note_on(&plug->engine, &plug->bank, &plug->midi_map, note, 0.8f);
}

static void cycle_selector(ggd_plugin_t *plug, GuiTab tab, int sel_idx) {
    switch (tab) {
    case TAB_KICK:    plug->kick_size = (plug->kick_size + 1) % 2; break;
    case TAB_SNARE:   plug->snare_type = (plug->snare_type + 1) % 5; break;
    case TAB_TOMS:
        if (sel_idx >= 0 && sel_idx < 4)
            plug->tom_head[sel_idx] = (plug->tom_head[sel_idx] + 1) % 2;
        break;
    case TAB_CYMBALS:
        if (sel_idx == 1)      plug->lcrash_size = (plug->lcrash_size + 1) % 2;
        else if (sel_idx == 2) plug->rcrash_size = (plug->rcrash_size + 1) % 2;
        else if (sel_idx == 4) plug->stack_type = (plug->stack_type + 1) % 2;
        else if (sel_idx == 6) plug->china_size = (plug->china_size + 1) % 2;
        break;
    default: break;
    }
    midi_map_update_variants(&plug->midi_map, &plug->bank,
                             plug->kick_size, plug->snare_type,
                             plug->tom_head, plug->china_size, plug->stack_type,
                             plug->lcrash_size, plug->rcrash_size);
    send_param_change(plug);
}

// ---------- Mouse event handlers (called by platform backends) ----------

void gui_handle_mouse_down(PluginGui *gui, int mx, int my) {
    HitResult hit = hit_test(gui, mx, my);
    int strip_count;
    const StripInfo *strips = get_strips(gui->active_tab, &strip_count);

    uint32_t now = get_time_ms();

    switch (hit.type) {
    case HIT_TAB:
        gui->active_tab = (GuiTab)hit.strip_index;
        break;

    case HIT_FADER:
        if (hit.strip_index < strip_count) {
            int pid = (int)param_channel_id(strips[hit.strip_index].channel, PARAM_CH_GAIN);
            // Double-click: reset to 0 dB
            if (gui->last_click_hit == pid && (now - gui->last_click_time_ms) < DOUBLE_CLICK_MS) {
                gui->plug->engine.channels[strips[hit.strip_index].channel].gain_db = 0.0f;
                engine_update_channel(&gui->plug->engine, strips[hit.strip_index].channel);
                gui->last_click_hit = -1;
                break;
            }
            gui->last_click_hit = pid;
            gui->last_click_time_ms = now;

            gui->dragging = true;
            gui->drag_param_id = pid;
            gui->drag_start_y = my;
            gui->drag_start_value = db_to_fader(
                gui->plug->engine.channels[strips[hit.strip_index].channel].gain_db);
            float v = fader_y_to_value(my, STRIP_TOP + 20, FADER_H - 50);
            gui->plug->engine.channels[strips[hit.strip_index].channel].gain_db = fader_to_db(v);
            engine_update_channel(&gui->plug->engine, strips[hit.strip_index].channel);
        }
        break;

    case HIT_TAB_MASTER_FADER: {
        int tab_pid = PARAM_TAB_MASTER_BASE + gui->active_tab;
        if (gui->last_click_hit == tab_pid && (now - gui->last_click_time_ms) < DOUBLE_CLICK_MS) {
            gui->plug->engine.tab_master_db[gui->active_tab] = 0.0f;
            engine_update_tab_master(&gui->plug->engine, gui->active_tab);
            gui->last_click_hit = -1;
            break;
        }
        gui->last_click_hit = tab_pid;
        gui->last_click_time_ms = now;

        gui->dragging = true;
        gui->drag_param_id = tab_pid;
        gui->drag_start_y = my;
        gui->drag_start_value = db_to_fader(gui->plug->engine.tab_master_db[gui->active_tab]);
        float tv = fader_y_to_value(my, STRIP_TOP + 20, FADER_H - 50);
        gui->plug->engine.tab_master_db[gui->active_tab] = fader_to_db(tv);
        engine_update_tab_master(&gui->plug->engine, gui->active_tab);
        break;
    }

    case HIT_GLOBAL_MASTER_FADER:
        if (gui->last_click_hit == PARAM_MASTER_GAIN && (now - gui->last_click_time_ms) < DOUBLE_CLICK_MS) {
            gui->plug->engine.master_gain_db = 0.0f;
            engine_update_master(&gui->plug->engine);
            gui->last_click_hit = -1;
            break;
        }
        gui->last_click_hit = PARAM_MASTER_GAIN;
        gui->last_click_time_ms = now;

        gui->dragging = true;
        gui->drag_param_id = PARAM_MASTER_GAIN;
        gui->drag_start_y = my;
        gui->drag_start_value = db_to_fader(gui->plug->engine.master_gain_db);
        {
            float v = fader_y_to_value(my, STRIP_TOP + 20, FADER_H - 50);
            gui->plug->engine.master_gain_db = fader_to_db(v);
            engine_update_master(&gui->plug->engine);
        }
        break;

    case HIT_MUTE:
        if (hit.strip_index < strip_count)
            toggle_param(gui->plug, param_channel_id(strips[hit.strip_index].channel, PARAM_CH_MUTE));
        break;
    case HIT_SOLO:
        if (hit.strip_index < strip_count)
            toggle_param(gui->plug, param_channel_id(strips[hit.strip_index].channel, PARAM_CH_SOLO));
        break;
    case HIT_PHASE:
        if (hit.strip_index < strip_count)
            toggle_param(gui->plug, param_channel_id(strips[hit.strip_index].channel, PARAM_CH_PHASE));
        break;
    case HIT_STEREO:
        if (hit.strip_index < strip_count)
            toggle_param(gui->plug, param_channel_id(strips[hit.strip_index].channel, PARAM_CH_STEREO));
        break;

    case HIT_PAN_KNOB:
        if (hit.strip_index < strip_count) {
            int pan_pid = (int)param_channel_id(strips[hit.strip_index].channel, PARAM_CH_PAN);
            if (gui->last_click_hit == pan_pid && (now - gui->last_click_time_ms) < DOUBLE_CLICK_MS) {
                gui->plug->engine.channels[strips[hit.strip_index].channel].pan = 0.0f;
                engine_update_channel(&gui->plug->engine, strips[hit.strip_index].channel);
                gui->last_click_hit = -1;
                break;
            }
            gui->last_click_hit = pan_pid;
            gui->last_click_time_ms = now;

            gui->dragging = true;
            gui->drag_param_id = pan_pid;
            gui->drag_start_y = my;
            gui->drag_start_value = gui->plug->engine.channels[strips[hit.strip_index].channel].pan;
        }
        break;

    case HIT_SELECTOR:
        cycle_selector(gui->plug, gui->active_tab, hit.selector_idx);
        break;

    case HIT_PREVIEW:
        play_preview(gui->plug, gui->active_tab, hit.selector_idx);
        break;

    case HIT_MIDI_MAP:
        gui->plug->midi_map_mode = (gui->plug->midi_map_mode + 1) % 2;
        fprintf(stderr, "ggd-libre: MIDI map mode changed to %s\n",
                gui->plug->midi_map_mode == 0 ? "GGD" : "GM");
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
    if (!gui->dragging) return;

    MixerChannel ch;
    int offset;

    if (gui->drag_param_id == PARAM_MASTER_GAIN) {
        float v = fader_y_to_value(my, STRIP_TOP + 20, FADER_H - 50);
        gui->plug->engine.master_gain_db = fader_to_db(v);
        engine_update_master(&gui->plug->engine);
    } else if (gui->drag_param_id >= PARAM_TAB_MASTER_BASE &&
               gui->drag_param_id < PARAM_TAB_MASTER_BASE + TAB_COUNT) {
        int t = gui->drag_param_id - PARAM_TAB_MASTER_BASE;
        float v = fader_y_to_value(my, STRIP_TOP + 20, FADER_H - 50);
        gui->plug->engine.tab_master_db[t] = fader_to_db(v);
        engine_update_tab_master(&gui->plug->engine, (GuiTab)t);
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
