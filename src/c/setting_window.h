/*
 * setting_window.h - Setting screen to select a time duration
 *
 * AUTHOR :    Eric Phillips        START DATE :    07/12/15
 */

#pragma once

#include <pebble.h>
#include "countdown_timer.h"

// called when the user is done setting the time
typedef void (*SettingWindowComplete)(int64_t time_duration, void *context);

typedef struct SettingWindowCallbacks {
  SettingWindowComplete setting_complete;
} SettingWindowCallbacks;

typedef struct SettingWindow SettingWindow;

// creates a new SettingWindow in memory but does not push it into view
SettingWindow *setting_window_create(SettingWindowCallbacks setting_window_callbacks);

// destroys an existing SettingWindow
void setting_window_destroy(SettingWindow *setting_window);

// push the window onto the stack
void setting_window_push(SettingWindow *setting_window, bool animated);

// pop the window off the stack
void setting_window_pop(SettingWindow *setting_window, bool animated);

// sets the CountdownTimer being edited (NULL for a new timer) and presets the fields from it
void setting_window_set_timer(SettingWindow *setting_window, CountdownTimer *countdown_timer);

// gets the CountdownTimer associated with this SettingWindow (NULL for a new timer)
CountdownTimer *setting_window_get_timer(SettingWindow *setting_window);

// sets the over-all color scheme of the window
void setting_window_set_highlight_color(SettingWindow *setting_window, GColor color);
