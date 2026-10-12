# Input (btn/btnp) -- findings

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
MMU helpers in the earlier framebuffer spike).

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
formed, stable ball. Doesn't eliminate tearing *during* an actual change -- that needed real
double buffering, see below.

## RESOLVED (after interactive testing): tearing during actual movement needed double buffering, not just "redraw on change" and not more CPU speed

Playing it interactively (not just comparing screenshots) found the
"redraw on change" fix above wasn't enough: the ball visibly
flickered/tore *while moving*, settling down once it stopped -- exactly
the pattern you'd expect from redrawing in place inside a framebuffer the
display is simultaneously scanning out, since a moving sprite redraws
every single frame by definition (there's no "unchanged" case to skip).
First guess was raw CPU speed (the Next boots at classic-compatible
3.5MHz, and every `pico_pset()` pays real per-pixel overhead -- a DI/EI
pair plus two NextReg port accesses for the MMU page switch) -- bumping to
the Next's documented 14MHz (`REG_TURBO_MODE`/`RTM_14MHZ`) only helped "a
little" on real play, not enough. The real giveaway: `dark-room-next`'s
own Space minigame, which animates sprites every frame too, never tore --
but it uses Next *hardware* sprites (a separate compositing layer the
video hardware reads independently), not framebuffer pixel writes, so it
was never exposed to this class of problem at all regardless of CPU
speed.

**Fix**: proper double buffering via Layer 2's shadow bank
(`REG_LAYER_2_SHADOW_RAM_BANK`), the same mechanism both
`next-resources/code/zxnext_level9-master/src/layer2.c` and
`brahman-master-zxnext_magnetic`'s own `layer2.c` already use (confirmed
via their source, not invented from scratch) -- `pico_pset()`/`pico_cls()`
now always target the shadow (back) buffer, never the displayed (main)
one, and a new `pico_flip()` (`pico_video.h`) swaps which physical bank is
which. Since the back buffer alternates between two physical banks, each
one only reflects what was drawn into it *two* flips ago -- any animated
state has to be tracked per-buffer, not as one current value (see
`games/demo/main.c`'s `ball_x[2]`/`ball_y[2]`/`o_lit[2]`/`x_lit[2]`,
indexed by a `buf_idx` that toggles every `pico_flip()`). A
double-buffered game must also prime both physical buffers with identical
initial content itself (`pico_video_init()` only guarantees both start
*cleared*, not with any particular content) -- the demo does this by
drawing its static content and initial sprite twice, flipping in between.

**Verified rigorously, not just "looks fine now"**: `set-ui-io-ports`
(ZRCP's direct keyboard-matrix control -- deterministic, unlike
`send-keys-ascii`'s uncertain timing, see below) held RIGHT down for a
sustained period while four screenshots were taken mid-movement. Every
single one showed a complete, correctly-shaped 8x8 ball (208 pixels,
16x16 screenshot-px bounding box, exactly matching the stationary case) --
never a partial/torn shape, which is exactly what double buffering
predicts (the displayed buffer is never touched mid-draw) and exactly
what the pre-fix version failed at (a screenshot caught only the top few
rows of the ball mid-redraw). Also re-ran the full-framebuffer corruption
scan (`docs/layer2-notes.md`'s "RESOLVED" section's method) against
*both* physical buffers (12 pages now, not 6) -- zero corruption.

**Also resolves the cross-axis-drift mystery below**: the same sustained
RIGHT-held test showed the Y coordinate exactly constant (303.5,
unchanged) across all four mid-movement captures, only X changing. Using
`set-ui-io-ports` for deterministic, non-racing key state instead of
`send-keys-ascii`'s uncertain timing settles it -- pressing only RIGHT
really does only move RIGHT, confirming the earlier drift was a synthetic
rapid-tap timing artifact as suspected, not a real `pico_btn()`/`pico_input`
bug. No further interactive session happened to re-confirm this specific
point, but the deterministic ZRCP method used here is a much stronger
signal than the original timing-dependent one that raised the question.

## Earlier automated testing via `send-keys-ascii`, superseded by `set-ui-io-ports` above

Left as-written below for the record (including the original "not safe to
assume fixed" caution, which was the right call at the time -- it's what
motivated going further rather than guessing) -- see the "RESOLVED"
section above for how this was actually settled.

ZRCP's `send-keys-ascii`/`send-keys-event` can simulate keypresses, so
some verification was possible without actually sitting at the keyboard:

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
  something like that, in a way pressing one key at a time during normal
  play never would. But this is a plausible explanation, not a confirmed
  one -- the automated tooling available couldn't settle it either way,
  which is exactly the kind of thing described in the project's own
  stated testing philosophy ("measure, don't assume") as needing real
  interactive play or real hardware rather than more synthetic-input
  guessing.

**Original "what to check during interactive testing" ask, now
superseded**: hold RIGHT (P) alone for a bit -- does the ball ever drift
vertically? The deterministic `set-ui-io-ports` test in the "RESOLVED"
section above answered this (no drift, Y stayed exactly constant across
four mid-movement captures) -- still worth a glance during normal play,
since ZRCP testing however rigorous isn't the same as an actual keyboard,
but no longer an open question blocking anything.

## Deliberate scope cuts (not yet implemented)

- Single local player only -- PicoJS supports up to 4; multiplayer
  deferred until a real port needs it, consistent with the project's
  general "don't build speculatively" calls elsewhere (sprite-sheet
  banking, `palt()`).
- `btnp()` is edge-triggered only, no auto-repeat-after-hold -- Pico-8's
  own `btnp()` repeats after an initial delay; not implemented here.
