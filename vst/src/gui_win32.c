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
        if (gui && gui->cr) {
            gui_draw(gui);  // draws to image surface, then blits in gui_platform_flush
        }
        // Validate so Windows stops sending WM_PAINT
        ValidateRect(hwnd, NULL);
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
        return 1;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void register_window_class(void) {
    if (s_class_registered) return;

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = 0;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = GGD_WND_CLASS;

    RegisterClassExA(&wc);
    s_class_registered = true;
}

bool gui_platform_create(PluginGui *gui) {
    register_window_class();
    return true;
}

void gui_platform_destroy(PluginGui *gui) {
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

    SetWindowLongPtr(gui->hwnd, GWLP_USERDATA, (LONG_PTR)gui);

    // Use a Cairo image surface — we blit to screen in gui_platform_flush
    gui->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, GUI_WIDTH, GUI_HEIGHT);
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
    if (!gui->hwnd || !gui->surface) return;

    cairo_surface_flush(gui->surface);
    unsigned char *data = cairo_image_surface_get_data(gui->surface);
    int stride = cairo_image_surface_get_stride(gui->surface);

    // Create a DIB from the Cairo ARGB32 image and blit to the window
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = GUI_WIDTH;
    bmi.bmiHeader.biHeight = -GUI_HEIGHT;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(gui->hwnd);
    SetDIBitsToDevice(hdc, 0, 0, GUI_WIDTH, GUI_HEIGHT,
                      0, 0, 0, GUI_HEIGHT,
                      data, &bmi, DIB_RGB_COLORS);
    ReleaseDC(gui->hwnd, hdc);
}

void gui_platform_process_events(PluginGui *gui) {
    (void)gui;
}

void gui_platform_register_fd(PluginGui *gui) { (void)gui; }
void gui_platform_unregister_fd(PluginGui *gui) { (void)gui; }

#endif // _WIN32
