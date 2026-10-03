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

## Open, not chased further: a handful of stray single-pixel colours in screenshots, and the default palette

A `save-screen`/ZRCP screenshot of the demo shows ~10 single-pixel spots
in colours (white, orange, magenta, yellow) that **never appear anywhere
in the direct memory reads above** -- the underlying framebuffer bytes at
every row checked are clean. Two things ruled out by the memory-read
evidence: this is not a `pico_pset()` corruption bug (the bytes are
provably correct), and it's not simply "the default palette renders index
0 oddly" (same conclusion -- the data is right, so any odd appearance is
in the *display* path, not the *write* path). Most likely a ZEsarUX-side
rendering artifact (a HUD/status overlay, or a brief ULA-layer bleed at
specific scanlines) given the colours fall outside this test pattern's
own 0-7 palette range entirely. Also open: indices 6 and 7 render
visually near-black on screen despite their framebuffer bytes being
correct (`06`/`07` exactly where expected) -- reading `REG_PALETTE_VALUE_8`
back after setting `REG_PALETTE_INDEX` just echoed the index instead of a
colour, so the *default* Layer 2 palette's actual RGB332 contents weren't
pinned down this session. **Neither blocks M1**: the library will program
its own palette explicitly once `pal()`/`palt()` exist (M3), at which
point default-palette quirks stop mattering. Per `sprite-notes.md`'s own
precedent for an unresolved ZEsarUX-only oddity: worth a real-hardware or
second-emulator (CSpect/MAME) check before relying on default-palette
behaviour, not worth chasing further in a spike whose actual goal (prove
the access mechanism) is already solidly proven above.

## Summary for M1

- Access mechanism, resolution default, and page-crossing address math:
  proven, via direct memory reads, not just visual inspection.
- `ZXN_WRITE_MMU2` is a dead end under `-clib=classic` -- use
  `ZXN_WRITE_REG`/`ZXN_READ_REG` on NextReg `$52` instead, same pattern as
  `bank.c`.
- Any loop reaching all the way to `PICO_SCREEN_W`/`PICO_SCREEN_H` needs a
  counter wider than `uint8_t`.
- Default Layer 2 palette contents and the handful of stray screenshot
  pixels are open curiosities, not correctness risks -- revisit if `pal()`
  work in M3 turns up something related, otherwise leave them.
