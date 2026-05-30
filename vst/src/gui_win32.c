#ifdef _WIN32

#include "gui.h"
#include "gui_platform.h"
#include "plugin.h"
#include <windowsx.h>

static const char *GGD_WND_CLASS = "GGDLibrePluginWindow";
static bool s_class_registered = false;

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PluginGui *gui = (PluginGui *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_PAINT: {
        // Validate the dirty region without using the paint DC.
        // Cairo draws directly to the window DC obtained via GetDC(),
        // so we just need to tell Windows the region is clean.
        ValidateRect(hwnd, NULL);
        if (gui && gui->cr)
            gui_draw(gui);
        return 0;
    }
    case WM_LBUTTONDOWN:
        if (gui) {
            SetCapture(hwnd);
            gui_handle_mouse_down(gui, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        return 0;
    case WM_LBUTTONUP:
        if (gui) {
            gui_handle_mouse_up(gui);
            ReleaseCapture();
        }
        return 0;
    case WM_MOUSEMOVE:
        if (gui)
            gui_handle_mouse_move(gui, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_ERASEBKGND:
        // Prevent flicker — we paint the entire surface ourselves
        return 1;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void register_window_class(void) {
    if (s_class_registered) return;

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;  // no background brush — we draw everything
    wc.lpszClassName = GGD_WND_CLASS;

    RegisterClassExA(&wc);
    s_class_registered = true;
}

bool gui_platform_create(PluginGui *gui) {
    register_window_class();
    return true;
}

void gui_platform_destroy(PluginGui *gui) {
    if (gui->hdc && gui->hwnd) {
        ReleaseDC(gui->hwnd, gui->hdc);
        gui->hdc = NULL;
    }
    if (gui->hwnd) {
        DestroyWindow(gui->hwnd);
        gui->hwnd = NULL;
    }
}

bool gui_platform_set_parent(PluginGui *gui, const clap_window_t *window) {
    gui->parent_hwnd = (HWND)window->win32;

    gui->hwnd = CreateWindowExA(
        0, GGD_WND_CLASS, "GGD Libre",
        WS_CHILD,
        0, 0, GUI_WIDTH, GUI_HEIGHT,
        gui->parent_hwnd,
        NULL, GetModuleHandle(NULL), NULL
    );
    if (!gui->hwnd) return false;

    // Store gui pointer for WndProc BEFORE any messages can be dispatched
    SetWindowLongPtr(gui->hwnd, GWLP_USERDATA, (LONG_PTR)gui);

    // CS_OWNDC means GetDC returns the same DC every time — safe to hold
    gui->hdc = GetDC(gui->hwnd);
    gui->surface = cairo_win32_surface_create(gui->hdc);
    gui->cr = cairo_create(gui->surface);

    return true;
}

bool gui_platform_show(PluginGui *gui) {
    if (!gui->hwnd) return false;
    ShowWindow(gui->hwnd, SW_SHOW);
    return true;
}

bool gui_platform_hide(PluginGui *gui) {
    if (!gui->hwnd) return false;
    ShowWindow(gui->hwnd, SW_HIDE);
    return true;
}

void gui_platform_flush(PluginGui *gui) {
    // Cairo with CS_OWNDC draws directly to the window — just flush GDI
    if (gui->hwnd) GdiFlush();
}

void gui_platform_process_events(PluginGui *gui) {
    // On Windows, the host's message loop dispatches WM_* to our WndProc.
    // We must NOT pump messages ourselves — that causes reentrancy.
    // This function is intentionally empty; the timer just triggers a redraw.
    (void)gui;
}

void gui_platform_register_fd(PluginGui *gui) { (void)gui; }
void gui_platform_unregister_fd(PluginGui *gui) { (void)gui; }

#endif // _WIN32
