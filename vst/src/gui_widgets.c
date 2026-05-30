#include "gui_widgets.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

void widget_draw_text(cairo_t *cr, int x, int y, const char *text, double size) {
    cairo_set_font_size(cr, size);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
}

void widget_draw_text_centered(cairo_t *cr, int x, int y, int w,
                               const char *text, double size) {
    cairo_set_font_size(cr, size);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, text, &ext);
    double tx = x + (w - ext.width) / 2.0 - ext.x_bearing;
    cairo_move_to(cr, tx, y);
    cairo_show_text(cr, text);
}

bool rect_contains(Rect r, int mx, int my) {
    return mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h;
}

float fader_y_to_value(int y, int fader_y, int fader_h) {
    float v = 1.0f - (float)(y - fader_y) / (float)fader_h;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return v;
}

static void draw_fader_impl(cairo_t *cr, int x, int y, int w, int h,
                            float value, const char *label, const char *value_text,
                            double fr, double fg, double fb) {
    int track_w = 6;
    int track_x = x + (w - track_w) / 2;
    int track_y = y + 20;
    int track_h = h - 50;

    cairo_set_source_rgb(cr, COL_FADER_BG_R, COL_FADER_BG_G, COL_FADER_BG_B);
    cairo_rectangle(cr, track_x, track_y, track_w, track_h);
    cairo_fill(cr);

    int fill_h = (int)(value * track_h);
    cairo_set_source_rgb(cr, fr, fg, fb);
    cairo_rectangle(cr, track_x, track_y + track_h - fill_h, track_w, fill_h);
    cairo_fill(cr);

    // Handle
    int handle_y = track_y + track_h - fill_h - 4;
    int handle_h = 8;
    int handle_w = w - 8;
    int handle_x = x + 4;
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    cairo_rectangle(cr, handle_x, handle_y, handle_w, handle_h);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);
    widget_draw_text_centered(cr, x, y + h - 5, w, label, 10);

    if (value_text) {
        cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
        widget_draw_text_centered(cr, x, y + 14, w, value_text, 9);
    }
}

void widget_draw_fader(cairo_t *cr, int x, int y, int w, int h,
                       float value, const char *label, const char *value_text) {
    draw_fader_impl(cr, x, y, w, h, value, label, value_text,
                    COL_FADER_FG_R, COL_FADER_FG_G, COL_FADER_FG_B);
}

void widget_draw_fader_colored(cairo_t *cr, int x, int y, int w, int h,
                               float value, const char *label, const char *value_text,
                               double fr, double fg, double fb) {
    draw_fader_impl(cr, x, y, w, h, value, label, value_text, fr, fg, fb);
}

void widget_draw_pan_knob(cairo_t *cr, int cx, int cy, int radius,
                          float value) {
    // Background circle
    cairo_set_source_rgb(cr, COL_PANEL_R, COL_PANEL_G, COL_PANEL_B);
    cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
    cairo_fill(cr);

    // Border
    cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
    cairo_set_line_width(cr, 1.5);
    cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
    cairo_stroke(cr);

    // Indicator line
    // value: -1 (left, 210 deg) to +1 (right, 330 deg)
    double angle = (M_PI * 0.75) + ((value + 1.0) * 0.5) * (M_PI * 1.5);
    double lx = cx + cos(angle) * (radius - 3);
    double ly = cy + sin(angle) * (radius - 3);
    cairo_set_source_rgb(cr, COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
    cairo_set_line_width(cr, 2);
    cairo_move_to(cr, cx, cy);
    cairo_line_to(cr, lx, ly);
    cairo_stroke(cr);
}

void widget_draw_toggle(cairo_t *cr, int x, int y, int w, int h,
                        bool active, const char *label,
                        double r, double g, double b) {
    if (active) {
        cairo_set_source_rgb(cr, r, g, b);
    } else {
        cairo_set_source_rgb(cr, COL_PANEL_R, COL_PANEL_G, COL_PANEL_B);
    }
    // Rounded rect
    double rad = 3;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - rad, y + rad, rad, -M_PI/2, 0);
    cairo_arc(cr, x + w - rad, y + h - rad, rad, 0, M_PI/2);
    cairo_arc(cr, x + rad, y + h - rad, rad, M_PI/2, M_PI);
    cairo_arc(cr, x + rad, y + rad, rad, M_PI, 3*M_PI/2);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Border
    cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
    cairo_set_line_width(cr, 1);
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - rad, y + rad, rad, -M_PI/2, 0);
    cairo_arc(cr, x + w - rad, y + h - rad, rad, 0, M_PI/2);
    cairo_arc(cr, x + rad, y + h - rad, rad, M_PI/2, M_PI);
    cairo_arc(cr, x + rad, y + rad, rad, M_PI, 3*M_PI/2);
    cairo_close_path(cr);
    cairo_stroke(cr);

    // Label
    cairo_set_source_rgb(cr, COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);
    widget_draw_text_centered(cr, x, y + h/2 + 4, w, label, 10);
}

void widget_draw_tab(cairo_t *cr, int x, int y, int w, int h,
                     bool active, const char *label) {
    if (active) {
        cairo_set_source_rgb(cr, COL_ACCENT_R, COL_ACCENT_G, COL_ACCENT_B);
    } else {
        cairo_set_source_rgb(cr, COL_PANEL_R, COL_PANEL_G, COL_PANEL_B);
    }
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);
    widget_draw_text_centered(cr, x, y + h/2 + 5, w, label, 12);
}

void widget_draw_selector(cairo_t *cr, int x, int y, int w, int h,
                          const char *label, const char *value) {
    // Background
    cairo_set_source_rgb(cr, COL_PANEL_R, COL_PANEL_G, COL_PANEL_B);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);

    // Border
    cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, x, y, w, h);
    cairo_stroke(cr);

    // Label
    cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
    widget_draw_text(cr, x + 5, y + 14, label, 9);

    // Value
    cairo_set_source_rgb(cr, COL_TEXT_R, COL_TEXT_G, COL_TEXT_B);
    widget_draw_text(cr, x + 5, y + h - 5, value, 11);
}
