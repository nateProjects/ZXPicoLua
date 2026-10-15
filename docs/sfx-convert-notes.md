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

## Real bug found sharing the AY chip with music -- two rounds, the first fix was wrong

**Round 1 symptom**: first playable build produced only "a click" per
sfx, not the real held note -- confirmed by the user, who'd already
heard `games/sfxtest/` play correctly standalone (no music running there
to interfere). Diagnosed as: `pico_sfx_synth_update()` only ever called
`apply_note()` (the function that actually writes `psg_tone()`/
`psg_volume()`) on a note TRANSITION -- for every other tick of a note's
own `duration_ticks`, it just decremented a counter and left the AY
registers alone, relying on nothing else touching them in between. True
in isolation, but `pico_music_banked_play_frame()` rewrites all 3
channels' registers unconditionally every frame, reclaiming the sfx's
channel before its own next transition.

**That diagnosis was real but incomplete, and the fix made things worse,
not better.** Reasserting the note's registers every tick (not just on
transitions) turned "a click" into "a series of clicks" -- confirmed by
the user immediately. The actual, deeper cause: `update_channel_mask()`
(called from `apply_note()`, now running every single tick instead of
occasionally) rewrites the AY's WHOLE mixer register (R7) from scratch,
computed ONLY from this module's own 3-channel state -- any channel
`pico_sfx_synth` isn't actively driving gets its mixer bits forced to
"disabled," every time. `lib/pico_music_player.asm`'s Arkos playback
*also* rewrites R7 unconditionally every tick for its own 3 channels,
with no idea `pico_sfx_synth` exists. Whichever writes R7 last each
frame wins -- and since `pico_sfx_synth_update()` runs after
`pico_music_banked_play_frame()`, it always wins, muting whatever
channels Arkos was using that aren't the sfx's own channel, then
Arkos re-enables them next frame, repeating at 50Hz. That rapid gating
IS "a series of clicks." Making `apply_note()` run every tick (round
1's fix) just made this already-present conflict fire continuously
instead of only at note transitions.

**This is a known, real, unavoidable limitation of running two
independent AY-register-writing systems on ONE chip with no
coordination between them** -- not unique to this project. Checked
Shiru's AYFX driver for comparison (the user's own
`/Users/nat/Code/Games/ayfxResources/next-ayfx-main/src/ayfx.asm`,
real Z80 source, not NextBASIC-locked): its `AFXFRAME` routine has the
identical shape (an internal shadow mixer/noise byte, written
unconditionally every frame, blind to anything else using the chip) --
and its own README documents the same symptom under "Known issues":
*"if you're playing some music at the same time on the AY channels...
the sound can sometimes become crunchy. This is the noise mix being
left on when the AYFX jumps in."* Adopting AYFX on the same chip as
Arkos music would not have fixed this -- it has the same limitation,
just differently shaped.

**The real fix, found by reading AYFX's own source**: it supports
targeting any of the Next's 3 independent AY chips (`ay_chip_select`).
Running `pico_sfx_synth` on a SEPARATE physical chip from
`pico_music_banked`'s Arkos playback eliminates the register conflict
entirely -- not by coordinating who writes what, but by making sure
they never touch the same hardware registers at all. z88dk's own
`<arch/zxn.h>` already exposes this (`IO_TURBOSOUND` at port `$FFFD`,
`IT_SELECT_PSG_0`/`_1`, `RP3_ENABLE_TURBOSOUND` on NextReg `$08` --
numeric values confirmed against `libsrc/_DEVELOPMENT/target/zxn/
config_zxn_private.inc`, not just the symbolic names). `pico_sfx_synth`
now: enables Turbosound once (NextReg `$08` |= `0x02`, read-modify-write
to preserve other bits), selects PSG chip 1 before its own writes,
restores chip 0 immediately after -- transparent to the caller, and
needs ZERO changes to the vendored (frozen-on-purpose) Arkos player,
which never explicitly selects a chip itself and so always targets
whichever chip is currently selected when it runs; as long as
`pico_sfx_synth` always restores chip 0 before returning, Arkos's own
next per-frame write lands on chip 0 exactly as before. Round 1's
"reassert every tick" fix was reverted -- no longer needed once nothing
else shares the chip, AY registers hold their last-written value
indefinitely on their own.

Verified via the same `--aofile` capture method (real audio output,
not crashed/silent) -- but this specific "is the clicking really gone"
question is at the edge of what a crude waveform/autocorrelation check
can reliably distinguish from normal music+sfx texture (round 1's own
automated check incorrectly looked like an improvement when it wasn't,
a real lesson: don't trust automated signal analysis alone for a
timing-sensitive audible-artifact question like this one -- a listening
pass is the only real confirmation here, more than for any other AY
check in this project so far).

**User's own listening test: still just "a click," unchanged.** The
Turbosound separation above didn't fix anything audible, because it
never actually worked -- confirmed by direct ZRCP readback
(`tbblue-get-register 8` showed the enable bit set correctly) ruling
out the *enable* step, but the chip-SELECT values themselves
(`z88dk`'s own `IT_SELECT_PSG_0`/`_1` = `0x0F`/`0x0E`) turned out to be
wrong: real Turbosound Next chip-select needs the upper nibble SET
(confirmed via web search and this project's own prior spike notes,
which recorded `%11111101`/`%11111110` = `$FD`/`$FE`) -- `0x0E`/`0x0F`
instead land on ordinary, mostly-unused AY register indices 14/15, a
silent no-op, meaning `pico_sfx_synth` was writing to chip 0 the whole
time regardless of the "separation" code. Corrected to the literal
values `$FF`/`$FE` directly rather than trusting z88dk's own symbol
names -- still no audible change ("just a click still"). At this
point `pico_sfx_synth` was abandoned for picodiver (kept in the
library for other uses, like `games/sfxtest/`, which never shares a
chip with music) in favor of a completely different, independently-
authored player: AYFX.

## Switching to AYFX (Shiru's format)

The user had their own AYFX resources locally
(`/Users/nat/Code/Games/ayfxResources/`, including a real Z80 driver
port, `next-ayfx-main/src/ayfx.asm`, and example `.afb` effect banks)
and suggested trying it once `pico_sfx_synth` had failed three separate
times. Vendored just the engine (`AFXINIT`/`AFXPLAY`/`AFXFRAME` from
the "Minimal ayFX player v0.15" section, not the NextBASIC driver
wrapper around it) into `lib/pico_ayfx_player.asm`, adapting two real
things from the original: the hardcoded `START_OF_FX_BANK equ $c000`
(assumed a NextBASIC-paged bank) became an `EXTERN` to this project's
own linked effects-bank symbol, and `ay_chip_select` (a private
variable the NextBASIC driver's own API used to set) was made `PUBLIC`
so `lib/pico_ayfx.c` can set it directly. `tools/afb_to_c.py` embeds a
raw `.afb` file as a flat C byte array -- no format parsing needed
here, `AFXPLAY` parses its own format at runtime.

**Same AY-conflict lesson, independently reconfirmed**: AYFX's own
`AFXFRAME` has the identical shape as `pico_sfx_synth`'s own bug (an
internal shadow mixer byte, rewritten unconditionally every frame,
blind to anything else on the chip) -- and its own README documents
the exact same symptom ("can sometimes become crunchy... noise mix
being left on") when sharing a chip with other AY activity. Confirmed
directly: tested AYFX and Arkos music on the SAME chip (Turbosound off
entirely) specifically to see whether AYFX's architecture coordinated
any better than `pico_sfx_synth`'s did -- it didn't. User's own
report: "the music... sounds like it has white noise mixed in" --
continuously, not just during an sfx, exactly matching AYFX's default
idle mixer state (`0x00` = both tone AND noise generators enabled on
ALL channels per the AY's active-low convention), rewritten over
Arkos's own per-note mixer state every single frame.

**Turbosound separation, attempt 2 -- order matters.** With AYFX
correctly isolated to its own chip (same `$FF`/`$FE` literal values,
independently reconfirmed by reading AYFX's own `ay_chip_select`
convention -- a SECOND unrelated source agreeing with the earlier
correction), the FIRST attempt (enable Turbosound, start music, call
`pico_ayfx_init()` after) silenced the music ENTIRELY. Found a real
precedent while reading AYFX's own README: its NextDAW compatibility
note says *"make sure to install the NextDAW driver first, otherwise
you'll hear very bad audio corruption"* -- the same class of problem
(an order-of-operations dependency around enabling Turbosound).
Reversed the order (`pico_ayfx_init()`, which enables Turbosound,
BEFORE `pico_music_banked_init()`) -- this fixed it. Root cause not
fully explained (why enabling Turbosound mid-playback silences an
already-running AY chip is still not understood in detail), but the
fix is simple, real, and confirmed by the user directly: "that is much
better - the music still plays."

## The long-effect slowdown -- a real bug, but not where first suspected

With chip separation working, the user reported: "the music still
plays (albeit slows down) when I pick up a fish." Measured via ZRCP
(forced state + a frame-counter poll over fixed wall-clock windows,
not audio) -- confirmed a real, sustained ~35% framerate drop lasting
several seconds after a single trigger, recovering afterward. This
pointed at the triggered effect's own real DURATION, not a logic bug
-- `games/picodiver/picodiver_ayfx_bank.c`'s indices had been picked
arbitrarily (1/2/3) without knowing each effect's actual length.

**A real byte-alignment bug in my own manual analysis, not the vendored
engine.** Tried to verify by computing each effect's address from the
bank's own offset table by hand -- got nonsensical, wildly
out-of-file-bounds results for EVERY effect in TWO different bank
files, strongly suggesting a bug. Confirmed via web search that the
real AYFX bank format spec: *"offset +0 contains the total number of
effects... offset +1 contains a table of offsets."* **Byte 0 is the
effect COUNT, not the first table entry** -- my manual verification
script read the table starting at byte 0, one byte too early,
corrupting every single entry's value. The VENDORED ASSEMBLY ITSELF
was never wrong (it already used `bank+1` as the table base, matching
the real spec) -- this was purely a bug in my own diagnostic script,
not the shipped code.

Recomputing with the correct alignment, `next-ayfx-main/example/
example.afb`'s own 6 effects resolved to sensible in-bounds addresses
AND had real embedded names (the bank format stores a null-terminated
label after each effect): `0=empty 1=gold 2=levelup 3=gameover 4=key
5=hit`, with real sizes 30/29/80/336/74/40 bytes. **`gameover` (336
bytes, by far the largest) was exactly the effect assigned to the
fish-catch trigger** -- the single most frequent event in the game,
matching the user's own report precisely ("when I pick up a fish").
Reassigned to the three short effects instead: `gold` (catch, a
strong thematic match by luck), `hit` (bad hit), `key` (life lost, a
weaker thematic match but short). Confirmed by the user directly
afterward: "it seems okay - much better than before."

**Also discovered along the way**: the other 19 "named" `.afb` files
in `/Users/nat/Code/Games/ayfxResources/examples/` (`blip.afb`,
`hitbat.afb`, etc.) are byte-for-byte identical to each other (same
MD5) -- not 19 distinct sounds, but 19 copies of the SAME 20-effect
bank, whose own effects are anonymously labelled `noname`/`test1`
through `test19` (not real content). The filenames are unrelated to
what's actually inside -- a dead end for picking "the blip sound" by
filename alone; would need the same label-decoding approach used above
on `example.afb` to find anything meaningful in that bank, if it's
ever needed.

## Known-good configuration, as shipped

`pico_ayfx_init()` before `pico_music_banked_init()` (order matters,
see above); AYFX on Turbosound PSG chip 1, Arkos music on PSG chip 0;
effects picked by real embedded name/size, not blindly by index. Real,
picodiver-specific sound effects (replacing this placeholder bank)
are still a future task, same workflow as `sailing-by.aks` for music
-- author in Shiru's editor or an AYFX-compatible tool, export a
fresh `.afb`, convert via `tools/afb_to_c.py`.
