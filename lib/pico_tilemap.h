#ifndef PICO_TILEMAP_H
#define PICO_TILEMAP_H

#include <stdint.h>

/* Hardware tilemap background -- see pico_tilemap.c's own header comment
 * for why this exists (a zero-per-frame-cost alternative to pico_map()'s
 * software blit, for a static or rarely-changing background) and how it
 * composites with Layer 2 (pico_video.c/pico_sprite.c): the player and
 * other sprites drawn via pico_spr() sit on Layer 2, which this sets up
 * to show the tilemap through wherever Layer 2 itself is "blank". */

/* 40x32 is the hardware's own tile grid in standard (non-80-column)
 * mode -- the tilemap's internal row pitch is always 40 regardless of
 * how many columns are actually visible on screen, so this must match
 * that exactly or every row after the first reads from the wrong
 * offset (confirmed directly: using 32 here produced a visibly shifted,
 * garbled render). */
#define PICO_TM_COLS 40
#define PICO_TM_ROWS 32

/* The Layer 2 colour index treated as "see-through to the tilemap" --
 * must match whatever colour pico_cls() clears to each frame (0, Pico-8
 * black, by this library's own convention) so the background shows
 * through everywhere a sprite hasn't drawn, with no extra per-frame work
 * needed to keep the two in sync. */
#define PICO_TILEMAP_TRANSPARENT_COLOR 0

/* Sets up the tilemap hardware and the Layer-2-over-tilemap compositing
 * registers. Call once, after pico_video_init(). */
void pico_tilemap_init(void);

/* Sets tilemap palette entry `index` (0-15) to an explicit RGB colour --
 * r/g/b all 0-7, full 9-bit precision -- rather than relying on the
 * Next's default boot palette happening to put a usable colour there
 * (confirmed directly: index 1 defaulted to a dark, barely-visible
 * colour; index 15 to an unrelated yellow). */
void pico_tilemap_set_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/* Defines an 8x8 4-bit (16-colour) tile pattern at the given pattern
 * index (0-255). 32 bytes: 8 rows x 4 bytes/row, two 4-bit pixels packed
 * per byte, leftmost pixel in the upper nibble. */
void pico_tilemap_define_tile(uint8_t index, const uint8_t *pattern32);

/* tile = pattern index; attr = palette offset (0-15, high nibble of the
 * tilemap word) -- lets the same tile pattern be recoloured per cell. */
void pico_tilemap_set(uint8_t col, uint8_t row, uint8_t tile, uint8_t attr);

/* Fills the whole tilemap with one tile/attr combination -- the common
 * case for a uniform or simply-patterned background. */
void pico_tilemap_fill(uint8_t tile, uint8_t attr);

/* Loads Pico-8's real default palette (pico_palette.h) into the
 * tilemap's own palette bank 0, indices 0-15 -- the general-purpose
 * replacement for hand-picking/empirically-matching one colour at a time
 * via pico_tilemap_set_color(), which is how this library's own
 * picodiver port first did it (see pico_tilemap_from_map()'s own
 * comment). Called automatically by pico_tilemap_from_map(); exposed
 * separately in case a caller wants it without that function (e.g. for a
 * hand-authored tile that still wants Pico-8-accurate colours). */
void pico_tilemap_load_pico8_palette(void);

/* Pico-8-map-driven tilemap population -- the hardware-tilemap
 * equivalent of pico_map(), for a STATIC background (see pico_tilemap.c's
 * own header comment for why a hardware tilemap exists at all). Takes
 * the same kind of inputs a ported cart already has lying around (its
 * sprite sheet and map data, both plain Pico-8 8bpp arrays -- one byte
 * per pixel/cell, values 0-15/0-255) and does everything pico_map()'s
 * caller would otherwise have to do by hand once, up front: for each
 * distinct non-zero sprite index actually used in the requested map
 * region, converts that one 8x8 block from the sheet's 8bpp pixels into
 * the tilemap's packed 4bpp tile format (a Pico-8 colour index IS a
 * tilemap pixel value directly -- both are 0-15 -- so this is a pure
 * repack, no colour remapping needed) and defines it as a new tilemap
 * tile pattern; then walks the map region again, writing each cell's
 * tile pattern index into the tilemap grid starting at (dest_col,
 * dest_row). Also loads the real Pico-8 palette into the tilemap's own
 * palette bank (pico_tilemap_load_pico8_palette()) so the converted
 * tiles' colours are correct without the caller needing to know that.
 *
 * sheet/sheet_w match pico_spr_init()'s own parameters; map_data/map_w/
 * map_h match pico_map_init()'s; cel_x/cel_y/cel_w/cel_h match
 * pico_map()'s own region parameters (so an existing pico_map() call can
 * be ported to this almost verbatim, replacing its sx/sy screen-pixel
 * destination with a dest_col/dest_row TILEMAP-cell destination instead --
 * see pico_tilemap.h's own note on why the tilemap has its own coordinate
 * space, distinct from the logical canvas pico_map() draws into).
 *
 * At most 255 distinct non-zero sprite indices may appear in the
 * requested region (tile pattern 0 is reserved "no tile"/transparent-
 * within-the-tilemap, matching pico_map()'s own "tile 0 is always empty"
 * convention) -- comfortably enough for any real background, and simply
 * not drawn (left as whatever was there before) past that, rather than
 * silently misrendering. */
void pico_tilemap_from_map(const uint8_t *sheet, uint16_t sheet_w,
                            const uint8_t *map_data, uint16_t map_w, uint16_t map_h,
                            uint8_t cel_x, uint8_t cel_y, uint8_t cel_w, uint8_t cel_h,
                            uint8_t dest_col, uint8_t dest_row);

#endif
