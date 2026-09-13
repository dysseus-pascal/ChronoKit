// ChronoKit: kombiniert die offiziellen Pebble-Apps Stopwatch und Timer
// (Core Devices, github.com/coredevices) hinter einem kleinen Launcher.
// Dritte Zeile ist der eigene Zeitzonen-Screen.
#include <pebble.h>
#include "stopwatch.h"
#include "timer_app.h"
#include "timezone_window.h"
#include "strings.h"
#include "theme.h"

static Window *s_launcher_window;
static MenuLayer *s_launcher_menu;
static StatusBarLayer *s_status_bar;

static uint16_t launcher_num_rows(MenuLayer *ml, uint16_t section, void *ctx) {
  return 3;
}

static void launcher_draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  if (idx->row == 0) {
    menu_cell_basic_draw(ctx, cell, S(STR_LAUNCHER_STOPWATCH), S(STR_LAUNCHER_STOPWATCH_SUB), NULL);
  } else if (idx->row == 1) {
    menu_cell_basic_draw(ctx, cell, S(STR_LAUNCHER_TIMER), S(STR_LAUNCHER_TIMER_SUB), NULL);
  } else {
    // static, weil menu_cell_basic_draw den String nicht kopiert. Hoechstens
    // einmal je Sekunde neu rechnen: der Untertitel kostet Zeitzonen- und
    // Persist-Zugriffe, und waehrend einer Scroll-Animation wird die Zeile
    // viele Male je Sekunde gezeichnet. Ein eigener Takt waere dafuer zu viel -
    // tick_timer_service ist eine globale Einzelanmeldung und gehoert dem
    // Zeitzonen-Screen, solange er offen ist.
    static char s_tz_sub[32];
    static time_t s_tz_sub_at;
    const time_t now = time(NULL);
    if (s_tz_sub[0] == '\0' || now != s_tz_sub_at) {
      s_tz_sub_at = now;
      // Im selben Sekundenzweig auch die Sprache neu lesen: es gibt kein
      // Ereignis fuer einen Sprachwechsel, und hier wird der Text ohnehin neu
      // gebaut. Waehrend einer Scroll-Animation kostet es dadurch nichts.
      strings_refresh();
      timezone_get_launcher_subtitle(s_tz_sub, sizeof(s_tz_sub));
    }
    menu_cell_basic_draw(ctx, cell, S(STR_LAUNCHER_TIMEZONE), s_tz_sub, NULL);
  }
}

static void launcher_select(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  if (idx->row == 0) {
    stopwatch_window_push();
  } else if (idx->row == 1) {
    timer_app_open();
  } else {
    timezone_window_push();
  }
}

static void launcher_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  // Auf runden Displays nutzt das Menue die volle Flaeche und stellt die
  // Auswahl mittig, sonst wird es unter der Statusleiste eingehaengt.
  // Das entspricht dem Verhalten der Timer-Liste.
  s_launcher_menu = menu_layer_create(PBL_IF_ROUND_ELSE(b,
      GRect(0, STATUS_BAR_LAYER_HEIGHT, b.size.w, b.size.h - STATUS_BAR_LAYER_HEIGHT)));
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
  char buff[50];
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
  // Sprache der Uhr uebernehmen, bevor das erste Fenster Texte holt
  strings_refresh();
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
