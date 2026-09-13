// Alle Texte der Oberflaeche, eine Zeile je Text.
//
// ACHTUNG, ZWEI DINGE SIND ABSICHT:
//  1. KEIN #pragma once und keine Include-Waechter. Diese Datei wird MEHRFACH
//     eingebunden (X-Makro): einmal fuer die Aufzaehlung der Schluessel in
//     strings.h und einmal fuer die Tabelle in strings.c. Ein Waechter wuerde
//     die zweite Einbindung verschlucken und eine leere Tabelle erzeugen.
//  2. Endung .h, obwohl es kein gewoehnlicher Header ist. Sie hiess frueher
//     .def; Build-Umgebungen, die nur .c und .h in ihren Baum kopieren, haben
//     sie dann nicht gefunden ("strings.def: No such file or directory").
//
//   STR(schluessel, maxbytes, en, de)
//
//   maxbytes  Groesse des Zielpuffers in BYTES, 0 wenn der Text in keinen
//             festen Puffer kopiert wird. Umlaute zaehlen als zwei Bytes.
//             tools/strings_check.js prueft diese Grenze.
//   en        Englisch. Spalte 0 und zugleich der Rueckfall fuer jede Sprache,
//             die hier keine eigene Spalte hat. Fuer die uebernommenen Apps
//             (Stoppuhr, Timer) stehen hier woertlich die Originaltexte von
//             Core Devices, damit ein englisch eingestelltes ChronoKit in
//             diesen Screens ausgabegleich mit dem Original bleibt.
//   de        Deutsch.
//
// EINE SPRACHE ERGAENZEN: in strings.h die Aufzaehlung StringLang und
// STRINGS_LANG_COUNT erweitern, in strings.c prv_pick_language() den
// Zwei-Buchstaben-Vergleich ergaenzen, hier eine Spalte anfuegen und in
// strings.c die Tabellenzeile um sie erweitern. Fehlt die Spalte in auch nur
// einer Zeile, ist das ein Praeprozessorfehler - kein stiller Rueckfall.
//
// ACHTUNG: kein uebersetzter Text darf je an rendering_draw_text gehen. Die
// LECO-Pfade dort kennen nur 0-9, ':' und '.' und ueberspringen alles andere
// kommentarlos.

// ---- Startmenue -----------------------------------------------------------
STR(STR_LAUNCHER_STOPWATCH,      0,   "Stopwatch",         "Stoppuhr")
STR(STR_LAUNCHER_STOPWATCH_SUB,  0,   "Laps & splits",     "Runden & Zwischenzeit")
STR(STR_LAUNCHER_TIMER,          0,   "Timer",             "Timer")
STR(STR_LAUNCHER_TIMER_SUB,      0,   "Countdown w/ alarm", "Countdown mit Alarm")
STR(STR_LAUNCHER_TIMEZONE,       0,   "Time zone",         "Zeitzone")

// ---- Timer (Port) ---------------------------------------------------------
STR(STR_TIMER_LIST_EMPTY,        0,   "No timers",         "Keine Timer")
STR(STR_SETTING_TITLE,           0,   "Set timer",         "Timer stellen")
// Steht vor der formatierten Endzeit. Bewusst NICHT im strftime-Format, damit
// eine laengere Uebersetzung dort nichts abschneiden kann.
STR(STR_SETTING_END,             12,  "End:",              "Ende:")
STR(STR_POPUP_TIME_UP,           0,   "Time's up!",        "Zeit ist um!")
STR(STR_POPUP_TIMER_DELETED,     0,   "Timer deleted",     "Timer gelöscht")

// ---- Zeitzone: Untertitel der Menuezeile (Puffer s_tz_sub[32]) ------------
STR(STR_TZ_SUB_ZONE_UNKNOWN,     32,  "Zone unknown",      "Zone unbekannt")
STR(STR_TZ_SUB_NO_HOME,          32,  "Set home time",     "Heimatzeit merken")
STR(STR_TZ_SUB_AT_HOME_FMT,      32,  "Home: %s",          "Daheim: %s")

// ---- Zeitzone: Bildschirm -------------------------------------------------
STR(STR_TZ_NO_ZONE,              32,  "No zone",           "Keine Zone")
STR(STR_TZ_AT_HOME,              0,   "home",              "daheim")
STR(STR_TZ_TOMORROW,             0,   "tomorrow",          "morgen")
STR(STR_TZ_YESTERDAY,            0,   "yesterday",         "gestern")
STR(STR_TZ_NO_TIMEZONE_MSG,      110, "The watch has no time zone from the phone yet. Saving is disabled.", "Die Uhr hat noch keine Zeitzone vom Telefon. Merken ist gesperrt.")
STR(STR_TZ_NO_HOME_MSG,          110, "No home time. Select saves %s as home.", "Keine Heimatzeit. Mitteltaste merkt %s als Heimat.")

// ---- Zeitzone: Hinweise (zwei Sekunden, unterer Bereich) ------------------
STR(STR_TZ_HINT_HOME_SAVED,      0,   "Home saved",        "Heimat gemerkt")
STR(STR_TZ_HINT_HOME_UPDATED,    0,   "Home updated",      "Heimat aktualisiert")
STR(STR_TZ_HINT_HOME_DELETED,    0,   "Home deleted",      "Heimat gelöscht")
STR(STR_TZ_HINT_LONG_REPLACE,    0,   "Hold to replace",   "Lang drücken zum Ersetzen")
STR(STR_TZ_HINT_LONG_DELETE,     0,   "Hold to delete",    "Lang drücken zum Löschen")
STR(STR_TZ_HINT_FIX_PLUS,        0,   "Home +1 h",         "Daheim +1 h")
STR(STR_TZ_HINT_FIX_MINUS,       0,   "Home -1 h",         "Daheim -1 h")
STR(STR_TZ_HINT_FIX_OFF,         0,   "Correction off",    "Korrektur aus")

// ---- Zeitzone: Kuerzel ----------------------------------------------------
// clock_is_24h_style() entscheidet, ob diese ueberhaupt erscheinen.
STR(STR_AM,                      0,   "AM",                "AM")
STR(STR_PM,                      0,   "PM",                "PM")

// Wochentage. MUESSEN zusammenhaengend und in dieser Reihenfolge stehen: der
// Code greift mit STR_WD_SUN + tm_wday zu. strings.c prueft das beim Bauen.
// Nur zwei Buchstaben - auf emery und gabbro sind dafuer rund 26 px da.
STR(STR_WD_SUN,                  0,   "Su",                "So")
STR(STR_WD_MON,                  0,   "Mo",                "Mo")
STR(STR_WD_TUE,                  0,   "Tu",                "Di")
STR(STR_WD_WED,                  0,   "We",                "Mi")
STR(STR_WD_THU,                  0,   "Th",                "Do")
STR(STR_WD_FRI,                  0,   "Fr",                "Fr")
STR(STR_WD_SAT,                  0,   "Sa",                "Sa")
