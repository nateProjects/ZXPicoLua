# M0 spike findings -- hardware sprites in ZXPicoLua's own config

De-risking pass for the Islander-as-Next-game plan (`/Users/nat/.claude/plans/
polished-churning-meerkat.md`), same role `dark-room-next/docs/sprite-notes.md`
played for that project. Throwaway spike, `games/hwspike/main.c` (not part of
the real game build). Reference material: `next-resources/docs/
zxnext_notes.pdf` (Register $15/$19/$1C sections, read directly via
`pdftotext` after code-level investigation stalled -- see "the actual bug"
below), `next-resources/code/zxnext_level9-master/src/sprite.c` (proven
reference implementation, confirmed byte-for-byte correct by this spike).

## Question 1: does a hardware sprite render at all in this project's init sequence?

Yes, once correctly positioned (see Question 2). `pico_tilemap_init()`
already enables `RSLS_SPRITES_VISIBLE` and sets a sane layer priority
(Layer 2 front, sprites middle, tilemap back) -- no extra per-game setup
needed beyond writing sprite attributes/patterns themselves. Explicit sprite
palette (identity, matching `zxnext_level9`'s `sprite_set_default_palette()`)
and an explicit `REG_CLIP_WINDOW_SPRITES` reset were added defensively
during debugging but turned out NOT to be the fix -- cheap insurance, kept,
but not load-bearing.

## Question 2: is sprite-X 1:1 with this project's 40-column tilemap, or scaled?

**Measured: 1:1, confirmed, no scaling.** `dark-room-next`'s own spike found
sprite-X scaled 2:1 against ITS 80-column (640px) tilemap; the open question
here was whether that holds for ZXPicoLua's 40-column (320px) tilemap, which
is the same native width as the sprite layer's own fixed 320x256 surface.

Measurement (ZRCP `save-screen` + pixel-cluster analysis, not eyeballing):
tilemap column markers at columns 0/20/39 landed at screenshot x 39.5/359.5/
663.5 -- 2.0 screenshot-px per native tilemap pixel (160 native px -> 320
screenshot px between col 0 and col 20). Two sprites at nominal X 32 and 192
(160 units apart, matching dark-room-next's own test delta) landed at
screenshot x 111.5/431.5 -- also exactly 2.0 screenshot-px per sprite-X-unit.
**Same ratio -> sprite-X-unit and tilemap-native-pixel are the same unit.**

## The actual bug this spike hit (not a scaling question at all)

Sprites were invisible for the first 4 attempts, with attribute bytes,
pattern data, clip window, and the layer-priority register all independently
confirmed correct via direct ZRCP hardware-state readback (`tbblue-get-
sprite`, `tbblue-get-pattern`, `tbblue-get-clipwindow`) -- not just "what the
code intended to write". The actual cause, found only by reading
`zxnext_notes.pdf`'s Register $19 section directly after exhausting every
code-level hypothesis: **sprite position is always in sprite coordinates
with (32,32) corresponding to the top-left corner of the visible ULA/Layer-2
screen, regardless of over-border mode.** A sprite at nominal (0,16) is
therefore actually positioned 32px above and 32px left of the visible
screen entirely -- not clipped, not misconfigured, just off-screen by a
fixed, undocumented-in-code offset.

Confirmed by measurement, not just the spec text: the visible screen
(Layer 2's cleared-black area) starts at screenshot x=96 in this test's
2.0x screenshot scale, i.e. native tilemap-x=32. A sprite placed at nominal
X=32 (my own "+32" fix) landed at screenshot x=96 -- exactly the visible
screen's own left edge. **This means sprite-X and the tilemap's own raw
column-0-based native pixel X are not just the same unit (Question 2) but
the exact same coordinate space, zero relative offset** -- tilemap column 0
itself already starts 32px into the border (same reason picodiver's own
`TILE_ORIGIN_COL=4`, 4*8=32px, exists -- "re-derived empirically for the
zero-origin canvas").

**Practical rule for the Islander port**: `sprite_x = tile_origin_px +
world_relative_x`, where `tile_origin_px` is the SAME `TILE_ORIGIN_COL*8`
(and `TILE_ORIGIN_ROW*8` for Y) offset picodiver already uses for tilemap
placement -- hardware sprites and the hardware tilemap share one coordinate
space once that one fixed border offset is accounted for. No per-axis
scale conversion needed anywhere in this project (unlike dark-room-next's
80-column case), just the one shared +32,+32 (or whatever `TILE_ORIGIN_*`
ends up being for Islander's own viewport) additive offset.

## Still open (not this spike's job)

Whether the tilemap hardware's backing store can exceed the visible 40x32
cell grid via the offset registers (`NR_TILEMAP_OFFSET_X_H/L`, `_Y`,
`lib/pico_tilemap.c:30-32`, defined but never used for scrolling yet), or
whether camera movement across Islander's 128x64 world needs explicit
software tile-streaming at the grid edges regardless. Separate spike, M1's
first real task per the plan.
