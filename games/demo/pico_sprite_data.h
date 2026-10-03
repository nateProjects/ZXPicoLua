#ifndef DEMO_SPRITE_DATA_H
#define DEMO_SPRITE_DATA_H

#include <stdint.h>

/* M2 demo spritesheet: 16x16 (a 2x2 grid of 8x8 Pico-8-style sprite
 * cells). Cell 0 (top-left) is a filled ball (colour 8, symmetric -- a
 * flip-correctness control). Cell 1 (top-right) is an "L" shape (colour
 * 11), deliberately asymmetric under both horizontal and vertical flip,
 * so flip_x/flip_y actually prove something visually instead of looking
 * identical either way. Cells 2/3 (bottom row) are blank. Colour 0 is
 * transparent (pico_palt()'s default). Generated
 * programmatically from an 8x8 ASCII pattern, not hand-transcribed as a
 * flat array -- see the project history for the generating script. */

#define DEMO_SHEET_W 16
#define DEMO_SHEET_H 16

extern const uint8_t demo_sheet_data[DEMO_SHEET_W * DEMO_SHEET_H];

#endif
