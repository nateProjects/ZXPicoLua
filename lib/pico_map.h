#ifndef PICO_MAP_H
#define PICO_MAP_H

#include <stdint.h>

/* Registers the tile map games read/draw from: a flat, row-major array of
 * tile indices (one byte per cell, matching a sprite index 0-255) --
 * map_w/map_h in CELLS, not pixels (unlike pico_spr_init()'s sheet_w/h).
 * Must be called before pico_mget()/pico_mset()/pico_map(). */
void pico_map_init(uint8_t *map_data, uint16_t map_w, uint16_t map_h);

/* mget(x,y)/mset(x,y,v) -- read/write one cell, in map (cell) coordinates.
 * Out-of-range coordinates return/ignore 0, matching PicoJS's picoJS.mget/
 * mset (picoJS.js ~line 961) rather than this project's usual "unchecked"
 * policy -- map access is naturally bounds-prone (scrolling, edge tiles)
 * in a way raw pixel coordinates aren't, and PicoJS already made the
 * right call here. */
uint8_t pico_mget(uint8_t x, uint8_t y);
void pico_mset(uint8_t x, uint8_t y, uint8_t v);

/* fget(n,f)/fset(n,f,v) -- the 8 Pico-8 sprite flags for sprite index n,
 * one bit (f, 0-7) at a time -- split from Pico-8's own fget(n)/fset(n,v)
 * whole-byte forms into pico_fget_all()/pico_fset_all() below, same
 * explicit-functions-instead-of-optional-args policy as print()/
 * print_at_cursor() in pico_gfx.h. */
uint8_t pico_fget(uint8_t n, uint8_t f);
void pico_fset(uint8_t n, uint8_t f, uint8_t v);
uint8_t pico_fget_all(uint8_t n);
void pico_fset_all(uint8_t n, uint8_t flags);

/* map(cel_x, cel_y, sx, sy, cel_w, cel_h) -- draws a cel_w x cel_h block
 * of map cells starting at map-space (cel_x,cel_y) to screen position
 * (sx,sy), one pico_spr() call per cell (8x8 each, 1:1 scale). Tile index
 * 0 is always skipped (Pico-8 convention: sprite 0 means "empty" in a
 * map, ported from PicoJS's own picoJS.map). pico_map_layer() is the
 * same, plus an fget()-based layer filter -- a cell only draws if
 * (pico_fget_all(tile) & layer_mask) is non-zero, matching Pico-8's
 * optional map(..., layer) argument, split into its own function for the
 * same reason as pico_fget()/pico_fget_all() above (0 is a legitimate
 * real layer mask, so it can't double as an optional argument's
 * not-supplied sentinel). Requires pico_spr_init() to have been called
 * too, since each cell draws via pico_spr().
 *
 * sx/sy (the SCREEN position, unlike cel_x/cel_y which are map-space) are
 * signed, matching pico_spr()'s own x/y -- a scrolling background drawn
 * via two map() calls (the normal position, plus a second a full screen-
 * width to one side to cover the wrap) needs the second call to start
 * off-screen on purpose. */
void pico_map(uint8_t cel_x, uint8_t cel_y, int16_t sx, int16_t sy, uint8_t cel_w, uint8_t cel_h);
void pico_map_layer(uint8_t cel_x, uint8_t cel_y, int16_t sx, int16_t sy, uint8_t cel_w, uint8_t cel_h, uint8_t layer_mask);

#ifndef PICO_NO_MAP_FLAGS
/* Precomputes a per-cell collision byte for a cel_w x cel_h map region
 * (typically once, on room/level load), instead of re-deriving it from
 * pico_mget()+pico_fget_all() on every collision query -- the same
 * technique a real shipped Pico-8-to-Next port (Celeste Classic) used for
 * its own tile_flag_at()-style check: identical collision-loop shape to
 * Pico-8's own fget()-based query (mget the tile, mask its flags), just a
 * cheap array index at query time afterward instead of a map+flag lookup
 * every single call. out_mask must be at least cel_w*cel_h bytes,
 * row-major (dy*cel_w+dx) -- the layout pico_colbox_at() below expects.
 * Each cell's stored byte is pico_fget_all(tile) & flag_mask -- pass 0xFF
 * to capture every flag bit, or a narrower mask (e.g. just a "solid" bit)
 * if that's all a game's collision actually needs. */
void pico_map_build_colmask(uint8_t *out_mask, uint8_t cel_x, uint8_t cel_y,
                             uint8_t cel_w, uint8_t cel_h, uint8_t flag_mask);

/* Queries a mask built by pico_map_build_colmask() against a pixel-space
 * box (px,py)-(px+pw-1,py+ph-1), tile-aligned internally (8px cells, same
 * convention as pico_spr()/pico_map()) -- returns the OR of every covered
 * cell's mask byte (0 if the box doesn't touch any flagged cell, or lies
 * entirely outside the mask's own cel_w x cel_h region). mask_w/mask_h
 * must match the cel_w/cel_h originally passed to
 * pico_map_build_colmask(). */
uint8_t pico_colbox_at(const uint8_t *mask, uint8_t mask_w, uint8_t mask_h,
                        int16_t px, int16_t py, uint8_t pw, uint8_t ph);
#endif

#endif
