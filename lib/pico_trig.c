#include "pico_trig.h"

/* cos_table[i] = round(cos(2*pi*i/256) * 32767), Q0.15 fixed-point
 * (range -32767..32767 representing -1.0..1.0), generated programmatically
 * (not hand-transcribed -- same discipline as pico_font.c's own glyph
 * data) via:
 *
 *   python3 -c "
 *   import math
 *   for i in range(256):
 *       v = round(math.cos(2*math.pi*i/256.0) * 32767)
 *       print(max(-32768, min(32767, v)), end=',')"
 */
static const int16_t cos_table[256] = {
    32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285, 32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
    30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683, 27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731,
    23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868, 18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
    12539, 11793, 11039, 10278, 9512, 8739, 7962, 7179, 6393, 5602, 4808, 4011, 3212, 2410, 1608, 804,
    0, -804, -1608, -2410, -3212, -4011, -4808, -5602, -6393, -7179, -7962, -8739, -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530, -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790, -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
    -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971, -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285, -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683, -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
    -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868, -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
    -12539, -11793, -11039, -10278, -9512, -8739, -7962, -7179, -6393, -5602, -4808, -4011, -3212, -2410, -1608, -804,
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
};

/* Floor-mods x into one full turn (PICO_FIX_ONE = 65536 raw units), then
 * scales straight to a 0-255 table index -- PICO_FIX_ONE/256 == 256
 * exactly (both powers of 2), so this is a plain shift, no rounding
 * error. PICO_FIX_ONE is ALSO a power of 2, so masking the low 16 bits
 * (`& (PICO_FIX_ONE-1)`) is floor-mod by it for any x, positive or
 * negative -- a standard two's-complement identity, not C's own
 * truncating '%' (which needs an explicit negative-result correction,
 * and -- confirmed the hard way -- pulls in sccz80's 32-bit signed
 * division library, real measured cost on top of the table itself). */
static uint8_t cos_table_index(pico_fix_t x) {
    return (uint8_t)(((uint32_t)x & (PICO_FIX_ONE - 1)) >> 8);
}

pico_fix_t pico_cos(pico_fix_t x) {
    return (pico_fix_t)((int32_t)cos_table[cos_table_index(x)] << 1); /* Q0.15 -> Q16.16 */
}

pico_fix_t pico_sin(pico_fix_t x) {
    /* sin(t) = cos(t - 0.25) (standard identity, t in turns), then
     * Pico-8's own sign flip -- this file's own header's top comment. */
    pico_fix_t shifted = x - PICO_FIX_FRAC(1, 4);
    return (pico_fix_t)(-((int32_t)cos_table[cos_table_index(shifted)] << 1));
}
