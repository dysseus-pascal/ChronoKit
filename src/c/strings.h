#pragma once
#include <pebble.h>

// Texte der Oberflaeche. Die Uhr gibt die Sprache vor
// (Settings -> Display -> Language); die App folgt ihr, es gibt keinen eigenen
// Sprachschalter. Alle Texte stehen in strings_table.h, eine Zeile je Text.
//
// Rueckfall ist ENGLISCH: die Pebble Time 2 kennt eingebaute Sprachen, fuer
// die wir keine Spalte haben (Catala, Nederlands, Portugues, Polski und
// weitere). Eine deutsche Oberflaeche auf einer polnischen Uhr waere
// schlechter als eine englische.
//
// Die Reihenfolge ist zugleich die Nummer, die als KEY_LANG ans Telefon geht
// (src/pkjs/index.js waehlt damit die Sprache des Pins). Neue Sprachen kommen
// deshalb nur HINTEN dazu - sonst spraeche eine alte Telefonseite mit einer
// neuen Uhr die falsche Sprache.

typedef enum {
  STRINGS_EN = 0,   //< Spalte 0, zugleich der Rueckfall
  STRINGS_DE,       //< 1
  STRINGS_FR,       //< 2
  STRINGS_IT,       //< 3
  STRINGS_ES,       //< 4
  STRINGS_LANG_COUNT,
} StringLang;

typedef enum {
#define STR(id, maxbytes, en, de, fr, it, es) id,
#include "strings_table.h"
#undef STR
  STR_COUNT,
} StringId;

// Liefert den Text in der aktuellen Sprache. Nie NULL: ein unbekannter
// Schluessel und eine fehlende Spalte fallen auf Englisch zurueck, eine
// fehlende englische Spalte auf den leeren String.
const char *S(StringId id);

// Sprache neu von der Uhr lesen. i18n_get_system_locale() ist ein Syscall und
// es gibt kein Ereignis fuer einen Sprachwechsel - also einmal beim Start und
// danach dort, wo ohnehin neu aufgebaut wird.
void strings_refresh(void);

// Fuer die wenigen Stellen, die sich sprachabhaengig anders verhalten muessen
// (etwa die Uebersetzung von Ortsnamen, die auf Englisch falsch waere, oder
// die Reihenfolge im Datum).
StringLang strings_language(void);
