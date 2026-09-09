#pragma once

// Gemeinsame Zeiteinheiten. Vorher in vier Dateien einzeln definiert.
#define MSEC_IN_SEC 1000
#define MSEC_IN_MIN 60000
#define MSEC_IN_HR  3600000
#define MIN_IN_HR   60
#define HR_IN_DAY   24

// Kuerzeste zulaessige Timerdauer. Das Original verlangte 5 s und verwarf
// kuerzere Eingaben wortlos. 1 s genuegt; 0 s bleibt der Abbruchweg, wenn man
// den Einstellscreen ohne Eingabe durchklickt. Gilt fuer timer_app.c (Anlegen)
// und setting_window.c (Anzeige der Endzeit) gleichermassen.
#define TIMER_MIN_LENGTH 1000
// Ab dieser Dauer bekommt ein Timer einen Timeline-Pin (15 Minuten).
#define TIMELINE_MIN_LENGTH 900000
