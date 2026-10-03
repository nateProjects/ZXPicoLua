#ifndef PICO_FONT_H
#define PICO_FONT_H

#include <stdint.h>

/* The real Pico-8 4x6 bitmap font, printable ASCII 32-126 (95 glyphs),
 * ported byte-for-byte from PicoJS's own PICO8_FONT_HEX table
 * (PicoJS/picoJS.js) -- itself extracted from althonos/embedded-picofont's
 * font.png (sourced from Lexaloffle's own Pico-8 font asset, CC-0).
 * Ported programmatically (not hand-transcribed) to avoid transcription
 * errors -- see the project history for the extraction script. */

#define PICO_FONT_CELL_W 4
#define PICO_FONT_CELL_H 6
#define PICO_FONT_FIRST_CHAR 32
#define PICO_FONT_NUM_GLYPHS 95

/* One byte per row, low nibble = that row's 4 pixels, bit 3 = leftmost
 * column .. bit 0 = rightmost (matches PicoJS's own bit order). 6 bytes
 * per glyph, glyphs in ASCII order starting at PICO_FONT_FIRST_CHAR. */
extern const uint8_t pico_font_data[PICO_FONT_NUM_GLYPHS * PICO_FONT_CELL_H];

#endif
