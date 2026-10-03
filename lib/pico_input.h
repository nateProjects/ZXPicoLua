#ifndef PICO_INPUT_H
#define PICO_INPUT_H

#include <stdint.h>

/* Pico-8 button indices -- single local player only in v1 (PicoJS
 * supports up to 4; multiplayer deferred until a real port needs it, same
 * "don't build speculatively" call as sprite-sheet banking/palt()). Keys
 * are Pico-8's own real keyboard defaults (Z/X for the two action
 * buttons) plus the classic Spectrum "QAOP" direction convention, since
 * the classic 48K-compatible keyboard matrix this reads (z88dk's
 * <input.h>, not a hand-rolled Next-specific scan) has no real arrow-key
 * signals of its own. */
#define PICO_BTN_LEFT  0
#define PICO_BTN_RIGHT 1
#define PICO_BTN_UP    2
#define PICO_BTN_DOWN  3
#define PICO_BTN_O     4 /* Pico-8's default "Z" key */
#define PICO_BTN_X     5 /* Pico-8's default "X" key */

/* Call once per frame (after intrinsic_halt(), same spot a Pico-8 game
 * calls btn()/btnp() from _update()) before pico_btn()/pico_btnp() --
 * samples the keyboard once and caches the previous frame's state for
 * btnp()'s edge detection. */
void pico_input_update(void);

/* btn(i) -- true if button i is currently held. */
uint8_t pico_btn(uint8_t i);

/* btnp(i) -- true only on the frame button i transitions from not-held to
 * held (one pulse per press). Pico-8's own btnp() also auto-repeats after
 * an initial hold delay; not implemented here -- v1 is edge-triggered
 * only. */
uint8_t pico_btnp(uint8_t i);

#endif
