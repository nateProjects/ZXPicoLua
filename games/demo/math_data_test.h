#ifndef MATH_DATA_TEST_H
#define MATH_DATA_TEST_H

#include <stdint.h>

/* Split in two because the demo primes BOTH double-buffered physical
 * buffers with identical startup content (see main.c's own priming-loop
 * comment) -- the math/data checks must run exactly once (an incrementing
 * boot counter via pico_dset()/pico_cartdata_save() would double-count
 * otherwise), while the visual result (pal()/map() demo + pass/fail
 * indicator) must be drawn into each buffer separately. */

/* Runs pico_math.c's functions once, each compared against a known-
 * correct expected value baked in here (not just displayed to eyeball),
 * plus pico_data.c's cartdata/dget/dset round-trip. Returns 1 if
 * everything checked out, 0 otherwise. See docs/math-notes.md for the
 * expected values this was checked against (independently computed via a
 * Python port of the same RNG algorithm, not just assumed correct because
 * the C and the check were written by the same hand). */
uint8_t math_data_run_checks(void);

/* Draws the visual half: a pico_pal() remap demonstration (a remapped
 * rect next to an unremapped control), a small pico_map() tile grid, and
 * a single pass/fail indicator rect for whatever math_data_run_checks()
 * last returned. Call once per physical buffer during startup priming. */
void math_data_draw(uint8_t pass);

#endif
