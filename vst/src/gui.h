#ifndef GGD_GUI_H
#define GGD_GUI_H

#include <clap/clap.h>
#include "types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __linux__
#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#endif

typedef struct ggd_plugin ggd_plugin_t;

typedef struct {
#ifdef __linux__
    Display          *display;
    Window            window;
    Window            parent_window;
    Visual           *visual;
    int               screen;
    cairo_surface_t  *surface;
    cairo_t          *cr;
    Atom              wm_delete;
    bool              owns_display;
#endif

    GuiTab active_tab;
    int    width;
    int    height;

    // Interaction
    int    drag_param_id;
    float  drag_start_value;
    int    drag_start_y;
    bool   dragging;

    // Back-reference
    ggd_plugin_t *plug;

    bool   visible;
    bool   created;
} PluginGui;

// CLAP GUI extension
extern const clap_plugin_gui_t       ggd_gui_ext;
extern const clap_plugin_timer_support_t ggd_timer_ext;
extern const clap_plugin_posix_fd_support_t ggd_posix_fd_ext;

#define GUI_WIDTH  800
#define GUI_HEIGHT 500

#endif
