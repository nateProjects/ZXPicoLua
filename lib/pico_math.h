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

#endif
