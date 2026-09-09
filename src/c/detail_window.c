/*******************************************************************************
 * FILENAME :        detail_window.c
 *
 * DESCRIPTION :
 *      Display a timer with controls to modify it
 *
 * AUTHOR :     Eric Phillips        START DATE :    07/11/15
 */

#include <pebble.h>
#include "detail_window.h"
#include "theme.h"
#include "common.h"

#define TEXT_LAYER_MAX_LARGE_CHARACTERS 5

struct DetailWindow {
  Window      *window;    //< main window
  Layer       *layer;     //< drawing layer
  TextLayer   *main_text; //< main, larger text
  TextLayer   *sub_text;  //< footer, small text
  ActionBarLayer *action; //< action bar
  GBitmap     *edit_icon, *play_icon, *pause_icon, *delete_icon;  //< icons
  GFont       large_font, medium_font, small_font; //< fonts
  GColor      highlight_color;        //< main color for highlights
  StatusBarLayer *status;             //< status bar
  DetailWindowCallbacks callbacks;    //< callbacks for button presses

  char        main_buff[12];          //< text buffer for main_text
  char        sub_buff[12];           //< text buffer for sub_text

  CountdownTimer *countdown_timer;    //< the CountdownTimer being shown
};

/*******************************************************************************
 * PRIVATE FUNCTIONS
 */

// layer update proc: draws the progress fill in the background
static void layer_update_proc(Layer *layer, GContext *ctx) {
  DetailWindow *detail_window = *(DetailWindow**)layer_get_data(layer);
  int64_t current_time = countdown_timer_get_display_time(detail_window->countdown_timer);
  int64_t total_time = countdown_timer_get_duration(detail_window->countdown_timer);
  if (total_time <= 0) {
    return;
  }
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, detail_window->highlight_color);
#ifdef PBL_ROUND
  graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, bounds.size.w / 2,
    TRIG_MAX_ANGLE - TRIG_MAX_ANGLE * current_time / total_time, TRIG_MAX_ANGLE);
#else
  int16_t water_level = bounds.size.h - bounds.size.h * current_time / total_time;
  graphics_fill_rect(ctx, GRect(0, water_level, bounds.size.w, bounds.size.h - water_level),
    1, GCornerNone);
  // Kante der Fuellung markieren. Auf S/W-Geraeten ist die Fuellung weiss und
  // damit unsichtbar, dort ist diese Linie die einzige Fortschrittsanzeige.
  // Auf Farbgeraeten schaerft sie die sonst weiche Grenze zwischen den
  // beiden Gruentoenen.
  if (water_level > 0 && water_level < bounds.size.h) {
    graphics_context_set_fill_color(ctx, ZM_COLOR_ON_SURFACE);
    graphics_fill_rect(ctx, GRect(0, water_level, bounds.size.w, 2), 0, GCornerNone);
  }
#endif
}

static int64_t prv_round_up_to_next_second(int64_t value) {
  return value > 0 ? value + MSEC_IN_SEC - 1 : 0;
}

// UP edits, SELECT plays/pauses, DOWN deletes the timer
static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  DetailWindow *detail_window = (DetailWindow*)context;
  detail_window->callbacks.edit_timer(detail_window->countdown_timer, context);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  DetailWindow *detail_window = (DetailWindow*)context;
  detail_window->callbacks.playpause_timer(detail_window->countdown_timer, context);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  DetailWindow *detail_window = (DetailWindow*)context;
  detail_window->callbacks.delete_timer(detail_window->countdown_timer, context);
}

static void click_config_provider(void *context) {
  window_set_click_context(BUTTON_ID_UP, context);
  window_set_click_context(BUTTON_ID_SELECT, context);
  window_set_click_context(BUTTON_ID_DOWN, context);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void prv_window_load(Window* window){
  DetailWindow *detail_window = window_get_user_data(window);
  window_set_background_color(detail_window->window, ZM_COLOR_SURFACE);

  // load resources
  detail_window->edit_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_EDIT);
  detail_window->play_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PLAY);
  detail_window->pause_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PAUSE);
  detail_window->delete_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DELETE);
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  const uint32_t font_ids[] = {RESOURCE_ID_FONT_LECO_REGULAR_SUBSET_48,
    RESOURCE_ID_FONT_LECO_REGULAR_SUBSET_36, RESOURCE_ID_FONT_LECO_REGULAR_SUBSET_26};
  const uint8_t main_h = 52, sub_h = 30;
#else
  const uint32_t font_ids[] = {RESOURCE_ID_FONT_LECO_REGULAR_SUBSET_36,
    RESOURCE_ID_FONT_LECO_REGULAR_SUBSET_26, RESOURCE_ID_FONT_LECO_REGULAR_SUBSET_20};
  const uint8_t main_h = 40, sub_h = 24;
#endif
  detail_window->large_font = fonts_load_custom_font(resource_get_handle(font_ids[0]));
  detail_window->medium_font = fonts_load_custom_font(resource_get_handle(font_ids[1]));
  detail_window->small_font = fonts_load_custom_font(resource_get_handle(font_ids[2]));

  Layer *root = window_get_root_layer(detail_window->window);
  GRect bounds = layer_get_frame(root);
  // create animation layer
  // IMPORTANT: must be created with data for the DetailWindow pointer
  // so that it can be accessed in the layer_update_proc callback
  detail_window->layer = layer_create_with_data(bounds, sizeof(DetailWindow*));
  *(DetailWindow**)layer_get_data(detail_window->layer) = detail_window;
  layer_set_update_proc(detail_window->layer, layer_update_proc);
  layer_add_child(root, detail_window->layer);
  // create main text
  detail_window->main_text = text_layer_create(GRect(0,
    PBL_IF_ROUND_ELSE(bounds.size.h/2-main_h/2, bounds.size.h*2 / 17),
    bounds.size.w - ACTION_BAR_WIDTH, main_h));
  text_layer_set_font(detail_window->main_text, detail_window->large_font);
  text_layer_set_text(detail_window->main_text, "00:00");
  text_layer_set_text_alignment(detail_window->main_text, GTextAlignmentCenter);
  text_layer_set_background_color(detail_window->main_text, GColorClear);
  layer_add_child(root, text_layer_get_layer(detail_window->main_text));
  // create sub text
  detail_window->sub_text = text_layer_create(GRect(PBL_IF_ROUND_ELSE(0, 10),
    bounds.size.h-sub_h-PBL_IF_ROUND_ELSE(11, 6),
    bounds.size.w - PBL_IF_ROUND_ELSE(0, ACTION_BAR_WIDTH), sub_h));
  text_layer_set_text_alignment(detail_window->sub_text,
    PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
  text_layer_set_font(detail_window->sub_text, detail_window->small_font);
  text_layer_set_text(detail_window->sub_text, "00:00");
  text_layer_set_background_color(detail_window->sub_text, GColorClear);
  layer_add_child(root, text_layer_get_layer(detail_window->sub_text));
  // create action bar
  detail_window->action = action_bar_layer_create();
  action_bar_layer_add_to_window(detail_window->action, detail_window->window);
  action_bar_layer_set_click_config_provider(detail_window->action, click_config_provider);
  action_bar_layer_set_context(detail_window->action, detail_window);
  action_bar_layer_set_icon(detail_window->action, BUTTON_ID_UP, detail_window->edit_icon);
  action_bar_layer_set_icon(detail_window->action, BUTTON_ID_SELECT, detail_window->pause_icon);
  action_bar_layer_set_icon(detail_window->action, BUTTON_ID_DOWN, detail_window->delete_icon);
  // create status bar
  detail_window->status = status_bar_layer_create();
  layer_set_frame(status_bar_layer_get_layer(detail_window->status),
    GRect(0, 0, bounds.size.w - PBL_IF_ROUND_ELSE(0, ACTION_BAR_WIDTH), STATUS_BAR_LAYER_HEIGHT));
  status_bar_layer_set_colors(detail_window->status, GColorClear, GColorBlack);
  layer_add_child(root, status_bar_layer_get_layer(detail_window->status));
}

static void prv_window_unload(Window* window){
  DetailWindow *detail_window = window_get_user_data(window);
  status_bar_layer_destroy(detail_window->status);
  action_bar_layer_destroy(detail_window->action);
  text_layer_destroy(detail_window->sub_text);
  text_layer_destroy(detail_window->main_text);
  layer_destroy(detail_window->layer);
  window_destroy(detail_window->window);
  gbitmap_destroy(detail_window->edit_icon);
  gbitmap_destroy(detail_window->play_icon);
  gbitmap_destroy(detail_window->pause_icon);
  gbitmap_destroy(detail_window->delete_icon);
  fonts_unload_custom_font(detail_window->large_font);
  fonts_unload_custom_font(detail_window->medium_font);
  fonts_unload_custom_font(detail_window->small_font);
  detail_window->window = NULL;
}

/*******************************************************************************
 * API FUNCTIONS
 */

// create a new DetailWindow; the Window itself is created lazily on push
DetailWindow *detail_window_create(DetailWindowCallbacks detail_window_callbacks) {
  DetailWindow *detail_window = (DetailWindow*)malloc(sizeof(DetailWindow));
  if (detail_window == NULL) {
    return NULL;
  }
  *detail_window = (DetailWindow) { .callbacks = detail_window_callbacks };
  return detail_window;
}

void detail_window_destroy(DetailWindow *detail_window) {
  free(detail_window);
}

// push the window onto the stack
void detail_window_push(DetailWindow *detail_window, bool animated) {
  if (detail_window->window == NULL) {
    detail_window->window = window_create();
    window_set_user_data(detail_window->window, detail_window);
    window_set_window_handlers(detail_window->window,
      (WindowHandlers){
        .load = prv_window_load,
        .unload = prv_window_unload
      });
  }
  if (detail_window->window) {
    window_stack_push(detail_window->window, animated);
  }
}

// pop the window off the stack
void detail_window_pop(DetailWindow *detail_window, bool animated) {
  if (detail_window->window) {
    window_stack_remove(detail_window->window, animated);
  }
}

bool detail_window_get_topmost_window(DetailWindow *detail_window) {
  return window_stack_get_top_window() == detail_window->window;
}

void detail_window_set_countdown_timer(DetailWindow *detail_window,
                                       CountdownTimer *countdown_timer) {
  detail_window->countdown_timer = countdown_timer;
}

// refresh texts and progress fill
void detail_window_refresh(DetailWindow *detail_window) {
  if (detail_window->window == NULL) {
    return;
  }

  layer_mark_dirty(detail_window->layer);
  // main text
  countdown_timer_format_text(
    prv_round_up_to_next_second(countdown_timer_get_display_time(detail_window->countdown_timer)),
    detail_window->main_buff, sizeof(detail_window->main_buff));
  text_layer_set_text(detail_window->main_text, detail_window->main_buff);
  text_layer_set_font(detail_window->main_text,
    strlen(detail_window->main_buff) > TEXT_LAYER_MAX_LARGE_CHARACTERS ?
    detail_window->medium_font : detail_window->large_font);
  // sub text
  countdown_timer_format_text(countdown_timer_get_duration(detail_window->countdown_timer),
    detail_window->sub_buff, sizeof(detail_window->sub_buff));
  text_layer_set_text(detail_window->sub_text, detail_window->sub_buff);
}

// deep refresh: also updates the play/pause icon
void detail_window_deep_refresh(DetailWindow *detail_window) {
  if (detail_window->window != NULL && detail_window->countdown_timer != NULL) {
    action_bar_layer_set_icon(detail_window->action, BUTTON_ID_SELECT,
      countdown_timer_get_paused(detail_window->countdown_timer) ?
      detail_window->play_icon : detail_window->pause_icon);
    detail_window_refresh(detail_window);
  }
}

void detail_window_set_highlight_color(DetailWindow *detail_window, GColor color) {
  detail_window->highlight_color = color;
}
