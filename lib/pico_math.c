#include "pico_math.h"

int16_t pico_mid(int16_t a, int16_t b, int16_t c) {
    /* Branchless median-of-three, ported from PicoJS's picoJS.mid --
     * not a sort, the formula real carts use constantly for clamping,
     * e.g. mid(0, x, 127). */
    int16_t lo = (a < b) ? a : b;
    int16_t hi = (a > b) ? a : b;
    if (c < lo) return lo;
    if (c > hi) return hi;
    return c;
}

int16_t pico_min(int16_t a, int16_t b) { return (a < b) ? a : b; }
int16_t pico_max(int16_t a, int16_t b) { return (a > b) ? a : b; }
int16_t pico_abs(int16_t x) { return (x < 0) ? (int16_t)-x : x; }
int16_t pico_sgn(int16_t x) { return (x < 0) ? -1 : 1; }

int16_t pico_band(int16_t a, int16_t b) { return a & b; }
int16_t pico_bor(int16_t a, int16_t b)  { return a | b; }
int16_t pico_bxor(int16_t a, int16_t b) { return a ^ b; }
int16_t pico_bnot(int16_t a)            { return ~a; }
int16_t pico_shl(int16_t a, int16_t b)  { return (int16_t)(a << b); }
int16_t pico_shr(int16_t a, int16_t b)  { return (int16_t)(a >> b); }
uint16_t pico_lshr(uint16_t a, int16_t b) { return (uint16_t)(a >> b); }

/* --- Seeded RNG -------------------------------------------------------
 *
 * Ported directly from PicoJS's own reverse-engineered Pico-8 algorithm
 * (picoJS.js, "Seeded RNG" section) -- two 32-bit fixed-point state words
 * (hi/lo), rotate+add each call, reduce mod n via Lua-style modulo (result
 * takes the sign of the divisor, unlike C's own % which takes the sign of
 * the dividend -- they disagree exactly when the dividend is negative,
 * which happens constantly here). PicoJS's own comment notes this was
 * verified against an independent Python port for every seed/n
 * combination checked -- ported here as the exact same bit-level
 * arithmetic (uint32_t for the parts that must not trigger C's shift-into-
 * sign-bit undefined behaviour, int32_t everywhere else, relying on
 * wrapping overflow the same way every Z80-target C function in this
 * project already does), not reimplemented from the general description. */

static int32_t rng_hi = 0, rng_lo = 0;
static uint8_t rng_seeded = 0;

#define RNG_DEADBEEF ((int32_t)0xdeadbeefL)
#define RNG_BEAD29BA ((int32_t)0xbead29baL)
#define RNG_INIT_HI  ((int32_t)0x60009755L)
#define RNG_MASK7FFF ((int32_t)0x7fffffffL)
#define RNG_SIGNBIT  ((int32_t)0x80000000L)

static int32_t rng_to_raw(uint16_t n) {
    return (int32_t)((uint32_t)n << 16);
}

static int32_t rng_rotl16(int32_t x) {
    uint32_t u = (uint32_t)x;
    return (int32_t)((u << 16) | (u >> 16));
}

static int32_t rng_lua_mod(int32_t a, int32_t b) {
    int32_t r;
    if (b == 0) return 0;
    r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

static int32_t rng_advance(int32_t n_raw) {
    int32_t term1, term2;
    rng_hi = rng_rotl16(rng_hi) + rng_lo;
    rng_lo = rng_lo + rng_hi;
    if (n_raw == 0) return 0;
    term1 = (rng_hi < 0) ? rng_lua_mod(RNG_SIGNBIT - n_raw, n_raw) : 0;
    term2 = rng_lua_mod(rng_hi & RNG_MASK7FFF, n_raw);
    return rng_lua_mod(term1 + term2, n_raw);
}

void pico_srand(uint16_t seed) {
    int32_t seed_raw = rng_to_raw(seed);
    uint8_t i;
    if (seed_raw == 0) {
        rng_hi = RNG_INIT_HI;
        rng_lo = RNG_DEADBEEF;
    } else {
        rng_hi = seed_raw ^ RNG_BEAD29BA;
        rng_lo = seed_raw;
    }
    rng_seeded = 1;
    for (i = 0; i < 32; i++) rng_advance(65536L); /* warm-up, discarded */
}

uint16_t pico_rnd(uint16_t n) {
    if (!rng_seeded) pico_srand(0);
    if (n == 0) return 0;
    return (uint16_t)(rng_advance(rng_to_raw(n)) >> 16);
}

void pico_tostr_int(int16_t val, char *buf) {
    char tmp[6];
    uint8_t i = 0, j = 0;
    uint16_t v;
    uint8_t neg = 0;

    if (val < 0) { neg = 1; v = (uint16_t)(-(int32_t)val); } else v = (uint16_t)val;
    if (v == 0) { buf[0] = '0'; buf[1] = 0; return; }
    while (v > 0) { tmp[i++] = (char)('0' + (v % 10)); v = (uint16_t)(v / 10); }
    if (neg) buf[j++] = '-';
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = 0;
}

void pico_sub(const char *str, int16_t pos0, int16_t pos1, char *out) {
    int16_t len = 0;
    int16_t i;
    while (str[len] != 0) len++; /* strlen -- avoids pulling in <string.h> for one call */

    if (pos0 < 0) pos0 = (int16_t)(len + pos0 + 1);
    if (pos1 < 0) pos1 = (int16_t)(len + pos1 + 1);
    if (pos0 < 1) pos0 = 1;
    if (pos1 > len) pos1 = len;

    if (pos0 > pos1 || pos0 > len) {
        out[0] = 0;
        return;
    }
    for (i = pos0; i <= pos1; i++) out[i - pos0] = str[i - 1];
    out[pos1 - pos0 + 1] = 0;
}

void pico_chr(uint8_t code, char *out) {
    out[0] = (char)code;
    out[1] = 0;
}

uint8_t pico_ord(const char *str, int16_t index) {
    int16_t len = 0;
    while (str[len] != 0) len++;
    if (index < 1 || index > len) return 0;
    return (uint8_t)str[index - 1];
}

static uint32_t time_ticks = 0;

void pico_tick(void) {
    time_ticks++;
}

uint32_t pico_time(void) {
    return time_ticks;
}

int16_t pico_flr(pico_fix_t x) {
    return PICO_FIX_TO_INT(x);
}

int16_t pico_ceil(pico_fix_t x) {
    return (int16_t)(-pico_flr(-x));
}

pico_fix_t pico_fix_mul(pico_fix_t a, pico_fix_t b) {
    int64_t wide = (int64_t)a * (int64_t)b;
    return (pico_fix_t)(wide >> PICO_FIX_SHIFT);
}

pico_fix_t pico_fix_lerp(pico_fix_t pos, pico_fix_t tar, pico_fix_t perc) {
    return pos + pico_fix_mul(tar - pos, perc);
}
