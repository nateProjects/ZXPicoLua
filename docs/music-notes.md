# Music/sfx (Arkos Tracker 2) -- findings

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
details." An automated `__sfx__`-to-AY converter (cheap placeholder audio,
lower fidelity) remains a separate, later task -- deliberately not started
here.

## z88dk already bundles a working Arkos Tracker 2 player -- confirmed under this exact build target

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
compiles and links through this project's real build pipeline. **No real
Arkos Tracker 2 content has been authored for this project yet**, so this
only calls `pico_music_stop()` (the one entry point that needs no song
data -- it just silences the AY channels) rather than claiming full
playback is verified when it isn't. Confirmed via ZRCP screenshot: boots
cleanly to "PICO_MUSIC.C LINKED OK / (no song content yet)" with no
crash. Full playback (a real `.aks` file, authored in Arkos Tracker 2,
exported via `support/arkos/arkos-song-to-asm.pl`, actually heard) is
still unverified and blocked on real content existing -- not something
this can self-check without a composer at the tracker.

## Credits (see also README.md's own Credits section)

- **Arkos Tracker 2** and its player: (c) Julien "Targhan" Nevo
  (<https://www.julien-nevo.com/arkostracker/>). Nothing from the
  player's own source is vendored in this repo -- `lib/pico_music.c` only
  wraps the C API z88dk already ships.
- **z88dk's own sccz80/classic-clib port** of the Arkos player
  (`support/arkos/`, `libsrc/target/zx/psg/asm_arkos_zx_smc.asm`):
  credited there to ZXjogv.
- The **recompose-by-ear-in-Arkos approach itself** (as opposed to an
  automated converter) follows the precedent set by David Crespo's
  CelesteNext (<https://davidprograma.itch.io/celeste>,
  <https://github.com/dcrespo3d/CelesteNext>), a real shipped
  Pico-8-to-Next port -- see that project's own README for the original
  writeup this decision is based on.

## Workflow, for whenever real content gets authored

1. Compose the song/sound effects in Arkos Tracker 2, export as `.aks`.
2. `support/arkos/arkos-song-to-asm.pl -s <symbol> song.aks > song.asm`
   (song) or `-e -s <symbol> effects.aks > effects.asm` (sound effects) --
   see that script's own `README.md` for the full option set (needs a
   real Arkos Tracker 2 install, for its own export tools).
3. Link the exported `.asm` alongside everything else (an ordinary extra
   source, same as any other banked or unbanked data file this project
   already links).
4. `pico_music_init(song, subsong)` once, `pico_music_play_frame()` once
   per frame thereafter; `pico_sfx_init(effects_table)` once,
   `pico_sfx_play(effect, channel, inv_volume)` at the same points a
   ported cart's own `sfx(n)` calls lived.
5. If the song/player data ends up banked (likely, given the player's own
   resident-size cost above plus real song data on top): page in around
   `pico_music_play_frame()`/`pico_sfx_play()`, same
   `pico_bank_page_in()`/`pico_bank_page_out()` pattern as any other
   banked code/data call -- CelesteNext's own `sfx_update()` does exactly
   this around its own per-frame player tick.
