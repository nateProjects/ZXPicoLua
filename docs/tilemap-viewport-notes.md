# M0 spike findings -- tilemap backing-store size & scrolling

Second open question from the Islander-as-Next-game plan (`/Users/nat/.claude/
plans/polished-churning-meerkat.md`): can the tilemap's backing store be
larger than the visible 40×32 cell grid (hardware-scrolled across a bigger
virtual map), or is it strictly 40×32, requiring the game to stream new tile
data into the fixed grid as the camera moves? Settled directly from
`next-resources/docs/zxnext_notes.pdf`'s own register spec -- unambiguous
enough that no live hardware spike was needed to confirm it (unlike the
sprite coordinate-offset question in `docs/hwsprite-notes.md`, where the
spec was clear but nothing in this project's code had applied it yet).

## Answer: backing store is fixed at 40×32 (or 80×32); scrolling is a pixel-offset window into that fixed grid, not a bigger virtual map

Register $6B ("Layer 3 Setup", `lib/pico_tilemap.c`'s own `NR_TILEMAP_
CONTROL`) bit 6 selects 40×32 or 80×32 as the *only* two tilemap sizes --
directly stated: "The tilemap will be 40×32 or 80×32 in size depending on
the resolution selected in nextreg $6B." No larger backing-store option
exists.

Registers $2F/$30/$31 (`lib/pico_tilemap.c`'s own `NR_TILEMAP_OFFSET_X_H/L`/
`_Y`, defined but never used for scrolling yet, only ever zeroed) are a
10-bit X offset (0-1023) and 8-bit Y offset (0-255) -- a pixel-granular
*read offset* into the same fixed 320×256 (or 640×256) backing store, not an
address into a larger memory region. Since the offset range (up to 1023)
exceeds the backing store's own width (320px in 40-column mode), this reads
as a wraparound/ring-buffer shift: it lets the display start reading from
any pixel offset within the fixed grid, smoothly scrolling the *visible
window* without moving tile data, but the underlying 40×32 cells of tile
data stay exactly where they are.

## Practical implication for Islander's M1 (confirmed, not newly discovered -- the plan already anticipated this as the likely answer)

Islander's world is 128×64 tiles, far bigger than one 40×32 screen's worth
either way. **Camera-driven tile streaming is required**, regardless of the
scroll-offset registers: as the player moves, M1 needs to write newly-
exposed tile data into the row/column of the fixed 40×32 grid that's about
to scroll into view (classic ring-buffer streaming, matching how most retro
tile engines -- including real SNES hardware in modes without extra VRAM --
actually work), while the $2F/$30/$31 offset registers handle smooth
*sub-tile* (sub-8px) pixel scrolling on top of that streaming, avoiding the
need to re-stream on every single pixel of movement -- only when crossing
an 8px tile boundary.

## M0 status: both open questions now settled

1. Sprite-X vs. tilemap-X coordinate space: 1:1, confirmed by measurement
   (`docs/hwsprite-notes.md`).
2. Tilemap backing-store size: fixed at 40×32, confirmed by spec. Tile
   streaming is needed for M1 regardless, as already planned.

Next: M1 (world + camera streaming + player movement), per the plan.
