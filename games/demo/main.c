/* M2.5 demo: interactive input test. A ball sprite moves around the
 * 128x128 logical canvas under btn()/btnp() (pico_input.h), plus two
 * small indicator boxes: one lit solid while button O is held, the other
 * flashing for exactly one frame on each button X *press* (not hold) --
 * so held-vs-just-pressed semantics (pico_btn() vs pico_btnp()) are both
 * independently visible, not just "does any input work at all". Keys:
 * Q/A/O/P for up/down/left/right (classic Spectrum "QAOP" convention --
 * the matrix this reads has no real arrow-key signals of its own), Z/X
 * for the two action buttons (Pico-8's own real keyboard defaults).
 *
 * The M1/M2 static shape/sprite showcase this replaced already did its
 * job (screenshotted and verified, see docs/gfx-notes.md and
 * docs/sprite-notes.md) -- no need to keep re-rendering it every frame
 * alongside a focused input test. */

#include <intrinsic.h>
#include "pico_video.h"
#include "pico_gfx.h"
#include "pico_sprite.h"
#include "pico_sprite_data.h"
#include "pico_input.h"

#define BALL_MIN 0
#define BALL_MAX 120 /* 128 - 8 (sprite size), keeps the ball fully on screen */

int main(void) {
    uint8_t px = 60, py = 60;
    uint8_t new_px, new_py;
    uint8_t o_lit = 0, x_lit = 0, new_o_lit, new_x_lit;

    intrinsic_ei(); /* --tbblue-fast-boot-mode leaves interrupts off */

    pico_video_init(128, 128);
    pico_spr_init(demo_sheet_data, DEMO_SHEET_W, DEMO_SHEET_H);
    pico_spr_transparent(0);

    pico_print("MOVE: Q A O P", 4, 4, 7);
    pico_print("O:Z  X:X", 4, 12, 7);

    pico_spr(0, px, py, 1, 1, 0, 0);

    for (;;) {
        intrinsic_halt(); /* blocks until the next 50Hz interrupt/frame */
        pico_input_update();

        /* Only touch the framebuffer where something actually changed --
         * an unconditional erase+redraw every single frame (even while
         * stationary) let the video hardware's own raster scanout
         * occasionally catch a mid-redraw frame, visibly tearing the
         * sprite -- same class of issue dark-room-next's own
         * docs/tilemap-notes.md hit and fixed for text, for the same
         * reason: redraw on change, not unconditionally every frame. */
        new_px = px;
        new_py = py;
        if (pico_btn(PICO_BTN_LEFT)  && new_px > BALL_MIN) new_px--;
        if (pico_btn(PICO_BTN_RIGHT) && new_px < BALL_MAX) new_px++;
        if (pico_btn(PICO_BTN_UP)    && new_py > BALL_MIN) new_py--;
        if (pico_btn(PICO_BTN_DOWN)  && new_py < BALL_MAX) new_py++;

        if (new_px != px || new_py != py) {
            pico_rectfill(px, py, px + 7, py + 7, 0); /* erase old position */
            px = new_px;
            py = new_py;
            pico_spr(0, px, py, 1, 1, 0, 0);
        }

        /* O: solid while held. X: lit for exactly the one frame it was
         * just pressed, otherwise cleared. */
        new_o_lit = pico_btn(PICO_BTN_O) ? 1 : 0;
        if (new_o_lit != o_lit) {
            pico_rectfill(116, 4, 123, 11, new_o_lit ? 8 : 0);
            o_lit = new_o_lit;
        }
        new_x_lit = pico_btnp(PICO_BTN_X) ? 1 : 0;
        if (new_x_lit != x_lit) {
            pico_rectfill(116, 16, 123, 23, new_x_lit ? 11 : 0);
            x_lit = new_x_lit;
        }
    }
}
