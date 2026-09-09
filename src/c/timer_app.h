#pragma once
#include <pebble.h>

// Eingebettete Variante der offiziellen Pebble-Timer-App (Core Devices).
void timer_app_init(void);
void timer_app_deinit(void);
void timer_app_open(void);
bool timer_app_handle_launch(void);

// Liefert den Text fuer den App-Glance-Eintrag des Timers.
// false, wenn kein Timer laeuft und der letzte zu lange her ist.
bool timer_app_get_glance(char *buff_glance, size_t size, time_t *expiration_time);
