#ifndef PICO_STACK_H
#define PICO_STACK_H

#include <stdint.h>

/* A cheap, runtime stack-headroom check -- a periodic alternative to the
 * static .map-file margin reading this project has done by hand several
 * times (picodiver-notes.md's "Round 2", the "tentacle trail" bug fixed
 * in v0.8.1). A build clearing z88dk classic's own 64K link-time ceiling
 * says nothing about how deep real call chains push SP at runtime --
 * only a live reading can catch that, and this makes doing so a one-line
 * call instead of a ZRCP session.
 *
 * Idea credited to pocket-aot (a PICO-8-to-RP2040 transpiler)'s own
 * `p8_stack_check()`, which checks SP against a Pico-SDK linker symbol
 * the same way this checks SP against __BSS_END_tail -- the general
 * technique (ask the linker, don't guess or hardcode an address) is the
 * same, this project's own version of "the linker symbol" and "the
 * failure action" (return a number, not longjmp/abort -- no exception
 * handling exists here, and a false positive aborting a whole game is
 * worse than a game choosing its own response) are specific to this
 * project's constraints. */

/* Current stack pointer (minus this call's own 2-byte return-address
 * overhead -- lib/pico_stack_sp.asm's own comment). Exposed mainly so
 * pico_stack_margin() below has something to subtract; rarely needed
 * directly. */
uint16_t pico_stack_sp(void);

/* Bytes of real headroom between the current SP and __BSS_END_tail --
 * the z88dk classic crt0's own linker symbol for the first byte of this
 * program's BSS segment (confirmed present and public in every build's
 * own .map file, not something this invents). Returns 0, not a wrapped-
 * around huge number, if SP has already dropped to or below that
 * address -- i.e. 0 unambiguously means "no headroom left, real trouble"
 * rather than being confusable with "huge headroom" by unsigned
 * wraparound. */
uint16_t pico_stack_margin(void);

/* Convenience: true once pico_stack_margin() has dropped to or below
 * min_margin. Saves duplicating the comparison at every call site -- the
 * usual way to use this is one check per frame (or before a known deep
 * call chain) against a margin picked with real headroom to react
 * before, not after, a collision. */
uint8_t pico_stack_low(uint16_t min_margin);

#endif
