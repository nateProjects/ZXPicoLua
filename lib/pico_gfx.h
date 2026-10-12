#ifndef PICO_GFX_H
#define PICO_GFX_H

#include <stdint.h>

/* Shape/text primitives, built entirely on pico_pset()/pico_pget()
 * (pico_video.h) -- no direct hardware access here, same "correctness
 * first" staging the project's plan called for. All coordinates are
 * logical-canvas coordinates (see pico_video.h) before camera() is
 * applied -- see pico_camera() below.
 *
 * Deliberate deviation from PicoJS/Pico-8: every primitive here still
 * REQUIRES an explicit colour argument -- color() wasn't implemented as
 * real global state originally (2026-10 scoping note: "may be revisited
 * if porting real carts proves this painful in practice"). That real
 * cart showed up (`color(11) ... centre_print(...)` wrapping a print()
 * call with no colour argument, relying on the surrounding color()) --
 * see pico_color()/PICO_CURRENT_COLOR below for the fix, added rather
 * than silently resolved by hand at each translated call site, since the
 * genuinely hard case isn't a local 3-line block (trivial to trace by
 * eye) but a color() set in one function relied on by a print() in a
 * DIFFERENT one -- exactly the kind of cross-function implicit state a
 * human translator can silently get wrong, and a real stateful variable
 * in the port carries correctly for free. */

/* color(c) -- sets the "current" pen colour used by any pico_rect()/
 * pico_line()/pico_print()/pico_print_at_cursor() call that receives
 * PICO_CURRENT_COLOR instead of an explicit 0-15 value (0xFF, well
 * outside the real 0-15 colour range, so it can never collide with a
 * genuine colour by accident). Deliberately NOT wired into pico_rectfill()
 * /pico_circ()/pico_circfill()/pico_oval()/pico_ovalfill() -- those
 * already use the full 0-255 range for Pico-8's own two-colour dither
 * encoding (low nibble = primary, high nibble = secondary when color>15,
 * see pico_rectfill()'s own comment), so there's no free sentinel value
 * left there; pass an explicit colour (and an explicit fillp() dither if
 * wanted) to those regardless. Also deliberately NOT wired into the
 * lower-level pico_pset()/pico_cls() (pico_video.h) -- those are
 * deliberately-still-hot per-pixel paths this project has already fought
 * hard to keep fast (docs/gfx-notes.md's own batch-row rewrite); adding
 * a branch to every single pixel plot for a convenience only the higher-
 * level shape functions actually need wasn't worth it. */
#define PICO_CURRENT_COLOR 0xFF
void pico_color(uint8_t c);

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
/* pico_rect()'s own color accepts PICO_CURRENT_COLOR (see this header's
 * top comment) -- pico_rectfill() does not (dither encoding owns the
 * >15 range there). */
void pico_rect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);

/* Accepts PICO_CURRENT_COLOR (see this header's top comment). */
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
 * no clean equivalent). Both accept PICO_CURRENT_COLOR (this header's
 * top comment) for a call translated from Pico-8's own color-omitted
 * print(str)/print(str,x,y) forms. */
void pico_print(const char *text, uint8_t x, uint8_t y, uint8_t color);
void pico_print_at_cursor(const char *text, uint8_t color);

/* camera(x, y) -- offsets every subsequent drawing call in THIS file
 * (rect/line/circ/oval and their fill variants, print/print_at_cursor) by
 * (-x, -y), matching Pico-8's own camera() exactly. Scope deliberately
 * limited to pico_gfx.c's own shape/text primitives -- NOT pico_sprite.c's
 * spr()/sspr() (no reference game needs sprite camera-offsetting yet) and
 * NOT pico_video.c's raw pset()/pget()/cls() (deliberately-hot per-pixel
 * paths already fought hard to keep fast, see pico_color()'s own comment
 * above for the same reasoning). Call camera(0,0) to reset, same as
 * Pico-8. Coordinates/results are still clipped to the logical canvas the
 * same way they always were -- camera() only shifts what world position
 * maps to a given screen pixel, it doesn't change the canvas size. */
void pico_camera(int16_t x, int16_t y);

#endif
