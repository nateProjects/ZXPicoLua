/* M1/M2 demo: exercises every pico_gfx.c and pico_sprite.c primitive in
 * one static scene within a 128x128 logical canvas (Pico-8's own native
 * resolution), so a correct render proves rect/rectfill, line,
 * circ/circfill, oval/ovalfill, fillp dithering, the bitmap-font print(),
 * and spr()/sspr() (cell selection, transparency, flip_x/flip_y, and
 * nearest-neighbour scaling) all work together -- not just individually.
 * See docs/gfx-notes.md and docs/sprite-notes.md for the verification
 * screenshots. */

#include <intrinsic.h>
#include "pico_video.h"
#include "pico_gfx.h"
#include "pico_sprite.h"
#include "pico_sprite_data.h"

int main(void) {
    intrinsic_ei(); /* --tbblue-fast-boot-mode leaves interrupts off */

    pico_video_init(128, 128);

    pico_print("ZXPICOLUA", 4, 4, 7);
    pico_print("M1 GFX DEMO", 4, 12, 6);

    pico_rectfill(4, 24, 36, 48, 2);
    pico_rect(4, 24, 36, 48, 7);

    pico_line(44, 24, 76, 48, 8);
    pico_line(76, 24, 44, 48, 8);

    pico_circfill(100, 36, 12, 9);
    pico_circ(100, 36, 12, 7);

    pico_ovalfill(4, 60, 60, 84, 11);
    pico_oval(4, 60, 60, 84, 7);

    /* fillp: a 50% checkerboard dither, two-colour (primary 1, secondary
     * 10 via the >15 colour-argument convention). 0xA5A5 alternates both
     * per-column AND per-row (row0/2 = 1010, row1/3 = 0101) for a true
     * checkerboard -- 0xAAAA (every row = 1010) looked right on paper but
     * actually renders as vertical stripes, caught by screenshot
     * verification, not by inspection; see docs/gfx-notes.md. */
    pico_fillp(0xA5A5, 0);
    pico_rectfill(70, 60, 124, 84, 1 | (10 << 4));
    pico_fillp(0, 0);

    pico_cursor(4, 90);
    pico_print_at_cursor("SPR BELOW", 10);

    /* spr(): a symmetric sprite (the ball, cell 0 -- a flip-correctness
     * control, should look identical regardless of flip args) next to an
     * asymmetric one (the "L", cell 1) drawn plain and in all three flip
     * combinations, so flip_x/flip_y are visually provable, not just
     * assumed to do something. */
    pico_spr_init(demo_sheet_data, DEMO_SHEET_W, DEMO_SHEET_H);
    pico_spr_transparent(0);

    pico_spr(0, 4, 100, 1, 1, 0, 0);  /* ball */
    pico_spr(1, 16, 100, 1, 1, 0, 0); /* L, plain */
    pico_spr(1, 28, 100, 1, 1, 1, 0); /* L, flip_x */
    pico_spr(1, 40, 100, 1, 1, 0, 1); /* L, flip_y */
    pico_spr(1, 52, 100, 1, 1, 1, 1); /* L, flip_x + flip_y */

    /* sspr(): the same "L" cell (sheet coords 8,0,8,8 -- matches
     * pico_spr()'s own math for cell 1) nearest-neighbour scaled 3x. */
    pico_sspr(8, 0, 8, 8, 80, 98, 24, 24, 0, 0);

    for (;;) {
        intrinsic_halt();
    }
}
