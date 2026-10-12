# Shape and text primitives -- findings

`lib/pico_gfx.c` (rect/rectfill, line, circ/circfill, oval/ovalfill, fillp,
print/cursor) built entirely on `pico_pset()` from the Layer 2 framebuffer
spike -- no new hardware access. Verified with a single combined demo scene
(`games/demo/main.c`) exercising every primitive together, screenshotted and
pixel-inspected via the same ZRCP flow as that spike.

## Bounds-checking added beyond the original framebuffer primitives

`pico_pset()`/`pico_pget()` stay unchecked by design (cheap hot path). But
every composite shape here derives coordinates via subtraction -- a
circle's `cx-r`, an ellipse's `cx-x` -- which can legitimately go negative
for a shape that's simply partly off-screen (`circfill(2, 2, 10, ...)` is
an ordinary small circle near a corner, not misuse). Casting a negative
value straight to `uint8_t` would wrap (`-8` -> `248`) and corrupt the
framebuffer at a wildly wrong position instead of clipping, the way
Pico-8/PicoJS's own renderer does. Addressed with `pset_safe()` and a
clipped `fill_run()` (both take signed `int16_t`), which every primitive
routes all actual pixel writes through. Not in the original framebuffer
design -- added once building real shapes made the risk concrete, same as
that earlier spike's own `uint8_t`-loop-bound finding.

## Oval: hand-derived integer midpoint ellipse algorithm, not a port of PicoJS's float version

PicoJS's own `plotOval()` uses `Math.sqrt()` per scanline (fill) or
`Math.sin()`/`Math.cos()` per step (outline) -- fine in a browser, a poor
fit for Z80 (no hardware float, and z88dk's software float/sqrt is slow
per-pixel). Implemented instead as the standard two-region integer
midpoint ellipse algorithm (Hearn & Baker), using `int32_t` decision
variables (`rx`/`ry` up to ~128, `rx*ry` products comfortably fit) and no
floating point or sqrt at all. This is **not** a port of existing
PicoJS/Pico-8 source -- a from-scratch implementation against the
well-known algorithm, so it got the most scrutiny: rendered and visually
inspected at 3-4x zoom during development for symmetry and smoothness at
the region1/region2 transition. Came out clean on the first attempt --
round, symmetric, no gaps or doubled pixels -- but this is the one
primitive here without a second independent source to cross-check against
(unlike line/circle,
which are direct, proven ports of PicoJS's own Bresenham implementations),
so worth a second look (an extreme aspect ratio, e.g. `rx` >> `ry`, or
very small radii) if an oval bug ever surfaces in real use.

## Caught by screenshot, not by inspection: 0xAAAA is stripes, not a checkerboard

The demo's first `fillp()` test used `0xAAAA` as a "checkerboard" pattern
-- looked plausible by inspection (alternating bits), but every row of a
4x4 `fillp` tile is the same 4-bit nibble (`A` = `1010`) repeated, so it
actually renders as 1px vertical stripes, not a checkerboard (confirmed
via screenshot, then understood from the bit layout). A true checkerboard
needs rows to alternate too: `0xA5A5` (`1010/0101/1010/0101`). The
underlying `fillp` mechanism itself was correct the whole time -- this was
purely picking the wrong demo constant, caught by the project's established
"measure, don't assume" habit rather than being assumed correct from the
code alone.

## Text: ported PicoJS's exact Pico-8 4x6 font data, not re-authored

`lib/pico_font.c`'s 570-byte glyph table was extracted **programmatically**
from PicoJS's own `PICO8_FONT_HEX` string (a Python script parsed
`picoJS.js` directly and converted hex digits to a C byte array) rather
than hand-transcribed, to rule out transcription error in 570 characters
of hex data. Verified rendering correctly in the demo (`print()` and
`cursor()`/`print_at_cursor()`, including an embedded `\n` splitting into
two correctly-positioned lines).

## RESOLVED: the stray single-pixel artifacts were real corruption, not a display quirk

The same handful of isolated, off-content single-pixel colour anomalies
from the earlier Layer 2 framebuffer spike (`docs/layer2-notes.md`)
reappeared in this completely different scene (text/shapes instead of
colour bars) -- e.g. a single stray dark-blue pixel sitting in empty black
space near the line-X test, on neither line's path. At the time this was
logged as "still reads as a ZEsarUX-side rendering artifact... worth
keeping in mind", following that earlier (as it turned out, premature)
conclusion. **That conclusion was
wrong.** Chased down properly afterward: the framebuffer bytes at these
exact coordinates genuinely held stray data our code never wrote. Root
cause was the classic ROM's 50Hz interrupt handler writing into a fixed
system-variable address range that happened to overlap the MMU window
`pico_pset()`/`pico_cls()` use to reach Layer 2 RAM -- full writeup and
the fix (move the window, and critically, disable interrupts around each
access) in `docs/layer2-notes.md`'s "RESOLVED" section. Three independent
sightings across two different scenes turned out to be the same real bug,
not three coincidental emulator quirks -- a reminder that an anomaly
matching an earlier "probably harmless" explanation is a reason to
double-check that explanation, not a reason to stop looking.

## Deliberate API deviations from PicoJS/Pico-8 (see pico_gfx.h for the full rationale)

- Every primitive takes an explicit colour argument -- no implicit
  "current colour" fallback (PicoJS/Pico-8 allow omitting colour to reuse
  the last `color()` call). C has no clean optional-trailing-argument
  mechanism without a sentinel or a second function; threading a global
  "current colour" through every primitive adds real complexity for a
  convenience a ported game can trivially work around.
- `print(text, x, y, color)` and `print_at_cursor(text, color)` are two
  functions instead of PicoJS's single `print()` overloaded by argument
  count (`print(str)` / `print(str, col)` / `print(str, x, y)` /
  `print(str, x, y, col)`) -- C has no argument-count-based dispatch.
- `fillp(pattern, transparent)` takes transparency as an explicit second
  argument instead of Pico-8's fractional-pattern convention
  (`fillp(pattern + 0x0.8)`), which relies on a real Lua float literal
  trick with no clean C equivalent.
- `camera()` was **not** implemented alongside the rest of these
  primitives originally -- introducing it meant revisiting every
  primitive's clipping for the much-more-common negative-coordinate case
  a scrolling camera implies, deferred at the time until a real port
  needed it (it's since been added -- see `pico_gfx.h`).
