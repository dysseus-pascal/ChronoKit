// ChronoKit: kombiniert die offiziellen Pebble-Apps Stopwatch und Timer
// (Core Devices, github.com/coredevices) hinter einem kleinen Launcher.
#include <pebble.h>
#include "stopwatch.h"
#include "timer_app.h"
#include "theme.h"

static Window *s_launcher_window;
static MenuLayer *s_launcher_menu;
static StatusBarLayer *s_status_bar;

static uint16_t launcher_num_rows(MenuLayer *ml, uint16_t section, void *ctx) {
  return 2;
}

static void launcher_draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  if (idx->row == 0) {
    menu_cell_basic_draw(ctx, cell, "Stoppuhr", "Runden & Zwischenzeit", NULL);
  } else {
    menu_cell_basic_draw(ctx, cell, "Timer", "Countdown mit Alarm", NULL);
  }
}

static void launcher_select(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  if (idx->row == 0) {
    stopwatch_window_push();
  } else {
    timer_app_open();
  }
}

static void launcher_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  // Auf runden Displays nutzt das Menue die volle Flaeche und stellt die
  // Auswahl mittig, sonst wird es unter der Statusleiste eingehaengt.
  // Das entspricht dem Verhalten der Timer-Liste.
#ifdef PBL_ROUND
  s_launcher_menu = menu_layer_create(b);
#else
  s_launcher_menu = menu_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, b.size.w,
                                            b.size.h - STATUS_BAR_LAYER_HEIGHT));
#endif
  menu_layer_set_callbacks(s_launcher_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = launcher_num_rows,
    .draw_row = launcher_draw_row,
    .select_click = launcher_select,
  });
#ifdef PBL_ROUND
  menu_layer_set_center_focused(s_launcher_menu, true);
#endif
  menu_layer_set_highlight_colors(s_launcher_menu, ZM_COLOR_ACCENT,
                                  ZM_COLOR_ON_ACCENT);
  menu_layer_set_click_config_onto_window(s_launcher_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_launcher_menu));

  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, GColorClear, GColorBlack);
  layer_add_child(root, status_bar_layer_get_layer(s_status_bar));
}

static void launcher_window_unload(Window *window) {
  status_bar_layer_destroy(s_status_bar);
  menu_layer_destroy(s_launcher_menu);
}

// ---- App-Glance -----------------------------------------------------------
// Beide Module schrieben den Glance frueher selbst. app_glance_reload loescht
// zuerst alle Slices, also ueberschrieb der Timer beim Beenden immer den
// Eintrag der Stoppuhr. Jetzt baut ihn der Launcher einmal zusammen.
#define ZM_GLANCE_BUFF_SIZE 50

static void prv_add_slice(AppGlanceReloadSession *session, const char *str,
                          time_t expiration_time) {
  const AppGlanceSlice slice = {
    .layout.subtitle_template_string = str,
    .expiration_time = expiration_time
  };
  AppGlanceResult result = app_glance_add_slice(session, slice);
  if (result != APP_GLANCE_RESULT_SUCCESS) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "AppGlanceSlice: %d", result);
  }
}

static void prv_update_app_glance(AppGlanceReloadSession *session, size_t limit, void *context) {
  size_t used = 0;
  char buff[ZM_GLANCE_BUFF_SIZE];
  time_t expiration;

  // laufender Timer zuerst: er hat ein Ende und ist die dringendere Information
  if (used < limit && timer_app_get_glance(buff, sizeof(buff), &expiration)) {
    prv_add_slice(session, buff, expiration);
    used++;
  }
  if (used < limit && stopwatch_get_glance(buff, sizeof(buff), &expiration)) {
    prv_add_slice(session, buff, expiration);
    used++;
  }
}

static void init(void) {
  timer_app_init();

  s_launcher_window = window_create();
  window_set_window_handlers(s_launcher_window, (WindowHandlers) {
    .load = launcher_window_load,
    .unload = launcher_window_unload,
  });
  window_stack_push(s_launcher_window, true);

  // Wakeup (abgelaufener Timer) oder Timeline-Pin: direkt in den Timer-Teil
  timer_app_handle_launch();
}

static void deinit(void) {
  // Glance bauen, solange die Timer-Daten noch existieren
  app_glance_reload(prv_update_app_glance, NULL);
  timer_app_deinit();
  window_destroy(s_launcher_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
