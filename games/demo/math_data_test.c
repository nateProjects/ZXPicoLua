#include <stdint.h>
#include "math_data_test.h"
#include "pico_math.h"
#include "pico_video.h"
#include "pico_gfx.h"
#include "pico_map.h"
#include "pico_data.h"

/* 4x4 tile grid using the existing demo spritesheet (pico_sprite_data.h):
 * 1 = the "L" shape, 0 = empty (Pico-8 convention: map tile value 0 is
 * always skipped, regardless of what sprite index 0 actually contains --
 * ported from PicoJS's own picoJS.map, see pico_map.c). Checkerboard
 * pattern doubles as confirmation that 0-tiles really are skipped (gaps
 * visible, not drawn as sprite 0's own graphic -- the ball). */
static uint8_t map_data[16] = {
    1, 0, 1, 0,
    0, 1, 0, 1,
    1, 0, 1, 0,
    0, 1, 0, 1,
};

uint8_t math_data_run_checks(void) {
    uint8_t pass = 1;
    int16_t boot_count;

    if (pico_mid(2, 10, 5) != 5) pass = 0;
    if (pico_min(3, 7) != 3) pass = 0;
    if (pico_max(3, 7) != 7) pass = 0;
    if (pico_abs(-5) != 5) pass = 0;
    if (pico_sgn(-3) != -1) pass = 0;
    if (pico_sgn(0) != 1) pass = 0; /* Pico-8 quirk: sgn(0) is 1, not 0 */
    if (pico_band(0x0F, 0x03) != 0x03) pass = 0;
    if (pico_bor(0x0F, 0x30) != 0x3F) pass = 0;
    if (pico_bxor(0xFF, 0x0F) != 0xF0) pass = 0;
    if (pico_shl(1, 3) != 8) pass = 0;
    if (pico_shr(-8, 1) != -4) pass = 0;

    /* Checked against an independent Python port of the same RNG
     * algorithm (docs/math-notes.md), not just whatever this C port
     * happens to produce. */
    pico_srand(1);
    if (pico_rnd(10) != 7) pass = 0;

    /* cartdata round-trip: a persistent boot counter. Not part of the
     * pass/fail verdict above (it's expected to differ every run, not
     * match one fixed value) -- persistence verified separately across
     * repeated runs, see docs/data-notes.md. A failed save still fails
     * the overall check, though. */
    pico_cartdata("TEST");
    boot_count = pico_dget(0);
    boot_count++;
    pico_dset(0, boot_count);
    if (!pico_cartdata_save()) pass = 0;

    return pass;
}

void math_data_draw(uint8_t pass) {
    pico_pal(8, 11);
    pico_rectfill(4, 40, 11, 47, 8);  /* remapped -- should render colour 11 */
    pico_pal_reset();
    pico_rectfill(14, 40, 21, 47, 8); /* unremapped control -- stays colour 8 */

    pico_map_init(map_data, 4, 4);
    pico_map(0, 0, 4, 92, 4, 4); /* 4x4 cells, 8px each = 32x32 at (4,92) */

    pico_rectfill(100, 100, 115, 115, pass ? 11 : 8); /* 11=blue (pass), 8=green (fail) -- just needs to be visibly distinct, not literally red/green */
}
