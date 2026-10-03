#ifndef PICO_VIDEO_H
#define PICO_VIDEO_H

#include <stdint.h>

/* Classic ZX Spectrum Next Layer 2 resolution: a true linear framebuffer,
 * 1 byte = 1 pixel = palette index (0-255). This is the closest Next
 * hardware analogue to PicoJS's canvas -- see docs/layer2-notes.md for the
 * M0 spike that proved out the access mechanism below. */
#define PICO_SCREEN_W 256
#define PICO_SCREEN_H 192

/* Sets up and shows the Layer 2 display, clears it to colour 0. Must be
 * called once before any other pico_video_* call. */
void pico_video_init(void);

/* Fills the whole framebuffer with a single palette index. */
void pico_cls(uint8_t color);

/* Writes/reads a single pixel's palette index. x in [0,255], y in [0,191]
 * -- out-of-range coordinates are not checked (same as Pico-8/PicoJS). */
void pico_pset(uint8_t x, uint8_t y, uint8_t color);
uint8_t pico_pget(uint8_t x, uint8_t y);

#endif
