# Sprites (spr/sspr) -- findings

`lib/pico_sprite.c` (`pico_spr_init`, `pico_spr_transparent`, `pico_spr`,
`pico_sspr`) built entirely on `pico_pset()` -- no new hardware access,
same "software blit into the Layer 2 framebuffer" architecture decision
from the original plan (not Next hardware sprites, which are a separate
display layer `pget()` can't read back and whose z-order isn't under
program control -- see the original plan doc / `dark-room-next`'s own
`docs/sprite-notes.md` for why).

## Architecture: `pico_spr()` is a thin wrapper over `pico_sspr()`

Matches PicoJS's own structure (`picoJS.js`'s `spr()` computes a sheet
cell's `sx/sy/sw/sh` then calls straight through to `sspr()`). `pico_spr()`
generalises the "16 sprites per row" Pico-8 convention to
`sheet_w / 8` instead of hardcoding 16, so a game with a smaller sheet (as
the demo's 16x16/2-cells-per-row one is) still gets correct cell indexing
-- not just a port of the 128-wide case.

## Scaling: nearest-neighbour via per-pixel integer division, not an accumulator

`pico_sspr()`'s dest-to-source mapping is a straight `u = dxi * sw / dw`
(and the `flip_x`/`flip_y` mirrored `sw-1-u`/`sh-1-v` variants) computed
once per destination pixel, using `uint32_t` intermediates defensively
(the actual maximum product, `254 * 255`, fits `uint16_t` -- the wider
type costs nothing meaningful at Pico-8-scale sprite sizes and removes any
need to re-check the bound later if larger sheets are ever supported).
Simpler than a Bresenham-style running accumulator, and "correctness
first" per the project's own staging philosophy -- worth revisiting if a
real game's frame budget can't absorb a per-pixel divide on real Z80
timings, but not before that's actually measured.

## Verified: all three flip combinations are visually distinct and correctly oriented, not just "different"

Easy to get flip logic subtly wrong (e.g. swapping which axis each flag
affects) and not notice if the test sprite happens to be symmetric. The
demo deliberately uses an asymmetric "L" shape (vertical stroke top-left,
foot extending right along the bottom) specifically so each of
`flip_x`/`flip_y`/both has a visually distinguishable, independently
verifiable correct answer, alongside a symmetric "ball" sprite as a
same-either-way control. All four renders
(`/tmp/m2_flip_zoom.png`-equivalent during development) matched their
expected orientation exactly: plain (vertical top-left, foot
bottom-right), `flip_x` (vertical top-right, foot bottom-left), `flip_y`
(vertical bottom-left, foot top-right), both (vertical bottom-right, foot
top-left). A 3x nearest-neighbour `sspr()` scale of the same cell also
rendered with the correct (unflipped) orientation and no visible
distortion or missing/doubled pixel columns.

## Bounds-checking: same `pset_safe()` pattern as `pico_gfx.c`, independently reimplemented

`pico_sprite.c` has its own small `static pset_safe()` rather than sharing
`pico_gfx.c`'s (which is also `static`, module-local) -- a sprite blitted
near a screen edge produces exactly the same "derived destination
coordinate can legitimately go negative or past the logical canvas" case
`pico_gfx.c`'s primitives already had to handle (see `gfx-notes.md`), so
the same clip-not-wrap discipline applies here too. Worth promoting to a
shared internal header if a third module ends up needing the same helper,
not worth the indirection for two.

## Regression-checked: the earlier framebuffer-corruption fix holds

Re-ran the same full-48KB-memory-dump verification from the interrupt/MMU-
window corruption fix (`docs/layer2-notes.md`'s "RESOLVED" section) against
this demo scene, specifically because `pico_sspr()` adds a lot more
`pico_pset()` call volume (every sprite pixel, including the ones skipped
for transparency, which still read a byte) than the shape primitives did --
more opportunities for the same class of interrupt-timing bug to resurface
if the DI/EI fix were ever accidentally scoped too narrowly. Zero stray
bytes found in the physical letterbox border. Fix holds.

## Deliberate scope cuts (not yet implemented)

- No sprite-sheet banking -- the demo's spritesheet is a small resident C
  array. A real Pico-8-scale sheet (128x128 = 16KB) would likely need the
  same banking treatment `dark-room-next`'s `bank.c` gives its own large
  data, once a real port actually needs a sheet that size -- not built
  speculatively.
- No `palt()` -- one global transparent colour (`pico_spr_transparent()`,
  default 0) instead of Pico-8's per-colour transparency table. Deferred
  until the `pal()` work, consistent with the original plan.
- `pico_spr()`'s `w`/`h` (whole-sprite-cell block size) are integer only,
  matching the overwhelming majority of real Pico-8 usage -- Pico-8 itself
  allows fractional `w`/`h` in rare cases, not supported here.
