#include <pebble.h>
#include "rendering.h"
#include "theme.h"
#include "timezone_window.h"

// Zeitzone: oben die AKTUELL GESETZTEN Werte der Uhr (Datum, UTC-Versatz,
// Ortszeit gross in LECO, Ort), unten die gemerkte Heimatzeit.
//
// Rechenweg (bei jedem Zeichnen frisch, nichts wird zwischengespeichert):
//   time(NULL)                 -> UTC, nie Ortszeit
//   localtime()->tm_gmtoff     -> Sekunden oestlich UTC, Sommerzeit BEREITS
//                                 enthalten (tm_isdst ist nur ein Indikator
//                                 und wird niemals aufaddiert)
//   gmtime(utc + heimversatz)  -> die Uhrzeit daheim. Aus diesem struct tm
//                                 duerfen nur die Kalenderfelder benutzt
//                                 werden; tm_gmtoff ist dort 0 und tm_zone
//                                 "UTC", beides wuerde luegen.
// localtime und gmtime liefern Zeiger auf denselben statischen Puffer - jedes
// Ergebnis wird deshalb sofort in eine eigene struct tm kopiert.
//
// Sommerzeit: gespeichert wird der effektive Versatz im Moment des Merkens.
// Steht man wieder in der Heimatzone (gleicher Rohname), zieht prv_refresh den
// Wert stillschweigend nach - daheim stimmt es also immer. Nur wenn der
// Umstellungstermin in eine laufende Reise faellt, friert er ein; dagegen gibt
// es die Handkorrektur +/- 1 h (Zeichen "*") und ab 90 Tagen ein "?".
// Kennt die Firmware die Region nicht, liefert sie statt "Europe/Zurich" den
// Ersatznamen "UTC+2". Der aendert sich bei der Zeitumstellung mit, und dann
// erkennt die App die Heimat nicht wieder. Auf einer mit dem Telefon
// gekoppelten Uhr kommt der Regionsname; im Emulator immer der Ersatzname.

#define PERSIST_TZ_VERSION_KEY     200
#define PERSIST_TZ_VERSION         1
#define PERSIST_TZ_HOME_NAME_KEY   201   // voller Rueckgabewert von clock_get_timezone
#define PERSIST_TZ_HOME_OFFSET_KEY 202   // Sekunden oestlich UTC
#define PERSIST_TZ_SET_AT_KEY      203   // time_t des Merkens
#define PERSIST_TZ_FIX_KEY         204   // Handkorrektur in Minuten: -60, 0, 60

#define TZ_BORDER      PBL_IF_ROUND_ELSE(22, 6)
#define TZ_TOP         PBL_IF_ROUND_ELSE(26, 4)
// Rund bleibt unten ein Streifen frei: dort ist der Kreis schon so schmal,
// dass die Ziffern der Heimatzeit sonst angeschnitten wuerden.
#define TZ_BOTTOM      PBL_IF_ROUND_ELSE(28, 0)
#define TZ_HINT_MS     2000
#define TZ_STALE_SECS  (90 * 24 * 3600)
#define TZ_FIX_STEP    60                // Minuten
#define TZ_NIGHT_FROM  22
#define TZ_NIGHT_TO    7
#define TZ_LONG_MS     700

// Zusatz neben den Ziffern (AM/PM, gestern/morgen) immer klein - er muss sich
// die Breite mit den Ziffern teilen.
#define TZ_FONT_EXTRA  FONT_KEY_GOTHIC_14

#if defined(PBL_PLATFORM_GABBRO)
  // Rund: kleinere Ziffern als auf emery. Sie stehen weiter oben, wo der Kreis
  // noch schmal ist, und muessen zugleich links vom Rand und rechts von der
  // Aktionsleiste wegbleiben.
  #define TZ_HEADER_H       22
  #define TZ_GAP            6
  #define TZ_CITY_H         30
  #define TZ_HOME_H         62
  #define TZ_HOME_H_SHORT   30
  #define TZ_DIGIT_MAX      44
  #define TZ_HOME_DIGIT_MAX 26
  #define TZ_FONT_HEADER    FONT_KEY_GOTHIC_18
  #define TZ_FONT_CITY      FONT_KEY_GOTHIC_24_BOLD
  #define TZ_FONT_HOME      FONT_KEY_GOTHIC_18
  #define TZ_WEEKDAY        1
#elif defined(PBL_PLATFORM_EMERY)
  #define TZ_HEADER_H       22
  #define TZ_GAP            6
  #define TZ_CITY_H         30
  #define TZ_HOME_H         62
  #define TZ_HOME_H_SHORT   30
  #define TZ_DIGIT_MAX      48
  #define TZ_HOME_DIGIT_MAX 28
  #define TZ_FONT_HEADER    FONT_KEY_GOTHIC_18
  #define TZ_FONT_CITY      FONT_KEY_GOTHIC_24_BOLD
  #define TZ_FONT_HOME      FONT_KEY_GOTHIC_18
  #define TZ_WEEKDAY        1
#else
  #define TZ_HEADER_H       16
  #define TZ_GAP            4
  #define TZ_CITY_H         22
  #define TZ_HOME_H         50
  #define TZ_HOME_H_SHORT   22
  #define TZ_DIGIT_MAX      34
  #define TZ_HOME_DIGIT_MAX 20
  #define TZ_FONT_HEADER    FONT_KEY_GOTHIC_14
  #define TZ_FONT_CITY      FONT_KEY_GOTHIC_18_BOLD
  #define TZ_FONT_HOME      FONT_KEY_GOTHIC_14
  // flint ist zu schmal fuer Wochentag UND Versatz in einer Zeile, sobald der
  // Versatz einen Minutenanteil hat ("UTC+5:30"). Dort nur das Datum.
  #define TZ_WEEKDAY        0
#endif

// Der Ortsname kommt als Olson-String ("Europe/Zurich") von der Uhr, also
// englisch. Reine Kosmetik. Die Tabelle liegt im App-Abbild und zaehlt damit
// zum Speicherabdruck, belegt aber keinen Heap.
static const char *const s_city_de[][2] = {
  {"Zurich", "Zürich"}, {"Vienna", "Wien"}, {"Rome", "Rom"}, {"Athens", "Athen"},
  {"Brussels", "Brüssel"}, {"Copenhagen", "Kopenhagen"}, {"Moscow", "Moskau"},
  {"Lisbon", "Lissabon"}, {"Prague", "Prag"}, {"Warsaw", "Warschau"},
};

#if TZ_WEEKDAY
static const char *const s_weekday_de[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
#endif

// gemerkte Heimatzeit
typedef struct {
  char    name[TIMEZONE_NAME_LENGTH];   //< Rohname, so wie clock_get_timezone ihn liefert
  int32_t offset;                       //< Sekunden oestlich UTC beim Merken
  int32_t set_at;                       //< time_t des Merkens
  int32_t fix_min;                      //< Handkorrektur -60 / 0 / +60
  bool    valid;
} TzHome;

typedef enum {
  TzZoneUnknown,   //< die Uhr kennt gar keine Zeitzone
  TzNoHome,        //< Zone bekannt, nichts gemerkt
  TzAtHome,        //< gemerkte Zone == aktuelle Zone
  TzAway,          //< unterwegs
} TzState;

typedef struct {
  Layer          *drawing_layer;
  ActionBarLayer *action_bar;
  GBitmap        *icon_up;
  GBitmap        *icon_down;
  GBitmap        *icon_lap;      //< merken
  GBitmap        *icon_delete;   //< loeschen
  AppTimer       *hint_timer;
  const char     *hint;          //< Literal oder NULL
  TzHome          home;
} WindowData;

static WindowData *s_data = NULL;


// ---- Zeit und Namen -------------------------------------------------------

// aktueller Zonenname; true, wenn die Uhr ueberhaupt eine Zone kennt.
// Der Puffer muss TIMEZONE_NAME_LENGTH gross sein - der Regionsnamen-Pfad der
// Firmware ignoriert buffer_size und schreibt bis zu 32 Byte.
static bool prv_current_zone(char *buff, size_t size) {
  clock_get_timezone(buff, size);
  buff[size - 1] = '\0';
  return clock_is_timezone_set() && strcmp(buff, "---") != 0;
}

// Sekunden oestlich UTC, Sommerzeit eingeschlossen
static int32_t prv_current_offset(time_t utc) {
  return (int32_t)localtime(&utc)->tm_gmtoff;
}

// Anzeigename aus dem Olson-String: Teil hinter dem letzten '/', '_' zu ' ',
// bekannte Staedte eingedeutscht. Ohne '/' bleibt der String stehen - das ist
// der Rueckfall "UTC+2", den die Firmware liefert, wenn sie die Region nicht
// aufloesen kann.
static void prv_display_name(const char *raw, char *out, size_t size) {
  if (raw[0] == '\0' || strcmp(raw, "---") == 0) {
    strncpy(out, "Keine Zone", size);
    out[size - 1] = '\0';
    return;
  }
  const char *base = raw;
  for (const char *p = raw; *p; p++) {
    if (*p == '/') base = p + 1;
  }
  for (unsigned i = 0; i < ARRAY_LENGTH(s_city_de); i++) {
    if (strcmp(base, s_city_de[i][0]) == 0) {
      strncpy(out, s_city_de[i][1], size);
      out[size - 1] = '\0';
      return;
    }
  }
  strncpy(out, base, size);
  out[size - 1] = '\0';
  for (char *p = out; *p; p++) {
    if (*p == '_') *p = ' ';
  }
}

// "14:37" bzw. im 12-Stunden-Modus "02:37"; LECO kennt nur Ziffern und ':'
static void prv_format_clock(char *buff, size_t size, const struct tm *t) {
  int h = t->tm_hour;
  if (!clock_is_24h_style()) {
    h = h % 12;
    if (h == 0) h = 12;
  }
  snprintf(buff, size, "%02d:%02d", h, t->tm_min);
}

static const char *prv_ampm(const struct tm *t) {
  if (clock_is_24h_style()) return "";
  return t->tm_hour < 12 ? "AM" : "PM";
}

static void prv_format_utc(char *buff, size_t size, int32_t secs) {
  const int32_t a = secs < 0 ? -secs : secs;
  const int h = (int)(a / 3600), m = (int)((a % 3600) / 60);
  const char sign = secs < 0 ? '-' : '+';
  if (m == 0) snprintf(buff, size, "UTC%c%d", sign, h);
  else        snprintf(buff, size, "UTC%c%d:%02d", sign, h, m);
}

// "+7 h", "+5:30 h"; mark ist "*" (Handkorrektur), "?" (lange her) oder ""
static void prv_format_delta(char *buff, size_t size, int32_t secs, const char *mark) {
  const int32_t a = secs < 0 ? -secs : secs;
  const int h = (int)(a / 3600), m = (int)((a % 3600) / 60);
  const char sign = secs < 0 ? '-' : '+';
  if (m == 0) snprintf(buff, size, "%c%d h%s", sign, h, mark);
  else        snprintf(buff, size, "%c%d:%02d h%s", sign, h, m, mark);
}

// "gestern" / "morgen" / "". Erst das Jahr vergleichen, dann den Tag im Jahr -
// eine reine Differenz von tm_yday kippt sonst am Jahreswechsel ins Gegenteil.
static const char *prv_day_mark(const struct tm *home, const struct tm *here) {
  if (home->tm_year != here->tm_year) {
    return home->tm_year > here->tm_year ? "morgen" : "gestern";
  }
  if (home->tm_yday > here->tm_yday) return "morgen";
  if (home->tm_yday < here->tm_yday) return "gestern";
  return "";
}


// ---- Persist --------------------------------------------------------------

// Erst Existenz, dann Version, dann Inhalt pruefen (Muster countdown_timer.c).
// Faellt eine Pruefung durch, gilt "keine Heimat" - es wird nie geraten.
static bool prv_home_load(TzHome *h) {
  memset(h, 0, sizeof(*h));
  if (!persist_exists(PERSIST_TZ_VERSION_KEY) ||
      persist_read_int(PERSIST_TZ_VERSION_KEY) != PERSIST_TZ_VERSION ||
      !persist_exists(PERSIST_TZ_HOME_NAME_KEY) ||
      !persist_exists(PERSIST_TZ_HOME_OFFSET_KEY)) {
    return false;
  }
  persist_read_string(PERSIST_TZ_HOME_NAME_KEY, h->name, sizeof(h->name));
  h->name[sizeof(h->name) - 1] = '\0';
  h->offset  = persist_read_int(PERSIST_TZ_HOME_OFFSET_KEY);
  h->set_at  = persist_read_int(PERSIST_TZ_SET_AT_KEY);
  h->fix_min = persist_read_int(PERSIST_TZ_FIX_KEY);
  const bool sane = h->name[0] != '\0' && strcmp(h->name, "---") != 0 &&
                    h->offset >= -14 * 3600 && h->offset <= 14 * 3600 &&
                    (h->fix_min == -TZ_FIX_STEP || h->fix_min == 0 ||
                     h->fix_min == TZ_FIX_STEP);
  if (!sane) {
    memset(h, 0, sizeof(*h));
    return false;
  }
  h->valid = true;
  return true;
}

// Version zuletzt. Das schuetzt den ERSTEN Datensatz: ein Abbruch mittendrin
// hinterlaesst keine gueltige Version. Beim Ueberschreiben eines bestehenden
// Satzes steht die alte Version schon da - dagegen hilft nur die
// Plausibilitaetspruefung beim Lesen.
static void prv_home_save(const TzHome *h) {
  persist_write_string(PERSIST_TZ_HOME_NAME_KEY, h->name);
  persist_write_int(PERSIST_TZ_HOME_OFFSET_KEY, h->offset);
  persist_write_int(PERSIST_TZ_SET_AT_KEY, h->set_at);
  persist_write_int(PERSIST_TZ_FIX_KEY, h->fix_min);
  persist_write_int(PERSIST_TZ_VERSION_KEY, PERSIST_TZ_VERSION);
}

static void prv_home_clear(void) {
  persist_delete(PERSIST_TZ_VERSION_KEY);
  persist_delete(PERSIST_TZ_HOME_NAME_KEY);
  persist_delete(PERSIST_TZ_HOME_OFFSET_KEY);
  persist_delete(PERSIST_TZ_SET_AT_KEY);
  persist_delete(PERSIST_TZ_FIX_KEY);
}

static TzState prv_state(const TzHome *h, const char *zone_now, bool zone_known) {
  if (!zone_known) return TzZoneUnknown;
  if (!h->valid) return TzNoHome;
  return strcmp(zone_now, h->name) == 0 ? TzAtHome : TzAway;
}


// ---- Launcher-Untertitel --------------------------------------------------

void timezone_get_launcher_subtitle(char *buff, size_t size) {
  char zone[TIMEZONE_NAME_LENGTH];
  const bool known = prv_current_zone(zone, sizeof(zone));
  TzHome h;
  prv_home_load(&h);
  char city[TIMEZONE_NAME_LENGTH];

  switch (prv_state(&h, zone, known)) {
    case TzZoneUnknown:
      strncpy(buff, "Zone unbekannt", size);
      buff[size - 1] = '\0';
      return;
    case TzNoHome:
      strncpy(buff, "Heimatzeit merken", size);
      buff[size - 1] = '\0';
      return;
    case TzAtHome:
      prv_display_name(h.name, city, sizeof(city));
      snprintf(buff, size, "Daheim: %s", city);
      return;
    default: {
      const time_t shifted = time(NULL) + (time_t)(h.offset + h.fix_min * 60);
      struct tm ht;
      memcpy(&ht, gmtime(&shifted), sizeof(ht));
      char hhmm[8];
      prv_format_clock(hhmm, sizeof(hhmm), &ht);
      prv_display_name(h.name, city, sizeof(city));
      const char *ap = prv_ampm(&ht);
      snprintf(buff, size, "%s %s%s%s", city, hhmm, ap[0] ? " " : "", ap);
      return;
    }
  }
}


// ---- Zeichnen -------------------------------------------------------------

static int16_t prv_text_w(const char *text, const char *font_key, int16_t max_w) {
  if (!text || !text[0]) return 0;
  return graphics_text_layout_get_content_size(text, fonts_get_system_font(font_key),
             GRect(0, 0, max_w, 40), GTextOverflowModeTrailingEllipsis,
             GTextAlignmentLeft).w;
}

// Zwei Texte in einer Zeile: links und rechts buendig. Die rechte Seite wird
// gemessen und die linke auf den Rest begrenzt - sonst schieben sich ein
// langer Ortsname und der Versatz auf flint uebereinander.
static void prv_draw_pair(GContext *ctx, const char *left, const char *right,
                          const char *font_key, GRect box) {
  GFont f = fonts_get_system_font(font_key);
  const int16_t rw = prv_text_w(right, font_key, box.size.w);
  if (rw) {
    graphics_draw_text(ctx, right, f, box, GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentRight, NULL);
  }
  GRect lb = box;
  lb.size.w = box.size.w - (rw ? rw + 6 : 0);
  if (lb.size.w > 0) {
    graphics_draw_text(ctx, left, f, lb, GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft, NULL);
  }
}

// LECO-Ziffern, Groesse aus der verfuegbaren Breite gerechnet (Muster Stoppuhr).
// left_x >= 0 zeichnet linksbuendig ab left_x, sonst mittig um center_x.
// min_size ist die Untergrenze: lieber den Zusatz daneben kuerzen als die
// Uhrzeit unleserlich klein machen.
// Fuell- UND Strichfarbe muessen vorher stehen. Liefert die benutzte Groesse,
// damit der Aufrufer genau so weit weiterruecken kann.
static int16_t prv_draw_digits(GContext *ctx, char *text, size_t size, int16_t y,
                               int16_t max_size, int16_t min_size, int16_t avail_w,
                               int16_t center_x, int16_t left_x) {
  // Negative Restbreite darf nicht in den vorzeichenlosen Cast laufen - daraus
  // wuerde die groesste statt der kleinsten Schrift.
  if (avail_w < 0) avail_w = 0;
  const uint32_t ref = rendering_get_size(text, 100);
  int32_t fs = ref ? (int32_t)((uint32_t)100 * (uint32_t)avail_w / ref) : max_size;
  if (fs > max_size) fs = max_size;
  if (fs < min_size) fs = min_size;
  const int16_t w = (int16_t)rendering_get_size(text, (uint8_t)fs);
  const int16_t x = (left_x >= 0) ? left_x : (int16_t)(center_x - w / 2);
  rendering_draw_text(ctx, text, (uint8_t)size, (uint16_t)fs, GPoint(x, y));
  return (int16_t)fs;
}

static void prv_layer_draw(Layer *layer, GContext *ctx) {
  WindowData *data = *(WindowData**)layer_get_data(layer);
  const GRect b = layer_get_bounds(layer);
  const int16_t content_w = b.size.w - ACTION_BAR_WIDTH;
  const int16_t text_w = content_w - 2 * TZ_BORDER;
  const GTextAlignment align = PBL_IF_RECT_ELSE(GTextAlignmentLeft, GTextAlignmentCenter);
  // Eckig: der Inhalt steht links neben der Aktionsleiste, linksbuendig.
  // Rund: alles auf die BILDMITTE zentrieren. Wuerde man wie eckig auf die
  // Inhaltsbreite zentrieren, saesse der Block um die halbe Aktionsleiste zu
  // weit links - genau dort schneidet der Kreis die grossen Ziffern an. Die
  // halbe Kastenbreite reicht von der Mitte bis kurz vor die Aktionsleiste,
  // damit rechts nichts darunter laeuft.
  const int16_t cx = PBL_IF_ROUND_ELSE(b.size.w / 2, content_w / 2);
  const int16_t half = PBL_IF_ROUND_ELSE(content_w - b.size.w / 2 - 6, text_w / 2);
  const int16_t box_x = cx - half;
  const int16_t box_w = 2 * half;
  const int16_t bottom = b.size.h - TZ_BOTTOM;

  char zone[TIMEZONE_NAME_LENGTH];
  const bool known = prv_current_zone(zone, sizeof(zone));
  const TzState state = prv_state(&data->home, zone, known);

  const time_t utc = time(NULL);
  struct tm here;
  memcpy(&here, localtime(&utc), sizeof(here));
  const int32_t here_off = here.tm_gmtoff;
  const char *here_ampm = prv_ampm(&here);

  graphics_context_set_text_color(ctx, ZM_COLOR_ON_SURFACE);

  // Kopfzeile: Datum links, UTC-Versatz rechts (rund: eine zentrierte Zeile).
  // Ohne gesetzte Zone bleibt der Versatz weg - er waere 0 und wuerde luegen.
  int16_t y = TZ_TOP;
  char date[16];
#if TZ_WEEKDAY
  snprintf(date, sizeof(date), "%s %02d.%02d.", s_weekday_de[here.tm_wday % 7],
           here.tm_mday, here.tm_mon + 1);
#else
  snprintf(date, sizeof(date), "%02d.%02d.", here.tm_mday, here.tm_mon + 1);
#endif
  char utcs[12] = "";
  if (known) prv_format_utc(utcs, sizeof(utcs), here_off);
#ifdef PBL_ROUND
  char head[40];
  snprintf(head, sizeof(head), "%s%s%s%s%s", date, known ? "  " : "", utcs,
           here_ampm[0] ? "  " : "", here_ampm);
  graphics_draw_text(ctx, head, fonts_get_system_font(TZ_FONT_HEADER),
                     GRect(box_x, y, box_w, TZ_HEADER_H),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
#else
  prv_draw_pair(ctx, date, utcs, TZ_FONT_HEADER, GRect(box_x, y, box_w, TZ_HEADER_H));
#endif
  y += TZ_HEADER_H;

  // Ortszeit gross
  char hhmm[8];
  prv_format_clock(hhmm, sizeof(hhmm), &here);
  graphics_context_set_fill_color(ctx, ZM_COLOR_ON_SURFACE);
  graphics_context_set_stroke_color(ctx, ZM_COLOR_ON_SURFACE);
  y += prv_draw_digits(ctx, hhmm, sizeof(hhmm), y, TZ_DIGIT_MAX, 12, box_w, cx, -1);
  y += TZ_GAP;

  // Ort. Eckig traegt diese Zeile im 12-Stunden-Modus rechts das AM/PM der
  // Ortszeit - neben den grossen Ziffern ist dafuer kein Platz.
  char city[TIMEZONE_NAME_LENGTH];
  if (known) prv_display_name(zone, city, sizeof(city));
  else       strncpy(city, "Keine Zone", sizeof(city));
  city[sizeof(city) - 1] = '\0';
#ifdef PBL_ROUND
  graphics_draw_text(ctx, city, fonts_get_system_font(TZ_FONT_CITY),
                     GRect(box_x, y, box_w, TZ_CITY_H),
                     GTextOverflowModeTrailingEllipsis, align, NULL);
#else
  const int16_t ampm_w = prv_text_w(here_ampm, TZ_FONT_HOME, box_w);
  if (ampm_w) {
    graphics_draw_text(ctx, here_ampm, fonts_get_system_font(TZ_FONT_HOME),
                       GRect(box_x, y + TZ_CITY_H - 20, box_w, 20),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  }
  graphics_draw_text(ctx, city, fonts_get_system_font(TZ_FONT_CITY),
                     GRect(box_x, y, box_w - (ampm_w ? ampm_w + 6 : 0), TZ_CITY_H),
                     GTextOverflowModeTrailingEllipsis, align, NULL);
#endif
  y += TZ_CITY_H;

  // ---- unterer Bereich: Heimatzeit ----
  const bool has_home = (state == TzAtHome || state == TzAway);
  const int16_t block_h = (state == TzAtHome) ? TZ_HOME_H_SHORT : TZ_HOME_H;
  const int16_t block_y = has_home ? bottom - block_h : y + TZ_GAP;

  struct tm home_tm;
  bool night = false;
  int32_t home_off = 0;
  if (has_home) {
    home_off = data->home.offset + data->home.fix_min * 60;
    const time_t shifted = utc + (time_t)home_off;
    memcpy(&home_tm, gmtime(&shifted), sizeof(home_tm));
    night = home_tm.tm_hour >= TZ_NIGHT_FROM || home_tm.tm_hour < TZ_NIGHT_TO;
  }

  // Ein Hinweis ersetzt den unteren Bereich fuer zwei Sekunden und bekommt
  // dafuer immer die volle Blockhoehe - sonst passt "Heimat aktualisiert" im
  // kurzen Block des Zustands "daheim" auf flint nicht hinein.
  // Fuellung, Trennlinie und Text MUESSEN dieselbe Oberkante benutzen. Sonst
  // steht der Text zur Haelfte neben der Flaeche, traegt aber trotzdem deren
  // Schriftfarbe - nachts also weiss auf weissem Grund -, und tagsueber laeuft
  // die Trennlinie mitten durch die Schrift.
  const int16_t fill_y = (has_home && data->hint) ? bottom - TZ_HOME_H : block_y;

  // Nacht daheim: der Block wird zum Negativ und beantwortet "darf ich jetzt
  // anrufen" ohne ein Wort. Tagsueber steht dort nur eine Trennlinie.
  GColor ink = ZM_COLOR_ON_SURFACE;
  if (has_home && night) {
    graphics_context_set_fill_color(ctx, ZM_COLOR_ACCENT);
    graphics_fill_rect(ctx, GRect(0, fill_y, content_w, bottom - fill_y), 0, GCornerNone);
    ink = ZM_COLOR_ON_ACCENT;
  } else {
    graphics_context_set_fill_color(ctx, ZM_COLOR_ON_SURFACE);
    graphics_fill_rect(ctx, GRect(box_x, fill_y, box_w, 2), 0, GCornerNone);
  }
  graphics_context_set_text_color(ctx, ink);

  if (data->hint) {
    graphics_draw_text(ctx, data->hint, fonts_get_system_font(TZ_FONT_HOME),
                       GRect(box_x, fill_y + 4, box_w, bottom - fill_y - 6),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    return;
  }

  if (!has_home) {
    char msg[110];
    if (state == TzZoneUnknown) {
      // Ohne gesetzte Zone liefert time() faktisch Ortszeit, eine Heimatzeit
      // waere also falsch gerechnet. Deshalb wird sie hier gar nicht gezeigt.
      strncpy(msg, "Die Uhr hat noch keine Zeitzone vom Telefon. Merken ist gesperrt.",
              sizeof(msg));
      msg[sizeof(msg) - 1] = '\0';
    } else {
      snprintf(msg, sizeof(msg), "Keine Heimatzeit. Mitteltaste merkt %s als Heimat.", city);
    }
    graphics_draw_text(ctx, msg, fonts_get_system_font(TZ_FONT_HOME),
                       GRect(box_x, block_y + 6, box_w, bottom - block_y - 8),
                       GTextOverflowModeWordWrap, align, NULL);
    return;
  }

  char home_city[TIMEZONE_NAME_LENGTH];
  prv_display_name(data->home.name, home_city, sizeof(home_city));
  const int16_t line_h = TZ_HOME_H_SHORT - 4;
  const int16_t line_y = block_y + 4;

  if (state == TzAtHome) {
    prv_draw_pair(ctx, home_city, "daheim", TZ_FONT_HOME,
                  GRect(box_x, line_y, box_w, line_h));
    return;
  }

  // "*" = Handkorrektur aktiv, "?" = seit ueber 90 Tagen nicht aufgefrischt
  const char *mark = "";
  if (data->home.fix_min != 0) mark = "*";
  else if (data->home.set_at && (int32_t)utc - data->home.set_at > TZ_STALE_SECS) mark = "?";
  char delta[16];
  prv_format_delta(delta, sizeof(delta), home_off - here_off, mark);
  const char *day_mark = prv_day_mark(&home_tm, &here);
  const char *home_ampm = prv_ampm(&home_tm);

  // Zusatz neben den Ziffern zuerst bauen und MESSEN - die Ziffern bekommen
  // nur den Rest der Breite. Auf rund ist daneben zu wenig Platz, dort wandert
  // die Tagesmarke in die erste Zeile.
  char extra[20] = "";
#ifdef PBL_ROUND
  char right[28];
  snprintf(right, sizeof(right), "%s%s%s", delta, day_mark[0] ? "  " : "", day_mark);
  prv_draw_pair(ctx, home_city, right, TZ_FONT_HOME, GRect(box_x, line_y, box_w, line_h));
  snprintf(extra, sizeof(extra), "%s", home_ampm);
#else
  prv_draw_pair(ctx, home_city, delta, TZ_FONT_HOME, GRect(box_x, line_y, box_w, line_h));
  snprintf(extra, sizeof(extra), "%s%s%s", home_ampm, (home_ampm[0] && day_mark[0]) ? " " : "",
           day_mark);
#endif

  // Zweite Zeile: die Heimatzeit in LECO. Eigener Puffer, damit die oben
  // gezeichnete Ortszeit nicht ueberschrieben wird.
  char home_hhmm[8];
  prv_format_clock(home_hhmm, sizeof(home_hhmm), &home_tm);
  // Der Zusatz bekommt nur, was nach den Ziffern uebrig bleibt. Die Ziffern
  // haben Vorrang (Untergrenze 12 px): die Heimatzeit ist der Zweck dieses
  // Screens, "PM gestern" daneben ist die Zugabe und darf notfalls kuerzen.
  const int16_t extra_w = prv_text_w(extra, TZ_FONT_EXTRA, box_w);
  const int16_t digits_w = box_w - (extra_w ? extra_w + 6 : 0);
  const int16_t dy = line_y + line_h;
  graphics_context_set_fill_color(ctx, ink);
  graphics_context_set_stroke_color(ctx, ink);
  const int16_t fs = prv_draw_digits(ctx, home_hhmm, sizeof(home_hhmm), dy,
                                     TZ_HOME_DIGIT_MAX, 12, digits_w, cx,
                                     PBL_IF_RECT_ELSE(box_x, -1));
  if (extra[0]) {
    // Kasten erst hinter den Ziffern beginnen lassen, sonst koennen sich
    // beide ueberlagern, wenn die Untergrenze der Ziffern gegriffen hat.
    const int16_t dw = (int16_t)rendering_get_size(home_hhmm, (uint8_t)fs);
    const int16_t dleft = PBL_IF_RECT_ELSE(box_x, (int16_t)(cx - dw / 2));
    const int16_t ex = dleft + dw + 6;
    const int16_t ew = box_x + box_w - ex;
    if (ew > 10) {
      graphics_context_set_text_color(ctx, ink);
      graphics_draw_text(ctx, extra, fonts_get_system_font(TZ_FONT_EXTRA),
                         GRect(ex, dy + (fs - 16) / 2, ew, 18),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
    }
  }
}


// ---- Zustand und Symbole --------------------------------------------------

static TzState prv_data_state(WindowData *data) {
  char zone[TIMEZONE_NAME_LENGTH];
  const bool known = prv_current_zone(zone, sizeof(zone));
  return prv_state(&data->home, zone, known);
}

static void prv_update_icons(WindowData *data) {
  const TzState state = prv_data_state(data);
  action_bar_layer_clear_icon(data->action_bar, BUTTON_ID_UP);
  action_bar_layer_clear_icon(data->action_bar, BUTTON_ID_SELECT);
  action_bar_layer_clear_icon(data->action_bar, BUTTON_ID_DOWN);
  if (state == TzZoneUnknown) return;

  action_bar_layer_set_icon(data->action_bar, BUTTON_ID_SELECT, data->icon_lap);
  if (state == TzAtHome) {
    action_bar_layer_set_icon(data->action_bar, BUTTON_ID_DOWN, data->icon_delete);
  } else if (state == TzAway) {
    action_bar_layer_set_icon(data->action_bar, BUTTON_ID_UP, data->icon_up);
    action_bar_layer_set_icon(data->action_bar, BUTTON_ID_DOWN, data->icon_down);
  }
}

// Selbstheilung: steht man in der Heimatzone, wird der gespeicherte Versatz
// stillschweigend auf den aktuellen nachgezogen. Damit stimmt er nach jeder
// Sommerzeitumstellung wieder, sobald man die App daheim einmal oeffnet.
// Hoechstens zwei Schreibvorgaenge im Jahr: danach ist offset == now_off und
// fix_min == 0, die Bedingung also falsch.
static void prv_refresh(WindowData *data) {
  char zone[TIMEZONE_NAME_LENGTH];
  const bool known = prv_current_zone(zone, sizeof(zone));
  if (known && data->home.valid && strcmp(zone, data->home.name) == 0) {
    const time_t now = time(NULL);
    const int32_t now_off = prv_current_offset(now);
    if (data->home.offset != now_off || data->home.fix_min != 0) {
      data->home.offset = now_off;
      data->home.fix_min = 0;
      data->home.set_at = (int32_t)now;
      prv_home_save(&data->home);
    }
  }
  prv_update_icons(data);
  layer_mark_dirty(data->drawing_layer);
}

static void prv_hint_timeout(void *context) {
  WindowData *data = (WindowData*)context;
  data->hint_timer = NULL;
  data->hint = NULL;
  layer_mark_dirty(data->drawing_layer);
}

static void prv_hint(WindowData *data, const char *text) {
  if (data->hint_timer) app_timer_cancel(data->hint_timer);
  data->hint = text;
  data->hint_timer = app_timer_register(TZ_HINT_MS, prv_hint_timeout, data);
  layer_mark_dirty(data->drawing_layer);
}


// ---- Tasten ---------------------------------------------------------------
//
// Tastenbelegung (BACK bleibt ueberall der Standardweg zurueck):
//
//   Zustand          UP kurz   UP lang   SELECT kurz  SELECT lang  DOWN kurz   DOWN lang
//   Zone unbekannt   -         -         -            -            -           -
//   keine Heimat     -         -         merken       merken       -           -
//   daheim           -         -         auffrischen  auffrischen  Hinweis     loeschen
//   unterwegs        +1 h      Kor. aus  Hinweis      ersetzen     -1 h        -1 h
//
// Zwei Regeln stecken darin:
//   * Ein KURZER Druck ist nirgends zerstoerend. Loeschen gibt es nur als
//     langen Druck und nur daheim - unterwegs koennte man die Heimatzone gar
//     nicht wiederherstellen, und ein zu lang gehaltenes DOWN beim Korrigieren
//     wuerde genau das vernichten, wofuer man den Screen geoeffnet hat.
//   * Wo kein langer Druck vorgesehen ist, wirkt er wie der kurze. Sonst
//     passiert bei zu langem Halten gar nichts, weil der lange Druck den
//     kurzen verschluckt.

static void prv_remember(WindowData *data) {
  char zone[TIMEZONE_NAME_LENGTH];
  if (!prv_current_zone(zone, sizeof(zone))) return;
  const time_t now = time(NULL);
  strncpy(data->home.name, zone, sizeof(data->home.name));
  data->home.name[sizeof(data->home.name) - 1] = '\0';
  data->home.offset = prv_current_offset(now);
  data->home.set_at = (int32_t)now;
  data->home.fix_min = 0;
  data->home.valid = true;
  prv_home_save(&data->home);
  vibes_short_pulse();
}

static void prv_forget(WindowData *data) {
  memset(&data->home, 0, sizeof(data->home));
  prv_home_clear();
  vibes_short_pulse();
}

// Handkorrektur: die einzige Handhabe, wenn daheim umgestellt wird, waehrend
// man unterwegs ist. Steht getrennt vom gemerkten Wert, wird mit "*" am
// Versatz angezeigt und sofort gesichert - alles andere wird auch sofort
// gesichert, und ein Ende ohne Fenster-Pop wuerde sie sonst verschlucken.
static void prv_set_fix(WindowData *data, int32_t minutes, const char *hint) {
  if (minutes > TZ_FIX_STEP) minutes = TZ_FIX_STEP;
  if (minutes < -TZ_FIX_STEP) minutes = -TZ_FIX_STEP;
  data->home.fix_min = minutes;
  prv_home_save(&data->home);
  prv_hint(data, hint);
  prv_refresh(data);
}

static void prv_select_click(ClickRecognizerRef rec, void *context) {
  WindowData *data = (WindowData*)context;
  switch (prv_data_state(data)) {
    case TzZoneUnknown: return;
    case TzNoHome:  prv_remember(data); prv_hint(data, "Heimat gemerkt"); break;
    case TzAtHome:  prv_remember(data); prv_hint(data, "Heimat aktualisiert"); break;
    // Unterwegs schuetzt der lange Druck genau die Information, wegen der man
    // den Screen aufgemacht hat.
    default:        prv_hint(data, "Lang drücken zum Ersetzen"); return;
  }
  prv_refresh(data);
}

static void prv_select_long(ClickRecognizerRef rec, void *context) {
  WindowData *data = (WindowData*)context;
  if (prv_data_state(data) != TzAway) {
    prv_select_click(rec, context);
    return;
  }
  prv_remember(data);
  prv_hint(data, "Heimat gemerkt");
  prv_refresh(data);
}

static void prv_up_click(ClickRecognizerRef rec, void *context) {
  WindowData *data = (WindowData*)context;
  if (prv_data_state(data) != TzAway) return;
  const int32_t next = data->home.fix_min + TZ_FIX_STEP;
  prv_set_fix(data, next, next >= TZ_FIX_STEP ? "Daheim +1 h" : "Korrektur aus");
}

static void prv_up_long(ClickRecognizerRef rec, void *context) {
  WindowData *data = (WindowData*)context;
  if (prv_data_state(data) != TzAway) return;
  prv_set_fix(data, 0, "Korrektur aus");
}

static void prv_down_click(ClickRecognizerRef rec, void *context) {
  WindowData *data = (WindowData*)context;
  const TzState state = prv_data_state(data);
  if (state == TzAtHome) {
    prv_hint(data, "Lang drücken zum Löschen");
    return;
  }
  if (state != TzAway) return;
  const int32_t next = data->home.fix_min - TZ_FIX_STEP;
  prv_set_fix(data, next, next <= -TZ_FIX_STEP ? "Daheim -1 h" : "Korrektur aus");
}

static void prv_down_long(ClickRecognizerRef rec, void *context) {
  WindowData *data = (WindowData*)context;
  if (prv_data_state(data) != TzAtHome) {
    prv_down_click(rec, context);
    return;
  }
  prv_forget(data);
  prv_hint(data, "Heimat gelöscht");
  prv_refresh(data);
}

static void prv_click_config_provider(void *context) {
  window_set_click_context(BUTTON_ID_UP, context);
  window_set_click_context(BUTTON_ID_SELECT, context);
  window_set_click_context(BUTTON_ID_DOWN, context);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click);
  window_long_click_subscribe(BUTTON_ID_UP, TZ_LONG_MS, prv_up_long, NULL);
  window_long_click_subscribe(BUTTON_ID_SELECT, TZ_LONG_MS, prv_select_long, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, TZ_LONG_MS, prv_down_long, NULL);
}


// ---- Fenster --------------------------------------------------------------

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (s_data) prv_refresh(s_data);
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);

  WindowData *data = (WindowData*)malloc(sizeof(WindowData));
  window_set_user_data(window, data);
  s_data = data;
  if (data) {
    // malloc liefert nicht genullten Speicher, und prv_home_load setzt nur
    // einen Teil der Felder (Muster stopwatch.c).
    memset(data, 0, sizeof(*data));
    prv_home_load(&data->home);

    data->icon_up = gbitmap_create_with_resource(RESOURCE_ID_ICON_UP);
    data->icon_down = gbitmap_create_with_resource(RESOURCE_ID_ICON_DOWN);
    data->icon_lap = gbitmap_create_with_resource(RESOURCE_ID_ICON_LAP);
    data->icon_delete = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DELETE);

    data->drawing_layer = layer_create_with_data(bounds, sizeof(WindowData*));
    data->action_bar = action_bar_layer_create();
    // Ohne diese Pruefung wuerde layer_get_data(NULL) sofort einen Nullzeiger
    // ausschreiben; der Abbau unten haelt beide Faelle aus.
    if (data->drawing_layer && data->action_bar) {
      WindowData **layer_data = (WindowData**)layer_get_data(data->drawing_layer);
      (*layer_data) = data;
      layer_set_update_proc(data->drawing_layer, prv_layer_draw);
      layer_add_child(root, data->drawing_layer);

      action_bar_layer_set_context(data->action_bar, data);
      action_bar_layer_set_click_config_provider(data->action_bar, prv_click_config_provider);
      action_bar_layer_add_to_window(data->action_bar, window);

      tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_handler);
      prv_refresh(data);
      return;
    }
    APP_LOG(APP_LOG_LEVEL_ERROR, "Exiting: Failed to allocate layers");
    window_stack_remove(window, false);
    return;
  }

  window_stack_remove(window, false);
  APP_LOG(APP_LOG_LEVEL_ERROR, "Exiting: Failed to allocate WindowData");
}

static void prv_window_unload(Window *window) {
  WindowData *data = (WindowData*)window_get_user_data(window);
  if (data) {
    if (data->hint_timer) {
      app_timer_cancel(data->hint_timer);
      data->hint_timer = NULL;
    }
    // tick_timer_service ist eine globale Einzelanmeldung. Sie gehoert
    // waehrend dieses Fensters allein diesem Screen und wird hier wieder
    // freigegeben - kein anderes Modul von ChronoKit meldet sich an.
    tick_timer_service_unsubscribe();
    // Kann NULL sein, wenn das Anlegen im Load-Handler fehlgeschlagen ist
    if (data->action_bar) action_bar_layer_destroy(data->action_bar);
    if (data->drawing_layer) layer_destroy(data->drawing_layer);
    gbitmap_destroy(data->icon_up);
    gbitmap_destroy(data->icon_down);
    gbitmap_destroy(data->icon_lap);
    gbitmap_destroy(data->icon_delete);
    s_data = NULL;
    window_destroy(window);
    free(data);
    return;
  }

  window_destroy(window);
  APP_LOG(APP_LOG_LEVEL_ERROR, "Error: Tried to free NULL WindowData");
}

void timezone_window_push(void) {
  Window *window = window_create();
  window_set_background_color(window, ZM_COLOR_SURFACE);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(window, true);
}
