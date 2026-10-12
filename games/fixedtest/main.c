/* Standalone test for lib/pico_math's new Q16.16 fixed-point functions
 * (pico_fix_mul/pico_fix_lerp), added for the Islander port's movement
 * physics -- see /Users/nat/.claude/plans/polished-churning-meerkat.md,
 * M1. Tested here in isolation, not inside Islander's own loop, per the
 * plan's own testing discipline. Self-checking: each case compares
 * against a hand-computed expected value with a small tolerance (integer
 * division in PICO_FIX_FRAC means exact equality isn't always possible)
 * and prints PASS/FAIL, verified via one ZRCP screenshot -- no simulated
 * input needed, so none of today's session's input-automation pitfalls
 * apply here. */

#include <intrinsic.h>
#include <stdint.h>
#include "pico_video.h"
#include "pico_gfx.h"
#include "pico_math.h"

static uint8_t pass_count, fail_count, row;

static int32_t iabs32(int32_t v) { return v < 0 ? -v : v; }

static void check(const char *label, pico_fix_t got, pico_fix_t want, pico_fix_t tol) {
    uint8_t ok = iabs32(got - want) <= tol;
    char buf[32];
    uint8_t i = 0;
    while (label[i]) { buf[i] = label[i]; i++; }
    buf[i++] = ':';
    buf[i++] = ' ';
    if (ok) { buf[i++] = 'P'; buf[i++] = 'A'; buf[i++] = 'S'; buf[i++] = 'S'; pass_count++; }
    else    { buf[i++] = 'F'; buf[i++] = 'A'; buf[i++] = 'I'; buf[i++] = 'L'; fail_count++; }
    buf[i] = 0;
    pico_print(buf, 2, (int16_t)(2 + row * 8), 7);
    row++;
}

static void draw_all(void) {
    pico_fix_t a, b, r;

    pico_cls(0);
    pass_count = 0;
    fail_count = 0;
    row = 0;

    /* PICO_FIX_INT: whole numbers round-trip exactly. */
    check("int(5)", PICO_FIX_INT(5), 5L << PICO_FIX_SHIFT, 0);

    /* PICO_FIX_FRAC(1,10) ~= fric=0.1 -- integer division gives 6553
     * (65536/10 truncated), 0.1*65536=6553.6 -- tolerance 1 covers the
     * truncation. */
    check("frac(1/10)", PICO_FIX_FRAC(1, 10), 6553, 1);

    /* mul: 2 * 3 == 6 exactly (both whole numbers, no truncation). */
    a = PICO_FIX_INT(2);
    b = PICO_FIX_INT(3);
    r = pico_fix_mul(a, b);
    check("mul(2,3)=6", r, PICO_FIX_INT(6), 0);

    /* lerp halfway from 0 to 100 at perc=0.5 -> 50 exactly (0.5 is exact
     * in binary fixed-point, no truncation anywhere in this one). */
    r = pico_fix_lerp(PICO_FIX_INT(0), PICO_FIX_INT(100), PICO_FIX_FRAC(1, 2));
    check("lerp(0,100,.5)=50", r, PICO_FIX_INT(50), 0);

    /* The actual islander-4.p8 case this all exists for: velocity
     * accumulating by fric=0.1 each tick toward spd=1, clamped. After 10
     * ticks of `if(hs<spd) hs+=fric`, hs should be at or just under 1.0
     * (10 * 0.0999... from the truncated fric constant, not quite 1.0
     * exactly -- matches the original's own float behaviour not landing
     * on a perfectly round number either). Tolerance covers the
     * accumulated truncation error from 10 additions of a truncated
     * constant. */
    {
        pico_fix_t hs = 0;
        pico_fix_t fric = PICO_FIX_FRAC(1, 10);
        pico_fix_t spd = PICO_FIX_INT(1);
        uint8_t t;
        for (t = 0; t < 10; t++) {
            if (hs < spd) hs += fric;
        }
        check("accel 10x0.1->~1", hs, PICO_FIX_INT(1), 50);
    }

    {
        char summary[24];
        uint8_t i = 0;
        const char *p = "pass=";
        while (*p) summary[i++] = *p++;
        summary[i++] = (char)('0' + pass_count);
        p = " fail=";
        while (*p) summary[i++] = *p++;
        summary[i++] = (char)('0' + fail_count);
        summary[i] = 0;
        pico_print(summary, 2, (int16_t)(2 + row * 8 + 8), 7);
    }
}

int main(void) {
    intrinsic_ei();
    pico_video_init(128, 128);

    /* pico_video_init() leaves BOTH physical buffers blank -- a caller
     * that wants content visible from the first frame must draw, flip,
     * then draw the SAME content again into the now-exposed other buffer
     * (pico_video.h's own documented pattern, games/demo/main.c's own
     * priming). This test has no animation (draw once, halt), so this is
     * the whole story -- no per-buffer tracking needed beyond this. */
    draw_all();
    pico_flip();
    draw_all();

    for (;;) intrinsic_halt();
}
