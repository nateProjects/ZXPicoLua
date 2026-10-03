/* M2.5 demo: interactive input test, now double-buffered (pico_flip(),
 * pico_video.h) after real human testing showed visible tearing on the
 * moving ball -- even after bumping the Next to 14MHz, which only
 * narrowed the problem, since the real cause was drawing directly into
 * the buffer the display was simultaneously scanning out, not raw speed.
 * See docs/input-notes.md's "double buffering" section for the full
 * story.
 *
 * Double buffering means pico_pset() always writes to a back buffer that
 * alternates between two physical banks each pico_flip() -- so each
 * physical buffer only reflects what was drawn into it *two* flips ago,
 * not one. Every piece of animated state here (the ball's position, each
 * indicator's lit state) is therefore tracked per-buffer (the [2]-sized
 * arrays below, indexed by buf_idx), not as a single current value --
 * get this wrong and you erase/redraw against the wrong buffer's stale
 * content instead of this buffer's own. Static content (the instruction
 * text) is drawn once into each buffer during startup priming, since
 * pico_video_init() only guarantees both buffers start cleared, not with
 * any particular content -- a double-buffered game must prime both
 * itself before relying on incremental (draw-the-difference) updates. */

#include <intrinsic.h>
#include "pico_video.h"
#include "pico_gfx.h"
#include "pico_sprite.h"
#include "pico_sprite_data.h"
#include "pico_input.h"
#include "m3_test.h"

#define BALL_MIN 0
#define BALL_MAX 120 /* 128 - 8 (sprite size), keeps the ball fully on screen */
#define MOVE_SPEED 3 /* pixels/frame while a direction is held (was 1) */

static void draw_static_content(void) {
    pico_print("MOVE: Q A O P", 4, 4, 7);
    pico_print("O:Z  X:X", 4, 12, 7);
}

int main(void) {
    uint8_t px = 60, py = 60; /* authoritative game-state position */
    uint8_t new_px, new_py, new_o, new_x;
    uint8_t buf_idx;
    uint8_t ball_x[2], ball_y[2], o_lit[2], x_lit[2];
    uint8_t i, m3_pass;

    intrinsic_ei(); /* --tbblue-fast-boot-mode leaves interrupts off */

    pico_video_init(128, 128);
    pico_spr_init(demo_sheet_data, DEMO_SHEET_W, DEMO_SHEET_H);
    /* pico_palt() (M3) defaults to colour 0 transparent already -- no
     * explicit call needed unless a game wants something different. */

    /* Runs exactly once (not inside the priming loop below) -- see
     * m3_test.h for why: it includes a cartdata boot counter that would
     * double-count if run once per buffer. */
    m3_pass = m3_run_checks();

    /* Prime both physical buffers with identical starting content. */
    buf_idx = 0;
    for (i = 0; i < 2; i++) {
        draw_static_content();
        m3_draw(m3_pass);
        pico_spr(0, px, py, 1, 1, 0, 0);
        ball_x[buf_idx] = px;
        ball_y[buf_idx] = py;
        o_lit[buf_idx] = 0;
        x_lit[buf_idx] = 0;
        pico_flip();
        buf_idx ^= 1;
    }

    for (;;) {
        intrinsic_halt(); /* blocks until the next 50Hz interrupt/frame */
        pico_input_update();

        new_px = px;
        new_py = py;
        if (pico_btn(PICO_BTN_LEFT))
            new_px = (new_px >= BALL_MIN + MOVE_SPEED) ? new_px - MOVE_SPEED : BALL_MIN;
        if (pico_btn(PICO_BTN_RIGHT))
            new_px = (new_px <= BALL_MAX - MOVE_SPEED) ? new_px + MOVE_SPEED : BALL_MAX;
        if (pico_btn(PICO_BTN_UP))
            new_py = (new_py >= BALL_MIN + MOVE_SPEED) ? new_py - MOVE_SPEED : BALL_MIN;
        if (pico_btn(PICO_BTN_DOWN))
            new_py = (new_py <= BALL_MAX - MOVE_SPEED) ? new_py + MOVE_SPEED : BALL_MAX;

        /* Compare against THIS buffer's own last-known position (not the
         * authoritative px/py) -- this buffer may still show where the
         * ball was two flips ago, which is exactly what needs erasing. */
        if (new_px != ball_x[buf_idx] || new_py != ball_y[buf_idx]) {
            pico_rectfill(ball_x[buf_idx], ball_y[buf_idx], ball_x[buf_idx] + 7, ball_y[buf_idx] + 7, 0);
            pico_spr(0, new_px, new_py, 1, 1, 0, 0);
            ball_x[buf_idx] = new_px;
            ball_y[buf_idx] = new_py;
        }
        px = new_px;
        py = new_py;

        new_o = pico_btn(PICO_BTN_O) ? 1 : 0;
        if (new_o != o_lit[buf_idx]) {
            pico_rectfill(116, 4, 123, 11, new_o ? 8 : 0);
            o_lit[buf_idx] = new_o;
        }
        new_x = pico_btnp(PICO_BTN_X) ? 1 : 0;
        if (new_x != x_lit[buf_idx]) {
            pico_rectfill(116, 16, 123, 23, new_x ? 11 : 0);
            x_lit[buf_idx] = new_x;
        }

        pico_flip();
        buf_idx ^= 1;
    }
}
