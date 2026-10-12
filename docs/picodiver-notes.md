# Porting PicoDiver -- the real integration test

`games/picodiver/` ports `PicoJS/games/picodiver/picodiver.js` (itself a
port of the original Pico-8 cart `picodiver2.p8`,
https://github.com/nateProjects/PicoDiver) to this library. Not a demo
scene -- a complete, playable game, the real integration test this
project needed.

## Scope, settled by reading the actual source, not guessed

The earlier math/RNG/pal work was scoped against `picodiver.js`'s real API
usage (it only needed `flr`/`pal`/`rnd` beyond the framebuffer/drawing/
sprite/input work). Porting the full game confirmed that was the right
call for picking those priorities, but a full game needs much more:
`spr`/`map`/`btn`/`print`/`cls`/`color` all see heavy use. `color()`'s
*implicit* current-colour state has no equivalent here (see
`pico_gfx.h`'s own explicit-colour-argument policy) -- ported mechanically
by tracking a `curcol` variable in `main.c` itself, updated at each point
the original calls `color(X)`, not as a library feature. `add`/`del`/`all`
(JS array helpers, already decided against earlier) became a fixed-size
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
`dark-room-next`'s banking machinery right away (a materially bigger
addition, not worth building until a real need showed up), checked what
this *specific* cart's own map and sprite
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

## Round 2: still "far too slow" after the above -- a real bottom-up benchmark, and what it actually found

The three fixes above were a big improvement but still left the result
unacceptable (ships not animating fluidly on the title screen, several
seconds to respond to a keypress) -- and Pico-8 itself, at an *equivalent*
8MHz, slower than our 14MHz, is still much faster. Rather than guess
again, built an incremental `PERF_LEVEL`-gated
benchmark in `main.c` (empty loop, +cls, +one sprite, +one map tile, +full
192-tile map, +map+ship strip) to measure each layer's real cost.

**The benchmark's own counter variable was itself unreliable, and that was
a real finding, not just friction.** `perf_counter`'s linked BSS address
(from each build's own `.map`, re-extracted every time -- a known gotcha
by this point) landed within ~200 bytes of `$FFFF` in every build. Reading
it back via ZRCP gave static, implausible values (identical across a
3-second gap, including one build reading back a huge 32-bit "counter"
that never changed). `get-registers` showed why: `SP` dips as low as
`$FF2F` during heavy nested drawing (`map()` -> `spr()` -> `sspr()` ->
the blit loop is 4-5 calls deep, each consuming real stack bytes per
sccz80's calling convention) -- *below* where the counter variable lived,
meaning the "counter" was transiently reading live stack contents, not
the real variable. Padding to push the variable to a safer address wasn't
possible either: the build had essentially zero headroom left under the
64K resident limit (confirmed by `Section exceeds 64k` errors from adding
even 512 bytes). Abandoned the BSS-counter approach; `get-tstates-partial`
(a true hardware cycle counter, immune to this) and direct screenshot/
`set-ui-io-ports` timing (this project's other proven-reliable method)
were used instead.

**The real finding, from reading actual `.asm` output, not guessing
again:** compiled `lib/pico_sprite.c` with `zcc -S` and read the generated
Z80 for `pico_sspr()`'s per-pixel loop directly. Every iteration --
*including* the already-"fast" unscaled path -- re-fetched every local
through stack-relative addressing (`ld hl,N; add hl,sp; ...`) and made
real `CALL`/`RET` round trips through library helpers (`l_gint`, `l_pint`,
`l_asl`, `l_and`) even for trivial operations like copying a loop index,
on top of the two real function calls (`sheet_pixel()`,
`batch_pset_safe()`) every pixel already made. This -- not the MMU/page
logic the earlier batch-write fix already addressed -- was the dominant
remaining cost. `--opt-code-speed -O3` (sccz80's own speed-optimizer
flag) made no measurable difference (33 vs 36 units over a fixed window),
confirming this is sccz80's baseline codegen quality for this kind of
loop, not a missing flag.

Three more fixes, all exploiting that a Layer 2 **row never crosses a
physical page boundary** (`PICO_PHYS_W`=256 divides `LAYER2_WINDOW_SIZE`
=8192 evenly -- every row of 256 bytes sits entirely inside one 32-row
page):

1. **`pico_video_batch_row()`** (`pico_video.c`/`.h`) -- resolves the
   destination page/pointer once per *row* instead of once per pixel
   (`pico_video_batch_pset()`'s granularity), returning a raw `uint8_t*`
   the caller walks with plain pointer arithmetic. `pico_video_remap()`
   also exposes `pal()`'s remap table directly so a hot loop can inline
   the lookup instead of paying a function call for it too.
2. **`pico_sprite.c`: a new `blit_unscaled()`** replaces the unscaled
   (1:1) path's per-pixel `sheet_pixel()`/`batch_pset_safe()` calls with
   raw source/destination pointer walks, resolving the destination page
   once per row via (1). The general scaled path (needed by nothing in
   picodiver, kept for API completeness) is unchanged.
3. **`lib/pico_gfx.c`: `draw_glyph()`/`print_impl()`** had the *exact
   same* per-pixel-call pathology -- every lit font pixel went through
   `pset_safe()` -> `pico_pset()`, each paying a full DI/EI plus MMU
   page-select. The title screen alone draws ~150 glyphs across 7 lines.
   Rewritten to open one batch session per `print()` call (not per
   glyph, and nowhere near the "one tile" batch-size guideline the
   sprite fix above established -- a deliberate, accepted tradeoff here
   since a text line's pixels are far more likely to share one physical
   page than scattered sprite writes are) and write through (1) directly.

**`pico_cls()` was also clearing 3x more than necessary.** It cleared the
*entire* physical 256x192 surface (6 pages, 49152 bytes) every call, even
though picodiver's logical canvas is only the centred 128x128 (16384
bytes) -- the 64px/32px letterbox border is cleared once by
`pico_video_init()` and, by construction, never drawn into again (every
primitive clips to `[0,pico_screen_w)x[0,pico_screen_h)` before
translating to physical coordinates). Split into `cls_physical_full()`
(used only by `pico_video_init()`'s one-time double-buffer priming) and a
new `pico_cls()` that only clears the logical rectangle, row by row (same
one-page-per-row property as above). Measured directly: ~66ms/call before,
~32ms/call after, on an otherwise-unmodified build -- a real 2x, not just
an arithmetic argument.

**These three fixes together didn't fit in 64K without `-O3`.** Adding
`draw_glyph()`'s row-walking logic pushed the build ~46 bytes over the
resident limit; `--opt-code-speed -O3` (sccz80's peephole optimizer,
tested above for *speed* with no measurable effect) turned out to shrink
*code size* enough to fit. Added to the Makefile unconditionally
(`-O3` on both games' builds) since it's free and can only help.

**End-to-end result, measured honestly:** the real (non-benchmark) game
no longer hangs or appears crashed -- confirmed the title screen renders
within a few seconds of boot, and (via a 20-second held-key test) the full
state machine (title -> gameplay -> death) now completes within that
window, where before a *single frame* alone took 10-25+ seconds. Real,
substantial progress, but still well short of feeling responsive in real
time -- holding a key for 1-2 seconds still shows no visible reaction.
Not yet re-benchmarked layer-by-layer with a trustworthy counter (the
`PERF_LEVEL` harness was removed from `main.c` rather than fixed, since
`get-tstates-partial` + screenshot timing proved sufficient and more
reliable for the measurements that mattered here) -- a reasonable next
step if more speed work happens is a breakpoint-pass-count-based
iteration counter (`get-breakpointspasscount` after `enable-breakpoints`
+ `set-breakpoint <N> PC=<addr>`, 1-indexed, both needed before the
breakpoint can be configured at all) rather than a BSS variable, which
this round showed isn't safe to place blindly in this program's memory
layout.

**Open, not yet understood: this program's BSS now sits uncomfortably
close to where the stack actually operates.** `SP` was observed as low as
`$FF2F`-`$FF12` during normal (non-benchmark) play, while real game-state
globals (`_mode`, `_frame`, `_cab`, etc., per the `.map`) live at
`$FF64`-`$FF9D` -- *above* that low-water mark, meaning they're within the
stack's demonstrated working range, not just theoretically reachable.
z88dk classic's CRT starts `SP` at `$FFFF` with no fixed/reserved stack
region separate from BSS (`CRT_MAX_HEAP_ADDRESS = $FFFF - 169` is only a
heap-growth limit for `malloc`, which this project doesn't use, and
doesn't appear to bound BSS placement at all). Despite this, extensive
testing (register reads showing `PC` genuinely advancing through
different functions, not stuck; the full title->gameplay->death state
sequence completing correctly over a longer window) found no evidence of
actual corrupted game state -- the earlier-looking "corruption" (`_mode`
reading back as 205) was itself traced to the same stack-overlap-at-read-
time artifact as the benchmark counter, not a real in-game bug. Still,
this is a latent risk worth real attention before adding more deeply-
nested drawing code (each additional call level costs real stack bytes
per sccz80's convention, confirmed via its own generated `.asm`) --
either by reducing call depth in the hot drawing paths (`map()` ->
`spr()` -> `sspr()` -> `blit_unscaled()` is 4 levels deep today) or by
reducing overall resident size to buy back headroom, since the build
currently has essentially zero free bytes under the 64K limit.

## 28MHz: tried, confirmed it breaks keyboard input, reverted

The Next supports a 4th CPU speed step (28MHz, raw NextReg `$07` value 3
-- this z88dk checkout's `zxn.h` only names constants up to
`RTM_14MHZ`), pairable with
`REG_PERIPHERAL_3`/`RP3_DISABLE_CONTENTION` (both present in this
checkout). Direct ZRCP register pokes confirmed the emulator genuinely
accepts and reports back value 3 correctly. But setting it from inside
`pico_video_init()` broke keyboard input: a key held via ZRCP's
deterministic `set-ui-io-ports` (the same method that reliably verified
input throughout the earlier input work) stopped registering at all, even held for several
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

## Methodology lessons from this port, worth remembering

**A full-game performance bug can look exactly like a hang.** The first
sign of trouble was a completely black screen that stayed black for many
seconds -- indistinguishable, from a single screenshot, from a genuine
crash or infinite loop. Settled by bisection (an isolated single-cell
`map()` call, confirmed fast; the full 16x12 call, confirmed to
eventually complete given a long enough wait) rather than assuming either
answer. Don't conclude "hung" from one impatient screenshot.

**Raw physical-page memory scans aren't reliable on a continuously-
flipping double-buffered scene.** The corruption-scanning technique
proven earlier (dump all physical pages, check for unexpected non-zero
bytes) produced an apparent few thousand stray bytes in picodiver's
letterbox border during active gameplay -- but the actual *displayed*
pixel at those exact coordinates (checked via `save-screen`, which always
captures the true current front buffer) was confirmed pure black, no
discrepancy at all. The scan reads *fixed physical page numbers*, which
alternate between being the displayed buffer and the actively-being-drawn
one every single frame (picodiver redraws and flips unconditionally every
frame, unlike the input-testing demo, which only flips on actual change) -- a scan
spanning multiple frames' worth of wall-clock time will sometimes catch a
page mid-draw, transiently incomplete, and mistake that for corruption.
The fix for next time: verify a continuously-animating double-buffered
scene via `save-screen` pixel sampling (always correct, since it reads
what's truly displayed), not a raw page scan -- reserve the raw-page
method for static scenes or paused/idle moments, as every prior milestone
that used it successfully happened to be.

**The Makefile's `clean` target needs to be scoped per game, not global.**
Added once multiple games started sharing one `lib/`, `clean` originally
did `rm -rf build/*` --
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
