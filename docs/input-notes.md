# M2.5 findings -- input (btn/btnp)

`lib/pico_input.c` (`pico_input_update`, `pico_btn`, `pico_btnp`) reads the
classic 48K-compatible keyboard matrix via z88dk's portable `<input.h>`
(`in_LookupKey()`/`in_KeyPressed()`), not a hand-rolled Next-specific scan.
`next-resources/code/TrainyardExpress-ZXNext-main/keyb.c` rolls its own
NextReg `$B0`/`$B1`-based reader instead, but inspection showed that's
there for *extended*-key support (a full PS/2 keyboard's keys beyond the
classic 40-key matrix) -- unnecessary for six ordinary letter keys, all
within the classic matrix the Next's PS/2 keyboard controller already
emulates for compatibility. Confirmed linking cleanly under
`-clib=classic +zxn` (no special library flag needed, unlike the Layer 2
MMU helpers in M0).

Keys: Q/A/O/P for up/down/left/right (classic Spectrum "QAOP" convention
-- the matrix has no real arrow-key signals), Z/X for the two action
buttons (Pico-8's own real keyboard defaults).

## Caught before any interactive testing: redrawing every frame, even when nothing moved, tears

The first version of the demo's game loop unconditionally erased and
redrew the moving sprite (and the two indicator boxes) every single frame,
regardless of whether anything had actually changed. A screenshot taken
seconds into an otherwise idle run (no keys pressed, ball stationary)
showed only the top few rows of the ball sprite -- caught the video
hardware's own raster scanout mid-redraw, the exact same class of bug
`dark-room-next`'s `docs/tilemap-notes.md` already documented and fixed
for repeatedly-redrawn text ("redrawing unchanged tilemap text every
frame causes visible tearing... fix: track what's currently drawn and
only reissue when content actually changes"). Fixed the same way here:
track the sprite's position and each indicator's lit state, and only
touch the framebuffer when something actually changed. Confirmed fixed --
multiple screenshots of the idle scene afterward all show a clean, fully-
formed, stable ball. Doesn't eliminate tearing *during* an actual change
(no double-buffering here, a bigger feature appropriately deferred), only
the much more common "nothing changed, but redrawn anyway" case -- worth
remembering for any future per-frame drawing in this project.

## Automated testing via ZRCP got this far, then hit a wall -- real keyboard testing needed

ZRCP's `send-keys-ascii`/`send-keys-event` can simulate keypresses, so
some verification was possible without a human at the keyboard:

- A single brief `send-keys-ascii` tap of any key consistently produced
  **no** measurable change -- plausible, not confirmed: the synthetic
  press is likely shorter than one 20ms/50Hz frame, so `pico_input_update`
  's once-per-frame sample never catches it "held" at all. Not a code bug
  by itself, just a limitation of tap-testing against a polling loop.
- Ten rapid `send-keys-ascii` taps of `'p'` (RIGHT only) did move the ball
  right, confirming the basic input -> movement -> redraw path works
  end to end. But it **also** moved the ball vertically both times this
  was tried -- up on the first run, down on a second, nominally identical
  run. Pressing only RIGHT should never move the ball on the Y axis at
  all; `pico_btn(PICO_BTN_UP)`/`pico_btn(PICO_BTN_DOWN)` are checked
  completely independently of `PICO_BTN_RIGHT` in `main.c`'s movement
  code, and `pico_input_update()`'s loop reads each of the 6 scancodes
  independently with no shared state between them -- nothing in the C
  source suggests a path where pressing one button's key could flip
  another button's reading.
- **Not resolved, and not safe to assume fixed.** The opposite-direction
  drift between two nominally identical runs points away from a
  deterministic logic bug (that would misfire the same way every time)
  and towards a timing artifact of rapid-fire synthetic key injection --
  real ZX Spectrum keyboard matrix hardware has a well-known "ghosting"
  quirk where certain key-state combinations can register a phantom key,
  and overlapping press/release timing from ten 50ms-apart synthetic taps
  landing against a 20ms frame poll is a plausible way to trigger
  something like that, in a way an actual human pressing one key at a
  time during normal play never would. But this is a plausible
  explanation, not a confirmed one -- the automated tooling available
  couldn't settle it either way, which is exactly the kind of thing
  described in the project's own stated testing philosophy
  ("measure, don't assume") as needing a real human or real hardware
  rather than more synthetic-input guessing.

**What to check during human testing**: hold RIGHT (P) alone for a bit --
does the ball ever drift vertically? Same for the other three directions
in isolation. If it only ever moves on the axis actually pressed, this
was a synthetic-input artifact as suspected and can be marked resolved.
If real keypresses reproduce any cross-axis drift, it's a genuine bug
needing further investigation (worth checking `in_LookupKey`'s returned
scancodes for 'q'/'a'/'o'/'p' directly, and/or trying a hand-rolled
single-port keyboard read as a cross-check against `<input.h>`, before
assuming the matrix-ghosting theory above).

## Deliberate scope cuts (not yet implemented)

- Single local player only -- PicoJS supports up to 4; multiplayer
  deferred until a real port needs it, consistent with the project's
  general "don't build speculatively" calls elsewhere (sprite-sheet
  banking, `palt()`).
- `btnp()` is edge-triggered only, no auto-repeat-after-hold -- Pico-8's
  own `btnp()` repeats after an initial delay; not implemented here.
