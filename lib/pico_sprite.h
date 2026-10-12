#ifndef PICO_SPRITE_H
#define PICO_SPRITE_H

#include <stdint.h>

/* Registers the spritesheet games draw from: a flat, row-major, indexed-
 * colour (1 byte/pixel) bitmap, the same format as the Layer 2 framebuffer
 * itself -- sheet_w/sheet_h in pixels. Must be called before pico_spr()/
 * pico_sspr(). Pico-8's own spritesheet convention (128x128, 16x16 grid of
 * 8x8 sprites) is what pico_spr()'s sprite-index math generalises --
 * sheet_w need not be 128 (a game using fewer sprites can use a smaller
 * sheet) but should be a multiple of 8 for the cell math to make sense. */
void pico_spr_init(const uint8_t *sheet_data, uint16_t sheet_w, uint16_t sheet_h);

/* palt(c, transparent) -- sets whether colour c (0-15) is treated as
 * transparent (skipped) when blitting sprites. Matches real Pico-8's
 * palt(): any number of colours can be transparent at once (unlike the
 * earlier pico_spr_transparent(), a single-colour placeholder this
 * replaces). Defaults to colour 0 only, matching Pico-8's own default
 * palt() state. Checked against the sprite sheet's RAW stored colour, not
 * after pico_pal()'s remap (pico_video.h) -- the two are independent in
 * real Pico-8 too: palt() decides whether a pixel is drawn at all, pal()
 * separately decides what colour it's drawn as if it is. */
void pico_palt(uint8_t color, uint8_t transparent);

/* spr(n, x, y, w, h, flip_x, flip_y) -- draws sprite cell n (8x8, from the
 * registered sheet) at logical (x,y), 1:1 pixel scale. w/h (in whole 8px
 * cells) select a w*h block of adjacent sprite cells starting at n, NOT a
 * scale factor -- matches real Pico-8 spr() semantics exactly (pico_sspr()
 * below is the one that scales). Pico-8 defaults w/h to 1 when omitted;
 * pass 1 explicitly here, no implicit default (same explicit-argument
 * policy as pico_gfx.h, for the same reason).
 *
 * x/y are signed (int16_t), unlike every other logical-canvas coordinate
 * in this library (which are uint8_t) -- a sprite partly off the left/top
 * edge is ordinary, not misuse (pset_safe() inside pico_sspr() already
 * clips it correctly either way), and some real techniques (a scrolling
 * background drawn via two map() calls, one at the normal position and
 * one a full screen-width to its left/right to cover the wrap) need to
 * *start* off-screen on purpose -- added for the picodiver port, whose
 * title-screen ship background does exactly this. Small positive literals
 * at existing call sites still just work (plain int-to-int16_t
 * conversion). */
void pico_spr(uint8_t n, int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t flip_x, uint8_t flip_y);

/* sspr(sx, sy, sw, sh, dx, dy, dw, dh, flip_x, flip_y) -- blits an
 * arbitrary sheet rectangle to an arbitrary destination rectangle,
 * nearest-neighbour scaled if dw/dh differ from sw/sh. This is the
 * general blit pico_spr() itself is implemented on top of. dx/dy are
 * signed for the same reason as pico_spr()'s x/y above; sx/sy (the
 * SOURCE sheet position) stay uint8_t -- those are never negative. */
void pico_sspr(uint8_t sx, uint8_t sy, uint8_t sw, uint8_t sh,
               int16_t dx, int16_t dy, uint8_t dw, uint8_t dh,
               uint8_t flip_x, uint8_t flip_y);

/* sget(x, y) -- reads a single pixel's colour index directly from the
 * registered spritesheet (not the screen/framebuffer -- pico_pget(),
 * pico_video.h, is the framebuffer equivalent). Added for the picodiver
 * port, which uses this for pixel-perfect collision (dw817's method, the
 * same one the original Pico-8 cart and PicoJS's own port both use) --
 * out-of-range coordinates return 0, matching PicoJS's own picoJS.sget. */
uint8_t pico_sget(uint8_t x, uint8_t y);

/* Pixel-perfect collision between two sprite-sheet cells placed on
 * screen, dw817's method (pico_sget()'s own comment) -- the same
 * algorithm the original Pico-8 cart and PicoJS's own port used, not
 * invented for this port. n1/w1/h1 and n2/w2/h2 follow pico_spr()'s own
 * convention exactly (sprite index + size in whole 8px cells), so an
 * existing pico_spr(n, x, y, w, h, ...) call's own n/x/y/w/h can be
 * passed straight through. Walks the smaller sprite's own pixels against
 * the larger one (same optimisation the original algorithm uses) with a
 * cheap bounding-box pre-check first (NOT in the original -- a safe,
 * behaviour-equivalent addition: two sprites whose boxes don't overlap
 * can't have overlapping pixels either, so this only ever skips work the
 * original would have wasted). Returns 1 on any overlapping pair of
 * non-zero (non-colour-0) pixels, matching real Pico-8's own fget()-free
 * pixel collision convention -- colour 0 is always "empty" here, same as
 * pico_map()'s "tile 0 is empty" and pico_palt()'s own default. */
uint8_t pico_spr_collide(uint8_t n1, int16_t x1, int16_t y1, uint8_t w1, uint8_t h1,
                          uint8_t n2, int16_t x2, int16_t y2, uint8_t w2, uint8_t h2);

#endif
