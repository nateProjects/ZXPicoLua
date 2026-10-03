#ifndef PICO_VIDEO_H
#define PICO_VIDEO_H

#include <stdint.h>

/* Physical Layer 2 surface: fixed by the hardware, never changes. Every
 * drawing primitive's address math is ultimately against this. */
#define PICO_PHYS_W 256
#define PICO_PHYS_H 192

/* Logical canvas: what game code actually draws against. Pico-8 carts are
 * authored for 128x128 and expect to address (0,0)-(127,127); a native
 * ZXPicoLua game can instead declare the full 256x192 physical surface as
 * its own canvas. pico_video_init() centres whatever logical size is
 * requested within the fixed physical surface -- e.g. 128x128 gets a
 * 64px left/right and 32px top/bottom border -- and every pset/pget/future
 * primitive transparently offsets into that centred area, so ported Pico-8
 * code using its own native 0-127 coordinates needs no translation of its
 * own. This is a free coordinate offset, not a scaled blit -- see
 * docs/layer2-notes.md's "logical canvas" note for why scaling (e.g. to
 * fill more of the physical screen) is intentionally not implemented here:
 * it would need a separate offscreen-buffer-plus-blit design, not just an
 * offset, and isn't needed until a real port asks for it. */
void pico_video_init(uint16_t logical_w, uint16_t logical_h);

/* The logical canvas size last passed to pico_video_init(). */
extern uint16_t pico_screen_w;
extern uint16_t pico_screen_h;

/* Fills the whole physical screen (including any letterbox border) with a
 * single palette index. */
void pico_cls(uint8_t color);

/* Writes/reads a single pixel's palette index, in LOGICAL canvas
 * coordinates -- x in [0, pico_screen_w), y in [0, pico_screen_h).
 * Out-of-range coordinates are not checked (same as Pico-8/PicoJS). */
void pico_pset(uint8_t x, uint8_t y, uint8_t color);
uint8_t pico_pget(uint8_t x, uint8_t y);

#endif
