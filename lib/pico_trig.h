#ifndef PICO_TRIG_H
#define PICO_TRIG_H

#include "pico_math.h"

/* sin(x)/cos(x) -- a SEPARATE opt-in module from pico_math.h's other
 * functions (excluded from the Makefile's default LIB_SRCS wildcard --
 * see that file's own comment), same reasoning as pico_music.c/
 * pico_sfx_synth.c: real, measured resident cost, not worth paying in a
 * build that never calls it. Confirmed the hard way (2026-10-13): first
 * lived inside pico_math.c itself, where its 512-byte lookup table alone
 * tipped picodiver's build over (picodiver's actual main-resident margin
 * was already a razor-thin 263 bytes, unrelated to this feature) -- split
 * out here rather than trimmed, since picodiver has zero trig call sites
 * anyway (confirmed by grep), matching this project's own "bank/exclude
 * unused large modules, don't shrink gameplay caps as a stopgap"
 * precedent. A game that wants trig adds pico_trig.c to its own LIB_SRCS
 * explicitly (see games/fixedtest/'s own Makefile block for the pattern).
 *
 * Pico-8's own distinctive trig convention, confirmed against the real
 * manual, not assumed from standard math: x is a TURNS value (0.0-1.0 =
 * one full rotation), not radians, and sin() is NEGATED relative to the
 * usual math convention -- Pico-8's own documented quirk (sin(0.25) is
 * -1, not +1; cos() is NOT inverted). x can be negative or outside
 * [0,1) -- both wrap, same as real Pico-8 (cos(1.25) == cos(0.25)).
 *
 * Implemented via a single 256-entry cosine lookup table (pico_trig.c's
 * own cos_table, Q0.15 fixed-point, programmatically generated -- see
 * that file's own top comment for the exact script, same "ported
 * programmatically, not hand-transcribed" discipline as pico_font.c's
 * own glyph data) -- pico_sin() is derived from the SAME table via the
 * standard identity sin(t) = cos(t - 0.25) plus Pico-8's own sign flip,
 * not a second table. 256 steps per full turn (~1.4 degrees) is the
 * resulting resolution -- x's own value below that granularity is
 * rounded to the nearest table step, not interpolated; plenty for the
 * retro-scale rotations a Pico-8 port typically needs, revisit if a real
 * port ever needs finer angular precision than that. */
pico_fix_t pico_cos(pico_fix_t x);
pico_fix_t pico_sin(pico_fix_t x);

#endif
