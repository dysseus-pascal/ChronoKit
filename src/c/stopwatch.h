#pragma once
#include <pebble.h>

void stopwatch_window_push(void);

// Liefert den Text fuer den App-Glance-Eintrag der Stoppuhr.
// false, wenn die Stoppuhr zurueckgesetzt ist und nichts anzuzeigen ist.
bool stopwatch_get_glance(char *buff_glance, size_t size, time_t *expiration_time);
