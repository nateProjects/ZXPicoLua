#ifndef PICO_GFX_H
#define PICO_GFX_H

#include <stdint.h>

/* Shape/text primitives, built entirely on pico_pset()/pico_pget()
 * (pico_video.h) -- no direct hardware access here, same "correctness
 * first" staging the project's plan called for. All coordinates are
 * logical-canvas coordinates (see pico_video.h) -- no camera() offset
 * yet (deferred past M1, unlike PicoJS/Pico-8).
 *
 * Deliberate deviation from PicoJS/Pico-8: every primitive here takes an
 * explicit colour argument. PicoJS allows omitting colour to fall back to
 * a "current colour" set by a previous colour()/color() call -- C has no
 * clean way to make a trailing argument optional without a second
 * function or a sentinel value, and threading an implicit "current
 * colour" global through every primitive adds real complexity for a
 * convenience ported game code can trivially work around (pass the
 * colour explicitly at each call site). May be revisited if porting real
 * carts proves this painful in practice. */

/* fillp(pattern, transparent) -- sets the 4x4 dither pattern used by
 * rectfill()/circfill()/ovalfill() from here on; pattern 0 means solid
 * fill (the default). Bit 15 = top-left cell of the 4x4 tile, bit 0 =
 * bottom-right, raster order, repeating every 4 pixels in logical-canvas
 * space -- matches the Pico-8 manual's own bit layout exactly (ported
 * from PicoJS's fillp(), picoJS.js ~line 285).
 *
 * transparent: when true, a pattern's "on" bits are skipped (not drawn)
 * rather than drawn in a secondary colour -- same effect as PicoJS's
 * fractional-pattern convention (fillp(pattern + 0x0.8) in real Pico-8
 * syntax), spelled as a separate explicit argument instead, for the same
 * reason colour arguments aren't optional here. */
void pico_fillp(uint16_t pattern, uint8_t transparent);

/* rectfill/rect: (x0,y0)-(x1,y1) are inclusive corners, any order. color's
 * low nibble is the fill's "off"/primary colour; if fillp() has a pattern
 * active, the high nibble is the "on"/secondary colour for a two-colour
 * dither (same >15 convention as Pico-8) -- pass a plain 0-15 value for a
 * single-colour fill or stipple. */
void pico_rectfill(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);
void pico_rect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);

void pico_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);

void pico_circ(uint8_t cx, uint8_t cy, uint8_t r, uint8_t color);
void pico_circfill(uint8_t cx, uint8_t cy, uint8_t r, uint8_t color);

/* (x0,y0)-(x1,y1): the oval's bounding box, inclusive, any order. */
void pico_oval(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);
void pico_ovalfill(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);

/* cursor(x, y) -- sets the position a subsequent pico_print_at_cursor()
 * call starts at; mirrors PicoJS/Pico-8's cursor(), minus the optional
 * colour argument (same explicit-colour policy as above). */
void pico_cursor(uint8_t x, uint8_t y);

/* print(text, x, y, color) -- draws at an explicit position using the
 * built-in Pico-8 4x6 font (pico_font.h), honouring embedded '\n' the
 * same way PicoJS does (breaks to a new line at the call's own starting
 * x, advancing by the font's cell height) -- does NOT move the cursor.
 * print_at_cursor(text, color) draws at, and advances, the position set
 * by pico_cursor()/a previous print_at_cursor() call -- matches Pico-8's
 * console-like print(str) / print(str, col) forms, split into two
 * functions instead of PicoJS's argument-count-based overloading (C has
 * no clean equivalent). */
void pico_print(const char *text, uint8_t x, uint8_t y, uint8_t color);
void pico_print_at_cursor(const char *text, uint8_t color);

#endif
