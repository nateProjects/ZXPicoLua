#ifndef PICO_MATH_H
#define PICO_MATH_H

#include <stdint.h>

/* Integer-only by design: Pico-8's own numbers are 16.16 fixed-point, and
 * z88dk's software float on Z80 is slow with no hardware FPU to fall back
 * on -- the same reasoning pico_gfx.c's hand-derived integer midpoint
 * ellipse algorithm already used to avoid float/sqrt entirely. No sin/
 * cos/atan2 in v1: they'd need either real fixed-point trig (a lookup
 * table, a real feature in its own right) or float, and no reference
 * game (PicoJS/games/picodiver, the only one available) uses them --
 * deferred until a real port actually needs them, consistent with this
 * project's other "don't build speculatively" calls (sprite-sheet
 * banking, palt()'s original single-colour v1, multiplayer input). */

int16_t pico_mid(int16_t a, int16_t b, int16_t c);
int16_t pico_min(int16_t a, int16_t b);
int16_t pico_max(int16_t a, int16_t b);
int16_t pico_abs(int16_t x);
/* sgn(x) -- Pico-8 quirk: sgn(0) is 1, not 0 (ported exactly from
 * PicoJS's own picoJS.sgn -- "only negative numbers return -1"). */
int16_t pico_sgn(int16_t x);

/* Bitwise ops as functions, mechanically mirroring Pico-8/Lua's own
 * band/bor/bxor/bnot/shl/shr/lshr (Lua predates native bitwise operators,
 * so Pico-8 calls these as functions) -- useful only for porting a cart's
 * call sites verbatim; native C code should just use &|^~<<>> directly.
 * shr is arithmetic (sign-preserving); lshr is logical (zero-fill). */
int16_t pico_band(int16_t a, int16_t b);
int16_t pico_bor(int16_t a, int16_t b);
int16_t pico_bxor(int16_t a, int16_t b);
int16_t pico_bnot(int16_t a);
int16_t pico_shl(int16_t a, int16_t b);
int16_t pico_shr(int16_t a, int16_t b);
uint16_t pico_lshr(uint16_t a, int16_t b);

/* srand(seed)/rnd(n) -- the exact reverse-engineered Pico-8 RNG algorithm
 * PicoJS ports (picoJS.js, "Seeded RNG" section -- two 32-bit fixed-point
 * state words, rotate+add, Lua-style modulo), not an arbitrary C rand()
 * substitute, so a ported cart's srand(seed) reproduces the same sequence
 * a real Pico-8/PicoJS run would (for integer n; pico_rnd() returns the
 * integer part only -- see its own comment for why). PicoJS falls back to
 * Math.random() until srand() is first called; there's no equivalent
 * "true" entropy source here, so pico_rnd() auto-seeds with seed 0 on
 * first use instead if pico_srand() was never called. */
void pico_srand(uint16_t seed);
/* rnd(n) -- an integer in [0, n), or 0 if n is 0. Collapses Pico-8's
 * rnd(n) + flr() (the overwhelmingly common pairing for an integer
 * result, e.g. picodiver.js's `flr(rnd(4))`) into one call, consistent
 * with this module being integer-only -- see this header's top comment. */
uint16_t pico_rnd(uint16_t n);

/* tostr(val) -- decimal integer to string, writing into a caller-provided
 * buffer (at least 7 bytes: "-32768\0") rather than returning an
 * allocated string, the standard embedded-C idiom. Added for the
 * picodiver port, whose HUD concatenates a score/level number into
 * on-screen text ("fish " + cab.score) -- cut from the earlier math-module
 * scope (see this header's top comment) for lack of a real reference-game
 * need at the time; this is that need. No hex/no-flags-equivalent variants (Pico-8's
 * own tostr() flags) -- just the plain decimal case, the only one this
 * project has a real use for so far. */
void pico_tostr_int(int16_t val, char *buf);

/* sub(str, pos0, [pos1]) -- Pico-8's own 1-indexed substring extraction,
 * negative positions counting from the end (-1 = last character). Writes
 * into a caller-provided buffer (same embedded-C idiom as pico_tostr_int()
 * above -- at least strlen(str)+1 bytes to be safe) rather than returning
 * an allocated string. Pico-8's own pos1 is OPTIONAL (defaults to the
 * string's own end) -- there's no "omitted argument" concept in C, so
 * pass any value >= strlen(str) (INT16_MAX is always safe) for the same
 * effect at a call site translated from `sub(s, pos0)`. Added for a
 * real cart's own per-character wavy-text effect (`sub(text,i,i)` inside
 * a loop) -- see this header's own top comment on "don't build
 * speculatively", same reasoning, this is that need. */
void pico_sub(const char *str, int16_t pos0, int16_t pos1, char *out);

/* time()/t() -- Pico-8's own "seconds since cart start," returned as a
 * fractional number carts almost always use for periodic/modulo pacing
 * (`time() % n == 0`), not real wall-clock precision. Returned here as a
 * raw TICK counter instead of fractional seconds -- no floats on this
 * hardware by design (this header's own top comment), and a tick count
 * is the more honest zero-cost equivalent for what the modulo pattern
 * actually wants (elapsed UNITS, not seconds). Call pico_tick() once per
 * frame (main loop, same place pico_input_update() already gets called
 * in every existing game) to advance it; pico_time() just reads the
 * current count. Re-tune any `% n` divisor ported from a cart's own
 * time()-based pacing against this project's actual tick rate (native
 * 50Hz on the Next, not Pico-8's own 30/60fps) -- same as any other
 * frame-rate-dependent constant carried across a port, not something
 * this function can do for you. */
void pico_tick(void);
uint32_t pico_time(void);

/* chr(code) -- a single character as a 2-byte string (char + nul),
 * written into a caller-provided buffer (same embedded-C idiom as
 * pico_sub()/pico_tostr_int() above). Pico-8's own chr() can also take
 * MULTIPLE code arguments to build a longer string in one call
 * (chr(97,98,99) == "abc") -- not offered here, no reference cart has
 * needed that form yet; call pico_chr() in a loop (or build the string
 * by hand) for that case until one does. */
void pico_chr(uint8_t code, char *out);

/* ord(str, [index]) -- the character code at `index` (1-based, matching
 * pico_sub()'s own convention; pass 1 for Pico-8's own default omitted-
 * index form `ord(str)`). Returns 0 for an out-of-range index -- Pico-8
 * itself returns nil there, which C has no equivalent of; 0 happens to
 * also be a real, valid character code (nul), so check the index against
 * the string's own length first if that distinction matters to a call
 * site, same caveat as pico_sub()'s own clamping behaviour. */
uint8_t pico_ord(const char *str, int16_t index);

/* --- Fixed-point (Q16.16) --------------------------------------------
 *
 * Added for the Islander port: the first reference game whose own movement
 * genuinely needs fractional accumulation -- `fric=0.1`-per-tick velocity
 * build-up against a `spd=1` cap, and `lerp(pos,tar,perc)` with
 * percentages like 0.0125-0.325 used throughout (camera, menus, UI).
 * Every prior port (picodiver) only ever needed integer pixel steps.
 *
 * Q16.16 (32-bit, 16 integer + 16 fractional bits), matching Pico-8's own
 * native number format exactly -- not a narrower format (Q8.8 etc): the
 * integer part alone needs to hold real world-pixel positions (up to
 * ~1024 for Islander's 128x8-tile-wide map), which doesn't fit in Q8.8's
 * 8-bit (±127) integer range. Still integer-only under the hood (no
 * float) per this header's own top comment -- just a wider int. */
typedef int32_t pico_fix_t;
#define PICO_FIX_SHIFT 16
#define PICO_FIX_ONE ((pico_fix_t)1 << PICO_FIX_SHIFT)
/* Whole-number literal -> fixed, e.g. PICO_FIX_INT(1) for spd=1. */
#define PICO_FIX_INT(n) ((pico_fix_t)(n) << PICO_FIX_SHIFT)
/* A fraction num/den -> fixed, e.g. PICO_FIX_FRAC(1,10) for fric=0.1,
 * PICO_FIX_FRAC(125,10000) for perc=0.0125 -- avoids float literals
 * entirely (den up to ~10000 is exact enough for every percentage this
 * port's own source uses, confirmed by checking each one). Evaluates to
 * a single integer division, not a per-frame cost -- every call site uses
 * it only to build a `static const` constant once, never recomputed. */
#define PICO_FIX_FRAC(num, den) ((pico_fix_t)(((int32_t)(num) << PICO_FIX_SHIFT) / (den)))
/* Fixed -> integer, truncating toward zero (matches Pico-8's own flr()
 * for non-negative values, the only case this port's rendering code --
 * taking a fixed-point position to an integer pixel coordinate -- ever
 * needs: world positions never go negative). */
#define PICO_FIX_TO_INT(f) ((int16_t)((f) >> PICO_FIX_SHIFT))

/* flr(x)/ceil(x) -- Pico-8's own round-toward-negative/positive-infinity,
 * operating on a pico_fix_t (Q16.16) the same way Pico-8's own flr/ceil
 * operate on its native fixed-point numbers. pico_flr() is really just
 * PICO_FIX_TO_INT() under Pico-8's own familiar name for direct 1:1 cart
 * translation fidelity (`flr(x)` -> `pico_flr(x)`, no need to remember
 * the macro exists instead) -- the macro's own arithmetic right-shift
 * ALREADY rounds toward -infinity for negative values, i.e. already IS
 * flr(), confirmed by the identity flr(-0.5)==-1 (-32768 as a raw Q16.16
 * value, >>16 arithmetic == -1). pico_ceil() is `-flr(-x)`, the standard
 * identity -- avoids a second, separate rounding implementation.
 * Deliberately NOT offered as an int16_t/int16_t floored-division helper
 * (Pico-8's own `flr(a/b)` pattern on two plain integers, which needs
 * floor-toward-negative-infinity division, unlike C's own truncate-
 * toward-zero `/`) -- a real, different need from this one, not built
 * speculatively until a port actually has it (this header's own top
 * comment). */
int16_t pico_flr(pico_fix_t x);
int16_t pico_ceil(pico_fix_t x);

/* a*b, correctly scaled back down to Q16.16 -- needs a 32x32->64-bit
 * intermediate (a direct 32-bit multiply overflows for any operands both
 * above ~181 in integer-part magnitude, e.g. two world-scale positions),
 * so this goes through int64_t rather than pico_fix_t's own 32-bit width.
 * Rare call (lerp below, and any future velocity/direction scaling) --
 * the 64-bit cost is a non-issue at the frequency this is actually used. */
pico_fix_t pico_fix_mul(pico_fix_t a, pico_fix_t b);

/* lerp(pos, tar, perc) -- ported directly from islander-4.p8's own
 * `function lerp(pos,tar,perc) return pos+((tar-pos)*perc) end`, just in
 * fixed-point. perc is typically built via PICO_FIX_FRAC at a call site
 * (e.g. PICO_FIX_FRAC(325,1000) for the original's 0.325). */
pico_fix_t pico_fix_lerp(pico_fix_t pos, pico_fix_t tar, pico_fix_t perc);

#endif
