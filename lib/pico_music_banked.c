#include "pico_music_banked.h"
#include "pico_bank.h"
#include <arch/zxn.h>

extern void pico_music_banked_init_asm(const void *song, uint8_t subsong) __z88dk_callee;
extern void pico_music_banked_play_asm(void) __z88dk_callee;
extern void pico_music_banked_stop_asm(void) __z88dk_callee;

/* Defensive re-assert of 14MHz turbo mode (NextReg $07) after every call
 * into the player's own di/call/ei/ret-wrapped entry points (lib/
 * pico_music_player.asm) -- a REAL regression found the hard way, not a
 * precaution: wrapping those calls in di/ei (needed, see that file's own
 * comment, to stop song-loop corruption) reliably knocks NextReg $07 back
 * to its cold-boot default ($00, 3.5MHz) instead of leaving it at the
 * 14MHz pico_video_init() sets at startup -- confirmed via a direct A/B
 * ZRCP test (di/ei in -> NextReg7 reads $00 every time checked after
 * boot; di/ei reverted -> reads $33/14MHz, matching both this project's
 * own pre-DI/EI history and a sibling project, Harvester, which doesn't
 * have this di/ei change and reads $33 correctly). Root mechanism not
 * fully understood (plausibly some interaction with NextZXOS's own
 * interrupt housekeeping noticing the brief di/ei gaps), but the
 * practical effect is severe: the WHOLE game, not just music, runs at
 * roughly a quarter speed, perceived by ear as "music tempo is off" --
 * which is what actually prompted this investigation. Re-writing
 * NextReg $07 once per call is cheap (same class of cost as the
 * Turbosound chip-select writes already happening here) and keeps up
 * with whatever cadence the reset happens on, since it's the SAME calls
 * (play_frame, every frame) that trigger it. */
static void reassert_turbo_mode(void) {
    ZXN_WRITE_REG(REG_TURBO_MODE, 0x03);
}

void pico_music_banked_init(uint8_t bank_page, const void *song, uint8_t subsong) {
    uint8_t prev = pico_bank_page_in(bank_page);
    pico_music_banked_init_asm(song, subsong);
    pico_bank_page_out(prev);
    reassert_turbo_mode();
}

void pico_music_banked_play_frame(uint8_t bank_page) {
    uint8_t prev = pico_bank_page_in(bank_page);
    pico_music_banked_play_asm();
    pico_bank_page_out(prev);
    reassert_turbo_mode();
}

void pico_music_banked_stop(uint8_t bank_page) {
    uint8_t prev = pico_bank_page_in(bank_page);
    pico_music_banked_stop_asm();
    pico_bank_page_out(prev);
    reassert_turbo_mode();
}
