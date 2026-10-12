# Math/RNG, pal/palt, map, cartdata -- findings

Scoped against real usage, not the full Pico-8 API blindly: grepped
`PicoJS/games/picodiver/picodiver.js` (the only real ported-cart reference
available) for every Pico-8 API call it actually makes. Beyond the
framebuffer/drawing/sprite/input work already built, it uses exactly three
things: `flr`, `pal` (2-arg draw remap), `rnd` -- no
`map`/`mget`/`mset`/`fget`/`fset`, no `palt`, no `min`/`max`/`mid`/`abs`,
no bitwise ops, no `cartdata`/`dget`/`dset`. Built the full planned scope
anyway (all of it is cheap,
well-specified, and clearly useful for *future* ports even if this one
reference cart doesn't exercise it), but this narrower real-usage signal
is why `sin`/`cos`/`atan2` were cut from scope entirely -- see
`lib/pico_math.h`'s own header comment.

## RNG: ported PicoJS's exact reverse-engineered algorithm, verified independently before trusting it

`lib/pico_math.c`'s `pico_srand()`/`pico_rnd()` are a direct port of
PicoJS's own `picoJS.js` "Seeded RNG" section (two 32-bit fixed-point
state words, rotate+add, Lua-style modulo) -- not a from-scratch RNG, so a
ported cart's `srand(seed)` reproduces the same sequence a real
Pico-8/PicoJS run would. Before trusting the C port, independently
reimplemented the same algorithm in Python from PicoJS's source (not from
this project's own C code, to avoid the same mistake propagating through
both) and computed known-good values: `srand(1); rnd(10)` -> `7`,
`srand(0); rnd(4)` -> `0, 0, 1, 2, 2` for five successive calls. The C
port's self-test (`games/demo/m3_test.c`) checks the first of these on
every boot. Passed on the first real build.

Two C-specific porting concerns worth noting for anyone touching this
code: `n << 16` for `n` a `uint16_t` must happen in `uint32_t` (then cast
back) to avoid shifting into the sign bit, which is undefined behaviour
for a signed type in C but not in JS; and `rngRotl16`'s unsigned right
shift (`>>>`, zero-fill) has no direct C equivalent on a signed type
either, same fix (cast to `uint32_t` first). Both handled in
`rng_to_raw()`/`rng_rotl16()`.

## pal(): applied once, centrally, inside pico_pset()

`pico_pal(c0, c1)`/`pico_pal_reset()` (`pico_video.h`) set a 16-entry
remap table consulted inside `pico_pset()` and `pico_cls()` -- not
duplicated into every caller. Since every existing primitive
(`pico_gfx.c`'s shapes, `pico_sprite.c`'s blits, `pico_font.c`'s glyphs)
already funnels every actual pixel write through `pico_pset()`, this one
change point automatically makes `pal()` correctly affect sprites too
(real Pico-8's `pal()` is often used for palette-swap effects on
sprites) with zero changes needed anywhere else. `pico_cls()` writes raw
bytes via `memset`, not `pico_pset()`, so it needed the same remap check
added explicitly -- caught by checking PicoJS's own `cls()` (it goes
through the same `resolveColour()`/`palMap()` path as everything else)
rather than assuming a direct byte-fill didn't need it.

Verified visually and by construction: a remapped rect (colour 8, with
`pal(8, 11)` active) next to an unremapped control (same colour 8, after
`pal_reset()`) rendered as two different colours in the demo screenshot,
matching what each should look like based on this project's own earlier
established colour observations (colour 8 = green, colour 11 = blue, both
confirmed from earlier demo screenshots).

## palt(): upgraded from an earlier single-colour placeholder to a real per-colour bitmask

The earlier `pico_spr_transparent(colour)` is gone, replaced by
`pico_palt(colour, transparent)` (`pico_sprite.h`) backed by a 16-bit
mask (one bit per Pico-8 colour), matching real Pico-8's `palt()` --
any number of colours can be transparent at once, not just one. Checked
against the sprite sheet's *raw* stored colour, before any `pal()` remap
-- the two are independent in real Pico-8 too (`palt` decides whether a
pixel draws at all; `pal` separately decides what colour it draws as if
it does), and this project's `pico_sspr()` already naturally preserved
that ordering (transparency checked on the raw `sheet_pixel()` read,
`pal()`'s remap only applied later, inside `pico_pset()`) without needing
to change anything about the call order itself.

## map()/mget()/mset()/fget()/fset(): tile 0 is always skipped, by Pico-8 convention

Confirmed via PicoJS's own `picoJS.map()` source (not assumed): a map
cell with value 0 is always skipped, regardless of what sprite index 0
actually contains in the spritesheet -- a real, documented Pico-8
limitation (sprite 0 can never appear in a map), not a bug. The demo's
self-test exploits this directly as a verification tool: its 4x4 test map
checkerboards sprite 1 (an "L" shape) against 0 (skip), and the demo's
own sprite 0 is the *ball* graphic -- if the skip logic were broken, the
gaps in the checkerboard would show balls instead of empty space. The
rendered result was a clean, exact checkerboard match with no balls in
the gaps, confirming correct behaviour, not just "a plausible-looking
grid of sprites".

`fget`/`fset` split Pico-8's own overloaded one/two-argument forms
(`fget(n)` = whole byte, `fget(n, f)` = one bit) into
`pico_fget`/`pico_fget_all` and `pico_fset`/`pico_fset_all` -- same
explicit-functions-instead-of-optional-args policy as `pico_gfx.h`'s
`print`/`print_at_cursor`, for the same reason (C has no clean
argument-count-based dispatch). Not exercised by the self-test (no real
cart reference uses sprite flags either) -- worth a dedicated check if a
real port ever needs collision-flag-driven map behaviour.

## cartdata()/dget()/dset(): deliberately NOT write-through, unlike real Pico-8

Real Pico-8's `dset()` writes to disk immediately. `lib/pico_data.c`'s
`pico_dset()` only updates an in-memory array; a game must call
`pico_cartdata_save()` explicitly (e.g. on a pause/menu or level-complete
event) to actually touch the SD card -- hammering esxDOS I/O on every
`dset()` call (plausibly every frame, for a live score counter) is both
slow and a real failure-mode risk this project would rather design away
than defer. Matches how `dark-room-next`'s own save system already works
(explicit `autosave()` calls, not a write on every state change) --
follows established project precedent, not a novel choice. Implementation
uses the same low-level `esxdos_f_*` calls as `dark-room-next/src/save.c`
(not stdio, for the same resident-code-budget reason documented there),
no banking (this project's codebase is still small enough to stay fully
resident).

**Verified as a genuine round-trip, not just "the file got created"**:
ran the demo twice in a row (fresh `sdcard/` each boot is NOT wiped
between runs) and read the resulting `TEST.DAT` directly from the host
filesystem both times -- first run: boot counter byte `01 00`
(little-endian 1). Second run: `02 00` (2). Confirms `pico_cartdata()`'s
load path genuinely reads back a previous save (not just always starting
from 0) and `pico_dset()`/`pico_cartdata_save()`'s write path genuinely
persists across a full process restart, not just within one run's RAM.

## Deliberate scope cuts (not yet implemented)

- No `sin`/`cos`/`atan2` -- see `lib/pico_math.h`'s own header comment:
  would need either real fixed-point trig (a lookup table, a genuine
  feature of its own) or float (ruled out project-wide for Z80
  performance reasons already established by `pico_gfx.c`'s integer-only
  ellipse). No reference game needs them yet.
- No `tostr`/`tonum`/`chr`/`ord`/`sub` -- lower priority (no reference-game
  usage), and string handling raises its own embedded-C design questions
  (fixed vs. caller-provided buffers) better resolved against a real need
  than guessed at. The self-test here verified every numeric result by
  direct comparison instead of needing to format and display
  them as text, which is exactly the kind of real need that would justify
  building `tostr()` next.
- `pal()`'s screen-palette mode (`p=1` in real Pico-8, a whole-framebuffer
  remap at flip time) -- not implemented, matching PicoJS's own scope
  (its own source comment notes this explicitly).
