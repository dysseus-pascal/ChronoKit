/*******************************************************************************
 * FILENAME :        popup_window.c
 *
 * DESCRIPTION :
 *      Displays a pop-up window with a PDC, some text, and an optional
 *      ActionBar. It also can auto-close after a customizable length of time.
 *
 * AUTHOR :     Eric Phillips        START DATE :    07/13/15
 */

#include <pebble.h>
#include "popup_window.h"

#define NUM_VIBE_INTERVALS 6

static AppTimer *s_app_timer = NULL;

struct PopupWindow {
  Window          *window;        //< main window
  Layer           *layer;         //< drawing layer for PDC
  TextLayer       *text;          //< displays title text
  ActionBarLayer  *action;        //< optional action bar for dialogs
  PopupWindowCallbacks    callbacks;     //< callbacks for optional ActionBar
  GBitmap *snooze_icon,   *stop_icon;    //< icons for ActionBar

  GDrawCommandSequence    *draw_sequence;     //< draw command sequence
  GDrawCommandFrame       *draw_frame;        //< current draw command
  uint32_t frame_count;           //< total number of frames in draw sequence
  uint32_t frame_index;           //< index of current frame (for endless)
  bool     endless;               //< if PDC is endless, having no duration

  CountdownTimer  *countdown_timer;    //< timer associated with PopupWindow
  int64_t     set_time, close_time;    //< time opened and time to close
  bool            action_visible;      //< whether the ActionBar is visible

  GColor highlight_color;
  const char* title;
};



/*******************************************************************************
 * PRIVATE FUNCTIONS
 */

// layer update proc: draws the current PDC frame
static void layer_update_proc(Layer *layer, GContext *ctx) {
  PopupWindow *popup_window = window_get_user_data(layer_get_window(layer));
  if (popup_window->draw_sequence != NULL) {
    gdraw_command_frame_draw(ctx, popup_window->draw_sequence,
      popup_window->draw_frame, GPointZero);
  }
}

// center the PDC layer in the window (accounting for the optional ActionBar)
// and place the title directly beneath it
static void layers_center_in_window(PopupWindow *popup_window) {
  const int16_t horiz_off = PBL_IF_ROUND_ELSE(0, ACTION_BAR_WIDTH);
  const GRect window_frame = layer_get_frame(window_get_root_layer(popup_window->window));
  const int16_t content_w = window_frame.size.w - (popup_window->action_visible ? horiz_off : 0);

  GSize pdc_size = gdraw_command_sequence_get_bounds_size(popup_window->draw_sequence);
  GRect layer_frame = GRect(content_w / 2 - pdc_size.w / 2,
                            window_frame.size.h / 2 - pdc_size.h / 2,
                            pdc_size.w, pdc_size.h);
  layer_set_frame(popup_window->layer, layer_frame);

  // Titel direkt unter die Unterkante der Grafik, damit er darunter statt darauf
  // sitzt; die feste Hoehe stammte aus der 168-px-Aera und lag auf dem
  // groesseren emery-Display mitten im Bild
  Layer *text_layer = text_layer_get_layer(popup_window->text);
  GRect text_frame = layer_get_frame(text_layer);
  text_frame.size.w = content_w;
  text_frame.origin.y = layer_frame.origin.y + layer_frame.size.h + 6;
  const int16_t max_y = window_frame.size.h - text_frame.size.h;
  if (text_frame.origin.y > max_y) {
    text_frame.origin.y = max_y;
  }
  layer_set_frame(text_layer, text_frame);
  layer_set_bounds(text_layer, GRect(0, 0, text_frame.size.w, text_frame.size.h));
}



/*******************************************************************************
 * CALLBACKS
 */

// vibe callback: repeats the "timer finished" vibration pattern in intervals
static void app_timer_vibe_callback(void *data) {
  int num_vibes_left = (int)data;
  if (num_vibes_left != 0) {
    static const uint32_t vibe_seg[] = {300, 200, 300, 200, 300};
    const VibePattern pat_vibe = {
      .durations = vibe_seg,
      .num_segments = ARRAY_LENGTH(vibe_seg),
    };
    vibes_enqueue_custom_pattern(pat_vibe);
    --num_vibes_left;
    s_app_timer = app_timer_register(2300, app_timer_vibe_callback, (void*)num_vibes_left);
  }
}

// click handlers forward to the optional ActionBar callbacks
static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  PopupWindow *popup_window = context;
  if (popup_window->callbacks.up_click != NULL) {
    popup_window->callbacks.up_click(popup_window->countdown_timer, context);
  }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  PopupWindow *popup_window = context;
  if (popup_window->callbacks.down_click != NULL) {
    popup_window->callbacks.down_click(context);
  }
}

static void click_config_provider(void *context) {
  window_set_click_context(BUTTON_ID_UP, context);
  window_set_click_context(BUTTON_ID_DOWN, context);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void prv_window_load(Window *window) {
  PopupWindow *popup_window = window_get_user_data(window);
  popup_window->stop_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DISMISS);
  popup_window->snooze_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SNOOZE);

  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_frame(root);
  // PDC layer
  popup_window->layer = layer_create(bounds);
  layer_set_clips(popup_window->layer, false);
  layer_set_update_proc(popup_window->layer, layer_update_proc);
  layer_add_child(root, popup_window->layer);
  // title
  popup_window->text = text_layer_create(GRect(0, 125, bounds.size.w, 36));
  text_layer_set_font(popup_window->text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(popup_window->text, GTextAlignmentCenter);
  text_layer_set_background_color(popup_window->text, GColorClear);
  text_layer_set_text(popup_window->text, popup_window->title);
  layer_add_child(root, text_layer_get_layer(popup_window->text));
  // action bar
  popup_window->action = action_bar_layer_create();
  action_bar_layer_set_context(popup_window->action, popup_window);
  action_bar_layer_set_click_config_provider(popup_window->action, click_config_provider);
  action_bar_layer_set_icon(popup_window->action, BUTTON_ID_UP, popup_window->snooze_icon);
  action_bar_layer_set_icon(popup_window->action, BUTTON_ID_DOWN, popup_window->stop_icon);
  if (popup_window->action_visible) {
    action_bar_layer_add_to_window(popup_window->action, window);
  }

  layers_center_in_window(popup_window);
  window_set_background_color(window, popup_window->highlight_color);
}

static void prv_window_unload(Window *window) {
  PopupWindow *popup_window = window_get_user_data(window);
  action_bar_layer_destroy(popup_window->action);
  text_layer_destroy(popup_window->text);
  layer_destroy(popup_window->layer);
  window_destroy(window);
  gbitmap_destroy(popup_window->snooze_icon);
  gbitmap_destroy(popup_window->stop_icon);
  if (popup_window->draw_sequence != NULL) {
    gdraw_command_sequence_destroy(popup_window->draw_sequence);
    popup_window->draw_sequence = NULL;
  }
  popup_window->window = NULL;

  app_timer_cancel(s_app_timer);
  s_app_timer = NULL;
  vibes_cancel();
}



/*******************************************************************************
 * API FUNCTIONS
 */

// create a new PopupWindow; its window and layers are created on push
PopupWindow *popup_window_create(void) {
  PopupWindow *popup_window = calloc(1, sizeof(PopupWindow));
  if (popup_window == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create PopupWindow");
  }
  return popup_window;
}

// destroy a previously created PopupWindow
void popup_window_destroy(PopupWindow *popup_window) {
  if (popup_window == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Attempted to free NULL PopupWindow");
    return;
  }
  free(popup_window);
}

// push the PopupWindow's window onto the window stack, creating it if needed
void popup_window_push(PopupWindow *popup_window, bool animated) {
  if (popup_window->window == NULL) {
    popup_window->window = window_create();
    window_set_user_data(popup_window->window, popup_window);
    window_set_window_handlers(popup_window->window, (WindowHandlers){
      .load = prv_window_load,
      .unload = prv_window_unload
    });
  }
  if (popup_window->window) {
    window_stack_push(popup_window->window, animated);
  }
}

// remove the PopupWindow's window from the window stack
void popup_window_pop(PopupWindow *popup_window, bool animated) {
  if (popup_window->window) {
    window_stack_remove(popup_window->window, animated);
  }
}

bool popup_window_get_topmost_window(PopupWindow *popup_window) {
  return window_stack_get_top_window() == popup_window->window;
}

// set the time after which the PopupWindow pops itself on refresh (0 = never)
void popup_window_set_auto_close_duration(PopupWindow *popup_window, int64_t duration) {
  popup_window->set_time = countdown_timer_get_epoch_ms();
  popup_window->close_time = duration > 0 ? popup_window->set_time + duration : 0;
}

// run the vibration pattern NUM_VIBE_INTERVALS times via app_timer
void popup_window_set_vibes(void) {
  s_app_timer = app_timer_register(0, app_timer_vibe_callback, (void*)NUM_VIBE_INTERVALS);
}

void popup_window_set_countdown_timer(PopupWindow *popup_window, CountdownTimer *countdown_timer) {
  popup_window->countdown_timer = countdown_timer;
}

// refresh the PopupWindow: auto-pop if due and step the PDC animation
void popup_window_refresh(PopupWindow *popup_window) {
  int64_t current_time = countdown_timer_get_epoch_ms();
  if (popup_window->close_time != 0 && popup_window->close_time <= current_time) {
    popup_window_pop(popup_window, true);
  }
  if (popup_window->endless) {
    popup_window->draw_frame = gdraw_command_sequence_get_frame_by_index(
      popup_window->draw_sequence, popup_window->frame_index);
  } else {
    popup_window->draw_frame = gdraw_command_sequence_get_frame_by_elapsed(
      popup_window->draw_sequence, current_time - popup_window->set_time);
  }
  if (++popup_window->frame_index >= popup_window->frame_count) {
    popup_window->frame_index = 0;
  }
  layer_mark_dirty(popup_window->layer);
}

// set the PDC to display, freeing any previously set one
void popup_window_set_pdc(PopupWindow *popup_window, uint32_t resource_id, bool endless) {
  GDrawCommandSequence *old_sequence = popup_window->draw_sequence;
  popup_window->draw_sequence = gdraw_command_sequence_create_with_resource(resource_id);
  popup_window->frame_count = gdraw_command_sequence_get_num_frames(popup_window->draw_sequence);
  popup_window->frame_index = 0;
  popup_window->endless = endless;
  if (old_sequence != NULL) {
    gdraw_command_sequence_destroy(old_sequence);
  }
}

// duration of the current PDC in ms (0 if it loops)
int64_t popup_window_get_pdc_duration(PopupWindow *popup_window) {
  return gdraw_command_sequence_get_total_duration(popup_window->draw_sequence);
}

void popup_window_set_title(PopupWindow *popup_window, const char *text) {
  popup_window->title = text;
}

void popup_window_set_highlight_color(PopupWindow *popup_window, GColor color) {
  popup_window->highlight_color = color;
}

// the ActionBar itself is created on window load; these only toggle whether
// it gets added to the window
void popup_window_add_action_bar(PopupWindow *popup_window) {
  popup_window->action_visible = true;
}

void popup_window_remove_action_bar(PopupWindow *popup_window) {
  popup_window->action_visible = false;
}

void popup_window_set_action_bar_callbacks(PopupWindow *popup_window,
                                           PopupWindowCallbacks callbacks) {
  popup_window->callbacks = callbacks;
}
