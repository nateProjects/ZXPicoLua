#include <arch/zxn.h>
#include "pico_tilemap.h"
#include "pico_palette.h"

/* Hardware tilemap ("layer 3") -- composites a tile-based background
 * entirely in hardware during scanout, at zero per-frame CPU cost, as
 * opposed to pico_map()'s software blit through Layer 2 (which costs
 * real, measured, per-tile time every single frame it's redrawn --
 * docs/picodiver-notes.md "Round 2"/"Round 5"). For a STATIC or rarely-
 * changing background (the common case for an underwater/platform/maze
 * backdrop), defining it once here and letting the hardware draw it for
 * free every frame is the "as we'd do for a real Next game" answer the
 * software-blit approach was never going to match -- see this file's
 * own docs/picodiver-notes.md section for the measurement that prompted
 * this.
 *
 * Register numbers and the BASE_OFF/tile-format details are adapted from
 * dark-room-next/src/tilemap.c (a proven-working z88dk Next tilemap
 * implementation, itself confirmed against RustyPixelsUK/zxnext's
 * tilemap.c/.h since z88dk's own <arch/zxn.h> has no symbolic names for
 * these NextRegs). NEW here (dark-room-next never touched Layer 2, only
 * ULA): the REG_GLOBAL_TRANSPARENCY_COLOR + REG_SPRITE_LAYER_SYSTEM setup
 * needed so Layer 2 (where pico_sprite.c's blits live) can sit ON TOP of
 * this tilemap and show it through wherever Layer 2 itself is "blank". */

#define NR_TILEMAP_CONTROL      0x6B
#define NR_TILEMAP_DEFAULT_ATTR 0x6C
#define NR_TILEMAP_BASE_ADDR    0x6E
#define NR_TILE_DEFS_ADDR       0x6F
#define NR_TILEMAP_OFFSET_X_H   0x2F
#define NR_TILEMAP_OFFSET_X_L   0x30
#define NR_TILEMAP_OFFSET_Y     0x31
#define NR_TILEMAP_TRANSPARENCY 0x4C

#define TM_CONTROL_ENABLE         0x80
#define TM_CONTROL_80COL          0x40
#define TM_CONTROL_NO_ATTRIBUTES  0x20
#define TM_CONTROL_OVER_ULA       0x01

/* REG_GLOBAL_TRANSPARENCY_COLOR (NextReg 20/$14, named in z88dk's own
 * <arch/zxn.h>): a single palette-index that Layer 2, ULA, and sprites
 * all treat as "transparent" when deciding what to composite -- this is
 * the mechanism that lets Layer 2's own "blank" (uncleared-by-a-sprite)
 * pixels let the tilemap underneath show through, instead of Layer 2
 * always opaquely covering it. Set to match pico_cls()'s own colour 0
 * (PICO_TILEMAP_TRANSPARENT_COLOR, pico_tilemap.h), the same index
 * pico_cls() already clears Layer 2 to every frame -- no extra per-frame
 * work needed to keep this true. */

/* Sprite/Layer2/ULA display priority (REG_SPRITE_LAYER_SYSTEM, NextReg
 * 21/$15 -- z88dk's own <arch/zxn.h> already names this one, unlike the
 * tilemap registers above). RSLS_LAYER_PRIORITY_LSU = Layer 2 frontmost,
 * then Sprites, then ULA/tilemap at the back -- the tilemap shares the
 * ULA's slot in this ordering when enabled (NR_TILEMAP_CONTROL's
 * OVER_ULA bit only affects tilemap-vs-ULA, not tilemap-vs-Layer2), so
 * this puts Layer 2 above the tilemap, which is what we want: the
 * player (and later enemies), drawn on Layer 2, visible on top of the
 * hardware-composited background. */

#define BANK5_BASE 0x4000
#define BASE_OFF(addr) (((addr) - BANK5_BASE) >> 8)

/* Tile defs and tilemap storage, placed in bank 5 ($4000-$7FFF) per the
 * hardware's own requirement (same constraint dark-room-next documented).
 * This project doesn't use bank 5 for anything else (Layer 2 access goes
 * through MMU slot 3, $6000-$7FFF physically but a DIFFERENT physical
 * bank each access -- see pico_video.c -- not bank 5's own fixed
 * content), so the whole $4000-$7FFF window is free here. */
#define TILE_DEFS_ADDR 0x4000 /* up to 128 tiles * 32 bytes = 4096 bytes, ends $4FFF */
#define TILEMAP_ADDR   0x5000 /* PICO_TM_COLS*PICO_TM_ROWS words */

static uint8_t *const tile_defs = (uint8_t *)TILE_DEFS_ADDR;
static uint16_t *const tilemap_mem = (uint16_t *)TILEMAP_ADDR;

void pico_tilemap_init(void) {
    ZXN_WRITE_REG(NR_TILEMAP_CONTROL, TM_CONTROL_ENABLE | TM_CONTROL_OVER_ULA);
    ZXN_WRITE_REG(NR_TILEMAP_DEFAULT_ATTR, 0);
    /* Tile pixel value 0 is transparent *within the tilemap itself*
     * (lets the ULA show through on an undrawn cell) -- separate from
     * the Layer-2-vs-tilemap transparency below. Our tiles always draw
     * a full 8x8 pattern (no undrawn cells), so this mostly doesn't
     * matter here, but matches dark-room-next's own proven setup. */
    ZXN_WRITE_REG(NR_TILEMAP_TRANSPARENCY, 0);
    ZXN_WRITE_REG(NR_TILEMAP_BASE_ADDR, BASE_OFF(TILEMAP_ADDR));
    ZXN_WRITE_REG(NR_TILE_DEFS_ADDR, BASE_OFF(TILE_DEFS_ADDR));
    ZXN_WRITE_REG(NR_TILEMAP_OFFSET_X_H, 0);
    ZXN_WRITE_REG(NR_TILEMAP_OFFSET_X_L, 0);
    ZXN_WRITE_REG(NR_TILEMAP_OFFSET_Y, 0);

    ZXN_WRITE_REG(REG_GLOBAL_TRANSPARENCY_COLOR, PICO_TILEMAP_TRANSPARENT_COLOR);
    ZXN_WRITE_REG(REG_SPRITE_LAYER_SYSTEM, RSLS_LAYER_PRIORITY_LSU | RSLS_SPRITES_VISIBLE);
}

/* RPC_SELECT_TILEMAP_PALETTE_0 (0x30) -- a VALUE written into
 * REG_PALETTE_CONTROL (NextReg 67) to select which palette bank
 * subsequent REG_PALETTE_INDEX/REG_PALETTE_VALUE_8 writes edit, not a
 * register number itself (distinct from NR_TILEMAP_OFFSET_X_L above,
 * which happens to share the raw number 0x30 as an actual NextReg
 * index -- confirmed not a conflict, just a coincidence of the Next's
 * register map). Confirmed working directly via ZRCP before porting
 * here (games/picodiver's own validation, docs/picodiver-notes.md). */
#define RPC_SELECT_TILEMAP_PALETTE_0 0x30

/* Full 9-bit palette write (RRRGGGBBB, r/g/b all 0-7) via
 * REG_PALETTE_VALUE_16 -- two writes to the SAME register, first the
 * high byte (RRRGGGB, blue's top bit in bit0), second just blue's
 * bottom bit (in the write's own bit0, rest ignored). Confirmed
 * empirically, not from documentation alone: an earlier attempt using
 * the single-write REG_PALETTE_VALUE_8 path (2-bit blue precision only)
 * produced a visibly different shade than the cabin sprite's own native
 * colour 1 (RGB 0,0,73 vs the achievable 0,0,109) -- scanning actual
 * rendered output confirmed this two-write sequence reproduces 0,0,73
 * exactly (docs/picodiver-notes.md). */
void pico_tilemap_set_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    ZXN_WRITE_REG(REG_PALETTE_CONTROL, RPC_SELECT_TILEMAP_PALETTE_0);
    ZXN_WRITE_REG(REG_PALETTE_INDEX, index);
    ZXN_WRITE_REG(REG_PALETTE_VALUE_16, (uint8_t)((r << 5) | (g << 2) | (b >> 1)));
    ZXN_WRITE_REG(REG_PALETTE_VALUE_16, (uint8_t)(b & 1));
}

void pico_tilemap_define_tile(uint8_t index, const uint8_t *pattern32) {
    uint8_t i;
    uint8_t *dst = tile_defs + ((uint16_t)index * 32);
    for (i = 0; i < 32; i++) dst[i] = pattern32[i];
}

void pico_tilemap_set(uint8_t col, uint8_t row, uint8_t tile, uint8_t attr) {
    uint16_t offset = (uint16_t)row * PICO_TM_COLS + col;
    tilemap_mem[offset] = (uint16_t)(((uint16_t)(attr & 0x0F) << 12) | tile);
}

void pico_tilemap_fill(uint8_t tile, uint8_t attr) {
    uint16_t i;
    uint16_t word = (uint16_t)(((uint16_t)(attr & 0x0F) << 12) | tile);
    for (i = 0; i < (uint16_t)PICO_TM_COLS * PICO_TM_ROWS; i++) tilemap_mem[i] = word;
}

void pico_tilemap_load_pico8_palette(void) {
    uint8_t i;
    for (i = 0; i < 16; i++) {
        pico_tilemap_set_color(i, pico8_palette[i][0], pico8_palette[i][1], pico8_palette[i][2]);
    }
}

/* Tile pattern index == Pico-8 sprite index, directly -- no separate
 * dedup/remap table needed, just a 128-bit "already defined" bitmap
 * (16 bytes). Only works because the tile-def table's own fixed memory
 * layout (TILE_DEFS_ADDR..TILEMAP_ADDR, above) holds exactly 128 tile
 * patterns; a sprite index >=128 has nowhere to go and is skipped (left
 * untouched at its destination cell), same as a real Pico-8 cart's own
 * sheet never needing more than 128 tiles for a background in practice --
 * confirmed for picodiver's own ocean specifically (3 distinct tiles used
 * across its whole 16x12-cell region), not assumed to generalise to
 * every conceivable cart. */
void pico_tilemap_from_map(const uint8_t *sheet, uint16_t sheet_w,
                            const uint8_t *map_data, uint16_t map_w, uint16_t map_h,
                            uint8_t cel_x, uint8_t cel_y, uint8_t cel_w, uint8_t cel_h,
                            uint8_t dest_col, uint8_t dest_row) {
    uint8_t defined[16];
    uint8_t cells_per_row;
    uint8_t dx, dy, i;

    if (!sheet || !map_data || sheet_w < 8) return;
    cells_per_row = (uint8_t)(sheet_w / 8);

    for (i = 0; i < 16; i++) defined[i] = 0;
    pico_tilemap_load_pico8_palette();

    for (dy = 0; dy < cel_h; dy++) {
        uint16_t my = (uint16_t)cel_y + dy;
        if (my >= map_h) break;
        for (dx = 0; dx < cel_w; dx++) {
            uint16_t mx = (uint16_t)cel_x + dx;
            uint8_t tile, sx, sy, row;

            if (mx >= map_w) continue;
            /* Pico-8 convention: tile 0 is always "empty", never drawn --
             * same as pico_map()'s own map_draw(), so a cell referencing
             * it is left untouched rather than overwritten with an
             * undefined pattern. */
            tile = map_data[my * map_w + mx];
            if (tile == 0 || tile >= 128) continue;

            if (!(defined[tile >> 3] & (uint8_t)(1u << (tile & 7)))) {
                uint8_t packed[32];
                defined[tile >> 3] |= (uint8_t)(1u << (tile & 7));
                sx = (uint8_t)((tile % cells_per_row) * 8);
                sy = (uint8_t)((tile / cells_per_row) * 8);
                for (row = 0; row < 8; row++) {
                    const uint8_t *srcrow = sheet + (uint16_t)(sy + row) * sheet_w + sx;
                    uint8_t col;
                    for (col = 0; col < 8; col += 2) {
                        packed[row * 4 + col / 2] =
                            (uint8_t)(((srcrow[col] & 0x0F) << 4) | (srcrow[col + 1] & 0x0F));
                    }
                }
                pico_tilemap_define_tile(tile, packed);
            }

            pico_tilemap_set((uint8_t)(dest_col + dx), (uint8_t)(dest_row + dy), tile, 0);
        }
    }
}
