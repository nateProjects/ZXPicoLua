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

## Credits (see also README.md's own Credits section)

- **Arkos Tracker** (2 and 3) and its player: (c) Julien "Targhan" Nevo
  (<https://www.julien-nevo.com/arkostracker/>). Nothing from the
  player's own source is vendored in this repo -- `lib/pico_music.c` only
  wraps the C API z88dk already ships, and `tools/arkos_song_to_asm.py`
  only shells out to AT3's own bundled CLI tool, nothing from it copied
  in either.
- **z88dk's own sccz80/classic-clib port** of the Arkos player
  (`support/arkos/`, `libsrc/target/zx/psg/asm_arkos_zx_smc.asm`):
  credited there to ZXjogv.
- Full-playback verification (2026-10-13) used "Lop Ears" by Andy Severn,
  one of Arkos Tracker 3's own bundled example songs -- local/ad-hoc
  verification only, not committed to this repo (see above).
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
