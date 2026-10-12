# ZXPicoLua

A Pico-8-style C library for the ZX Spectrum Next, built on [z88dk](https://z88dk.org/). It provides C equivalents of the Pico-8 Lua API (`spr`, `map`, `btn`, `rnd`, `print`, fixed-point math, and more) plus Next-specific extras with no Pico-8 equivalent (a zero-cost hardware tilemap background, hardware sprites) — aimed at porting Pico-8 carts to real Spectrum Next hardware as native, non-emulated games, not running Lua on the Z80.

See [`docs/pico8-vs-zxpicolua-reference.md`](docs/pico8-vs-zxpicolua-reference.md) for a full side-by-side table of every Pico-8 built-in against its ZXPicoLua C equivalent, with real call-site examples.

## Status

Current version: **0.1.0** (pre-1.0, see [CHANGELOG.md](CHANGELOG.md) — [Semantic Versioning](https://semver.org/), no API-stability guarantee yet).

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

Each game lives in its own directory under `games/`, always linked against the shared `lib/`. `games/picodiver/` is the real, worked example; `games/demo/`, `games/fixedtest/`, `games/hwspike/`, and `games/speedtest/` are smaller milestone/scratch tests kept for reference, not polished examples.

## Library layout

| Module | Covers |
|---|---|
| `pico_video` | Layer 2 framebuffer, logical canvas, double buffering (`pico_flip`), palette remap |
| `pico_gfx` | Shape/text primitives (`rect`, `circ`, `oval`, `print`, `camera`) |
| `pico_sprite` | Software sprite blitting (`spr`/`sspr`), pixel-perfect collision |
| `pico_tilemap` | Hardware tilemap background — zero per-frame cost, for a static/scrolling world too big to redraw every frame |
| `pico_hwsprite` | Next hardware sprites — a separate compositing layer with no erase/redraw bookkeeping |
| `pico_input` | Keyboard-matrix button input (`btn`/`btnp`) |
| `pico_math` | Math/bitwise helpers, seeded RNG, strings, Q16.16 fixed-point |
| `pico_map` | Software tile map (`mget`/`mset`/`map`) |
| `pico_data` | Persistent save data via esxDOS (`cartdata`/`dget`/`dset`) |
| `pico_bank` | Paging large const data (sprite sheets, maps) outside z88dk classic mode's 64K resident-code ceiling |

## Credits

- The bitmap font (`lib/pico_font.c`) is the real Pico-8 4x6 font, extracted programmatically from the public-domain (CC-0) Pico-8 font asset.
- [PicoDiver](games/picodiver/) is ported from the author's own [PicoDiver](https://github.com/nateProjects/PicoDiver) Pico-8 cart, via its PicoJS web-port reference.

## License

MIT — see [LICENSE](LICENSE).
