/* Proves lib/pico_bank.h's page-in/page-out window works for banked
 * CODE, not just the banked const DATA every prior user has exercised
 * (picodiver_map_data.c, Harvester/Islander's sprite/map data). Compares
 * a banked_multiply() call (its own .c compiled to a banked module, see
 * the Makefile's GAME=bankcalltest block) against the same multiplication
 * done resident, as the known-correct baseline -- a wrong page-select or
 * a miscomputed banked address would show up as a mismatch, not silently
 * pass. Self-checking, verified via one ZRCP screenshot -- see
 * docs/bank-call-notes.md. */

#include <intrinsic.h>
#include <stdint.h>
#include "pico_video.h"
#include "pico_gfx.h"
#include "pico_math.h"
#include "pico_bank.h"
#include "banked_fn.h"

static void draw_all(void) {
    int16_t resident_result, banked_result;
    uint8_t prev_page, pass;
    char buf[8];

    pico_cls(0);

    resident_result = (int16_t)(37 * 53); /* 1961 -- known-correct baseline */

    prev_page = pico_bank_page_in(BANKED_FN_PHYSICAL_PAGE);
    banked_result = banked_multiply(37, 53);
    pico_bank_page_out(prev_page);

    pass = (banked_result == resident_result);

    pico_print("BANKED CODE CALL TEST", 2, 30, 7);
    pico_print("37*53 banked =", 2, 45, 6);
    pico_tostr_int(banked_result, buf);
    pico_print(buf, 90, 45, 6);
    pico_print(pass ? "PASS" : "FAIL", 2, 60, pass ? 11 : 8);
}

int main(void) {
    intrinsic_ei();
    pico_video_init(128, 128);

    /* Prime both physical buffers with identical content (no animation
     * here, draw once -- same pattern as games/fixedtest/main.c). */
    draw_all();
    pico_flip();
    draw_all();

    for (;;) intrinsic_halt();
}
