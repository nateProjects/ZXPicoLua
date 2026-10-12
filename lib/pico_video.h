#ifndef PICO_VIDEO_H
#define PICO_VIDEO_H

#include <stdint.h>

/* Physical Layer 2 surface: fixed by the hardware, never changes. Every
 * drawing primitive's address math is ultimately against this. */
#define PICO_PHYS_W 256
#define PICO_PHYS_H 192

/* Logical canvas: what game code actually draws against. Pico-8 carts are
 * authored for 128x128 and expect to address (0,0)-(127,127); a native
 * ZXPicoLua game can instead declare the full 256x192 physical surface as
 * its own canvas. pico_video_init() centres whatever logical size is
 * requested within the fixed physical surface -- e.g. 128x128 gets a
 * 64px left/right and 32px top/bottom border -- and every pset/pget/future
 * primitive transparently offsets into that centred area, so ported Pico-8
 * code using its own native 0-127 coordinates needs no translation of its
 * own. This is a free coordinate offset, not a scaled blit -- see
 * docs/layer2-notes.md's "logical canvas" note for why scaling (e.g. to
 * fill more of the physical screen) is intentionally not implemented here:
 * it would need a separate offscreen-buffer-plus-blit design, not just an
 * offset, and isn't needed until a real port asks for it. */
void pico_video_init(uint16_t logical_w, uint16_t logical_h);

/* The logical canvas size last passed to pico_video_init(). */
extern uint16_t pico_screen_w;
extern uint16_t pico_screen_h;

/* Fills the whole physical screen (including any letterbox border) with a
 * single palette index. */
void pico_cls(uint8_t color);

/* Clears just [x0,x1]x[y0,y1] (inclusive, logical coordinates) instead of
 * the whole canvas -- see pico_video.c's own comment for why this exists
 * (picodiver's gameplay screen needs its "sky" strip cleared every frame
 * without disturbing the ocean below it). */
void pico_cls_rect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);

/* Writes/reads a single pixel's palette index, in LOGICAL canvas
 * coordinates -- x in [0, pico_screen_w), y in [0, pico_screen_h).
 * Out-of-range coordinates are not checked (same as Pico-8/PicoJS).
 *
 * Both always act on the current BACK buffer (see pico_flip() below) --
 * pico_pget() therefore reflects what's been drawn so far *this frame*,
 * not necessarily what's currently on screen: a region untouched since
 * the last pico_flip() still holds whatever was drawn there two frames
 * ago, not last frame. Fine for the common case (read back a pixel you
 * just drew), a real limitation for anything reading untouched regions
 * across frames -- see docs/input-notes.md's "double buffering" section. */
void pico_pset(uint8_t x, uint8_t y, uint8_t color);
uint8_t pico_pget(uint8_t x, uint8_t y);

/* pal(c0, c1) -- remaps colour c0 (0-15) to c1 (0-15) for every
 * subsequent pico_cls()/pico_pset() call (and therefore every primitive
 * built on them -- pico_gfx.c's shapes, pico_sprite.c's blits, text),
 * until pico_pal_reset(). Matches Pico-8's own draw-palette pal(), minus
 * its p=1 "screen palette" mode -- not implemented, same as PicoJS's own
 * scope. Only the standard 0-15 Pico-8 range is remappable; this
 * project's own 16-255 native-Next colour extension is untouched.
 * pico_pget() is unaffected either way -- it reads back whatever was
 * actually stored (i.e. already post-remap), matching real Pico-8. */
void pico_pal(uint8_t c0, uint8_t c1);
void pico_pal_reset(void);

/* Presents everything drawn since the last pico_flip() and swaps which
 * physical Layer 2 bank is the back (writable) buffer vs the displayed
 * one. Call once per frame, after all of that frame's drawing is done --
 * see docs/input-notes.md's "double buffering" section for why this
 * exists (a sprite redrawn in place every frame, with no double
 * buffering, visibly tears when the display's own scanout catches the
 * framebuffer mid-redraw -- confirmed by playing it interactively, not
 * just theory). Because the back buffer alternates between two physical
 * banks, each one holds whatever was drawn into it *two* flips ago, not
 * one -- any animated content must track per-buffer state accordingly
 * (see the demo's own o_lit/x_lit/ball-position handling in
 * games/demo/main.c for the pattern). */
void pico_flip(void);

/* Batch-write session: writes many pixels (pico_video_batch_pset()) under
 * ONE interrupt-disable + MMU page resolution instead of pico_pset()'s
 * usual per-pixel cost, only re-selecting the physical page when it
 * actually changes between pixels -- added for the picodiver port after
 * profiling found this was the dominant cost in a real sprite/tile-heavy
 * scene (see pico_video.c's own comment for the measurement). Intended
 * for a library primitive blitting many pixels at once (pico_sprite.c's
 * pico_sspr(), the first user) -- not for general game code, which
 * should keep using the simpler pico_pset(). Keep a batch to one sprite/
 * tile's worth of pixels: interrupts stay off for the WHOLE batch, and a
 * long batch means a long window the earlier interrupt-corruption fix
 * (docs/layer2-notes.md) can't preempt at all.
 *
 *   uint8_t prev = pico_video_batch_begin();
 *   pico_video_batch_pset(x, y, color);  // repeat as needed
 *   pico_video_batch_end(prev);
 *
 * pico_video_batch_pset() applies pico_pal()'s remap exactly like
 * pico_pset() does, and does NOT bounds-check (also like pico_pset()) --
 * a caller with coordinates that can go out of logical-canvas range
 * (e.g. a partly off-screen sprite) must clip before calling, the same
 * requirement pico_sprite.c's own pset_safe() already meets for
 * pico_pset().
 *
 * NESTS: begin/end calls nest (a depth counter, pico_video.c) -- only
 * the outermost pair actually does DI/EI and page save/restore. A game
 * can wrap SEVERAL pico_spr() calls (each already a begin/end pair
 * internally) in one outer begin/end to turn N DI/EI pairs into 1 --
 * worthwhile once a frame draws enough sprites that per-call overhead
 * itself becomes the bottleneck (confirmed directly for picodiver's own
 * creature/HUD-icon-heavy frames, docs/picodiver-notes.md). */
uint8_t pico_video_batch_begin(void);
void pico_video_batch_pset(uint8_t x, uint8_t y, uint8_t color);
void pico_video_batch_end(uint8_t previous_page);

/* Row-granular batch access -- resolves/selects the destination page for
 * logical (x,y) and returns a raw pointer to that byte, which the caller
 * may then step by +-1 to reach (x+-1, y) with no further calls: a Layer 2
 * row never crosses a physical page boundary (PICO_PHYS_W=256 divides
 * evenly into an 8192-byte page). For a hot per-pixel blit loop where
 * pico_video_batch_pset()'s own per-pixel call overhead is itself the
 * bottleneck (confirmed by profiling, not assumed -- see pico_video.c).
 * Does not bounds-check x/y, like pico_video_batch_pset(). */
uint8_t *pico_video_batch_row(uint8_t x, uint8_t y);

/* Applies pal()'s remap to a raw colour index -- the same mapping
 * pico_pset()/pico_video_batch_pset() apply internally, exposed so a
 * tight blit loop can inline it instead of paying a function call per
 * pixel for it. */
uint8_t pico_video_remap(uint8_t color);

/* Loads Pico-8's real default palette (pico_palette.h's pico8_palette)
 * into Layer 2's own palette bank 0, indices 0-15 -- NOT called
 * automatically by pico_video_init(), so existing games keep whatever
 * colours they've already been drawing with (the Next's own default boot
 * palette) unless they opt in. A game that wants Pico-8-faithful colours
 * (and, if it also uses the hardware tilemap, consistent colours between
 * the two -- see pico_tilemap_load_pico8_palette()) should call this once
 * after pico_video_init(). */
void pico_video_load_pico8_palette(void);

#endif
