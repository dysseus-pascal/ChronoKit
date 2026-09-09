# ChronoKit

Zeitmess-App für Pebble. Kombiniert die beiden
offiziellen Pebble-Apps von Core Devices in einer App, erreichbar über ein
kleines Startmenü:

- **Stoppuhr** — Port von [coredevices/pebble-stopwatch](https://github.com/coredevices/pebble-stopwatch)
- **Timer** — Port von [coredevices/pebble-timer](https://github.com/coredevices/pebble-timer)

## Unterstützte Geräte

| Plattform | Display | Besonderheit |
|---|---|---|
| `emery` | 200×228, Farbe, eckig | Referenzplattform (Pebble Time 2) |
| `flint` | 144×168, **schwarz-weiss**, eckig | Theme fällt auf Schwarz/Weiss zurück, halbes Speicherbudget |
| `gabbro` | 260×260, Farbe, **rund** | Auswahl mittig, Fortschritt als radiale Füllung |

## Bilder

### emery (200×228, Farbe, eckig)

| Startmenü | Stoppuhr | Pausiert | Timer stellen |
|:--:|:--:|:--:|:--:|
| ![Startmenü auf emery](screenshots/emery/01-startmenue.png) | ![Stoppuhr auf emery](screenshots/emery/02-stoppuhr-runden.png) | ![Pausiert auf emery](screenshots/emery/03-stoppuhr-pausiert.png) | ![Timer stellen auf emery](screenshots/emery/04-timer-stellen.png) |

| Timer-Detail | Timer-Liste | Alarm |
|:--:|:--:|:--:|
| ![Timer-Detail auf emery](screenshots/emery/05-timer-detail.png) | ![Timer-Liste auf emery](screenshots/emery/06-timer-liste.png) | ![Alarm auf emery](screenshots/emery/07-alarm.png) |

### flint (144×168, schwarz-weiss, eckig)

| Startmenü | Stoppuhr | Pausiert | Timer stellen |
|:--:|:--:|:--:|:--:|
| ![Startmenü auf flint](screenshots/flint/01-startmenue.png) | ![Stoppuhr auf flint](screenshots/flint/02-stoppuhr-runden.png) | ![Pausiert auf flint](screenshots/flint/03-stoppuhr-pausiert.png) | ![Timer stellen auf flint](screenshots/flint/04-timer-stellen.png) |

| Timer-Detail | Timer-Liste | Alarm |
|:--:|:--:|:--:|
| ![Timer-Detail auf flint](screenshots/flint/05-timer-detail.png) | ![Timer-Liste auf flint](screenshots/flint/06-timer-liste.png) | ![Alarm auf flint](screenshots/flint/07-alarm.png) |

Auf flint ist die Fortschrittsfüllung weiss, sonst stünde die schwarze Zeit auf
schwarzem Grund. Sichtbar bleibt der Fortschritt durch die Linie an der Füllkante,
die auf Farbgeräten zusätzlich die Grenze zwischen den Grüntönen schärft.

### gabbro (260×260, Farbe, rund)

| Startmenü | Stoppuhr | Pausiert | Timer stellen |
|:--:|:--:|:--:|:--:|
| ![Startmenü auf gabbro](screenshots/gabbro/01-startmenue.png) | ![Stoppuhr auf gabbro](screenshots/gabbro/02-stoppuhr-runden.png) | ![Pausiert auf gabbro](screenshots/gabbro/03-stoppuhr-pausiert.png) | ![Timer stellen auf gabbro](screenshots/gabbro/04-timer-stellen.png) |

| Timer-Detail | Timer-Liste | Alarm |
|:--:|:--:|:--:|
| ![Timer-Detail auf gabbro](screenshots/gabbro/05-timer-detail.png) | ![Timer-Liste auf gabbro](screenshots/gabbro/06-timer-liste.png) | ![Alarm auf gabbro](screenshots/gabbro/07-alarm.png) |

Alle Aufnahmen stammen aus dem Emulator in nativer Auflösung der jeweiligen
Plattform, aufgenommen mit demselben Ablauf und denselben Runden-Abständen.

## Bedienung

**Stoppuhr** (mintgrüner Screen, LECO-Ziffern, Action-Bar rechts)
- Mitte: Start / Pause
- Läuft: Oben = Runde, Unten = Reset
- Pausiert mit vielen Runden: Oben/Unten scrollt durch die Rundenliste
- Zeit skaliert automatisch; bis 22 Runden, Zustand bleibt beim Schliessen erhalten
- Die Rundenliste zeigt **Zwischenzeiten**: jede Zeile ist die Dauer seit der
  vorherigen Runde, nicht die Gesamtzeit. Die Werte steigen daher nicht an,
  neueste Runde steht oben. Die Gesamtzeit steht gross darüber.

**App Glance** (Eintrag im Launcher der Uhr)
- Der Launcher baut ihn einmal beim Beenden: laufender Timer zuerst, dann die
  Stoppuhr. Vorher schrieb jedes Modul den Glance selbst und überschrieb dabei
  den Eintrag des anderen (`app_glance_reload` löscht zuerst alle Einträge).
- Beide Module liefern nur den Text (`stopwatch_get_glance`,
  `timer_app_get_glance`), zusammengesetzt wird er in `chronokit.c`.

**Timer** (Mehrfach-Timer wie im Original)
- Liste mit «+» zum Anlegen; pro Timer Fortschrittsbalken und Play/Pause-Symbol
- «Timer stellen»: Stunden/Minuten/Sekunden-Felder (Oben/Unten ändern, Mitte weiter);
  kürzeste Dauer 1 Sekunde, mit 0 bricht man ab
- Detailansicht: grüne Füllung zeigt den Fortschritt; Action-Bar: Stift = Bearbeiten,
  Play/Pause, Papierkorb = Löschen
- Bei Ablauf: Vibration + animiertes «Zeit ist um!»-Popup mit Schlummern (1 Min)
  und Verwerfen — auch bei geschlossener App (Wakeup)
- Bis 8 Timer; Timer ≥ 15 Min erzeugen einen Timeline-Pin (via Handy-App)

## Farbschema (Grün)

Alle Farben sind in `src/c/theme.h` zentralisiert:

- `ZM_COLOR_ACCENT` = GColorJaegerGreen (#00AA55): Menü-Hervorhebung und aktives Eingabefeld
- `ZM_COLOR_FILL` = GColorJaegerGreen: grossflächige Füllungen, die fremde Schrift tragen,
  also der Fortschritt im Timer-Detail und der Hintergrund der Popups
- `ZM_COLOR_SURFACE` = GColorMintGreen (#AAFFAA): getönte Flächen (Stoppuhr, oberer Teil des Timer-Details)
- `ZM_COLOR_ON_ACCENT` / `ZM_COLOR_ON_SURFACE` = `gcolor_legible_over(...)`: Schriftfarbe darauf

`ACCENT` und `FILL` sind auf Farbgeräten identisch und unterscheiden sich nur im
Schwarz-Weiss-Rückfall: `ACCENT` wird zu Schwarz, weil seine Schrift über
`ON_ACCENT` mitzieht, `FILL` dagegen zu Weiss, weil die Schrift darauf immer
schwarz ist. Genau daran krankte der erste Anlauf für flint.

Andere Grüntöne? Nur die beiden Defines ändern (Pebble-Palette: IslamicGreen, MayGreen, DarkGreen,
ScreaminGreen, MediumSpringGreen ...). Icons, PDC-Animationen und App-Icon sind rein schwarz/weiss
und brauchen keine Anpassung. Der Timeline-Pin (src/pkjs/index.js) nutzt ebenfalls #00AA55.

## Struktur

- `src/c/theme.h` — zentrale Farbpalette (siehe Farbschema oben)
- `src/c/common.h` — gemeinsame Zeiteinheiten, vorher in vier Dateien einzeln definiert
- `src/c/chronokit.c` — Launcher-Menü, bindet beide Module ein
- `src/c/stopwatch.c`, `rendering.*` — offizielle Stoppuhr (angepasst: kein eigenes `main`,
  Rundenzeit als einfache Differenz statt Modulo-Rechnung, `WindowData` wird beim
  Anlegen genullt, und `prv_get_epoch_ms` unterdrückt kleine Rückwärtssprünge:
  `time_ms()` liest Sekunden und Millisekunden nicht atomar, wodurch die Anzeige
  gelegentlich um knapp eine Sekunde zurücksprang)
- `src/c/timer_app.c` (ehem. `main.c`) + `menu/detail/setting/popup_window.*`,
  `selection_layer.*`, `countdown_timer.*`, `phone.*` — offizielle Timer-App
  (angepasst: Timer-Liste wird erst beim Öffnen gepusht, deutsche Texte, und die
  kürzeste Timerdauer von 5 auf 1 Sekunde gesenkt -- kürzere Eingaben wurden
  vorher wortlos verworfen)
- `resources/` — LECO-Fonts, Action-Bar-Icons, PDC-Animationen aus den Originalen

Der übernommene Code wurde in Version 1.1.2 um rund 40 % gekürzt, ohne das Verhalten zu
ändern: Zweige für nicht unterstützte Plattformen (aplite, basalt, chalk, diorite) und
immer wahre SDK-Bedingungen entfernt, ungenutzte Funktionen und Felder gestrichen,
doppelte Konstanten in `common.h` zusammengeführt, mehrfach kopierte Codepfade
(z. B. vier fast identische Animations-Konstruktoren) zu je einem Helfer verschmolzen,
und die seitenlangen Kommentarköpfe der Originale auf Einzeiler reduziert. Geprüft durch
Builds auf allen drei Plattformen, einen Bildvergleich gegen den vorherigen Stand und
eine unabhängige Durchsicht des Diffs mit Gegenprüfung jedes Befunds.

## Build (WSL, siehe auch Projekt-Memory)

```powershell
Copy-Item -Recurse -Force ".\src",".\resources",".\package.json" "\\wsl.localhost\Ubuntu\home\<wsl-benutzer>\chronokit\"
wsl -d Ubuntu -u <wsl-benutzer> -e sh -c "export PATH=`$HOME/.local/bin:`$PATH; cd ~/chronokit && pebble build && pebble install --emulator emery"
```

## Auf die echte Uhr

Pebble-App auf dem Handy → Developer Connection aktivieren, dann:

```powershell
wsl -d Ubuntu -u <wsl-benutzer> -e sh -c "export PATH=`$HOME/.local/bin:`$PATH; cd ~/chronokit && pebble install --phone <IP-DER-PEBBLE-APP>"
```

Werkzeuge: pebble-tool 5.0.40, SDK 4.33.1 (Python 3.13 via uv).

## Herkunft und Lizenz

Der Grossteil des C-Codes stammt aus den beiden offiziellen Apps (seit 1.1.2 gekuerzt und
bereinigt, siehe Abschnitt Struktur; die Logik ist unveraendert):

- `stopwatch.c`, `rendering.*` aus [coredevices/pebble-stopwatch](https://github.com/coredevices/pebble-stopwatch)
- `timer_app.c` (dort `main.c`), `menu/detail/setting/popup_window.*`, `selection_layer.*`,
  `countdown_timer.*`, `phone.*` sowie alle Dateien unter `resources/` aus
  [coredevices/pebble-timer](https://github.com/coredevices/pebble-timer), urspruenglich
  von Eric Phillips

Eigene Anteile: `chronokit.c` (Launcher), `theme.h` (Farbpalette), `common.h`, die
deutschen Texte, die im Abschnitt Struktur genannten Korrekturen und die Kürzung und
Bereinigung ab 1.1.2. Der gekürzte Code bleibt eine Bearbeitung der Originale.

**Lizenzlage:** Beide Quell-Repositories sind ohne Lizenzdatei veroeffentlicht. Eine
ausdrueckliche Nutzungsrechtseinraeumung fehlt daher, und dieses Repository kann fuer den
uebernommenen Code folglich auch keine Lizenz vergeben. Es liegt als oeffentliches
Repository auf GitHub, wo die Nutzungsbedingungen (Abschnitt D.5) das Ansehen und Forken
oeffentlicher Repositories abdecken. Wer den Code darueber hinaus verwenden will, sollte
sich an Core Devices wenden.
