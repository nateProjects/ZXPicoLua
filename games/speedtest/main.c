/* speedtest -- incremental, from-scratch performance staging ground.
 *
 * Built at the user's explicit request after several rounds of
 * optimizing/debugging the full picodiver game in place kept hitting
 * mysterious, reproducible crashes when adding seemingly-safe
 * "redraw on change" logic -- rather than keep guessing inside that
 * large, fragile program (near-zero stack/BSS margin, deep call chains),
 * start from a known-fast baseline (one sprite moving -- already proven
 * fast) and add ONE capability at a time, measuring real achieved
 * ticks/sec at each stage via STAGE, so we can see exactly which
 * addition (if any) costs real time, and catch any crash immediately
 * in a much simpler, more debuggable program.
 *
 * STAGE is set via -DSTAGE=N at build time:
 *   0 - one sprite moving over a cls() background (baseline)
 *   1 - + a static tiled background the sprite must erase/redraw against
 *   2 - + a second sprite and a pixel-perfect collision check every tick
 *   3 - + occasional (every 20 ticks) text/icon redraw-on-change --
 *       the exact pattern that crashed repeatedly in picodiver, to see
 *       if the pattern itself is at fault or something specific to
 *       picodiver's own code.
 *
 * Runs at a real, fixed 50Hz pace (one intrinsic_halt() per tick, not
 * picodiver's halved 25Hz) so "ticks/sec" directly reads off against the
 * Next's own vertical interrupt -- hitting 50 ticks/sec means we're
 * fully keeping up with zero slack, not just "fast". */

#include <intrinsic.h>
#include <string.h>
#include "pico_video.h"
#include "pico_gfx.h"
#include "pico_sprite.h"
#include "pico_map.h"
#include "pico_math.h"

#ifndef STAGE
#define STAGE 0
#endif

/* --- minimal sprite sheet: 2 8x8 tiles, 16x8 total ---------------------
 * tile 0: all colour 0 (transparent by default palt()) -- used as map
 * filler/background tile. tile 1: a solid colour 8 (red) square -- the
 * moving sprite. */
static const uint8_t sheet[16 * 8] = {
    /* row 0 */ 0,0,0,0,0,0,0,0,  8,8,8,8,8,8,8,8,
    /* row 1 */ 0,0,0,0,0,0,0,0,  8,8,8,8,8,8,8,8,
    /* row 2 */ 0,0,0,0,0,0,0,0,  8,8,8,8,8,8,8,8,
    /* row 3 */ 0,0,0,0,0,0,0,0,  8,8,8,8,8,8,8,8,
    /* row 4 */ 0,0,0,0,0,0,0,0,  8,8,8,8,8,8,8,8,
    /* row 5 */ 0,0,0,0,0,0,0,0,  8,8,8,8,8,8,8,8,
    /* row 6 */ 0,0,0,0,0,0,0,0,  8,8,8,8,8,8,8,8,
    /* row 7 */ 0,0,0,0,0,0,0,0,  8,8,8,8,8,8,8,8,
};

#if STAGE >= 1
/* 16x16-cell checkerboard map, alternating tile 0 (blank) / tile 1
 * (solid) -- a plain non-uniform background the moving sprite has to
 * erase/redraw against, exactly like picodiver's ocean. */
static uint8_t cmap[16 * 16];
static void build_checkerboard(void) {
    uint8_t x, y;
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            cmap[(uint16_t)y * 16 + x] = (uint8_t)(((x + y) & 1) ? 1 : 0);
}
#endif

/* Ticks completed, read via ZRCP between two known-apart t-states
 * samples (get-tstates-partial) to compute real ticks/sec -- NOT read
 * via a BSS counter (several previous rounds found those unreliable
 * near the top of RAM); this program is tiny so margin should be large
 * regardless, but t-states is the hardware-accurate, zero-assumption
 * method proven earlier in this project. Still exposed as a global so a
 * human (or screenshot) can sanity-check visually if needed. */
static uint16_t ticks;

int main(void) {
    int16_t x = 0, y = 60;
    int8_t dx = 1;
#if STAGE >= 2
    int16_t x2 = 100, y2 = 60;
    int8_t dx2 = -1;
    uint8_t hit = 0;
#endif
#if STAGE >= 3
    uint8_t last_hit_drawn = 0xFF;
#endif

    intrinsic_ei();
    pico_video_init(128, 128);
    pico_spr_init(sheet, 16, 8);
#if STAGE >= 1
    build_checkerboard();
    pico_map_init(cmap, 16, 16);
#endif

    for (;;) {
        intrinsic_halt(); /* real 50Hz pace -- see this file's header */

#if STAGE == 0
        pico_cls(0);
        pico_spr(1, x, y, 1, 1, 0, 0);
#elif STAGE >= 1
        pico_cls(0);
        pico_map(0, 0, 0, 0, 16, 16);
        pico_spr(1, x, y, 1, 1, 0, 0);
#if STAGE >= 2
        pico_spr(1, x2, y2, 1, 1, 0, 0);
#endif
#endif

        x = (int16_t)(x + dx);
        if (x <= 0 || x >= 120) dx = (int8_t)-dx;
        y = 60;

#if STAGE >= 2
        x2 = (int16_t)(x2 + dx2);
        if (x2 <= 0 || x2 >= 120) dx2 = (int8_t)-dx2;
        y2 = 60;

        /* Pixel-perfect-ish bounding check, cheap: just AABB overlap,
         * same spirit as picodiver's pixel_collide() but without the
         * per-pixel double loop -- added here as its own stage so its
         * cost is isolated from the per-pixel version if that's added
         * later. */
        hit = (uint8_t)(x < x2 + 8 && x + 8 > x2 && y < y2 + 8 && y + 8 > y2);
#endif

#if STAGE >= 3
        if (hit != last_hit_drawn) {
            pico_rectfill(0, 120, 40, 127, hit ? 8 : 0);
            last_hit_drawn = hit;
        }
#endif

        pico_flip();
        ticks++;
    }
}
