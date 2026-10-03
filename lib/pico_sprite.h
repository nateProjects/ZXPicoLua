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
 * palt(): any number of colours can be transparent at once (unlike M2's
 * original pico_spr_transparent(), a single-colour placeholder this
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
 * policy as pico_gfx.h, for the same reason). */
void pico_spr(uint8_t n, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t flip_x, uint8_t flip_y);

/* sspr(sx, sy, sw, sh, dx, dy, dw, dh, flip_x, flip_y) -- blits an
 * arbitrary sheet rectangle to an arbitrary destination rectangle,
 * nearest-neighbour scaled if dw/dh differ from sw/sh. This is the
 * general blit pico_spr() itself is implemented on top of. */
void pico_sspr(uint8_t sx, uint8_t sy, uint8_t sw, uint8_t sh,
               uint8_t dx, uint8_t dy, uint8_t dw, uint8_t dh,
               uint8_t flip_x, uint8_t flip_y);

#endif
