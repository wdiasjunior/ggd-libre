#ifndef GGD_GUI_WIDGETS_H
#define GGD_GUI_WIDGETS_H

#include <cairo/cairo.h>
#include <stdbool.h>

// Colors
#define COL_BG_R      0.15
#define COL_BG_G      0.15
#define COL_BG_B      0.17

#define COL_PANEL_R   0.20
#define COL_PANEL_G   0.20
#define COL_PANEL_B   0.22

#define COL_FADER_BG_R  0.12
#define COL_FADER_BG_G  0.12
#define COL_FADER_BG_B  0.14

#define COL_FADER_FG_R  0.45
#define COL_FADER_FG_G  0.65
#define COL_FADER_FG_B  0.85

#define COL_TEXT_R    0.85
#define COL_TEXT_G    0.85
#define COL_TEXT_B    0.85

#define COL_ACCENT_R  0.30
#define COL_ACCENT_G  0.60
#define COL_ACCENT_B  0.90

#define COL_MUTE_R    0.9
#define COL_MUTE_G    0.3
#define COL_MUTE_B    0.2

#define COL_SOLO_R    0.9
#define COL_SOLO_G    0.8
#define COL_SOLO_B    0.2

typedef struct {
    int x, y, w, h;
} Rect;

// Draw a vertical fader. Returns the rect of the fader track for hit testing.
// value: 0.0 (bottom) to 1.0 (top)
void widget_draw_fader(cairo_t *cr, int x, int y, int w, int h,
                       float value, const char *label, const char *value_text);

// Draw a pan knob. value: -1 (left) to +1 (right)
void widget_draw_pan_knob(cairo_t *cr, int cx, int cy, int radius,
                          float value);

// Draw a toggle button
void widget_draw_toggle(cairo_t *cr, int x, int y, int w, int h,
                        bool active, const char *label,
                        double r, double g, double b);

// Draw a tab button
void widget_draw_tab(cairo_t *cr, int x, int y, int w, int h,
                     bool active, const char *label);

// Draw a selector (dropdown-like)
void widget_draw_selector(cairo_t *cr, int x, int y, int w, int h,
                          const char *label, const char *value);

// Draw centered text
void widget_draw_text(cairo_t *cr, int x, int y, const char *text, double size);
void widget_draw_text_centered(cairo_t *cr, int x, int y, int w,
                               const char *text, double size);

// Hit test
bool rect_contains(Rect r, int mx, int my);

// Normalize fader position: y pixel position -> 0..1 value
float fader_y_to_value(int y, int fader_y, int fader_h);

#endif
