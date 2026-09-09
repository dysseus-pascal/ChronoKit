/*******************************************************************************
 * FILENAME :        popup_window.h
 *
 * DESCRIPTION :
 *      Displays a pop-up window with a PDC, some text, and an optional
 *      ActionBar. It also can auto-close after a customizable length of time.
 *
 * AUTHOR :     Eric Phillips        START DATE :    07/13/15
 */

#pragma once

#include <pebble.h>
#include "countdown_timer.h"



/*******************************************************************************
 * CALLBACK DECLARATIONS
 */

// UP bekommt den Timer (Schlummern), DOWN nur den Kontext (Verwerfen).
// SELECT ist im Popup nicht belegt.
typedef void (*PopupWindowUpClick)(CountdownTimer *countdown_timer, void *context);
typedef void (*PopupWindowDownClick)(void *context);

typedef struct PopupWindowCallbacks {
  PopupWindowUpClick up_click;
  PopupWindowDownClick down_click;
} PopupWindowCallbacks;

typedef struct PopupWindow PopupWindow;



/*******************************************************************************
 * API FUNCTIONS
 */

// creates a new PopupWindow in memory but does not push it into view
PopupWindow *popup_window_create(void);

// destroys an existing PopupWindow
void popup_window_destroy(PopupWindow *popup_window);

// push the window onto / pop it off the window stack
void popup_window_push(PopupWindow *popup_window, bool animated);
void popup_window_pop(PopupWindow *popup_window, bool animated);

// whether the PopupWindow is the topmost window on the stack
bool popup_window_get_topmost_window(PopupWindow *popup_window);

// milliseconds after which the PopupWindow pops itself on refresh; 0 = never
void popup_window_set_auto_close_duration(PopupWindow *popup_window, int64_t duration);

// sets up an app_timer callback to run multiple instances of a vibration pattern
void popup_window_set_vibes(void);

// sets the CountdownTimer associated with the PopupWindow (passed to up_click)
void popup_window_set_countdown_timer(PopupWindow *popup_window, CountdownTimer *countdown_timer);

// redraws the PopupWindow and steps any ongoing PDC animation
void popup_window_refresh(PopupWindow *popup_window);

// sets the animating PDC displayed in the center of the screen
//  resource_id: a RESOURCE_ID_XXX of the PDC; endless: loop instead of playing once
void popup_window_set_pdc(PopupWindow *popup_window, uint32_t resource_id, bool endless);

// duration in milliseconds of the currently assigned PDC
int64_t popup_window_get_pdc_duration(PopupWindow *popup_window);

// sets the title text shown beneath the PDC
void popup_window_set_title(PopupWindow *popup_window, const char *text);

// sets the background color of the PopupWindow
void popup_window_set_highlight_color(PopupWindow *popup_window, GColor color);

// show / hide the ActionBar (applied when the window is loaded on push)
void popup_window_add_action_bar(PopupWindow *popup_window);
void popup_window_remove_action_bar(PopupWindow *popup_window);

// sets the callbacks for the ActionBar buttons
void popup_window_set_action_bar_callbacks(PopupWindow *popup_window,
                                           PopupWindowCallbacks callbacks);
