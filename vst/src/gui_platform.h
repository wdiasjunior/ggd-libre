#ifndef GGD_GUI_PLATFORM_H
#define GGD_GUI_PLATFORM_H

#include "gui.h"

// Platform-specific functions implemented in gui_x11.c or gui_win32.c
bool gui_platform_create(PluginGui *gui);
void gui_platform_destroy(PluginGui *gui);
bool gui_platform_set_parent(PluginGui *gui, const clap_window_t *window);
bool gui_platform_show(PluginGui *gui);
bool gui_platform_hide(PluginGui *gui);
void gui_platform_process_events(PluginGui *gui);
void gui_platform_flush(PluginGui *gui);
void gui_platform_register_fd(PluginGui *gui);
void gui_platform_unregister_fd(PluginGui *gui);

#endif
