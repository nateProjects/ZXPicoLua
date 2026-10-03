/* M0 spike: prove the Layer 2 256x192 framebuffer access mechanism (see
 * lib/pico_video.c and docs/layer2-notes.md). Declares a 128x128 logical
 * canvas -- Pico-8's own native resolution -- and draws 8 vertical colour
 * bars filling it, so a correct render shows the bars centred with a
 * black letterbox border (64px left/right, 32px top/bottom) around them:
 * proves both the page-crossing address resolution *and* the logical-
 * canvas origin offset pico_video_init(128,128) sets up, in one test. */

#include <intrinsic.h>
#include "pico_video.h"

int main(void) {
    intrinsic_ei(); /* --tbblue-fast-boot-mode leaves interrupts off */

    pico_video_init(128, 128);

    /* x must be wider than uint8_t: pico_screen_w can be up to 256 (a
     * native-canvas game), and an 8-bit counter can never reach 256, so
     * "x < pico_screen_w" would never be false and the loop would never
     * terminate (caught via the compiler's own -Wlimited-range warning on
     * an earlier uint8_t version of this same loop -- worth remembering
     * for every future full-width loop in pico_gfx.c too, not just here). */
    for (uint16_t x = 0; x < pico_screen_w; x++) {
        uint8_t color = (uint8_t)(x / 16); /* 8 bars, 16px each, colours 0-7 */
        for (uint8_t y = 0; y < pico_screen_h; y++) {
            pico_pset((uint8_t)x, y, color);
        }
    }

    for (;;) {
        intrinsic_halt();
    }
}
