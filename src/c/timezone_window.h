#pragma once
#include <pebble.h>

// Zeitzonen-Screen: zeigt die aktuell gesetzten Werte der Uhr und merkt sich
// auf Wunsch die jetzige Zone als Heimatzeit.
//
// Fenster erstellen und anzeigen. Es raeumt sich beim Schliessen selbst ab
// (Muster stopwatch.c), ein timezone_window_destroy gibt es daher nicht.
void timezone_window_push(void);

// Fuellt buff mit dem Untertitel der Launcher-Zeile. Liest nur persist und
// localtime und braucht kein offenes Fenster (Muster stopwatch_get_glance).
void timezone_get_launcher_subtitle(char *buff, size_t size);
