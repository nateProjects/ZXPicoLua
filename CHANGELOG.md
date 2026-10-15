# Changelog

All notable changes to this project are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versioning
follows [Semantic Versioning](https://semver.org/) (`MAJOR.MINOR.PATCH`,
pre-1.0: no API-stability guarantee yet — a MINOR bump may still break a
call site).

## [Unreleased]

## [0.9.2] - 2026-10-15

### Fixed
- 0.9.1's fix made sfx worse, not better -- "a click" became "a series
  of clicks," caught immediately by the user. The real cause: both
  `pico_sfx_synth` and the Arkos music player rewrite the AY's whole
  mixer register (R7) unconditionally every tick, each blind to what
  the other wants on channels it doesn't control -- 0.9.1's "reassert
  every tick" fix just made this pre-existing conflict fire every frame
  instead of only at note transitions. Checked Shiru's AYFX driver
  (user's own `ayfxResources/`) for comparison: same architecture, same
  documented limitation when sharing a chip with other AY activity. The
  real fix: `pico_sfx_synth` now runs on the Next's SECOND AY chip via
  Turbosound (`<arch/zxn.h>`), eliminating the register conflict by
  construction instead of trying to coordinate it -- no changes needed
  to the vendored Arkos player. See docs/sfx-convert-notes.md's "two
  rounds, the first fix was wrong" section.

## [0.9.1] - 2026-10-15

### Fixed
- `lib/pico_sfx_synth.c` sound effects only played for a single frame
  ("a click") in PicoDiver, despite working correctly standalone in
  `games/sfxtest/` -- user caught it immediately after 0.9.0 shipped.
  `pico_sfx_synth_update()` only rewrote AY registers on a note
  transition, relying on nothing else touching the chip in between; fine
  in isolation, broken once real background music (`pico_music_banked.c`)
  shares the chip and rewrites all 3 channels every single frame as part
  of its own normal playback, reclaiming the sfx's channel before its
  own next transition. Fixed by reasserting the current note's registers
  every tick a channel is playing, not just on change. Re-verified via
  waveform capture: sustained same-period runs across many frames now,
  not a one-frame spike. See docs/sfx-convert-notes.md's "Real bug found
  sharing the AY chip with music" section.

## [0.9.0] - 2026-10-15

### Added
- Real sound effects in PicoDiver -- the original cart's `sfx(n)` call
  sites (5, collapsing to 3 distinct sounds: a bad hit, life lost, fish
  caught) were skipped since the port's very first milestone. Converted
  directly from the real cart's own `__sfx__` data
  (`games/picodiver/picodiver_sfx.c`, via `tools/p8_sfx_to_c.py`, zero
  warnings -- no effects or custom waveforms in any of the three sfx).
  Opts into `pico_sfx_synth.c` (resident, not banked -- affordable now
  that 0.8.1's sprite-data banking freed up thousands of bytes of
  margin). Plays alongside the real Arkos background music on the same
  AY chip; an active sfx simply overrides whatever the music wrote to
  its channel that frame, matching Pico-8's own `sfx()`/`music()`
  interaction. Verified via a `zesarux --aofile` capture around a forced
  trigger (ZRCP memory writes, not simulated keypresses) showing the
  dominant AY period shifting repeatedly -- confirms real, dynamic
  output. See `docs/sfx-convert-notes.md`'s "Shipped in PicoDiver for
  real" section.

## [0.8.2] - 2026-10-14

### Fixed
- `pico_tilemap_from_map()`'s own init-time call (building picodiver's
  ocean background) was missed in 0.8.1's sprite-data banking sweep --
  it reads both `picodiver_map_data` and `picodiver_sprite_data`
  interleaved, same as `ship_show()`, but wasn't caught by grepping for
  `pico_spr`/etc (different function name). Only the map page was paged
  in, so it built the tilemap from garbage sheet bytes -- user-reported:
  "the colouring has changed for the background." Fixed the same way as
  `ship_show()` (buffer the map region, then page to the sprite page).
  That fix's first attempt introduced a second bug (reordered the
  ocean-extension loop ahead of `pico_tilemap_from_map()`, so its own
  sand row survived as a stray band instead of being overwritten as
  intended) -- caught by screenshot-comparing against a rebuilt
  pre-banking reference, not assumed correct. See
  docs/picodiver-notes.md's "Stack/BSS collision" section.

## [0.8.1] - 2026-10-13

### Fixed
- The octopus "trail" bug (previously fixed by splitting its dirty-rect
  tracking) came back after the music work above -- turned out to be a
  real stack/BSS collision, not a logic regression. A live ZRCP register
  poll during ordinary play showed `SP` regularly dipping into
  `prev_oct_head`/`prev_oct_body`'s own memory (confirmed pre-existing,
  not caused by the music change, by re-measuring the prior commit).
  Fixed by banking `picodiver_sprite_data` (8192 bytes, exactly the
  single-page ceiling) the same way `picodiver_map_data` already is --
  `pico_spr_init()` only stores the pointer, so no library changes were
  needed, just a `pico_bank_page_in()`/`page_out()` pair around every
  `pico_spr()`/`pico_spr_collide()` call site. `BSS_END_tail` dropped
  from `$FF84` to `$E0C6` (~150 bytes of margin to ~8000). See
  `docs/picodiver-notes.md`'s "Stack/BSS collision" section.

## [0.8.0] - 2026-10-13

### Added
- Real background music for PicoDiver — `games/picodiver/sailing-by.aks`,
  the user's own original composition. Playing it required a new banked
  music path (`lib/pico_music_banked.c`/`.h`, `lib/pico_music_player.asm`):
  the unbanked Arkos player alone (`<psg/arkos.h>`, pulled from z88dk's
  own prebuilt library archive) overflowed picodiver's resident budget by
  3099 bytes with zero song data loaded, so banking just the song data
  (the usual mechanism) wasn't enough — the player itself needed
  vendoring and banking too, following the same approach CelesteNext
  used for the same problem. Player + song now share one physical page
  via a new Makefile mechanism (`BANKED_MUSIC_SONG`/`BANKED_MUSIC_PAGE`).
  Verified linking (picodiver now builds with 150 bytes of resident
  margin) and producing real AY waveform output via a `zesarux --aofile`
  capture. See `docs/music-notes.md`'s "Real music, really wired into
  PicoDiver" section for the full story, including a `.DEFAULT_GOAL`
  Makefile regression caught and fixed along the way.

## [0.7.1] - 2026-10-13

### Added
- `tools/arkos_song_to_asm.py` — converts an Arkos Tracker `.aks` song to
  a linkable `.asm` for `pico_music.h`, via Arkos Tracker 3's own bundled
  CLI tools. Confirmed end to end against a real track: converted,
  linked, and verified producing continuous real music on the emulated
  AY chip (a 12-second waveform capture, 91/91 windows active — a clear
  signature distinct from a one-shot sfx). Works around two real AT3-
  vs-AT2 CLI differences (see `docs/music-notes.md`). No copyrighted
  content committed — the verification track stayed local/ad-hoc.

## [0.7.0] - 2026-10-13

### Added
- `pico_sin()`/`pico_cos()` (`lib/pico_trig.h`, opt-in) — Pico-8's exact
  trig convention (turns, not radians; `sin()` negated relative to
  standard math, confirmed against the real manual). 256-entry lookup
  table, generated programmatically. Verified via a native host harness
  (10 cases) and a real on-device screenshot (`games/fixedtest/`, now
  9 checks total). Split into its own opt-in module after the table
  alone tipped picodiver's already razor-thin resident margin (263
  bytes) over by 348 bytes when tried unconditionally — same reasoning
  as `pico_music.c`/`pico_sfx_synth.c`.

## [0.6.0] - 2026-10-13

### Added
- `pico_clip()`/`pico_clip_reset()` (`pico_gfx.h`) — restricts drawing to
  a rectangle, same scope as `pico_camera()` (shape/text primitives
  only). Verified via a native clip-math harness (10 cases) and a real
  on-device screenshot (`games/cliptest/`).
- `pico_sset()` (`pico_sprite.h`) — writes a sprite-sheet pixel, `sget()`'s
  write counterpart. Requires the registered sheet to be genuinely
  mutable storage (documented caveat). Verified via `games/cliptest/`'s
  own `sset()`/`sget()` round-trip.

## [0.5.0] - 2026-10-13

### Added
- `tools/p8_sfx_to_c.py` + `lib/pico_sfx_synth.c`/`.h` — automated
  Pico-8 `__sfx__`-to-AY converter and player, the lower-fidelity
  alternative to `pico_music.c`'s Arkos Tracker 2 path. Opt-in (same
  reason as `pico_music.c` — real, measured resident-size cost). Verified
  against a real cart (byte-for-byte, by hand) and via a real AY
  waveform capture (`games/sfxtest/`, a self-authored synthetic sfx, not
  derived from any real game). Known limitations: per-note effects and
  custom waveforms are parsed but not synthesized — see
  `docs/sfx-convert-notes.md`.

## [0.4.1] - 2026-10-13

### Fixed
- Documented the root cause of physical pages 18-29 failing for any
  banked module (`lib/pico_bank.h`, `docs/bank-call-notes.md`): they're
  Layer 2's own main+shadow bank range, silently zeroed by
  `pico_video_init()` at startup. Previously only known as "these three
  pages empirically fail" with no explanation (`z88dk_banking_reference`);
  root-caused via a sibling project (Harvester), reconfirmed directly
  against this project's own `pico_video.c`. No code change.

## [0.4.0] - 2026-10-12

### Added
- `lib/pico_music.c`/`.h` — music/sfx playback via z88dk's bundled Arkos
  Tracker 2 player (`<psg/arkos.h>`). Opt-in only (excluded from the
  default `LIB_SRCS` wildcard — see the Makefile's own comment): linking
  it unconditionally pulled picodiver's build from 14336 free bytes to
  an outright overflow. Verified linking and running (silencing, the one
  entry point that needs no song data) via `games/musictest/`; full
  playback is unverified pending real Arkos Tracker 2 content.
- `tools/p8_map_to_c.py` — converts a Pico-8 cart's `__map__`/`__gff__`
  sections to C source for `pico_map_init()`/`pico_fset_all()`. Verified
  against two real carts.

## [0.3.0] - 2026-10-12

### Added
- Confirmed and documented that `lib/pico_bank.h`'s banking window works
  for banked **code**, not just banked data (`docs/bank-call-notes.md`,
  `games/bankcalltest/`) -- no new API, just a proven pattern: an
  ordinary forward declaration + a plain C call around
  `pico_bank_page_in()`/`pico_bank_page_out()`.

## [0.2.0] - 2026-10-12

### Added
- `pico_map_build_colmask()`/`pico_colbox_at()` (`pico_map.h`) — a
  precomputed per-room collision-mask helper, for a cheap array-index
  collision query instead of `pico_mget()`+`pico_fget_all()` on every
  call. Guarded by `PICO_NO_MAP_FLAGS` like the rest of the flags API.

## [0.1.0] - 2026-10-12

First versioned snapshot. Core API complete: Layer 2 framebuffer access,
shape/text primitives, software sprites, keyboard input, math/RNG, tile
maps, persistent storage (esxDOS), a hardware tilemap background, and
hardware sprites. [PicoDiver](games/picodiver/) ported end to end as the
first full integration test. No audio support yet.
