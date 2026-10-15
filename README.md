# ZXPicoLua

A Pico-8-style C library for the ZX Spectrum Next, built on [z88dk](https://z88dk.org/). It provides C equivalents of the Pico-8 Lua API (`spr`, `map`, `btn`, `rnd`, `print`, fixed-point math, and more) plus Next-specific extras with no Pico-8 equivalent (a zero-cost hardware tilemap background, hardware sprites) — aimed at porting Pico-8 carts to real Spectrum Next hardware as native, non-emulated games, not running Lua on the Z80.

See [`docs/pico8-vs-zxpicolua-reference.md`](docs/pico8-vs-zxpicolua-reference.md) for a full side-by-side table of every Pico-8 built-in against its ZXPicoLua C equivalent, with real call-site examples.

## Status

Current version: **0.9.0** (pre-1.0, see [CHANGELOG.md](CHANGELOG.md) — [Semantic Versioning](https://semver.org/), no API-stability guarantee yet).

The core API is complete: Layer 2 framebuffer access, shape/text primitives, software sprites, keyboard input, math/RNG, tile maps, persistent storage (esxDOS), a hardware tilemap background, and hardware sprites. [PicoDiver](games/picodiver/) is the primary example — a full playable game ported end to end (title screen, gameplay, collision, HUD, scrolling background), not a demo.

Each major module has a `docs/*-notes.md` writeup of real bugs found and fixed along the way (tearing, a z88dk `-O3` compiler miscompile, 64K resident-budget overflows and the banking mechanism used to fix them).

## Building

Requires a local [z88dk](https://github.com/z88dk/z88dk) checkout and [ZEsarUX](https://sourceforge.net/projects/zesarux/) for running/testing. Point the `Z88DK_DIR` variable at the top of the `Makefile` at your own z88dk checkout, then:

```sh
make                    # builds games/demo
make GAME=picodiver      # builds games/picodiver
make GAME=picodiver run  # builds and launches it in ZEsarUX
make clean-all           # removes all build/ output
```

Each game lives in its own directory under `games/`, always linked against the shared `lib/`. `games/picodiver/` is the real, worked example; `games/demo/`, `games/fixedtest/` (now also the `pico_sin`/`pico_cos` test bed), `games/hwspike/`, `games/speedtest/`, `games/bankcalltest/`, `games/musictest/`, `games/sfxtest/`, and `games/cliptest/` are smaller milestone/scratch tests kept for reference, not polished examples.

## Tools

- `tools/p8_map_to_c.py` converts a Pico-8 `.p8` cart's `__map__` (and `__gff__`, if present) sections into a `.c` file for `pico_map_init()`/`pico_fset_all()` — see [`docs/p8-map-convert-notes.md`](docs/p8-map-convert-notes.md).
- `tools/p8_sfx_to_c.py` converts a cart's `__sfx__` data into AY-3-8912 register data for `lib/pico_sfx_synth.h`, an automated (lower-fidelity) alternative to the Arkos Tracker path above — see [`docs/sfx-convert-notes.md`](docs/sfx-convert-notes.md) for the format, the fidelity tradeoffs, and verification.
- `tools/arkos_song_to_asm.py` converts an Arkos Tracker `.aks` song to a linkable `.asm` for `lib/pico_music.h`, via Arkos Tracker 3's own bundled CLI tools — see [`docs/music-notes.md`](docs/music-notes.md) for the exact workflow (confirmed against a real track) and a couple of AT3-vs-AT2 CLI differences it works around.

## Library layout

| Module | Covers |
|---|---|
| `pico_video` | Layer 2 framebuffer, logical canvas, double buffering (`pico_flip`), palette remap |
| `pico_gfx` | Shape/text primitives (`rect`, `circ`, `oval`, `print`, `camera`, `clip`) |
| `pico_sprite` | Software sprite blitting (`spr`/`sspr`), pixel-perfect collision, sheet read/write (`sget`/`sset`) |
| `pico_tilemap` | Hardware tilemap background — zero per-frame cost, for a static/scrolling world too big to redraw every frame |
| `pico_hwsprite` | Next hardware sprites — a separate compositing layer with no erase/redraw bookkeeping |
| `pico_input` | Keyboard-matrix button input (`btn`/`btnp`) |
| `pico_math` | Math/bitwise helpers, seeded RNG, strings, Q16.16 fixed-point |
| `pico_trig` | `sin`/`cos` via a 256-entry lookup table (opt-in — real resident cost) |
| `pico_map` | Software tile map (`mget`/`mset`/`map`), precomputed collision masks (`pico_map_build_colmask`/`pico_colbox_at`) |
| `pico_music` | Music/sfx playback via Arkos Tracker (opt-in — see [`docs/music-notes.md`](docs/music-notes.md)) |
| `pico_music_banked` | Same, but with the player itself banked alongside its song data — for when the unbanked player alone doesn't fit (opt-in, see [`docs/music-notes.md`](docs/music-notes.md)) |
| `pico_sfx_synth` | Automated Pico-8-sfx-to-AY playback (opt-in — see [`docs/sfx-convert-notes.md`](docs/sfx-convert-notes.md)) |
| `pico_data` | Persistent save data via esxDOS (`cartdata`/`dget`/`dset`) |
| `pico_bank` | Paging large const data or code outside z88dk classic mode's 64K resident-code ceiling |

## Credits

- The bitmap font (`lib/pico_font.c`) is the real Pico-8 4x6 font, extracted programmatically from the public-domain (CC-0) Pico-8 font asset.
- [PicoDiver](games/picodiver/) is ported from the author's own [PicoDiver](https://github.com/nateProjects/PicoDiver) Pico-8 cart, via its PicoJS web-port reference.
- `lib/pico_music.c`/`.h` wraps [z88dk](https://z88dk.org/)'s own bundled [Arkos Tracker](https://www.julien-nevo.com/arkostracker/) player (`support/arkos/`, `<psg/arkos.h>`) — the player itself is (c) Julien "Targhan" Nevo; its sccz80/classic-clib port is maintained in z88dk by ZXjogv. `lib/pico_music_player.asm` (used by the banked path, `lib/pico_music_banked.c`/`.h`) IS a vendored copy of that same player's source — banking it needs a file this project compiles itself, see `docs/music-notes.md`. [PicoDiver](games/picodiver/)'s own background music, `sailing-by.aks`, is the user's own original composition.
- The collision-mask technique in `pico_map_build_colmask()`/`pico_colbox_at()`, and the decision to recompose music by ear in Arkos Tracker rather than auto-convert it, both follow precedent set by David Crespo's [CelesteNext](https://davidprograma.itch.io/celeste) ([source](https://github.com/dcrespo3d/CelesteNext)), a real shipped Pico-8-to-Next port — see `docs/pico8-vs-zxpicolua-reference.md` and `docs/music-notes.md` for the specifics.
- `tools/p8_sfx_to_c.py`'s `__sfx__` parsing was cross-checked against [dansanderson/picotool](https://github.com/dansanderson/picotool)'s own cart parser; its pitch-to-frequency formula comes from [zepto8](https://github.com/samhocevar/zepto8) (Sam Hocevar), a byte-accurate Pico-8 reimplementation. Neither is vendored — read for reference only. No sample/test sfx data in the library's own test games is derived from any real cart; `games/sfxtest/` uses a small, self-authored synthetic sfx. [PicoDiver](games/picodiver/)'s own sound effects ARE from a real cart — the author's own original `picodiver2.p8` (same provenance as its map/sprite data above).

## License

MIT — see [LICENSE](LICENSE).
