# M4 findings -- porting PicoDiver, the real integration test

`games/picodiver/` ports `PicoJS/games/picodiver/picodiver.js` (itself a
port of the original Pico-8 cart `picodiver2.p8`,
https://github.com/nateProjects/PicoDiver) to this library. Not a demo
scene -- a complete, playable game, the real integration test M4 was
meant to be.

## Scope, settled by reading the actual source, not guessed

M3 was scoped against `picodiver.js`'s real API usage (it only needed
`flr`/`pal`/`rnd` beyond M0-M2.5). Porting the full game confirmed that
was the right call for picking M3's priorities, but a full game needs
much more: `spr`/`map`/`btn`/`print`/`cls`/`color` all see heavy use.
`color()`'s *implicit* current-colour state has no equivalent here (see
`pico_gfx.h`'s own explicit-colour-argument policy) -- ported mechanically
by tracking a `curcol` variable in `main.c` itself, updated at each point
the original calls `color(X)`, not as a library feature. `add`/`del`/`all`
(JS array helpers, already decided against in M1) became a fixed-size
`obj[MAX_OBJS]` array with swap-and-shrink removal instead of a dynamic
list -- the one place this diverges from a literal line-for-line port.

Two small, deliberately preserved oddities from the original Lua cart
(per `picodiver.js`'s own comments, not reinvented here): `obj[n][4] +=
obj[n][7]` uses the sprite's *width* field to step its animation frame,
not an `ani` field that looks like it should be used instead -- preserved
exactly, not "fixed". The original's "remove overlapping objects" block
(`del(obj.thing)`, a dot not a comma) is a confirmed no-op in the JS port
too -- dropped entirely here rather than ported as dead code.

## New library capability this port required: `sget()`

Pixel-perfect collision (dw817's method, used by the original cart and
PicoJS's own port) reads individual spritesheet pixels directly, bypassing
`spr()`'s drawing path entirely. Added `pico_sget(x, y)` to
`pico_sprite.c` -- a thin wrapper exposing the sheet's own pixel lookup,
previously internal-only.

## New library capability: signed sprite/map destination coordinates

The title screen's scrolling ship background draws itself via two
`map()` calls per frame -- one at the current scroll position, one a
full screen-width to its side, to cover the wrap seamlessly. The second
call needs to start **off-screen on purpose** (a negative screen x), which
`pico_spr()`/`pico_sspr()`/`pico_map()`'s original `uint8_t` destination
coordinates couldn't represent at all, even though the underlying
clipping (`pset_safe()`) already handled negative values correctly
internally. Widened `dx`/`dy` (and `pico_map()`'s `sx`/`sy`) to `int16_t`
-- purely additive, every existing call site with a small positive
literal still just works (plain `int`-to-`int16_t` conversion).

## Real resident-size limit hit, and how it was resolved with real data, not guesswork

The full ported game -- library + game logic + the cart's original
128x128 spritesheet (16KB) and 128x32 map (4KB) -- overflowed the 64K
address space its `-clib=classic` resident image has to fit in (`Section
exceeds 64k [0x8000,0x12361]`, ~9KB over). Rather than reach for
`dark-room-next`'s banking machinery (a materially bigger addition, and
the plan doc explicitly flagged this as not worth building until a real
need showed up), checked what this *specific* cart's own map and sprite
data actually reference:

- Highest sprite index referenced anywhere (map tiles or game logic):
  113 -- confirmed by scanning every value in the source data, not
  guessed. The real cart's 128x128 sheet-editor canvas always allocates
  all 16 rows regardless of how much is actually drawn; rows 8-15 here
  were genuinely empty padding. Truncated the shipped sheet to 128x64
  (8 rows, indices 0-127), halving it to 8KB.
- Both `pico_map()` call sites in this game only ever read map rows
  0-15 (`back()`'s rows 4-15, `shipshow()`'s rows 0-3) -- confirmed by
  checking every map() call, not guessed. Truncated the shipped map
  data from 32 to 16 rows, halving it again to 2KB.
- `picodiver_sprite_flags` (256 bytes) dropped from the build entirely
  -- this game's own logic branches on raw sprite-index ranges
  (`thing[3] <= 66` for "is a fish") instead of `fget()`/`fset()`,
  confirmed by reading `picodiver.js` itself, not assumed.

Combined: 20736 bytes of data down to 10240, enough to link cleanly.

## Performance: three real optimizations, each found by measuring, not guessing

The ported game rendered *correctly* on the first successful build but
took 10-25+ seconds for a single frame -- not a hang (confirmed via a
bisected minimal reproduction: an isolated `map()` call with a long
enough wait eventually completed and showed correct content), genuinely
that slow. Three real fixes, applied in the order they were found:

1. **`pico_sspr()`'s scaling math ran unconditionally.** Every
   `pico_spr()` call (so every sprite *and* every `map()` tile) hits the
   1:1 (no scaling) case by construction, yet the code always computed
   `dxi * sw / dw` via a 32-bit multiply+divide -- real z88dk/Z80
   software division, not a single instruction. Added a fast path that
   skips the multiply/divide entirely when `sw==dw && sh==dh`. Roughly a
   15-20x improvement on its own (a 16x12-tile `map()` call: ~10-25s to
   ~1-2.5s).
2. **`pico_pset()`'s per-pixel MMU page-switch overhead.** Still
   multiple seconds per heavy frame after (1). An 8x8 sprite/tile's
   destination pixels almost always land in the same physical page
   (pages are 32 rows tall) -- re-selecting it 64 times per tile via
   `pico_pset()`'s own DI/EI-plus-port-I/O path is pure waste. Added a
   batch-write session (`pico_video_batch_begin/pset/end()`,
   `pico_video.h`) that resolves the page and disables interrupts *once*
   per call, only re-selecting when the page actually changes between
   pixels -- `pico_sspr()` is the first (and so far only) user. Another
   ~5-10x.
3. **CPU speed.** The Next boots at classic-compatible 3.5MHz. Bumped
   to 14MHz (`RTM_14MHZ`) in `pico_video_init()`, on top of (1) and (2).

Combined: the title screen (a `cls()` + a 16x12-tile `map()` + a 2x
16x4-tile scrolling ship + 7 lines of text, every frame) went from
10-25+ seconds per frame to roughly 0.5-1 second -- a ~20-30x total
improvement. Still not smooth real-time (would need ~40ms/frame for
25fps), but the difference between "appears completely hung" and
"noticeably slow but genuinely interactive" is real and worth having
shipped rather than guessed at.

## 28MHz: tried, confirmed it breaks keyboard input, reverted

The user confirmed the Next supports a 4th CPU speed step (28MHz, raw
NextReg `$07` value 3 -- this z88dk checkout's `zxn.h` only names
constants up to `RTM_14MHZ`) and suggested pairing it with
`REG_PERIPHERAL_3`/`RP3_DISABLE_CONTENTION` (both present in this
checkout). Direct ZRCP register pokes confirmed the emulator genuinely
accepts and reports back value 3 correctly. But setting it from inside
`pico_video_init()` broke keyboard input: a key held via ZRCP's
deterministic `set-ui-io-ports` (the same method that reliably verified
input throughout M2.5) stopped registering at all, even held for several
seconds, while the identical test against a 14MHz build worked within
2-3 seconds as expected. Isolated to the speed change itself, not
contention -- tested with and without `RP3_DISABLE_CONTENTION`, both
failed identically.

Root cause not understood. `pico_input.c` reads the keyboard matrix via
`in_KeyPressed()`, documented as an instantaneous, undebounced read with
no timing loop that should plausibly depend on CPU clock speed at all --
so *why* 28MHz specifically breaks this isn't yet explained, just
reproduced reliably. One early, now-ruled-out red herring: a first look
at this seemed to show 28MHz not persisting at all (reading back as reset
to 3MHz) -- that turned out to be a *separate*, unrelated Makefile bug
(see below) that had silently deleted the binary being tested, so ZEsarUX
was booting its own ROM instead of running any of this project's code.
Reverted to the proven-reliable 14MHz rather than ship something with
broken input. Worth revisiting if more speed is needed later, starting
from "why does a documented-instantaneous matrix read depend on CPU
clock" rather than from scratch.

## Methodology lessons from this milestone, worth remembering

**A full-game performance bug can look exactly like a hang.** The first
sign of trouble was a completely black screen that stayed black for many
seconds -- indistinguishable, from a single screenshot, from a genuine
crash or infinite loop. Settled by bisection (an isolated single-cell
`map()` call, confirmed fast; the full 16x12 call, confirmed to
eventually complete given a long enough wait) rather than assuming either
answer. Don't conclude "hung" from one impatient screenshot.

**Raw physical-page memory scans aren't reliable on a continuously-
flipping double-buffered scene.** The corruption-scanning technique
proven in M0/M2.5 (dump all physical pages, check for unexpected non-zero
bytes) produced an apparent few thousand stray bytes in picodiver's
letterbox border during active gameplay -- but the actual *displayed*
pixel at those exact coordinates (checked via `save-screen`, which always
captures the true current front buffer) was confirmed pure black, no
discrepancy at all. The scan reads *fixed physical page numbers*, which
alternate between being the displayed buffer and the actively-being-drawn
one every single frame (picodiver redraws and flips unconditionally every
frame, unlike the M2.5 demo, which only flips on actual change) -- a scan
spanning multiple frames' worth of wall-clock time will sometimes catch a
page mid-draw, transiently incomplete, and mistake that for corruption.
The fix for next time: verify a continuously-animating double-buffered
scene via `save-screen` pixel sampling (always correct, since it reads
what's truly displayed), not a raw page scan -- reserve the raw-page
method for static scenes or paused/idle moments, as every prior milestone
that used it successfully happened to be.

**The Makefile's `clean` target needs to be scoped per game, not global.**
Added for M4 (multiple games sharing one `lib/`, see the plan's own
multi-game Makefile design), `clean` originally did `rm -rf build/*` --
wiping *every* game's build, not just the one being worked on. Building
`demo` then `picodiver` (each with its own `clean` first) silently
deleted the just-built demo binary, which briefly looked like a real
28MHz regression (ZEsarUX booting its own ROM instead of a missing `.nex`,
which happens to look similar to "the program crashed very early") before
being traced back to a missing file, not a code bug. Fixed: `clean`
now only removes `$(BUILD_DIR)` (the current `$(GAME)`'s own build
directory); `clean-all` removes everything.

## Deliberately not ported

- `sfx(n)` -- all 5 call sites skipped. No audio API in this library yet.
- The JS port's own "canvas tainted" `sget()` warning -- a workaround for
  a browser-only failure mode (opening via `file://`) with no equivalent
  here; `pico_sget()` always works.
