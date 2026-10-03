/* Reads the classic 48K-compatible keyboard matrix via z88dk's portable
 * <input.h> (in_LookupKey()/in_KeyPressed()), not a hand-rolled Next-
 * specific scan -- the Next's PS/2 keyboard controller emulates this same
 * matrix for compatibility, so this works unmodified on real hardware and
 * in ZEsarUX's tbblue emulation. next-resources' TrainyardExpress rolls
 * its own NextReg $B0/$B1-based reader instead, but that's there for
 * *extended*-key support (a full PS/2 keyboard's keys beyond the classic
 * 40-key matrix) -- unnecessary for Pico-8-style btn()/btnp(), which only
 * needs six ordinary letter keys, all within the classic matrix. */

#include <input.h>
#include "pico_input.h"

#define PICO_NUM_BTNS 6

static uint16_t scancodes[PICO_NUM_BTNS];
static uint8_t held[PICO_NUM_BTNS];
static uint8_t held_prev[PICO_NUM_BTNS];
static uint8_t initialized = 0;

static void ensure_init(void) {
    if (initialized) return;
    scancodes[PICO_BTN_LEFT]  = in_LookupKey('o');
    scancodes[PICO_BTN_RIGHT] = in_LookupKey('p');
    scancodes[PICO_BTN_UP]    = in_LookupKey('q');
    scancodes[PICO_BTN_DOWN]  = in_LookupKey('a');
    scancodes[PICO_BTN_O]     = in_LookupKey('z');
    scancodes[PICO_BTN_X]     = in_LookupKey('x');
    initialized = 1;
}

void pico_input_update(void) {
    uint8_t i;
    ensure_init();
    for (i = 0; i < PICO_NUM_BTNS; i++) {
        held_prev[i] = held[i];
        held[i] = in_KeyPressed(scancodes[i]) ? 1 : 0;
    }
}

uint8_t pico_btn(uint8_t i) {
    if (i >= PICO_NUM_BTNS) return 0;
    return held[i];
}

uint8_t pico_btnp(uint8_t i) {
    if (i >= PICO_NUM_BTNS) return 0;
    return held[i] && !held_prev[i];
}
