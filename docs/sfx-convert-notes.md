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
