/* M0 spike: prove the Layer 2 256x192 framebuffer access mechanism (see
 * lib/pico_video.c and docs/layer2-notes.md) by drawing a static test
 * pattern -- 8 vertical colour bars, one per low palette index, each
 * spanning the full 192-pixel height and crossing every one of the 6
 * framebuffer pages at least once (32px-wide bars over a 256px-wide
 * screen). If this renders as solid, correctly-ordered bars (confirmed by
 * screenshot pixel dump, not eyeballing -- see docs/layer2-notes.md), the
 * page-crossing address resolution in pico_pset() is proven correct. */

#include <intrinsic.h>
#include "pico_video.h"

int main(void) {
    intrinsic_ei(); /* --tbblue-fast-boot-mode leaves interrupts off */

    pico_video_init();

    /* x must be wider than uint8_t: PICO_SCREEN_W is 256, and an 8-bit
     * counter can never reach 256, so "x < PICO_SCREEN_W" would never be
     * false and this loop would never terminate (caught via the
     * compiler's own -Wlimited-range warning on the original uint8_t
     * version -- worth remembering for every future full-width loop in
     * pico_gfx.c too, not just here). */
    for (uint16_t x = 0; x < PICO_SCREEN_W; x++) {
        uint8_t color = x / 32; /* 8 bars, 32px each, colours 0-7 */
        for (uint8_t y = 0; y < PICO_SCREEN_H; y++) {
            pico_pset((uint8_t)x, y, color);
        }
    }

    for (;;) {
        intrinsic_halt();
    }
}
