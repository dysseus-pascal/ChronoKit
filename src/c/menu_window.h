/*******************************************************************************
 * FILENAME :        menu_window.h
 *
 * DESCRIPTION :
 *      Create, destroy, and manage a MenuWindow to display
 *      a list of CountdownTimers
 *
 * AUTHOR :     Eric Phillips        START DATE :    07/10/15
 *
 */

#pragma once

#include <pebble.h>
#include "countdown_timer.h"


/*******************************************************************************
 * CALLBACK DECLARATIONS
 */

// gets a pointer to a specific CountdownTimer
typedef CountdownTimer* (*MenuWindowGetTimer)(uint8_t index, void *context);

// gets the number of CountdownTimers to be displayed in the menu
typedef uint8_t (*MenuWindowGetTimerCount)(void *context);

// called when a timer is clicked on in the menu layer
typedef void (*MenuWindowClickCallback)(uint8_t index, void *context);

// structure containing all MenuWindow callbacks
typedef struct MenuWindowCallbacks {
  MenuWindowGetTimer get_timer;
  MenuWindowGetTimerCount get_timer_count;
  MenuWindowClickCallback clicked;
} MenuWindowCallbacks;

// main structure containing all data for a MenuWindow
typedef struct MenuWindow MenuWindow;


/*******************************************************************************
 * API FUNCTIONS
 */

// creates a new MenuWindow and pushes it onto the stack (with or without animation)
MenuWindow *menu_window_create(MenuWindowCallbacks menu_window_callbacks, bool animated);

// push an existing MenuWindow back onto the stack (no-op if already there)
void menu_window_push(MenuWindow *menu_window, bool animated);

// destroys an existing MenuWindow
void menu_window_destroy(MenuWindow *menu_window);

// gets whether it is the topmost window or not
bool menu_window_get_topmost_window(MenuWindow *menu_window);

// redraws the menu window
void menu_window_refresh(MenuWindow *menu_window);

// reload the menu layer's data
void menu_window_reload_data(MenuWindow *menu_window);

// sets the over-all color scheme (highlight color) of the window
void menu_window_set_highlight_color(MenuWindow *menu_window, GColor color);
