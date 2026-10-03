/* M1 demo: exercises every pico_gfx.c primitive in one static scene within
 * a 128x128 logical canvas (Pico-8's own native resolution), so a correct
 * render proves rect/rectfill, line, circ/circfill, oval/ovalfill, fillp
 * dithering, and the bitmap-font print() all work together -- not just
 * individually. See docs/gfx-notes.md for the verification screenshots. */

#include <intrinsic.h>
#include "pico_video.h"
#include "pico_gfx.h"

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

    pico_cursor(4, 96);
    pico_print_at_cursor("CURSOR LINE 1\nLINE 2", 10);

    for (;;) {
        intrinsic_halt();
    }
}
