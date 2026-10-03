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
 * Out-of-range coordinates are not checked (same as Pico-8/PicoJS).
 *
 * Both always act on the current BACK buffer (see pico_flip() below) --
 * pico_pget() therefore reflects what's been drawn so far *this frame*,
 * not necessarily what's currently on screen: a region untouched since
 * the last pico_flip() still holds whatever was drawn there two frames
 * ago, not last frame. Fine for the common case (read back a pixel you
 * just drew), a real limitation for anything reading untouched regions
 * across frames -- see docs/input-notes.md's "double buffering" section. */
void pico_pset(uint8_t x, uint8_t y, uint8_t color);
uint8_t pico_pget(uint8_t x, uint8_t y);

/* pal(c0, c1) -- remaps colour c0 (0-15) to c1 (0-15) for every
 * subsequent pico_cls()/pico_pset() call (and therefore every primitive
 * built on them -- pico_gfx.c's shapes, pico_sprite.c's blits, text),
 * until pico_pal_reset(). Matches Pico-8's own draw-palette pal(), minus
 * its p=1 "screen palette" mode -- not implemented, same as PicoJS's own
 * scope. Only the standard 0-15 Pico-8 range is remappable; this
 * project's own 16-255 native-Next colour extension is untouched.
 * pico_pget() is unaffected either way -- it reads back whatever was
 * actually stored (i.e. already post-remap), matching real Pico-8. */
void pico_pal(uint8_t c0, uint8_t c1);
void pico_pal_reset(void);

/* Presents everything drawn since the last pico_flip() and swaps which
 * physical Layer 2 bank is the back (writable) buffer vs the displayed
 * one. Call once per frame, after all of that frame's drawing is done --
 * see docs/input-notes.md's "double buffering" section for why this
 * exists (a sprite redrawn in place every frame, with no double
 * buffering, visibly tears when the display's own scanout catches the
 * framebuffer mid-redraw -- confirmed via real human testing, not just
 * theory). Because the back buffer alternates between two physical
 * banks, each one holds whatever was drawn into it *two* flips ago, not
 * one -- any animated content must track per-buffer state accordingly
 * (see the demo's own o_lit/x_lit/ball-position handling in
 * games/demo/main.c for the pattern). */
void pico_flip(void);

#endif
