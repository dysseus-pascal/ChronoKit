/*
 * setting_window.c - Setting screen to select a time duration
 *
 * AUTHOR :         Eric Phillips        START DATE :    07/12/15
 */

#include <pebble.h>
#include "setting_window.h"
#include "selection_layer.h"
#include "theme.h"
#include "common.h"

#define REPEATING_CLICK_THRESHOLD 10


#define NUM_CELLS 3 // hours, minutes, seconds
#define CELL_PADDING 4

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
#define MAIN_FONT FONT_KEY_GOTHIC_28_BOLD
#define SUB_FONT FONT_KEY_GOTHIC_24_BOLD
#else
#define MAIN_FONT FONT_KEY_GOTHIC_18_BOLD
#define SUB_FONT FONT_KEY_GOTHIC_18_BOLD
#endif


struct SettingWindow {
  Window          *window;            //< main window
  TextLayer       *main_text;         //< title text at top of screen
  TextLayer       *sub_text;          //< sub text at bottom for messages
  Layer           *selection;         //< SelectionLayer for input
  GColor          highlight_color;    //< color for selection highlights
  StatusBarLayer  *status;            //< status bar
  SettingWindowCallbacks callbacks;   //< callbacks

  CountdownTimer  *countdown_timer;   //< timer being set
  int32_t         field_values[NUM_CELLS];    //< values of selection fields
  char            field_buffs[NUM_CELLS][3];  //< buffers to draw field contents
};


/*******************************************************************************
 * PRIVATE FUNCTIONS
 */

/*
 * duration in milliseconds currently set in the selection fields
 */
static int64_t prv_get_duration(SettingWindow *setting_window) {
  return (int64_t)setting_window->field_values[0] * MSEC_IN_HR +
    (int64_t)setting_window->field_values[1] * MSEC_IN_MIN +
    (int64_t)setting_window->field_values[2] * MSEC_IN_SEC;
}

/*
 * update the sub text: shows the end time once the duration is long enough
 */
static void update_sub_text(SettingWindow *setting_window) {
  int64_t duration = prv_get_duration(setting_window);
  Layer *sub_layer = text_layer_get_layer(setting_window->sub_text);
  if (duration < TIMER_MIN_LENGTH) {
    text_layer_set_text(setting_window->sub_text, "");
    layer_set_hidden(sub_layer, false);
    return;
  }
  layer_set_hidden(sub_layer, duration < TIMELINE_MIN_LENGTH);
  if (duration < TIMELINE_MIN_LENGTH) {
    return;
  }

  // format end time
  time_t end = ((int64_t)time(NULL) * MSEC_IN_SEC + (int64_t)time_ms(NULL, NULL) + duration) / MSEC_IN_SEC;
  static char buff[] = "End: 00:00 AM";
  struct tm *tick_time = localtime(&end);
  if (clock_is_24h_style()) {
    strftime(buff, sizeof(buff), "End: %k:%M", tick_time);
  } else {
    strftime(buff, sizeof(buff), "End: %l:%M %p", tick_time);
  }
  text_layer_set_text(setting_window->sub_text, buff);
}


/*******************************************************************************
 * SELECTION LAYER CALLBACKS
 */

static char* selection_handle_get_text(unsigned index, void *context) {
  SettingWindow *setting_window = context;
  snprintf(setting_window->field_buffs[index], sizeof(setting_window->field_buffs[0]), "%02d",
    (int)setting_window->field_values[index]);
  return setting_window->field_buffs[index];
}

static void selection_handle_complete(void *context) {
  SettingWindow *setting_window = context;
  setting_window->callbacks.setting_complete(prv_get_duration(setting_window), setting_window);
}

static void selection_handle_inc(unsigned index, uint8_t clicks, void *context) {
  SettingWindow *setting_window = context;
  setting_window->field_values[index] += (clicks > REPEATING_CLICK_THRESHOLD) ? 2 : 1;
  int8_t max_value = (index == 0) ? HR_IN_DAY : MIN_IN_HR;
  if (setting_window->field_values[index] >= max_value) {
    setting_window->field_values[index] -= max_value;
  }
  update_sub_text(setting_window);
}

static void selection_handle_dec(unsigned index, uint8_t clicks, void *context) {
  SettingWindow *setting_window = context;
  setting_window->field_values[index] -= (clicks > REPEATING_CLICK_THRESHOLD) ? 2 : 1;
  int8_t max_value = (index == 0) ? HR_IN_DAY : MIN_IN_HR;
  if (setting_window->field_values[index] < 0) {
    setting_window->field_values[index] += max_value;
  }
  update_sub_text(setting_window);
}


/*******************************************************************************
 * WINDOW HANDLERS
 */

static void prv_window_load(Window *window) {
  SettingWindow *setting_window = window_get_user_data(window);
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_frame(root);
  // main text
  setting_window->main_text = text_layer_create(GRect(0, bounds.size.h/7, bounds.size.w, 40));
  text_layer_set_text(setting_window->main_text, "Timer stellen");
  text_layer_set_font(setting_window->main_text, fonts_get_system_font(MAIN_FONT));
  text_layer_set_text_alignment(setting_window->main_text, GTextAlignmentCenter);
  text_layer_set_background_color(setting_window->main_text, GColorClear);
  layer_add_child(root, text_layer_get_layer(setting_window->main_text));
  // sub text
  setting_window->sub_text = text_layer_create(GRect(1, bounds.size.h-43*bounds.size.h/168, bounds.size.w, 40));
  text_layer_set_text_alignment(setting_window->sub_text, GTextAlignmentCenter);
  text_layer_set_background_color(setting_window->sub_text, GColorClear);
  text_layer_set_font(setting_window->sub_text, fonts_get_system_font(SUB_FONT));
  layer_add_child(root, text_layer_get_layer(setting_window->sub_text));
  // selection layer
  int16_t margin = PBL_IF_ROUND_ELSE(26, 8);
  uint16_t cell_height = (bounds.size.h + 4) / 5;
  uint16_t cell_y_start = bounds.size.h/2 - cell_height/2;
  uint8_t cell_width = (bounds.size.w - 2*margin - (NUM_CELLS - 1) * CELL_PADDING) / NUM_CELLS;
  setting_window->selection = selection_layer_create(
    GRect(margin, cell_y_start, bounds.size.w - 2*margin, cell_height), NUM_CELLS);
  for (int i = 0; i < NUM_CELLS; i++) {
    selection_layer_set_cell_width(setting_window->selection, i, cell_width);
  }
  selection_layer_set_cell_padding(setting_window->selection, CELL_PADDING);
  selection_layer_set_active_bg_color(setting_window->selection, setting_window->highlight_color);
  selection_layer_set_inactive_bg_color(setting_window->selection, ZM_COLOR_FIELD_INACTIVE);
  selection_layer_set_click_config_onto_window(setting_window->selection, window);
  selection_layer_set_callbacks(setting_window->selection, setting_window,
    (SelectionLayerCallbacks) {
      .get_cell_text = selection_handle_get_text,
      .complete = selection_handle_complete,
      .increment = selection_handle_inc,
      .decrement = selection_handle_dec,
    });
  layer_add_child(root, setting_window->selection);
  // status bar
  setting_window->status = status_bar_layer_create();
  status_bar_layer_set_colors(setting_window->status, GColorClear, GColorBlack);
  layer_add_child(root, status_bar_layer_get_layer(setting_window->status));

  update_sub_text(setting_window);
}

static void prv_window_unload(Window *window) {
  SettingWindow *setting_window = window_get_user_data(window);
  status_bar_layer_destroy(setting_window->status);
  selection_layer_destroy(setting_window->selection);
  text_layer_destroy(setting_window->sub_text);
  text_layer_destroy(setting_window->main_text);
  window_destroy(setting_window->window);
  setting_window->window = NULL;
}


/*******************************************************************************
 * API FUNCTIONS
 */

/*
 * create a new SettingWindow (the window itself is created on push)
 */
SettingWindow *setting_window_create(SettingWindowCallbacks setting_window_callbacks) {
  SettingWindow *setting_window = malloc(sizeof(SettingWindow));
  if (setting_window) {
    *setting_window = (SettingWindow) { .callbacks = setting_window_callbacks };
  }
  return setting_window;
}

/*
 * destroy a previously created SettingWindow
 */
void setting_window_destroy(SettingWindow *setting_window) {
  free(setting_window);
}

/*
 * push the window onto the stack, creating it if needed
 */
void setting_window_push(SettingWindow *setting_window, bool animated) {
  if (setting_window->window == NULL) {
    setting_window->window = window_create();
    window_set_user_data(setting_window->window, setting_window);
    window_set_window_handlers(setting_window->window,
      (WindowHandlers){
        .load = prv_window_load,
        .unload = prv_window_unload
      });
  }
  if (setting_window->window) {
    window_stack_push(setting_window->window, animated);
  }
}

/*
 * pop the window off the stack
 */
void setting_window_pop(SettingWindow *setting_window, bool animated) {
  if (setting_window->window) {
    window_stack_remove(setting_window->window, animated);
  }
}

/*
 * sets the CountdownTimer associated with the SettingWindow (NULL for a new timer)
 * and presets the selection fields from its duration
 */
void setting_window_set_timer(SettingWindow *setting_window, CountdownTimer *countdown_timer) {
  setting_window->countdown_timer = countdown_timer;
  int64_t duration = countdown_timer ? countdown_timer_get_duration(countdown_timer) : 0;
  setting_window->field_values[0] = duration / MSEC_IN_HR;
  setting_window->field_values[1] = duration % MSEC_IN_HR / MSEC_IN_MIN;
  setting_window->field_values[2] = duration % MSEC_IN_MIN / MSEC_IN_SEC;
  if (setting_window->window) {
    update_sub_text(setting_window);
  }
}

/*
 * gets the CountdownTimer associated with this SettingWindow
 */
CountdownTimer *setting_window_get_timer(SettingWindow *setting_window) {
  return setting_window->countdown_timer;
}

/*
 * set highlight color of this window (the overall color scheme)
 */
void setting_window_set_highlight_color(SettingWindow *setting_window, GColor color) {
  setting_window->highlight_color = color;
}
