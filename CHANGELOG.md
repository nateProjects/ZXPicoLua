# Changelog

All notable changes to this project are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versioning
follows [Semantic Versioning](https://semver.org/) (`MAJOR.MINOR.PATCH`,
pre-1.0: no API-stability guarantee yet — a MINOR bump may still break a
call site).

## [Unreleased]

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
