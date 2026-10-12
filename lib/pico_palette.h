#ifndef PICO_PALETTE_H
#define PICO_PALETTE_H

#include <stdint.h>

/* Pico-8's own default (palette 0) 16 colours -- the actual source of
 * truth for "what colour is Pico-8 colour N", shared between Layer 2
 * (pico_video_load_pico8_palette()) and the hardware tilemap
 * (pico_tilemap_load_pico8_palette()) so anything drawn through either
 * one uses the same colours and blends correctly where a sprite
 * deliberately colour-matches a background instead of using real
 * transparency (see pico_tilemap.h's own note on that trick).
 *
 * Each entry is {r,g,b}, 0-7, already converted from Pico-8's real 0-255
 * RGB values to the Next's 3-bit-per-channel palette via round(c*7/255) --
 * see pico_palette.c for the original 0-255 values this was computed
 * from. Before this table existed, nothing in this project ever loaded
 * real Pico-8 colours into Layer 2 at all -- every game so far has been
 * drawing with whatever the Next's own default boot palette happens to
 * contain at indices 0-15, not Pico-8's actual colours (confirmed by
 * grepping the whole project for REG_PALETTE_CONTROL/VALUE writes against
 * the Layer 2 bank: there were none, anywhere, before this). */
extern const uint8_t pico8_palette[16][3];

#endif
