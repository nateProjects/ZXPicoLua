# Automated __sfx__ to AY converter -- findings

The other, cheaper half of audio support: unlike `lib/pico_music.c`
(Arkos Tracker 2, recompose by ear), `tools/p8_sfx_to_c.py` +
`lib/pico_sfx_synth.c` automatically converts a cart's own `__sfx__` data
to AY-3-8912 register writes, no composing required -- at a real,
accepted fidelity cost (see "Known limitations" below). Deliberately
started last, per the user's own explicit request, after the music
decision (`docs/music-notes.md`) was settled.

## Format: confirmed against three independent sources, not just one

The text-format `__sfx__` layout (168 hex digits per sfx: 4 header bytes,
then 32 notes x 5 hex digits) was cross-checked three ways before trusting
it: (1) the Pico-8 wiki's own Memory page (pasted in by the user), (2)
`dansanderson/picotool`'s `pico8/sfx/sfx.py` (a trusted independent cart
parser, fetched and read directly -- its `from_lines()` uses the exact
same `line[i:i+2]`/`[i+2:i+3]`/`[i+3:i+4]`/`[i+4:i+5]` slicing this
converter does), and (3) by hand against a real cart
(`/Users/nat/Code/Pico-8/celeste.p8`'s own sfx 0: first two notes decoded
by hand from the raw hex -- pitch `0x36`=54 and `0x23`=35 -- matched the
converter's own output exactly).

## Pitch-to-frequency formula: not in the wiki text, sourced from zepto8 instead

The pasted spec defines pitch as a 0-63 chromatic value but not what
frequency it maps to. Found via zepto8 (a byte-accurate C++ Pico-8
reimplementation, `src/pico8/sfx.cpp`, fetched directly rather than
guessed): `key_to_freq(key) = 440 * 2^((key - 33) / 12)` -- standard
12-tone equal temperament, pitch 33 = A4 = 440Hz. (zepto8's own note
NAMES differ from standard scientific pitch notation by 2 octaves --
their own code comments key 24 as "C2" where strict SPN would call that
frequency C4 -- but that's a naming-only quirk; the numeric formula is
what this converter actually uses, so it doesn't matter.) AY tone period
from frequency uses the same `110837.5 / freq` constant z88dk's own
`<psg.h>` `psgT()` macro already uses for `__SPECTRUM__` (the classic AY
clock, 1773400Hz, divided by 16) -- reused rather than re-derived.

## Known limitations (deliberate v1 scope cuts, not bugs)

- **Per-note effects (slide, vibrato, drop, fade_in, fade_out, arpeggio
  fast/slow) are parsed but not synthesized.** An affected note still
  plays, as a static tone/noise at its own pitch and volume for its full
  duration -- just without the effect's own pitch/volume motion. The
  converter prints a warning (to stderr) listing every affected sfx/note
  so this is never a silent downgrade. Real per-tick effect synthesis
  (porting zepto8's own `update_sfx_state()` logic -- vibrato is `freq *=
  1.059463` mixed over the note, drop ramps toward zero, etc.) is real,
  separate future work, not attempted here.
- **Custom waveforms (Pico-8 waveform values 8-15, meaning "use sfx
  slot N-8 as an instrument") are not synthesized** -- true support would
  need real 64-step wavetable playback, a materially bigger feature
  (likely needs the Next's own DAC output, not the AY's tone generator at
  all). Falls back to a plain tone at the note's own pitch instead, also
  warned about by the converter.
- **Every Pico-8 waveform except noise (6) collapses to a plain AY square
  tone** -- the AY-3-8912 has no hardware equivalent for Pico-8's
  sine/triangle/sawtooth/ringing/phaser shapes, only a true square-wave
  tone generator plus one shared noise generator. This is the same
  fidelity ceiling every AY chiptune conversion hits (confirmed by a real
  shipped port, CelesteNext, recomposing by ear rather than fighting
  this) -- not something a smarter converter could avoid without real
  software wavetable synthesis.
- **Volume scaling is a plain linear map** (Pico-8 0-7 -> AY 0-15, `round(v
  * 15/7)`) -- no attempt at matching Pico-8's own (likely non-linear,
  perceptual) volume curve.

## Verified: real AY output, not just "the converter ran and produced plausible numbers"

`games/sfxtest/` (`GAME=sfxtest`) plays a real converted sfx
(`celeste.p8`'s sfx 0, 4 sfx converted via `tools/p8_sfx_to_c.py --sfx 0 1
2 3`) every 2 seconds, forever, no video. Verified via `zesarux --aofile`
(dark-room-next's own proven method, `docs/music-notes.md`/that project's
`docs/sound-notes.md`) -- **found one new wrinkle this round**:
`--aofile` only captures real audio when `--ao coreaudio` is also passed
explicitly; `--ao null` (tried first, reasonably, since no one needs to
actually hear a ZRCP-only automated test) silences the whole audio
pipeline including the file sink, producing an empty capture. With `--ao
coreaudio --aofile <path>` the capture grew continuously and showed
exactly the predicted pattern: ~5 windows of oscillating (non-flat) byte
values (matching the sfx's own ~0.5 second duration) followed by a long
flat stretch pinned at the AY's own idle output level (~64), repeating
every ~2 seconds -- matching the test program's own `frame % 100`
retrigger exactly. Confirms real, controllable AY output reaching the
chip and genuine silence between/after plays, same verification ceiling
as every other AY check in this project (confirms the pipeline works, not
pitch-accuracy-by-ear -- that still needs a real listening pass once
someone cares about a specific sfx's exact sound).

## Resident-size cost -- also opt-in, smaller than Arkos but still real

`lib/pico_sfx_synth.c`'s own code is small (~1KB, confirmed via the
`.map` file) -- but linking it still pulls in `<psg.h>`'s own library
functions (`psg_tone`/`psg_volume`/`psg_channels`/`psg_noise`/
`psg_init`/`set_psg_callee`), and the combined cost tipped picodiver's
already-tight build over by 707 bytes when tried unconditionally. Same
fix as `pico_music.c`: excluded from the Makefile's default `LIB_SRCS`
wildcard, opt-in per game (see the Makefile's own comment next to that
line).

## Usage

```sh
python3 tools/p8_sfx_to_c.py cart.p8 --sfx 0 1 2 --out sfx_data.c --symbol my_sfx
# or: --all for every one of the 64 sfx slots

# in game code:
#include "pico_sfx_synth.h"
extern const pico_sfx_t my_sfx_0;
...
pico_sfx_synth_play(&my_sfx_0, 0);   /* channel 0-2, matches Pico-8's own sfx(n, channel) */
...
pico_sfx_synth_update();              /* once per frame */
```

Opt into `pico_sfx_synth.c` in the Makefile the same way `games/sfxtest/`
does (`LIB_SRCS += $(LIB_DIR)/pico_sfx_synth.c` in that game's own `ifeq`
block) -- it's excluded by default for the same resident-budget reason as
`pico_music.c`.

## Shipped in PicoDiver for real (2026-10-15)

PicoDiver's own `sfx(n)` call sites (5 in the original cart, collapsing
to 3 distinct sounds -- `docs/picodiver-notes.md`'s "Deliberately not
ported" list had these marked skipped since the port's very first
milestone) are wired up for real now, converted directly from the real
cart's own `__sfx__` data (`/Users/nat/Code/Pico-8/PicoDiver/picodiver2.p8`,
sfx 0/1/2 -- confirmed by reading every `sfx()` call site in the cart
source, not guessed): `games/picodiver/picodiver_sfx.c`. Mapping (read
off the cart directly, no `docs/reference` for audio existed before
this): sfx(0) = a bad hit (octopus or creature, score drain), sfx(1) =
life lost (oxygen ran out), sfx(2) = caught a fish (score up). All three
converted with zero warnings (no per-note effects, no custom waveforms
in any of the three -- full fidelity, not the lower-fidelity fallback
path this doc warns about above).

Resident, not banked -- affordable now that `picodiver_data.c`'s own
banking (`docs/picodiver-notes.md`'s "Stack/BSS collision" section)
freed up thousands of bytes of margin; the historical "707 bytes over"
finding above no longer applies to picodiver specifically (it was
measured against a build with far less headroom). Cost this time:
`BSS_END_tail` moved from `$E0C6` to `$E849`, ~1923 bytes -- still
leaving over 6000 bytes of margin.

Plays alongside the real Arkos background music (`docs/music-notes.md`)
on the same AY chip. Verified via a `zesarux --aofile` capture spanning a
forced trigger (ZRCP memory writes forcing `cab_oxy` to 0 mid-game,
rather than relying on uncertain simulated keypresses -- see
`docs/picodiver-notes.md`'s own note on that).

## Real bug found sharing the AY chip with music: a held note audible for one frame only

First playable build produced only "a click" per sfx, not the real held
note -- confirmed by the user, who'd already heard `games/sfxtest/` play
correctly standalone (no music running there to interfere). Root cause:
`pico_sfx_synth_update()` only ever called `apply_note()` (the function
that actually writes `psg_tone()`/`psg_volume()`) on a note TRANSITION --
for every other tick of a note's own `duration_ticks`, it just decremented
a counter and left the AY registers alone, relying on nothing else
touching them in between. That's fine in isolation (`sfxtest` has nothing
else writing to the chip), but `pico_music_banked_play_frame()` runs
*every* frame and (like most tracker players) rewrites all 3 channels'
registers unconditionally each tick -- so a sustained sfx note was only
ever actually audible for its own trigger frame; the very next frame's
music tick silently reclaimed the channel, and `pico_sfx_synth_update()`
had no reason to fight back until the note-transition boundary, several
frames later and already too late to matter perceptually. Fixed by
reasserting the current note's registers every tick an sfx channel is
playing, not just on transitions -- a few redundant identical AY writes
per frame when nothing else shares the chip (harmless), but now actually
correct once something else does. Re-verified via the same `--aofile`
capture method: a windowed autocorrelation scan now shows long runs
(15+ consecutive windows) holding the SAME dominant period, consistent
with a real held note surviving across many frames -- not the single-
frame spike the bug produced. Whether each individual sfx sounds right
by ear is still a listening question for the user, same verification
ceiling as every other AY check in this project -- but it's now at
least audible for its real duration, not one frame.
