#include "pico_ayfx.h"
#include <arch/zxn.h>

extern void pico_ayfx_init_asm(void) __z88dk_callee;
extern void pico_ayfx_play_asm(uint8_t effect) __z88dk_callee;
extern void pico_ayfx_frame_asm(void) __z88dk_callee;
extern uint8_t ay_chip_select;

/* Same literal values confirmed correct in lib/pico_sfx_synth.c's own
 * comment (z88dk's <arch/zxn.h> IT_SELECT_PSG_0/1 symbols turned out
 * wrong -- they decode to ordinary, harmless AY register indices 14/15,
 * not the real chip-select command, which needs the upper nibble SET).
 * Independently reconfirmed here by reading THIS player's own
 * `ay_chip_select` convention (lib/pico_ayfx_player.asm, vendored from
 * a real, shipped Next AYFX port) -- it expects the exact same raw
 * value ($FE for chip 1), not a 0x0E-style index, a second,
 * independent source agreeing with the correction. */
#define TURBOSOUND_SELECT_PSG_0 0xFF
#define TURBOSOUND_SELECT_PSG_1 0xFE

static uint8_t turbosound_ready = 0;

static void ensure_turbosound(void) {
    if (turbosound_ready) return;
    ZXN_WRITE_REG(REG_PERIPHERAL_3,
                  (uint8_t)(ZXN_READ_REG(REG_PERIPHERAL_3) | RP3_ENABLE_TURBOSOUND));
    turbosound_ready = 1;
}

void pico_ayfx_init(void) {
    ensure_turbosound();
    ay_chip_select = TURBOSOUND_SELECT_PSG_1;
    pico_ayfx_init_asm();
    IO_TURBOSOUND = TURBOSOUND_SELECT_PSG_0;
}

void pico_ayfx_play(uint8_t effect) {
    /* AFXPLAY only updates channel-descriptor bookkeeping in RAM -- no
     * AY register writes happen until the next pico_ayfx_frame() call
     * (confirmed by reading lib/pico_ayfx_player.asm), so no chip-select
     * needed around this one. */
    pico_ayfx_play_asm(effect);
}

void pico_ayfx_frame(void) {
    pico_ayfx_frame_asm();
    IO_TURBOSOUND = TURBOSOUND_SELECT_PSG_0;
}
