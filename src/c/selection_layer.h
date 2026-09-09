/*
 * somewhat modified implementation of the firmware's SelectionLayer
 * the changes include adding B&W support and the ability to move the
 * selector field backward and forward.
 */

#pragma once

#include <pebble.h>

typedef char* (*SelectionLayerGetCellText)(unsigned index, void *callback_context);

typedef void (*SelectionLayerCompleteCallback)(void *callback_context);

typedef void (*SelectionLayerIncrementCallback)(unsigned selected_cell_idx,
                                                uint8_t reapeating_count, void *callback_context);

typedef void (*SelectionLayerDecrementCallback)(unsigned selected_cell_idx,
                                                uint8_t reapeating_count, void *callback_context);

typedef struct SelectionLayerCallbacks {
  SelectionLayerGetCellText get_cell_text;
  SelectionLayerCompleteCallback complete;
  SelectionLayerIncrementCallback increment;
  SelectionLayerDecrementCallback decrement;
} SelectionLayerCallbacks;

Layer* selection_layer_create(GRect frame, unsigned num_cells);

void selection_layer_destroy(Layer* layer);

void selection_layer_set_cell_width(Layer *layer, unsigned cell_idx, unsigned width);

void selection_layer_set_inactive_bg_color(Layer *layer, GColor color);

void selection_layer_set_active_bg_color(Layer *layer, GColor color);

void selection_layer_set_cell_padding(Layer *layer, unsigned padding);

void selection_layer_set_click_config_onto_window(Layer *layer, struct Window *window);

void selection_layer_set_callbacks(Layer *layer, void *callback_context,
                                   SelectionLayerCallbacks callbacks);
