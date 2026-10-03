# M0 spike findings -- Layer 2 256x192 8bpp framebuffer

De-risking pass for the whole ZXPicoLua project, the same role
`dark-room-next/docs/tilemap-notes.md` and `sprite-notes.md` played there.
Two real, shipped z88dk Next games in `next-resources/code/`
(`zxnext_level9-master/src/layer2.c`, `brahman-master-zxnext_magnetic/.../
layer2.c`) gave the starting mechanism; everything below is this project's
own measurement against that starting point, adapted to 256x192 instead of
their 320x256.

## Toolchain gotcha: `ZXN_WRITE_MMU2`/`ZXN_READ_MMU2` are not linkable under `-clib=classic`

`arch/zxn.h` declares both as `extern` symbols, and both reference games
use them successfully -- but they build with `-clib=sdcc_iy`, not
`-clib=classic`. Under `-clib=classic` (this project's target, matching
`dark-room-next`), linking against them fails outright:
`undefined symbol: ZXN_WRITE_MMU2`. Confirmed the symbol genuinely isn't
present anywhere in this z88dk checkout's compiled `-clib=classic`
libraries (`grep -rl ZXN_WRITE_MMU2 lib/` turned up nothing).

**Fix**: write the MMU slot's NextReg directly via `ZXN_WRITE_REG`/
`ZXN_READ_REG` instead -- proven working, since `dark-room-next/src/bank.c`
already does exactly this for MMU slot 1 (NextReg `$51`). MMU slot *N*'s
NextReg is `$50+N` (confirmed against `bank.h`'s own comment for slot 1),
so slot 2 (`$4000-$5FFF`, used here) is NextReg `$52`. See
`lib/pico_video.c`'s `mmu2_page_in()`/`mmu2_page_out()`.

## NextReg `$70` (Layer 2 Control Register) cold-boot default is 256x192

Not findable via grep in `zxn.h` (no resolution-select macro exists there).
Settled two ways:
- `next-resources/code/operation-jeff-main/src/screen.c` and
  `zxnext_level9-master/src/main.c` both write `ZXN_NEXTREG(0x70, 0x10)`
  specifically to select *320x256* 8bpp mode -- i.e. bit 4 set. Neither
  writes `$70` for the classic resolution, implying bit 4 clear (0x00) is
  it.
- Measured directly: `tbblue-get-register 112` (ZRCP, decimal index,
  **not** `0x70` -- the command rejects a `0x`-prefixed argument) read
  back `00H` under `--tbblue-fast-boot-mode`, with no explicit write to
  `$70` anywhere in this project's code, and the resulting framebuffer
  (see below) is laid out exactly as a 256-wide/192-tall/8bpp surface
  would be. **No explicit NextReg `$70` write is needed for this
  project's target resolution** -- left unwritten in `pico_video_init()`
  by design, not by oversight.

## Access mechanism confirmed, including across a page boundary

`REG_LAYER_2_RAM_BANK` (NextReg `$12`/18 decimal -- not `$14`, which is
actually `REG_GLOBAL_TRANSPARENCY_COLOR`; easy to mix up, caught by
checking `config_zxn_nextreg.m4`'s actual numeric `defc`s rather than
trusting a half-remembered register map) read back `09H` live, giving
`base_page = 9 << 1 = 18`.

Verified the actual framebuffer bytes directly via ZRCP (`tbblue-set-register
82 18` to page physical page 18 into MMU slot 2 -- NextReg `$52` = 82
decimal -- then `read-memory 16384 256`), **bypassing this project's own
`pico_pset()` entirely** -- this reads hardware state, not just "what our
code thinks it wrote":
- Row 0 (address `$4000`, page 18): exactly `00`x32, `01`x32, ..., `07`x32
  -- byte-perfect match for the demo's 8-bar test pattern (see
  `games/demo/main.c`).
- Row 31 (address `$5F00`, still page 18 -- last row before the page
  boundary, since a page is 32 rows of 256 bytes) and row 32 (address
  `$4000` of the **next** physical page, 19) both read back the identical
  correct pattern. **The page-crossing address math in `pico_pset()`
  (`page = base + offset>>13`, `addr = 0x4000 + offset&0x1FFF`) is proven
  correct at the one boundary that matters most** -- the 6-pages-of-8K
  split this project's 256x192 (48KB) framebuffer needs, as opposed to the
  two reference games' 10-page/320x256 (80KB) split.

## Pitfall confirmed by this spike: a `uint8_t` loop bound of 256 never terminates

The demo's own color-bar test pattern (`for (uint8_t x = 0; x <
PICO_SCREEN_W; x++)`, `PICO_SCREEN_W` = 256) does not terminate: an 8-bit
counter can never equal 256, so the comparison is always true (z88dk's
own `-Wlimited-range` warning caught this directly). Confirmed by its
effect: the uncorrected build never reached its own `intrinsic_halt()`
loop and kept re-running the fill continuously; a later direct memory
read still showed correct data because the fill is idempotent per-pixel,
but relying on that would be fragile for any future *stateful* per-frame
code. **Fix applied**: loop counters that need to reach `PICO_SCREEN_W`
(256) must be wider than `uint8_t` (`main.c` now uses `uint16_t x`, cast
down only at the `pico_pset()` call site, which keeps the parameter
`uint8_t` since any single coordinate value 0-255 fits exactly). Worth
remembering for every future full-screen-width loop in `pico_gfx.c`, not
just this demo.

## Logical canvas: a free coordinate offset, not a scaled blit

Flagged before M1 started: ported Pico-8 content is natively 128x128, not
256x192, and uses a fixed 16-colour palette. Addressed now rather than
after M1's primitives were built assuming a single fixed 256x192 canvas,
since retrofitting an offset into every primitive later would be far more
work than building it in from the start.

**Resolution**: `pico_video_init()` now takes a logical width/height and
centres it within the fixed 256x192 physical surface -- e.g. 128x128 gets
a 64px left/right, 32px top/bottom black border. Implemented as a static
`origin_x`/`origin_y` pair added to every incoming logical coordinate
before the existing page/address math (`lib/pico_video.c`); every current
and future primitive routes through this unchanged, so this cost nothing
structurally. Confirmed working: a 128x128 centred test pattern (colour
bars, `games/demo/main.c`) rendered with the expected black letterbox
border on all sides, screenshot-verified.

**Scaling (filling more of the physical screen) is deliberately not
implemented.** 2x would be 256x256, taller than the physical 192 rows;
the next clean fit is 1.5x (192x192), but that requires drawing into an
offscreen 128x128 buffer and blitting it scaled once per frame, not just
an address offset -- a materially bigger feature (extra RAM for the
offscreen buffer, a non-trivial scaling blit, and an unknown per-frame
CPU cost on real Z80 timings) that no actual port has asked for yet.
Deferred until a real game needs it rather than spent now.

**Palette: map Pico-8's 16 colours onto Next-equivalent RGB in palette
slots 0-15, leave 16-255 free for the Next's fuller palette.** The Next's
palette registers support 9-bit (RGB333) colour via two writes to NextReg
`$44` (`REG_PALETTE_VALUE_16`, confirmed in
`config_zxn_nextreg.m4` -- misleadingly named for register width, not bit
depth; the reference `layer2_set_palette()` in both next-resources layer2.c
files already uses it this way), enough precision for a reasonable Pico-8
colour match. Since Layer 2 is already a full 256-colour indexed surface,
this needs no separate code path: seed indices 0-15 with the Pico-8-
equivalent 9-bit RGB values at init, and a ported game's `pset(x,y,c)`
with `c` in 0-15 just works, while a native ZXPicoLua game can use indices
16-255 (or override 0-15) directly for the Next's full range. Not yet
implemented -- belongs with M1/M3's `pal()`/`palt()` work, where the
actual Pico-8-to-Next colour table gets built and loaded.

## Follow-up: the "index 6/7 render black" mystery from the first spike is positional, not palette-related

The 128x128 centred test reused the same colour values (0-7) as the
original 256-wide test, just at different *physical* screen columns
(bars now sit at physical x 64-191, not 0-255). Colour index 7, which
rendered solid black in the original 256-wide test (physical x 224-255),
renders as the expected bright blue here (physical x 176-191) --
**same index, different physical position, different visible result**,
which rules out a palette-table explanation (that would reproduce
regardless of screen position) and points instead at something tied to
specific physical columns near the right edge of the 256-wide test
(possibly a border/edge rendering quirk, unconfirmed). Consistent with
the original memory-read evidence, which already showed the underlying
framebuffer bytes were correct everywhere. Still not chased further --
doesn't affect the logical-canvas design above, and real ZXPicoLua/ported
content won't usually paint all the way to the physical edge columns
anyway.

The handful of stray single-pixel screenshot artifacts (see below) also
reappeared in this test, at the same relative row spacing, confirming
they're independent of what's actually drawn -- further support for the
"ZEsarUX-side overlay, not a data bug" read.

## RESOLVED (post-M1): the stray single-pixel screenshot colours were real data corruption, not a ZEsarUX overlay -- root cause was the classic ROM's interrupt handler, not our own code

The original read here concluded the stray pixels were a ZEsarUX rendering
artifact, on the strength of one spot-check: a few specific rows read back
clean via direct memory reads. **That spot-check didn't generalize, and
the conclusion was wrong.** Revisited after the same dots reappeared in
M1's completely different demo scene (see `gfx-notes.md`) -- a proper
pixel-by-pixel screenshot diff against each column's own majority colour,
followed by reading the *exact* framebuffer bytes at the flagged
coordinates, found genuine stray values (`0xEF`, `0xED`) sitting in
framebuffer locations this project's code had only ever written `0x00`
(the border/letterbox area, cleared once by `pico_cls(0)` and never
touched again). Real corruption, not a display artifact -- the first
spot-check had simply gotten lucky and sampled rows that weren't hit.

**Root cause**: this project's `pico_pset()`/`pico_cls()` page a Layer 2
physical page into a fixed MMU window, write/clear through it, then page
back out -- a brief but real critical section. The window originally used
MMU slot 2 (`$4000-$5FFF`), which overlaps the classic ZX ROM's system
variables area (`$5B00-$5CFF`) -- the same range `dark-room-next`'s own
`docs/tilemap-notes.md` already flagged as hazardous for an unrelated
reason. Since this project runs with interrupts enabled throughout (one
`intrinsic_ei()` at startup), the ROM's 50Hz interrupt handler keeps
scanning the keyboard and writing its own housekeeping bytes (almost
certainly `KSTATE`, which the standard ROM keyboard scan updates every
interrupt) into that fixed absolute address range -- regardless of which
Layer 2 physical page happened to be paged into the window underneath it
at that moment. An interrupt landing during the handful-of-microseconds
window one `pico_pset()` call holds a page mapped is individually
unlikely, but with tens of thousands of calls across a real scene, a few
unlucky hits are expected -- matching exactly what was observed (a
handful of corrupted bytes out of 49152, varying between runs).

**Fix, and how it was actually verified**: two changes in `pico_video.c`.
(1) Moved the MMU window from slot 2 to slot 3 (`$6000-$7FFF`, NextReg
`$53`) -- clear of the ROM system variables range. This alone was
**not** sufficient -- re-testing with a full memory dump (below) still
showed corruption, meaning something can still land writes in that range
too (not root-caused further; the ROM interrupt handler remains the prime
suspect, but its exact reach wasn't pinned down byte-for-byte). (2) Wrapped
every critical section in `intrinsic_di()`/`intrinsic_ei()`
(`<intrinsic.h>`) -- this is the fix that actually mattered, confirmed by
rigorous measurement, not inference: a Python script drove ZRCP to dump
**all 48KB** of the Layer 2 framebuffer (all 6 physical pages, every
byte, not a sample) after a 12-second stress run (600 interrupts' worth of
opportunity, versus the few seconds the original spot-check ran), checked
every byte outside the area this project's own demo actually draws to
against the expected `0x00`, and found zero mismatches -- versus 14-15
real mismatches using the same full-dump method before the DI/EI fix was
added. This is the same "measure, don't assume" standard the project
holds itself to elsewhere, applied properly this time: the dots looked
cosmetic and were dismissed as an emulator quirk once; chasing them
properly the second time found a genuine, fixable correctness bug.

**Remaining open item, unrelated to the above**: indices 6 and 7 still
render visually near-black on screen in some earlier spot-checks despite
their framebuffer bytes being correct -- reading `REG_PALETTE_VALUE_8`
back after setting `REG_PALETTE_INDEX` just echoed the index instead of a
colour, so the *default* Layer 2 palette's actual RGB332 contents weren't
pinned down. Doesn't block M1/M2: the library will program its own
palette explicitly once `pal()`/`palt()` land (M3), at which point default-
palette quirks stop mattering -- but given the lesson just learned, this
should get the same full-rigour re-check (not just dismissed) before M3
actually ships `pal()`.

## Summary for M1

- Access mechanism, resolution default, and page-crossing address math:
  proven, via direct memory reads, not just visual inspection.
- `ZXN_WRITE_MMU2` is a dead end under `-clib=classic` -- use
  `ZXN_WRITE_REG`/`ZXN_READ_REG` directly instead, same pattern as
  `bank.c`. **Use NextReg `$53` (MMU slot 3, `$6000-$7FFF`), not `$52`/slot
  2** -- superseded post-M1, see the "RESOLVED" section above: slot 2
  overlaps the ROM's system variables, and even after moving off it, every
  access through this window must also be wrapped in
  `intrinsic_di()`/`intrinsic_ei()` to actually stop interrupt-driven
  corruption.
- Any loop reaching all the way to `PICO_SCREEN_W`/`PICO_SCREEN_H` needs a
  counter wider than `uint8_t`.
- Default Layer 2 palette contents and the handful of stray screenshot
  pixels are open curiosities, not correctness risks -- revisit if `pal()`
  work in M3 turns up something related, otherwise leave them.
