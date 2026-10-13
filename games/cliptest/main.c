/* Proves pico_clip()/pico_clip_reset() (pico_gfx.h) and pico_sset()
 * (pico_sprite.h) on real hardware, not just the native host-side clip-
 * math harness used to verify the clipping logic itself before this ran
 * anywhere near a Z80. Visual: a full-screen rectfill in colour 8, then
 * pico_clip() narrowed to a small window before a SECOND full-screen
 * rectfill in colour 11 -- only the clip window should show colour 11,
 * everything else should still show the first colour 8 fill underneath.
 * After pico_clip_reset(), a small marker rect is drawn OUTSIDE the old
 * clip window to confirm drawing isn't still restricted. sset()/sget()
 * get the project's usual self-checking PASS/FAIL text. */

#include <intrinsic.h>
#include <stdint.h>
#include "pico_video.h"
#include "pico_gfx.h"
#include "pico_sprite.h"

/* Genuinely mutable storage -- pico_sset()'s own documented requirement
 * (pico_sprite.h), unlike a game's real spritesheet, which is normally
 * `static const`. */
static uint8_t test_sheet[64];

static void draw_all(void) {
    uint8_t got;
    uint8_t pass;

    pico_cls(0);

    pico_rectfill(0, 0, 127, 127, 8);     /* fills the whole screen green */
    pico_clip(40, 40, 87, 87);            /* narrow window, inclusive corners */
    pico_rectfill(0, 0, 127, 127, 11);    /* should only land inside the window */
    pico_clip_reset();
    pico_rect(2, 2, 10, 10, 7);           /* drawn AFTER reset -- must not be clipped away */

    pico_spr_init(test_sheet, 8, 8);
    pico_sset(3, 4, 9);
    got = pico_sget(3, 4);
    pass = (got == 9);
    pico_print(pass ? "SSET/SGET PASS" : "SSET/SGET FAIL", 2, 115, pass ? 11 : 8);
}

int main(void) {
    intrinsic_ei();
    pico_video_init(128, 128);

    draw_all();
    pico_flip();
    draw_all();

    for (;;) intrinsic_halt();
}
