# Changelog

All notable changes to this project are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versioning
follows [Semantic Versioning](https://semver.org/) (`MAJOR.MINOR.PATCH`,
pre-1.0: no API-stability guarantee yet — a MINOR bump may still break a
call site).

## [Unreleased]

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
