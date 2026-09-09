#include <pebble.h>
#include "rendering.h"

#define FONT_CHARACTER_DEFINITION_HEIGHT 255
#define FONT_CHARACTER_DEFINITION_KERNING 50

// one font character: LECO is composed entirely of straight lines, so every glyph is one
// polygon made of overlapping rectangles (the overlap avoids gray anti-aliasing seams)
typedef struct FontCharacter {
  char character;
  uint8_t char_width;
  uint8_t num_points;
  GPoint points[14];
} FontCharacter;

// LECO glyph data on a 178x255 grid, scaled to font_size when drawn
static FontCharacter LECO_0 = {'0', 178, 10, {{0, 0}, {178, 0}, {178, 255}, {0, 255}, {0, 0},
  {50, 0}, {50, 205}, {128, 205}, {128, 50}, {0, 50}}};
static FontCharacter LECO_1 = {'1', 178, 10, {{0, 0}, {114, 0}, {114, 205}, {178, 205}, {178, 255},
  {0, 255}, {0, 205}, {64, 205}, {64, 50}, {0, 50}}};
static FontCharacter LECO_2 = {'2', 178, 14, {{0, 68}, {0, 0}, {178, 0}, {178, 153}, {50, 153},
  {50, 205}, {178, 205}, {178, 255}, {0, 255}, {0, 103}, {128, 103}, {128, 50}, {50, 50},
  {50, 68}}};
static FontCharacter LECO_3 = {'3', 178, 12, {{0, 0}, {178, 0}, {178, 255}, {0, 255}, {0, 205},
  {128, 205}, {128, 153}, {26, 153}, {26, 103}, {128, 103}, {128, 50}, {0, 50}}};
static FontCharacter LECO_4 = {'4', 178, 10, {{0, 0}, {0, 153}, {128, 153}, {128, 255}, {178, 255},
  {178, 0}, {128, 0}, {128, 103}, {50, 103}, {50, 0}}};
static FontCharacter LECO_5 = {'5', 178, 14, {{178, 0},{0, 0}, {0, 153}, {128, 153}, {128, 205},
  {50, 205}, {50, 187}, {0, 187}, {0, 255}, {178, 255}, {178, 103}, {50, 103}, {50, 50},
  {178, 50}}};
static FontCharacter LECO_6 = {'6', 178, 12, {{178, 0}, {0, 0}, {0, 255}, {178, 255}, {178, 103},
  {25, 103}, {25, 153}, {128, 153}, {128, 205}, {50, 205}, {50, 50}, {178, 50}}};
static FontCharacter LECO_7 = {'7', 178, 8, {{0, 76}, {0, 0}, {178, 0}, {178, 255}, {128, 255},
  {128, 50}, {50, 50}, {50, 76}}};
static FontCharacter LECO_8 = {'8', 178, 14, {{0, 153}, {0, 0}, {178, 0}, {178, 255}, {0, 255},
  {0, 103}, {163, 103}, {163, 153}, {50, 153}, {50, 205}, {128, 205}, {128, 50}, {50, 50},
  {50, 153}}};
static FontCharacter LECO_9 = {'9', 178, 12, {{0, 255}, {178, 255}, {178, 0}, {0, 0}, {0, 153},
  {163, 153}, {163, 103}, {50, 103}, {50, 50}, {128, 50}, {128, 205}, {0, 205}}};
static FontCharacter LECO_C = {':', 0, 4, {{0, 50}, {50, 50}, {50, 100}, {0, 100}}};
static FontCharacter LECO_P = {'.', 50, 4, {{0, 205}, {50, 205}, {50, 255}, {0, 255}}};

static FontCharacter *LECO_CHARS[] = {&LECO_0, &LECO_1, &LECO_2, &LECO_3, &LECO_4, &LECO_5,
  &LECO_6, &LECO_7, &LECO_8, &LECO_9, &LECO_C, &LECO_P};


// draw text in the LECO font, scaled to font_size, onto a drawing context
void rendering_draw_text(GContext *ctx, char *text, uint8_t size, uint16_t font_size,
                         GPoint position) {
  // A colon is only the upper dot with zero character width, so a period is inserted after
  // every colon: with zero kerning its dot lands right beneath the colon's dot.
  // The text is copied because of that in-place substitution.
  char *text_copy = (char*)malloc(size);

  GPoint cur_origin = position;
  GPath path = (GPath) {
    .points = (GPoint*)malloc(sizeof(LECO_CHARS[0]->points)),
  };
  if (path.points && text_copy) {
    memcpy(text_copy, text, size);
    for (uint32_t txt_idx = 0; txt_idx < strlen(text_copy); txt_idx++) {
      for (uint8_t char_idx = 0; char_idx < ARRAY_LENGTH(LECO_CHARS); char_idx++) {
        if (LECO_CHARS[char_idx]->character == text_copy[txt_idx]) {
          // build scaled GPath and draw it
          path.num_points = LECO_CHARS[char_idx]->num_points;
          memcpy(path.points, LECO_CHARS[char_idx]->points,
            sizeof(LECO_CHARS[char_idx]->points));
          for (uint8_t pt_idx = 0; pt_idx < path.num_points; pt_idx++) {
            path.points[pt_idx].x = path.points[pt_idx].x *
            font_size / FONT_CHARACTER_DEFINITION_HEIGHT + 0.5;
            path.points[pt_idx].y = path.points[pt_idx].y *
            font_size / FONT_CHARACTER_DEFINITION_HEIGHT + 0.5;
          }
          path.offset = cur_origin;
          gpath_draw_filled(ctx, &path);
          gpath_draw_outline(ctx, &path);
          // advance character origin (top left)
          cur_origin.x += (LECO_CHARS[char_idx]->char_width +
            ((LECO_CHARS[char_idx]->char_width == 0) ? 0 : FONT_CHARACTER_DEFINITION_KERNING)) *
            font_size / FONT_CHARACTER_DEFINITION_HEIGHT;
          break;
        }
      }
      // add period after colon (see above)
      if (text_copy[txt_idx] == ':') {
        text_copy[txt_idx] = '.';
        txt_idx--;
      }
    }
    free(path.points);
    free(text_copy);
    return;
  }

  // error handling
  APP_LOG(APP_LOG_LEVEL_ERROR, "Error: Could not allocate memory for text rendering.");
}


// gets the rendered width of the text at font_size
uint32_t rendering_get_size(char *buff, uint8_t font_size) {
  uint32_t tot_width = 0;
  for (uint32_t txt_idx = 0; txt_idx < strlen(buff); txt_idx++) {
    for (uint8_t char_idx = 0; char_idx < ARRAY_LENGTH(LECO_CHARS); char_idx++) {
      if (LECO_CHARS[char_idx]->character == buff[txt_idx]) {
        // a colon is drawn as colon plus period, so it takes the period's width
        uint8_t char_width = (buff[txt_idx] == ':') ? LECO_P.char_width
                                                    : LECO_CHARS[char_idx]->char_width;
        tot_width += (char_width + FONT_CHARACTER_DEFINITION_KERNING) *
          font_size / FONT_CHARACTER_DEFINITION_HEIGHT;
        break;
      }
    }
  }
  // remove final kerning spacing
  if (tot_width > 0) {
    tot_width -= FONT_CHARACTER_DEFINITION_KERNING * font_size / FONT_CHARACTER_DEFINITION_HEIGHT;
  }
  return tot_width;
}
