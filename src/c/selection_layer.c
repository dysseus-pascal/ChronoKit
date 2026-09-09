/*
 * somewhat modified implementation of the firmware's SelectionLayer
 * the changes include adding B&W support and the ability to move the
 * selector field backward and forward.
 */

#include <pebble.h>
#include "selection_layer.h"

#define MAX_SELECTION_LAYER_CELLS 3

// Look and feel
#define DEFAULT_CELL_PADDING 10
#define DEFAULT_FONT FONT_KEY_GOTHIC_28_BOLD
#define DEFAULT_ACTIVE_COLOR GColorWhite
#define DEFAULT_INACTIVE_COLOR GColorDarkGray

#define BUTTON_HOLD_REPEAT_MS 100

// Animation timings taken from the reference video (28fps, i.e. 35.7ms per frame)
#define BUMP_TEXT_DURATION_MS 107    // 3 frames
#define BUMP_SETTLE_DURATION_MS 214  // 6 frames
#define SLIDE_DURATION_MS 107        // 3 frames
#define SLIDE_SETTLE_DURATION_MS 179 // 5 frames
// In the video this is 3, but that's not enough (also even numbers work better)
#define SETTLE_HEIGHT_DIFF 6

typedef struct {
  unsigned num_cells;
  unsigned cell_widths[MAX_SELECTION_LAYER_CELLS];
  unsigned cell_padding;
  unsigned selected_cell_idx;

  GFont font;
  GColor inactive_background_color;
  GColor active_background_color;

  SelectionLayerCallbacks callbacks;
  void *callback_context;

  // Increment / decrement animation: the text bumps to the cell edge, then the cell settles
  bool bump_is_upwards;
  unsigned bump_text_anim_progress;
  AnimationImplementation bump_text_impl;
  unsigned bump_settle_anim_progress;
  AnimationImplementation bump_settle_anim_impl;

  // Slide animation: the selection box moves to the neighbouring cell, overshoots, then settles
  Animation *next_cell_animation;
  bool slide_is_forward;
  unsigned slide_amin_progress;
  AnimationImplementation slide_amin_impl;
  unsigned slide_settle_anim_progress;
  AnimationImplementation slide_settle_anim_impl;
} SelectionLayerData;


///////////////////////////////////////////////////////////////////////////////////////////////////
//! Drawing helpers

static int prv_get_pixels_for_bump_settle(int anim_percent_complete) {
  if (anim_percent_complete) {
    return SETTLE_HEIGHT_DIFF - ((SETTLE_HEIGHT_DIFF * anim_percent_complete) / 100);
  }
  return 0;
}

static int prv_get_font_top_padding(GFont font) {
  return (font == fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD)) ? 10 : 0;
}

// Assumes numbers / capital letters
static int prv_get_y_offset_which_vertically_centers_font(GFont font, int height) {
  int font_height = (font == fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD)) ? 18 : 0;
  return (height / 2) - (font_height / 2) - prv_get_font_top_padding(font);
}

// x-offset of the currently selected cell
static int prv_get_selected_cell_x_offset(SelectionLayerData *data) {
  int x_offset = 0;
  for (unsigned i = 0; i < data->selected_cell_idx; i++) {
    x_offset += data->cell_widths[i] + data->cell_padding;
  }
  return x_offset;
}

// Fills a cell rectangle: pill shaped on round displays, rectangular otherwise
static void prv_fill_cell_rect(GContext *ctx, GRect rect) {
#ifdef PBL_ROUND
  rect.origin.y -= (rect.size.w - rect.size.h) / 2;
  rect.size.h = rect.size.w;
  graphics_fill_rect(ctx, rect, rect.size.h / 2 - 1, GCornersAll);
#else
  graphics_fill_rect(ctx, rect, 1, GCornerNone);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Drawing the layer

static void prv_draw_cell_backgrounds(Layer *layer, GContext *ctx) {
  SelectionLayerData *data = layer_get_data(layer);
  for (unsigned i = 0, current_x_offset = 0; i < data->num_cells; i++) {
    if (data->cell_widths[i] == 0) {
      continue;
    }
    // During the bump animation the selected cell grows by the settle amount. For an upward
    // bump it grows above the frame (negative y-offset) so its bottom edge stays fixed.
    int y_offset = 0;
    int height = layer_get_bounds(layer).size.h;
    if (data->selected_cell_idx == i) {
      int bump = prv_get_pixels_for_bump_settle(data->bump_settle_anim_progress);
      height += bump;
      if (data->bump_is_upwards) {
        y_offset = -bump;
      }
    }
    // While the slide animation runs the slider is drawn over this later, so the selected
    // cell keeps the inactive color
    GColor bg_color = data->inactive_background_color;
    if (data->selected_cell_idx == i && !data->slide_amin_progress) {
      bg_color = data->active_background_color;
    }
    graphics_context_set_fill_color(ctx, bg_color);
    prv_fill_cell_rect(ctx, GRect(current_x_offset, y_offset, data->cell_widths[i], height));

    current_x_offset += data->cell_widths[i] + data->cell_padding;
  }
}

static void prv_draw_slider_slide(Layer *layer, GContext *ctx) {
  SelectionLayerData *data = layer_get_data(layer);
  int starting_x_offset = prv_get_selected_cell_x_offset(data);
  int cur_cell_width = data->cell_widths[data->selected_cell_idx];
  int next_cell_width = data->cell_widths[data->slide_is_forward ?
      data->selected_cell_idx + 1 : data->selected_cell_idx - 1];

  // The slider moves from the current cell to the next one. At the same time its width morphs
  // to the next cell's width plus padding, so it overshoots its mark (the settle animation
  // removes the extra width again).
  int slide_distance = next_cell_width + data->cell_padding;
  int current_slide_distance = (slide_distance * data->slide_amin_progress) / 100;
  int total_cell_width_change = next_cell_width - cur_cell_width + data->cell_padding;
  int current_cell_width_change =
      (total_cell_width_change * (int) data->slide_amin_progress) / 100;
  int current_cell_width = cur_cell_width + current_cell_width_change;

  int current_x_offset = starting_x_offset;
  if (data->slide_is_forward) {
    current_x_offset += current_slide_distance;
  } else {
    current_x_offset -= current_slide_distance + current_cell_width_change;
  }

  graphics_context_set_fill_color(ctx, data->active_background_color);
  prv_fill_cell_rect(ctx,
      GRect(current_x_offset, 0, current_cell_width, layer_get_bounds(layer).size.h));
}

static void prv_draw_slider_settle(Layer *layer, GContext *ctx) {
  SelectionLayerData *data = layer_get_data(layer);
  // The active cell is already updated and drawn by prv_draw_cell_backgrounds. This only draws
  // the receding extra (padding) width that created the overshoot, next to the active cell.
  int x_offset = prv_get_selected_cell_x_offset(data);
  int current_width = (data->cell_padding * data->slide_settle_anim_progress) / 100;
  if (data->slide_is_forward) {
    x_offset += data->cell_widths[data->selected_cell_idx];
  } else {
    x_offset -= current_width;
  }

  graphics_context_set_fill_color(ctx, data->active_background_color);
  prv_fill_cell_rect(ctx, GRect(x_offset, 0, current_width, layer_get_bounds(layer).size.h));
}

static void prv_draw_text(Layer *layer, GContext *ctx) {
  SelectionLayerData *data = layer_get_data(layer);
  for (unsigned i = 0, current_x_offset = 0; i < data->num_cells; i++) {
    char *text = data->callbacks.get_cell_text ?
        data->callbacks.get_cell_text(i, data->callback_context) : NULL;
    if (text) {
      bool selected = (data->selected_cell_idx == i);
      // Height of the box the text is vertically centered in (see prv_draw_cell_backgrounds)
      int height = layer_get_bounds(layer).size.h;
      if (selected) {
        height += prv_get_pixels_for_bump_settle(data->bump_settle_anim_progress);
      }
      int y_offset = prv_get_y_offset_which_vertically_centers_font(data->font, height);
      if (selected) {
        // Follow the cell if it is drawn above the frame, then apply the bump progress
        if (data->bump_is_upwards) {
          y_offset -= prv_get_pixels_for_bump_settle(data->bump_settle_anim_progress);
        }
        int delta = (data->bump_text_anim_progress * prv_get_font_top_padding(data->font)) / 100;
        y_offset += data->bump_is_upwards ? -delta : delta;
      }
      // While the slide animation runs the selected cell is drawn inactive
      GColor bg_color = (selected && !data->slide_amin_progress) ?
          data->active_background_color : data->inactive_background_color;
      graphics_context_set_text_color(ctx, gcolor_legible_over(bg_color));

      graphics_draw_text(ctx, text, data->font,
          GRect(current_x_offset, y_offset, data->cell_widths[i], height),
          GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    }
    current_x_offset += data->cell_widths[i] + data->cell_padding;
  }
}

static void prv_draw_selection_layer(Layer *layer, GContext *ctx) {
  SelectionLayerData *data = layer_get_data(layer);
  prv_draw_cell_backgrounds(layer, ctx);
  // The slider is drawn above the backgrounds, but below the text
  if (data->slide_amin_progress) {
    prv_draw_slider_slide(layer, ctx);
  }
  if (data->slide_settle_anim_progress) {
    prv_draw_slider_settle(layer, ctx);
  }
  prv_draw_text(layer, ctx);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Animations

// Creates a layer frame animation. impl must outlive the animation, so it lives in the layer data.
static Animation* prv_create_animation(Layer *layer, AnimationCurve curve, uint32_t duration_ms,
                                       AnimationStoppedHandler stopped,
                                       AnimationImplementation *impl,
                                       AnimationUpdateImplementation update) {
  Animation *animation = property_animation_get_animation(
      property_animation_create_layer_frame(layer, NULL, NULL));
  animation_set_curve(animation, curve);
  animation_set_duration(animation, duration_ms);
  animation_set_handlers(animation, (AnimationHandlers) { .stopped = stopped }, layer);
  *impl = (AnimationImplementation) { .update = update };
  animation_set_implementation(animation, impl);
  return animation;
}

//! Increment / Decrement: the text moves to the cell edge (bump_text) and "pushes" the cell,
//! which expands and then shrinks back to its original height with the text centered
//! (bump_settle).

static void prv_bump_settle_impl(Animation *animation, const AnimationProgress distance_normalized) {
  Layer *layer = animation_get_context(animation);
  SelectionLayerData *data = layer_get_data(layer);
  data->bump_settle_anim_progress = (100 * distance_normalized) / ANIMATION_NORMALIZED_MAX;
  layer_mark_dirty(layer);
}

static void prv_bump_settle_stopped(Animation *animation, bool finished, void *context) {
  SelectionLayerData *data = layer_get_data(animation_get_context(animation));
  data->bump_settle_anim_progress = 0;
  animation_destroy(animation);
}

static Animation* prv_create_bump_settle_animation(Layer *layer) {
  SelectionLayerData *data = layer_get_data(layer);
  return prv_create_animation(layer, AnimationCurveEaseOut, BUMP_SETTLE_DURATION_MS,
      prv_bump_settle_stopped, &data->bump_settle_anim_impl, prv_bump_settle_impl);
}

static void prv_bump_text_impl(Animation *animation, const AnimationProgress distance_normalized) {
  Layer *layer = animation_get_context(animation);
  SelectionLayerData *data = layer_get_data(layer);
  data->bump_text_anim_progress = (100 * distance_normalized) / ANIMATION_NORMALIZED_MAX;
  layer_mark_dirty(layer);
}

static void prv_bump_text_stopped(Animation *animation, bool finished, void *context) {
  Layer *layer = animation_get_context(animation);
  SelectionLayerData *data = layer_get_data(layer);
  data->bump_text_anim_progress = 0;
  // The value is updated once the text has reached the cell edge
  if (data->bump_is_upwards) {
    data->callbacks.increment(data->selected_cell_idx, 1, data->callback_context);
  } else {
    data->callbacks.decrement(data->selected_cell_idx, 1, data->callback_context);
  }
  animation_destroy(animation);
  animation_schedule(prv_create_bump_settle_animation(layer));
}

static void prv_run_value_change_animation(Layer *layer) {
  SelectionLayerData *data = layer_get_data(layer);
  Animation *bump_text = prv_create_animation(layer, AnimationCurveEaseIn, BUMP_TEXT_DURATION_MS,
      prv_bump_text_stopped, &data->bump_text_impl, prv_bump_text_impl);
  Animation *bump_settle = prv_create_bump_settle_animation(layer);
  animation_schedule(animation_sequence_create(bump_text, bump_settle, NULL));
}

//! Slide: the selection box moves to the neighbouring cell while growing by the padding width
//! (overshoot), then the extra width recedes again (slide_settle).

static void prv_slide_impl(Animation *animation, const AnimationProgress distance_normalized) {
  Layer *layer = animation_get_context(animation);
  SelectionLayerData *data = layer_get_data(layer);
  data->slide_amin_progress = (100 * distance_normalized) / ANIMATION_NORMALIZED_MAX;
  layer_mark_dirty(layer);
}

static void prv_slide_stopped(Animation *animation, bool finished, void *context) {
  SelectionLayerData *data = layer_get_data(animation_get_context(animation));
  data->slide_amin_progress = 0;
  if (data->slide_is_forward) {
    data->selected_cell_idx++;
  } else {
    data->selected_cell_idx--;
  }
  animation_destroy(animation);
}

static void prv_slide_settle_impl(Animation *animation, const AnimationProgress distance_normalized) {
  Layer *layer = animation_get_context(animation);
  SelectionLayerData *data = layer_get_data(layer);
  // Reverse animation: starts fully drawn, then the amount drawn decreases
  data->slide_settle_anim_progress = 100 - (100 * distance_normalized) / ANIMATION_NORMALIZED_MAX;
  layer_mark_dirty(layer);
}

static void prv_slide_settle_stopped(Animation *animation, bool finished, void *context) {
  SelectionLayerData *data = layer_get_data(animation_get_context(animation));
  data->slide_settle_anim_progress = 0;
  animation_destroy(animation);
}

static void prv_run_slide_animation(Layer *layer) {
  SelectionLayerData *data = layer_get_data(layer);
  Animation *slide = prv_create_animation(layer, AnimationCurveEaseIn, SLIDE_DURATION_MS,
      prv_slide_stopped, &data->slide_amin_impl, prv_slide_impl);
  Animation *settle = prv_create_animation(layer, AnimationCurveEaseOut, SLIDE_SETTLE_DURATION_MS,
      prv_slide_settle_stopped, &data->slide_settle_anim_impl, prv_slide_settle_impl);
  data->next_cell_animation = animation_sequence_create(slide, settle, NULL);
  animation_schedule(data->next_cell_animation);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Click handlers

static void prv_change_value(ClickRecognizerRef recognizer, Layer *layer, bool up) {
  SelectionLayerData *data = layer_get_data(layer);
  if (click_recognizer_is_repeating(recognizer)) {
    // Don't animate while the button is held down, just update the text
    SelectionLayerIncrementCallback change = up ? data->callbacks.increment : data->callbacks.decrement;
    change(data->selected_cell_idx, click_number_of_clicks_counted(recognizer), data->callback_context);
    layer_mark_dirty(layer);
  } else {
    // Run the animation; the increment / decrement callback runs when the text hits the edge
    data->bump_is_upwards = up;
    prv_run_value_change_animation(layer);
  }
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_change_value(recognizer, context, true);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_change_value(recognizer, context, false);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  Layer *layer = context;
  SelectionLayerData *data = layer_get_data(layer);
  animation_unschedule(data->next_cell_animation);
  if (data->selected_cell_idx >= data->num_cells - 1) {
    data->selected_cell_idx = 0;
    data->callbacks.complete(data->callback_context);
  } else {
    data->slide_is_forward = true;
    prv_run_slide_animation(layer);
  }
}

static void prv_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  Layer *layer = context;
  SelectionLayerData *data = layer_get_data(layer);
  animation_unschedule(data->next_cell_animation);
  if (data->selected_cell_idx == 0) {
    window_stack_pop(true);
  } else {
    data->slide_is_forward = false;
    prv_run_slide_animation(layer);
  }
}

static void prv_click_config_provider(Layer *layer) {
  window_set_click_context(BUTTON_ID_UP, layer);
  window_set_click_context(BUTTON_ID_DOWN, layer);
  window_set_click_context(BUTTON_ID_SELECT, layer);
  window_set_click_context(BUTTON_ID_BACK, layer);

  window_single_repeating_click_subscribe(BUTTON_ID_UP, BUTTON_HOLD_REPEAT_MS, prv_up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, BUTTON_HOLD_REPEAT_MS, prv_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click_handler);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//! API

Layer* selection_layer_create(GRect frame, unsigned num_cells) {
  if (num_cells > MAX_SELECTION_LAYER_CELLS) {
    num_cells = MAX_SELECTION_LAYER_CELLS;
  }
  Layer *layer = layer_create_with_data(frame, sizeof(SelectionLayerData));
  SelectionLayerData *data = layer_get_data(layer);
  *data = (SelectionLayerData) {
    .active_background_color = DEFAULT_ACTIVE_COLOR,
    .inactive_background_color = DEFAULT_INACTIVE_COLOR,
    .num_cells = num_cells,
    .cell_padding = DEFAULT_CELL_PADDING,
    .font = fonts_get_system_font(DEFAULT_FONT),
  };
  layer_set_clips(layer, false);
  layer_set_update_proc(layer, prv_draw_selection_layer);
  return layer;
}

void selection_layer_destroy(Layer* layer) {
  animation_unschedule_all();
  layer_destroy(layer);
}

void selection_layer_set_cell_width(Layer *layer, unsigned idx, unsigned width) {
  SelectionLayerData *data = layer_get_data(layer);
  if (data && idx < data->num_cells) {
    data->cell_widths[idx] = width;
  }
}

void selection_layer_set_inactive_bg_color(Layer *layer, GColor color) {
  SelectionLayerData *data = layer_get_data(layer);
  if (data) {
    data->inactive_background_color = color;
  }
}

void selection_layer_set_active_bg_color(Layer *layer, GColor color) {
  SelectionLayerData *data = layer_get_data(layer);
  if (data) {
    data->active_background_color = color;
  }
}

void selection_layer_set_cell_padding(Layer *layer, unsigned padding) {
  SelectionLayerData *data = layer_get_data(layer);
  if (data) {
    data->cell_padding = padding;
  }
}

void selection_layer_set_click_config_onto_window(Layer *layer, struct Window *window) {
  if (layer && window) {
    window_set_click_config_provider_with_context(window,
        (ClickConfigProvider) prv_click_config_provider, layer);
  }
}

void selection_layer_set_callbacks(Layer *layer, void *callback_context,
                                   SelectionLayerCallbacks callbacks) {
  SelectionLayerData *data = layer_get_data(layer);
  data->callbacks = callbacks;
  data->callback_context = callback_context;
}
