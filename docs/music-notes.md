# Music/sfx (Arkos Tracker) -- findings

Decision (2026-10-12): for anything worth actually listening to, recompose
by ear in Arkos Tracker 2 rather than auto-convert a cart's own
`__sfx__`/`__music__` data -- the AY chip's own waveform/effect ceiling
makes a faithful synthesis-level converter a materially bigger, separate
undertaking (see `docs/pico8-vs-zxpicolua-reference.md`'s own audio
section), and a real shipped Pico-8-to-Next port (CelesteNext,
`/Users/nat/Code/Games/CelesteNext-main`) already proved this is the
practical path -- its own README: "I transcribed and adapted the original
music and sound effects manually, using Arkos Tracker 2. The AY chip is
more limited than the Pico-8 tracker, so I had to downgrade some
details." (The automated `__sfx__`-to-AY converter mentioned here as a
later task was since built anyway -- see `docs/sfx-convert-notes.md` --
as a deliberately lower-fidelity, zero-composing-required alternative,
not a replacement for this decision.)

**Update 2026-10-13: full playback confirmed against a real track**, not
just the player linking in isolation -- see "Full playback confirmed"
below.

## z88dk already bundles a working Arkos Tracker player -- confirmed under this exact build target

CelesteNext vendored its own copies of the Arkos player asm
(`arkos2akg.asm`, `at2akgplayer.asm`) directly into its own repo. Checked
whether that was actually necessary first, rather than assuming it was:
z88dk's own tree already ships a proper `-clib=classic`/sccz80-targeted
build of the same player --
`libsrc/target/zx/psg/asm_arkos_zx_smc.asm` (auto-generated from
`support/arkos/`, confirmed by its own header comment: "Do not modify
this file directly... regenerate with the proper Makefile recipes"), with
a real public C header, `<psg/arkos.h>`
(`ply_akg_init`/`ply_akg_play`/`ply_akg_stop`/`ply_akg_initsoundeffects`/
`ply_akg_playsoundeffect`). Confirmed linking cleanly under this exact
target (`zcc +zxn -clib=classic -subtype=nex`) via a standalone throwaway
test, same "measure, don't assume" discipline as dark-room-next's own
`<psg.h>` spike (`dark-room-next/docs/sound-notes.md`) -- no vendoring
needed, same "z88dk already has this" result that spike found for plain
PSG tones.

`lib/pico_music.c`/`.h` wraps this C API directly (`pico_music_init`/
`pico_music_play_frame`/`pico_music_stop`/`pico_sfx_init`/`pico_sfx_play`)
-- a 1:1 pass-through, not a reimplementation.

## Real, measured resident-size cost -- opt-in, not wildcarded in by default

Unlike every other `lib/*.c` module, `pico_music.c` is **excluded from the
Makefile's default `LIB_SRCS` wildcard** (see the Makefile's own comment
next to that line). Confirmed the hard way, not guessed: adding it
unconditionally took picodiver's build from 14336 free bytes to a large
outright overflow (`Section exceeds 64k [0x8000,0x10ad2]`) -- the bundled
Arkos player is ~2200 lines of heavily-unrolled AY-trick asm, and the
linker pulls in the whole thing the moment anything references
`ply_akg_*`, whether or not a game ever actually calls
`pico_music_init()`. A game that wants music/sfx opts in explicitly (adds
`pico_music.c` to its own `LIB_SRCS` in its own Makefile `ifeq` block,
same spirit as `BANKED_CSRCS`) rather than paying this cost by default.

## Verified: the wrapper itself links and runs, not just the raw header

`games/musictest/` (`GAME=musictest`) opts in and confirms `pico_music.c`
itself -- not just the raw `<psg/arkos.h>` throwaway test above --
compiles and links through this project's real build pipeline. It only
calls `pico_music_stop()` (the one entry point that needs no song data --
it just silences the AY channels), deliberately, since this is the only
piece this project's own committed test can self-check without real
content (`games/sfxtest/`'s own synthetic-blip precedent doesn't apply
here -- there's no way to *compose* a tiny Arkos song programmatically the
way a few hex digits can hand-author a Pico-8 sfx). Confirmed via ZRCP
screenshot: boots cleanly to "PICO_MUSIC.C LINKED OK / (no song content
yet)" with no crash.

## Full playback confirmed (2026-10-13), against a real track -- not committed, see below

The user provided a real Arkos Tracker .aks file (one of Arkos Tracker 3's
own bundled example songs) for local verification. Full pipeline
confirmed end to end: converted, linked, and **actually produced
continuous real music on the emulated AY chip** -- a `zesarux --aofile`
capture over 12 seconds showed every single sampled window oscillating
(91/91 "active"), a clearly different signature from `games/sfxtest/`'s
own short blip-then-silence pattern, exactly what real continuous music
playback should look like versus a one-shot sound effect.

**Two real, previously-undocumented gaps found getting there, both now
handled by `tools/arkos_song_to_asm.py`:**

1. **Only Arkos Tracker 3 was actually installed, not Arkos Tracker 2**
   (`/Applications/Games/ArkosTracker3`) -- and AT3's `SongToAkg` CLI
   syntax has changed from what z88dk's own `support/arkos/
   arkos-song-to-asm.pl` expects. That script's `-sppostlbl :` option is
   gone in AT3 (`Error: Unknown option: sppostlbl`) -- confirmed directly,
   not assumed from a version number. AT3's own `SongToAkg --help` lists
   no equivalent flag; the nearest thing is a GUI-only "Setup > Source
   Profile > Export" flow to build a custom profile XML, not scriptable
   headlessly.
2. **AT3's default export omits the colon after label definitions**
   (`MyLabel` instead of `MyLabel:`) -- exactly what `-sppostlbl` used to
   control on AT2. z88dk's own assembler requires the colon and fails
   with a plain `syntax error` pointing at the bare label otherwise.
   Worked around by post-processing the output: any line that's just a
   bare identifier (every real label in this export format; every real
   data/instruction line always has more on it) gets a `:` appended.

Also needed, not a gap exactly but easy to get backwards: `SongToAkg
--labelPrefix _song_X_` bakes a leading underscore into the asm label
itself (`_song_X_Start`) -- the C-side `extern` must then be declared
**without** its own leading underscore (`extern uint8_t song_X_Start[];`),
since sccz80 adds one automatically to every C symbol it compiles; writing
the underscore on both sides looks right but produces an unresolved
`__song_X_Start` (double underscore) at link time.

**One of Arkos Tracker 3's own bundled CLI tools needed a one-time local
fix to run at all**: macOS killed `SongToAkg` outright (exit 137) with
`spctl` reporting "invalid signature (code or signature have been
modified)" -- a broken ad-hoc signature from the download/zip step, not a
real security concern, fixed with a local re-sign:
`codesign --force --deep --sign - <arkos-path>/tools/SongToAkg`. Clearing
the quarantine flag (`xattr -d com.apple.quarantine`) alone did NOT fix
it -- the signature itself needed repair, not just the quarantine
attribute.

**Not committed to this repo**: the `.aks` file, the converted `.asm`, and
a rendered `.wav` sample all stayed local/ad-hoc (one Desktop file handed
to the user to listen to) -- same reasoning as `games/sfxtest/`'s own
earlier decision not to commit celeste.p8-derived data: a real composer's
track is their own work, not something to ship inside a general-purpose
library's own test suite. `tools/arkos_song_to_asm.py` itself (the
reusable converter) IS committed; no copyrighted content is.

## Real music, really wired into PicoDiver (2026-10-13) -- the player had to be vendored and banked after all

The user supplied their own original composition, `games/picodiver/
sailing-by.aks`, explicitly to become picodiver's real background music --
unlike every other audio content this project touched (celeste.p8's sfx
data, "Lop Ears"), this one IS the user's own IP, explicitly committed, and
explicitly meant for real use. Converting it via `tools/arkos_song_to_asm.py`
worked exactly as it did against the earlier test track (same tool, same
pipeline, no new tooling gaps) -- but linking it exposed a real,
previously-undocumented complication:

**The player alone overflows picodiver's resident budget by 3099 bytes --
with ZERO song data loaded.** Measured directly: a standalone link combining
picodiver's existing code/data with `pico_music.c` (and nothing else new)
failed with `Section exceeds 64k [0x8000,0x10c1b]` (0x10c1b - 0x10000 =
3099). Adding the song on top pushed the overflow to 4933 bytes -- so the
song data itself only accounts for ~1834 of that. Banking just the song
(the mechanism every other banked module here uses) would NOT have been
enough; the player itself needed to come off the resident budget too, and
the section above's own "no vendoring needed" conclusion doesn't survive
this: **you only need to vendor the player once you need to BANK it.**
`pico_music.c`'s `<psg/arkos.h>` route pulls the player from z88dk's own
prebuilt classic-clib library archive the instant anything references
`ply_akg_*` -- not a file this project compiles itself, so the existing
`PAGE_<n>_<name>` banking trick (which works by compiling one of OUR OWN
`.c` files fresh to `.asm`, then section-renaming it) doesn't apply to it.

**Fix: vendor the player, bank it alongside the song, same physical page**
-- the same approach David Crespo's CelesteNext took for this exact
problem (confirmed by reading `CelesteNext-main/zxnext/music_player.asm`):

- `lib/pico_music_player.asm` -- the raw player
  (`asm_arkos_zx_smc.asm`), copied verbatim from z88dk's own tree, now a
  file THIS project compiles/links itself. Its own `section smc_sound_ay`
  line is removed (it's concatenated into a shared page, not assembled
  standalone) and three small C-callable entry trampolines are added
  directly in it (`_pico_music_banked_init_asm`/`_play_asm`/`_stop_asm`,
  `__z88dk_callee` convention, same register marshalling z88dk's own
  `ply_akg_init.asm` stub uses) so `lib/pico_music_banked.c` can call into
  it with an ordinary C call once paged in -- the same proven shape
  `docs/bank-call-notes.md` established for banked CODE in general.
- **Player + song share ONE physical page, not two sequential ones.**
  Unlike `BANKED_CSRCS` (page in, call, page out -- a module that's
  self-contained), the player needs its song's bytes mapped into memory
  *while it's running*, not just reachable beforehand -- a genuinely
  different relationship. The Makefile's new `BANKED_MUSIC_SONG`/
  `BANKED_MUSIC_PAGE` mechanism concatenates the vendored player and a
  game's song `.asm` (`tools/arkos_song_to_asm.py`'s output) into a single
  `build/<game>/music_bank.asm`, one shared `SECTION PAGE_<n>_music_bank`/
  `ORG $2000` header. Measured combined size (~4933 bytes) fits
  comfortably inside `lib/pico_bank.h`'s documented 8192-byte single-page
  ceiling, with ~3.2KB left over for a second short song or sfx data later.
- `lib/pico_music_banked.h`/`.c` -- the opt-in API for this path
  (`pico_music_banked_init(page, song, subsong)`/
  `pico_music_banked_play_frame(page)`/`pico_music_banked_stop(page)`),
  each wrapping `pico_bank_page_in()`/`page_out()` internally around one
  call into the vendored player's trampolines -- the caller doesn't do its
  own page-in/out (unlike the unbanked `pico_music.c`'s own doc comment,
  written before this was built, which assumed the caller would).
- Picked physical page 14 for picodiver (confirmed clear of Layer 2's
  18-29 range, distinct from page 8's existing `picodiver_map_data`).

**Result**: picodiver links clean with real music wired in end to end
(`pico_music_banked_init()` once after `titleinit()`,
`pico_music_banked_play_frame()` once per frame) -- `__BSS_END_tail` at
`$FF6A`, i.e. 150 bytes of resident margin left. Razor-thin, same
"measure, don't assume" territory as every other picodiver budget fight
this project has had, but real. Verified via a `zesarux --aofile` capture:
real AY waveform oscillation (not a frozen/dead register), including a
boot-silence gap before playback starts and a later low-amplitude passage
that's a genuine amplitude dip (confirmed by inspecting the raw bytes --
oscillating 0x3e-0x42, not a constant value) rather than a crash. Whether
that passage sounds right as composed is a human-ear call, not an
automatable one -- left to the user.

A side effect worth knowing about if this pattern is reused: the
Makefile's `BANKED_MUSIC_SONG` rule target (`build/<game>/music_bank.asm`)
is an explicit, non-pattern target, and briefly became the Makefile's
*default* goal just by being declared earlier in the file than `all:` --
`make GAME=picodiver` alone silently only built that one file and stopped,
no error. Fixed with an explicit `.DEFAULT_GOAL := all` near the top of
the Makefile, not by reordering (reordering is fragile against the next
person adding a rule above `all:` again).

## Credits (see also README.md's own Credits section)

- **Arkos Tracker** (2 and 3) and its player: (c) Julien "Targhan" Nevo
  (<https://www.julien-nevo.com/arkostracker/>). `lib/pico_music.c` (the
  unbanked path) only wraps the C API z88dk already ships, and
  `tools/arkos_song_to_asm.py` only shells out to AT3's own bundled CLI
  tool. `lib/pico_music_player.asm` (the banked path, added 2026-10-13 --
  see above) IS a vendored copy of the player's own source, needed because
  banking it requires a file this project compiles itself, not one pulled
  from z88dk's own prebuilt library archive.
- **z88dk's own sccz80/classic-clib port** of the Arkos player
  (`support/arkos/`, `libsrc/target/zx/psg/asm_arkos_zx_smc.asm`):
  credited there to ZXjogv. `lib/pico_music_player.asm` is a direct,
  frozen-on-purpose copy of this file (plus three small added entry
  trampolines -- see above), not kept in sync with z88dk's own copy.
- Full-playback verification (2026-10-13) used "Lop Ears" by Andy Severn,
  one of Arkos Tracker 3's own bundled example songs -- local/ad-hoc
  verification only, not committed to this repo (see above).
- `games/picodiver/sailing-by.aks` -- the user's own original composition,
  written specifically to be PicoDiver's real background music. Unlike
  every other audio content mentioned on this page, this one IS committed
  (it's the user's own work, not third-party content).
- The **recompose-by-ear-in-Arkos approach itself** (as opposed to an
  automated converter) follows the precedent set by David Crespo's
  CelesteNext (<https://davidprograma.itch.io/celeste>,
  <https://github.com/dcrespo3d/CelesteNext>), a real shipped
  Pico-8-to-Next port -- see that project's own README for the original
  writeup this decision is based on.

## Workflow (confirmed real, not speculative -- see above)

1. Compose the song in Arkos Tracker 2 **or 3** (both confirmed to export
   via their own bundled CLI tools; AT3 is what's actually installed and
   confirmed in this project), save/export as `.aks`.
2. `python3 tools/arkos_song_to_asm.py --arkos-path <path to your Arkos
   Tracker 3 install> --symbol my_song --out my_song.asm my_song.aks` --
   see that script's own top comment for the one-time `codesign` fix if
   macOS rejects `SongToAkg` with an "invalid signature" error.
3. Link the generated `.asm` alongside everything else (an ordinary extra
   source, same as any other unbanked data file this project already
   links).
4. `extern uint8_t my_song_Start[];` (no leading underscore -- see the
   tool's own top comment for why), `pico_music_init(my_song_Start, 0)`
   once, `pico_music_play_frame()` once per frame thereafter.
5. Sound effects use the same tool's own `SongToSoundEffects` sibling
   (not yet wrapped by `arkos_song_to_asm.py` -- that script only covers
   the song path so far, confirmed working; the effects path follows the
   same shape per z88dk's own original perl script but hasn't been
   exercised against AT3 yet) with `pico_sfx_init(effects_table)` once,
   `pico_sfx_play(effect, channel, inv_volume)` at the same points a
   ported cart's own `sfx(n)` calls lived.
6. If the song/player data ends up banked (likely, given the player's own
   resident-size cost above plus real song data on top): page in around
   `pico_music_play_frame()`/`pico_sfx_play()`, same
   `pico_bank_page_in()`/`pico_bank_page_out()` pattern as any other
   banked code/data call -- CelesteNext's own `sfx_update()` does exactly
   this around its own per-frame player tick.

## A real, documented requirement this project was violating: interrupts must be off during every player call

Found while chasing a real symptom: `sailing-by.aks` (the user's own
~3-minute composition, `games/picodiver/`) appeared to just stop
instead of looping, rather than reported as finished-and-silent from the
start -- a real difference from every earlier test here, which only
ever used short tracks (the earlier "Lop Ears" sample, verified via a
12-second capture) too brief to ever reach a real loop-back point.

`support/arkos/PlayerAkg.asm`'s own top-of-file comment (the
*original*, human-readable Arkos source -- not visible in the
z88dk-processed, self-modifying-code-obfuscated `asm_arkos_zx_smc.asm`
this project's own `lib/pico_music_player.asm` is vendored from, which
carries no such warning itself) says, verbatim: *"The player uses the
stack for optimizations. Make sure the interruptions are disabled
before it is called. The stack pointer is saved at the beginning and
restored at the end."* This project's own `pico_music_banked_*_asm()`
trampolines were plain `jp`s with no `di`/`ei` at all -- `intrinsic_ei()`
is called once at startup and never touched again, meaning every single
`pico_music_banked_play_frame()` call ran with interrupts fully enabled
the whole time.

Read the real reason this matters, not just trusted the warning: the
player's own song-end/loop handling (`PLY_AKG_ReadLinker` in the
original source) temporarily repoints `SP` at the song's own
position-table data and walks it via `pop` for speed -- a classic Z80
"stack as a fast sequential reader" trick. If a maskable interrupt
fires during that exact window, ITS OWN return address gets pushed
into/popped from SONG DATA instead of the real stack -- real
corruption, happening to coincide with exactly the one moment (the
loop-back transition) a short test track would never reach, but any
real several-minute composition left to loop definitely would.

Fixed in `lib/pico_music_player.asm`'s own trampolines: changed from a
tail `jp` into each `PLY_AKG_*` entry point to `di` / `call` / `ei` /
`ret`, masking interrupts for just the duration of that one call (the
50Hz interrupt will simply fire slightly later, after `ei` -- Z80's own
"`ei` takes effect after the next instruction" semantics make the
immediately-following `ret` itself interrupt-safe too, no race). Same
pattern applied to `_init`/`_play`/`_stop` for consistency, though
`_init`/`_stop` are far less likely to race against an interrupt than
`_play` (called every single frame).

**Not independently re-verified end to end against a full ~3-minute
loop cycle** -- doing so would mean waiting out the song's own real
length, impractical to automate reliably in this project's own testing
loop (and this session's own crude audio-capture tooling has already
proven unreliable for subtler audible questions more than once). The
fix matches the documented requirement and the exact failure mode
precisely enough to ship with real confidence, but genuine confirmation
needs a real listening pass through at least one full loop.
