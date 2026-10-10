#ifndef GGD_GUI_H
#define GGD_GUI_H

#include <clap/clap.h>
#include "types.h"
#include <stdbool.h>
#include <stdint.h>
#include <cairo/cairo.h>

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <cairo/cairo-win32.h>
#endif

typedef struct ggd_plugin ggd_plugin_t;

typedef struct {
#ifdef __linux__
    Display          *display;
    Window            window;
    Window            parent_window;
    Visual           *visual;
    int               screen;
    GC                gc;
    XImage           *ximage;
    Atom              wm_delete;
    bool              owns_display;
#endif
#ifdef _WIN32
    HWND              hwnd;
    HWND              parent_hwnd;
    HDC               hdc;
#endif

    // Shared (cross-platform)
    cairo_surface_t  *surface;
    cairo_t          *cr;

    GuiTab active_tab;
    int    loading_lib;   // library being loaded right now, or -1
    int    width;
    int    height;

    // Interaction
    int      drag_param_id;
    float    drag_start_value;
    int      drag_start_y;
    bool     dragging;
    uint32_t last_click_time_ms;
    int      last_click_hit;   // param id of last clicked fader

    // Back-reference
    ggd_plugin_t *plug;

    // Host timer/fd tracking
    clap_id timer_id;
    bool    timer_registered;
    bool    fd_registered;

    bool   visible;
    bool   created;
} PluginGui;

// CLAP extensions (defined in gui_common.c)
extern const clap_plugin_gui_t           ggd_gui_ext;
extern const clap_plugin_timer_support_t ggd_timer_ext;
#ifdef __linux__
extern const clap_plugin_posix_fd_support_t ggd_posix_fd_ext;
#endif

#define GUI_WIDTH  1010
#define GUI_HEIGHT 520

// Shared drawing/logic (defined in gui_common.c, called by platform backends)
void gui_draw(PluginGui *gui);
void gui_handle_mouse_down(PluginGui *gui, int mx, int my);
void gui_handle_mouse_up(PluginGui *gui);
void gui_handle_mouse_move(PluginGui *gui, int mx, int my);

#endif
