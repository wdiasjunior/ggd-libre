#ifdef __linux__

#include "gui.h"
#include "gui_platform.h"
#include "plugin.h"
#include <stdlib.h>
#include <string.h>

// We use a Cairo image surface and blit to X11 via XPutImage.
// This avoids depending on the cairo-xlib backend, keeping Cairo statically linked.

bool gui_platform_create(PluginGui *gui) {
    gui->display = XOpenDisplay(NULL);
    if (!gui->display) return false;
    gui->owns_display = true;
    gui->screen = DefaultScreen(gui->display);
    gui->visual = DefaultVisual(gui->display, gui->screen);
    return true;
}

void gui_platform_destroy(PluginGui *gui) {
    if (gui->ximage) {
        gui->ximage->data = NULL; // owned by cairo surface, don't let XDestroyImage free it
        XDestroyImage(gui->ximage);
        gui->ximage = NULL;
    }
    if (gui->gc) {
        XFreeGC(gui->display, gui->gc);
        gui->gc = NULL;
    }
    if (gui->window) XDestroyWindow(gui->display, gui->window);
    if (gui->owns_display && gui->display) XCloseDisplay(gui->display);
}

bool gui_platform_set_parent(PluginGui *gui, const clap_window_t *window) {
    if (!gui->display) return false;

    gui->parent_window = (Window)window->x11;
    gui->window = XCreateSimpleWindow(gui->display, gui->parent_window,
                                       0, 0, GUI_WIDTH, GUI_HEIGHT, 0,
                                       BlackPixel(gui->display, gui->screen),
                                       BlackPixel(gui->display, gui->screen));

    XSelectInput(gui->display, gui->window,
                 ExposureMask | ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask | StructureNotifyMask);

    // Create Cairo image surface (ARGB32)
    gui->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, GUI_WIDTH, GUI_HEIGHT);
    gui->cr = cairo_create(gui->surface);

    // Create GC and XImage for blitting
    gui->gc = XCreateGC(gui->display, gui->window, 0, NULL);
    int depth = DefaultDepth(gui->display, gui->screen);
    gui->ximage = XCreateImage(gui->display, gui->visual, depth, ZPixmap, 0,
                                NULL, GUI_WIDTH, GUI_HEIGHT, 32, 0);
    // XImage data will be pointed at the cairo surface buffer each frame

    XMapWindow(gui->display, gui->window);
    XFlush(gui->display);
    return true;
}

bool gui_platform_show(PluginGui *gui) {
    if (!gui->window) return false;
    XMapWindow(gui->display, gui->window);
    XFlush(gui->display);
    return true;
}

bool gui_platform_hide(PluginGui *gui) {
    if (!gui->window) return false;
    XUnmapWindow(gui->display, gui->window);
    return true;
}

void gui_platform_flush(PluginGui *gui) {
    if (!gui->display || !gui->window || !gui->surface || !gui->ximage) return;

    // Point XImage data at the Cairo image surface buffer and blit
    cairo_surface_flush(gui->surface);
    gui->ximage->data = (char *)cairo_image_surface_get_data(gui->surface);
    XPutImage(gui->display, gui->window, gui->gc, gui->ximage,
              0, 0, 0, 0, GUI_WIDTH, GUI_HEIGHT);
    XFlush(gui->display);
}

void gui_platform_process_events(PluginGui *gui) {
    if (!gui->display) return;

    while (XPending(gui->display)) {
        XEvent ev;
        XNextEvent(gui->display, &ev);

        switch (ev.type) {
        case Expose:
            if (ev.xexpose.count == 0)
                gui_draw(gui);
            break;
        case ButtonPress:
            gui_handle_mouse_down(gui, ev.xbutton.x, ev.xbutton.y);
            break;
        case ButtonRelease:
            gui_handle_mouse_up(gui);
            break;
        case MotionNotify:
            gui_handle_mouse_move(gui, ev.xmotion.x, ev.xmotion.y);
            break;
        default:
            break;
        }
    }
}

void gui_platform_register_fd(PluginGui *gui) {
    if (!gui->fd_registered && gui->plug->host_posix_fd && gui->display) {
        int fd = ConnectionNumber(gui->display);
        if (gui->plug->host_posix_fd->register_fd(gui->plug->host, fd, CLAP_POSIX_FD_READ))
            gui->fd_registered = true;
    }
}

void gui_platform_unregister_fd(PluginGui *gui) {
    if (gui->fd_registered && gui->plug->host_posix_fd && gui->display) {
        int fd = ConnectionNumber(gui->display);
        gui->plug->host_posix_fd->unregister_fd(gui->plug->host, fd);
        gui->fd_registered = false;
    }
}

// POSIX FD callback (Linux only)
static void posix_fd_on_fd(const clap_plugin_t *plugin, int fd, clap_posix_fd_flags_t flags) {
    (void)fd; (void)flags;
    ggd_plugin_t *plug = plugin->plugin_data;
    PluginGui *gui = plug->gui;
    if (!gui) return;
    gui_platform_process_events(gui);
}

const clap_plugin_posix_fd_support_t ggd_posix_fd_ext = {
    .on_fd = posix_fd_on_fd,
};

#endif // __linux__
