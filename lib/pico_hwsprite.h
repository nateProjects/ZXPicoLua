#ifndef PICO_HWSPRITE_H
#define PICO_HWSPRITE_H

#include <stdint.h>

/* Next hardware sprites -- a separate compositing layer from Layer 2
 * (pico_sprite.c's software blits), added for the Islander port: no
 * per-sprite erase/redraw bookkeeping needed (no DirtyRect, no buffer
 * desync, no tearing), unlike pico_sprite.c's own software blits -- a
 * hardware sprite is just "rewrite 4-5 attribute bytes, halt, repeat".
 * Mechanism proven in docs/hwsprite-notes.md's spike (next-resources/code/
 * zxnext_level9-master/src/sprite.c's own pattern, confirmed working in
 * this project's own init sequence) -- most importantly, confirmed by
 * direct measurement that sprite coordinates are 1:1 with this project's
 * 40-column tilemap's own native pixel space, with NO scale conversion
 * needed (unlike dark-room-next's 80-column tilemap, which needed a 2:1
 * conversion) -- see that doc for the full writeup. */

/* Call once, after pico_tilemap_init() (which already enables the sprite
 * layer and sets layer priority -- this only adds the sprite-specific
 * palette/transparency/clip-window setup pico_tilemap_init() doesn't
 * touch). Identity sprite palette (pattern byte N displays as raw RGB332
 * colour N, same convention islander_sprite_data's own Pico-8 colour
 * indices need remapping for -- see pico_hwsprite_set_pattern()) and a
 * transparency index reserved away from any real colour used. */
void pico_hwsprite_init(void);

/* Defines sprite PATTERN slot `slot`'s 256 bytes (16x16, 1 byte/pixel) by
 * blitting an 8x8 region from a Pico-8-style 8bpp sheet (sheet_w/sheet_h
 * match pico_spr_init()'s own convention) at (sheet_x, sheet_y), palette-
 * remapped from Pico-8 colour index (0-15) to this sprite layer's own
 * identity palette scheme (pico_hwsprite_init()'s RGB332-direct mapping,
 * NextReg $5B's RGB332 layout: RRRGGGBB -- Pico-8 colour N maps to a
 * fixed RGB332 byte via next-resources' own precedent, same approach
 * pico_video_load_pico8_palette() uses for Layer 2's palette but applied
 * per-pixel here since hardware sprites have no separate indexed palette
 * bank the way the tilemap/Layer 2 do -- see pico_hwsprite.c for the
 * exact table). The region is placed centred in the 16x16 pattern (4px
 * margin all sides) -- hardware sprites are always 16x16; Pico-8 content
 * is 8x8, so this avoids needing scale/subpattern addressing for a first
 * pass (every reference game's sprites fit this). Pixels outside the
 * blitted 8x8 region are left at the transparency index, same as Pico-8
 * colour 0 within it (spr()'s own default transparent colour).
 *
 * extra_transparent: a second Pico-8 colour index (0-15) to ALSO treat
 * as transparent, or 0xFF for none. Added for art that bakes its own
 * silhouette/outline colour into the base sprite (confirmed by direct
 * pixel inspection, not assumed, for Islander's own player sprite --
 * colour 1 forms a solid border around the whole figure) -- the
 * original cart draws its OWN separate outline via spr_outline()'s
 * 8-direction silhouette trick, so a baked-in outline colour is
 * redundant with that and, rendered through this project's coarser
 * RGB332 sprite palette against bright terrain, reads far more starkly
 * than it did against Pico-8's own smoother colour handling -- dropping
 * it here (not re-colouring it) is the simplest fix until a real
 * spr_outline() equivalent exists for hardware sprites (needs a redesign,
 * not a translation, of this exact trick). */
void pico_hwsprite_set_pattern(uint8_t slot, const uint8_t *sheet, uint16_t sheet_w,
                                uint16_t sheet_x, uint16_t sheet_y, uint8_t extra_transparent);

/* Same as pico_hwsprite_set_pattern(), but blits a full 16x16 region
 * straight into the pattern -- no margin, no centring. For art actually
 * authored/upscaled at 16x16 (ISLANDER16_SHEET_W-style sheets), which
 * already fills the hardware's own native pattern size for free (every
 * 8-bit pattern costs a full 256-byte/16x16 slot regardless -- see
 * zxnext_notes.pdf 2.5.1 -- pico_hwsprite_set_pattern()'s own 8x8-into-16x16
 * centring was only ever a content choice, not a hardware requirement).
 * Pair with pico_hwsprite_set16() below, not pico_hwsprite_set(), when
 * positioning a sprite using a pattern defined this way -- the two
 * disagree on where the pattern's own (0,0) sits relative to the visible
 * content (pico_hwsprite_set()'s -4,-4 correction would misplace a
 * pattern that has no margin to correct for). */
void pico_hwsprite_set_pattern16(uint8_t slot, const uint8_t *sheet, uint16_t sheet_w,
                                  uint16_t sheet_x, uint16_t sheet_y, uint8_t extra_transparent);

/* Sets sprite (anchor) `slot`'s position/visibility/mirroring, displaying
 * PATTERN `pattern` -- a separate number from `slot`, not assumed equal
 * (earlier versions of this function assumed 1:1; animation needs several
 * patterns -- one pre-defined pattern slot per walk frame -- shown from
 * ONE moving anchor sprite, swapped by just changing which pattern index
 * this call writes, no pattern re-upload needed). x/y are in the SAME
 * native pixel space as the tilemap (pico_tilemap_set()'s col*8/row*8),
 * per docs/hwsprite-notes.md's own measurement; no conversion needed.
 * flip_x mirrors the pattern horizontally (hardware-native, via the
 * sprite attribute's own XM bit) -- reuse one pattern for both facing
 * directions instead of needing separate left/right art, matching
 * islander-4.p8's own spr(...,flp,...) convention. Since position
 * updates happen every tick regardless (all 5 attribute bytes rewritten
 * either way), switching `pattern` or `flip_x` on a call that was going
 * to happen anyway costs nothing extra -- unlike software-sprite
 * animation (games/picodiver's own pico_sprite.c), which must re-blit
 * actual pixel data per frame. */
void pico_hwsprite_set(uint8_t slot, uint8_t pattern, int16_t x, int16_t y,
                        uint8_t visible, uint8_t flip_x);

/* Same as pico_hwsprite_set(), plus hardware magnification -- Next sprite
 * attribute 4's XX/YY fields (zxnext_notes.pdf 2.5.3-2.5.4) scale the
 * existing 16x16 pattern 1x/2x/4x/8x per axis, FOR FREE: no new pattern
 * data, no extra sprite-pattern-memory cost, fully reversible per call.
 * Found while scoping Islander's "double the object icon sizes" request --
 * this replaces what would otherwise need a real 2x-upscaled sprite sheet
 * (e.g. via ImageMagick) with two bits in a byte this project already
 * writes every tick regardless. `mag` is 0 (1x, identical to
 * pico_hwsprite_set()), 1 (2x), 2 (4x), or 3 (8x) -- same per-axis value
 * used for both X and Y since no caller has needed independent axes yet;
 * extend with a second parameter if one ever does. Magnification scales
 * outward from the pattern's own centre, so the 4px transparent margin
 * pico_hwsprite_set_pattern() bakes in scales too -- a 2x sprite's visible
 * 8x8 content ends up centred in a 32x32 box with an 8px margin, not
 * flush against the box edge. */
void pico_hwsprite_set_mag(uint8_t slot, uint8_t pattern, int16_t x, int16_t y,
                            uint8_t visible, uint8_t flip_x, uint8_t mag);

/* Same as pico_hwsprite_set(), but for a pattern defined by
 * pico_hwsprite_set_pattern16() -- no -4,-4 margin correction, since a
 * full 16x16 pattern's own (0,0) already IS the visible content's
 * top-left. x/y still mean the same thing every other call in this file
 * means (the visible content's own top-left in tilemap-native pixel
 * space); only the correction differs, matching how the pattern was
 * actually built. */
void pico_hwsprite_set16(uint8_t slot, uint8_t pattern, int16_t x, int16_t y,
                          uint8_t visible, uint8_t flip_x);

/* Defines sprite PATTERN slot `slot` as a solid `size`x`size` pixel block
 * of Pico-8 colour `pico8_color`, centred in the pattern (everything else
 * transparent) -- for flat-colour UI elements (a stack-count tally dot,
 * a menu backdrop panel, ...) that don't need real sheet art, so callers
 * don't need a dedicated sheet region just to get a single flat colour.
 * `size` is clamped to 16 (the hardware pattern's own fixed dimension). */
void pico_hwsprite_set_pattern_solid(uint8_t slot, uint8_t pico8_color, uint8_t size);

/* Hides sprite `slot` (visible=0) without touching its pattern/position --
 * cheaper than a full pico_hwsprite_set() call when only visibility
 * changes (e.g. an object pool slot going empty). */
void pico_hwsprite_hide(uint8_t slot);

#endif
