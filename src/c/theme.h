#pragma once
#include <pebble.h>

// Zentrale Farbpalette der App (einheitliches Gruen-Schema).
//
//  ACCENT          Hervorhebungen und aktives Eingabefeld. Die Schrift darauf
//                  kommt aus ON_ACCENT, deshalb ist Schwarz als S/W-Rueckfall
//                  hier richtig.
//  FILL            grossflaechige Fuellungen, die fremde Schrift tragen:
//                  Fortschritt im Timer-Detail und Hintergrund der Popups.
//                  Diese Schrift ist immer schwarz, daher muss der S/W-
//                  Rueckfall Weiss sein, sonst steht Schwarz auf Schwarz.
//                  Auf Farbgeraeten identisch mit ACCENT.
//  SURFACE         getoente Flaechen: Stoppuhr, oberer Teil des Timer-Details,
//                  Fortschritts-Spur in der Timer-Liste, inaktive Eingabefelder
//  ON_ACCENT/      Schriftfarbe auf ACCENT bzw. SURFACE. gcolor_legible_over
//  ON_SURFACE      waehlt Schwarz oder Weiss passend zur Helligkeit, damit alle
//                  Screens dieselbe Wahl treffen.
//  FIELD_INACTIVE  inaktive Felder im Screen "Timer stellen"
//
// AENDERN (anderer Gruenton):
//  1. Nur die beiden Defines ACCENT und SURFACE anpassen; die ON_*-Tokens und
//     alle Screens folgen automatisch.
//  2. SURFACE muss hell bleiben (lange Lesezeit, grosse schwarze Ziffern),
//     ACCENT mittel bis dunkel: die PDC-Animationen des Popups haben weisse
//     Fuellungen, die auf einer hellen Flaeche verschwinden wuerden.
//  3. Den Hex-Wert von ACCENT auch in src/pkjs/index.js (backgroundColor des
//     Timeline-Pins) nachziehen - das ist eine handgepflegte Kopie.
//  4. Danach nach WSL synchronisieren und neu bauen (siehe README).
//
// Pebble-Palette (Auswahl Gruen): JaegerGreen #00AA55, MintGreen #AAFFAA,
// IslamicGreen #00AA00, MayGreen #55AA55, DarkGreen #005500, KellyGreen #55AA00,
// ScreaminGreen #55FF55, MediumSpringGreen #00FFAA, MediumAquamarine #55FFAA.
#define ZM_COLOR_ACCENT         PBL_IF_COLOR_ELSE(GColorJaegerGreen, GColorBlack)
#define ZM_COLOR_FILL           PBL_IF_COLOR_ELSE(GColorJaegerGreen, GColorWhite)
#define ZM_COLOR_SURFACE        PBL_IF_COLOR_ELSE(GColorMintGreen, GColorWhite)
#define ZM_COLOR_ON_ACCENT      gcolor_legible_over(ZM_COLOR_ACCENT)
#define ZM_COLOR_ON_SURFACE     gcolor_legible_over(ZM_COLOR_SURFACE)
#define ZM_COLOR_FIELD_INACTIVE PBL_IF_COLOR_ELSE(GColorMintGreen, GColorDarkGray)
