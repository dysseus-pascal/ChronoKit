/*******************************************************************************
 * FILENAME :        detail_window.h
 *
 * DESCRIPTION :
 *      Display a timer with controls to modify it
 *
 * AUTHOR :     Eric Phillips        START DATE :    07/11/15
 */

#pragma once

#include <pebble.h>
#include "countdown_timer.h"

// button callback: called with the timer shown and the DetailWindow as context
typedef void (*DetailWindowCallback)(CountdownTimer *countdown_timer, void *context);

// all DetailWindow callbacks (edit / play-pause / delete button)
typedef struct DetailWindowCallbacks {
  DetailWindowCallback edit_timer;
  DetailWindowCallback playpause_timer;
  DetailWindowCallback delete_timer;
} DetailWindowCallbacks;

typedef struct DetailWindow DetailWindow;

/*******************************************************************************
 * API FUNCTIONS
 */

// creates a new DetailWindow in memory but does not push it into view
DetailWindow *detail_window_create(DetailWindowCallbacks detail_window_callbacks);

// destroys an existing DetailWindow
void detail_window_destroy(DetailWindow *detail_window);

// push the window onto the stack / pop it off
void detail_window_push(DetailWindow *detail_window, bool animated);
void detail_window_pop(DetailWindow *detail_window, bool animated);

// gets whether it is the topmost window or not
bool detail_window_get_topmost_window(DetailWindow *detail_window);

// sets the CountdownTimer shown by the detail window
void detail_window_set_countdown_timer(DetailWindow *detail_window,
                                       CountdownTimer *countdown_timer);

// redraws the detail window (texts and progress fill)
void detail_window_refresh(DetailWindow *detail_window);

// refreshes everything that may change, including the play/pause icon
void detail_window_deep_refresh(DetailWindow *detail_window);

// sets the over-all color scheme of the window
void detail_window_set_highlight_color(DetailWindow *detail_window, GColor color);
