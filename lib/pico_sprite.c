#include <stdint.h>
#include "pico_sprite.h"
#include "pico_video.h"

static const uint8_t *sheet = 0;
static uint16_t sheet_w = 0;
static uint16_t sheet_h = 0;
/* One bit per Pico-8 colour (0-15) -- bit set means "transparent when
 * blitting sprites". Defaults to colour 0 only (bit 0 set), matching
 * Pico-8's own default palt() state; was a single uint8_t colour before
 * M3's pico_palt() replaced it with this proper per-colour bitmask. */
static uint16_t transparent_mask = 0x0001;

void pico_spr_init(const uint8_t *sheet_data, uint16_t w, uint16_t h) {
    sheet = sheet_data;
    sheet_w = w;
    sheet_h = h;
}

void pico_palt(uint8_t color, uint8_t transparent) {
    if (color >= 16) return;
    if (transparent) transparent_mask |= (uint16_t)(1u << color);
    else transparent_mask &= (uint16_t)~(1u << color);
}

/* Sheet pixel index as uint16_t: fine up to a 256x256 sheet (65536 would
 * overflow, but Pico-8's own sheet is 128x128 = 16384 -- comfortably
 * inside range for any realistic port). No bounds check against
 * sheet_w/sheet_h here -- sx/sy are always derived from sw/sh-bounded
 * loop variables by the callers below, never raw user input. */
static uint8_t sheet_pixel(uint16_t sx, uint16_t sy) {
    return sheet[sy * sheet_w + sx];
}

/* Same reasoning as pico_gfx.c's pset_safe(): destination coordinates here
 * are derived (dx+u, dy+v for a sprite near a screen edge), so they can
 * legitimately go out of logical-canvas range for an ordinary partly
 * off-screen sprite -- clip instead of letting the raw uint8_t cast wrap
 * and corrupt the framebuffer. Writes through the batch-write session
 * (pico_video.h) rather than pico_pset() directly -- see pico_sspr()'s
 * own comment for why. */
static void batch_pset_safe(int16_t x, int16_t y, uint8_t color) {
    if (x < 0 || y < 0 || x >= (int16_t)pico_screen_w || y >= (int16_t)pico_screen_h) return;
    pico_video_batch_pset((uint8_t)x, (uint8_t)y, color);
}

/* Unscaled (1:1) blit -- the common case: every pico_spr() call (so every
 * sprite AND every map() tile) hits this, since pico_spr() always calls
 * through with sw==dw/sh==dh by construction. Walks raw src/dst pointers a
 * row at a time instead of calling sheet_pixel()/batch_pset_safe() per
 * pixel -- profiling a real scene (M4's picodiver) found THAT per-pixel
 * call overhead, not the MMU page logic itself, was still the dominant
 * cost even after the scaling-divide and page-batching fixes below: the
 * generated .asm for a two-call-per-pixel loop re-spills every local
 * through stack-relative addressing on sccz80 (checked directly, not
 * assumed), far outweighing the few T-states the actual work needs. A
 * Layer 2 row never crosses a physical page boundary (pico_video.h's
 * pico_video_batch_row()), so the destination page/pointer is resolved
 * once per row here, not once per pixel -- see docs/picodiver-notes.md. */
static void blit_unscaled(uint16_t sx, uint16_t sy, uint8_t sw, uint8_t sh,
                           int16_t dx, int16_t dy,
                           uint8_t flip_x, uint8_t flip_y) {
    uint8_t prev_page = pico_video_batch_begin();
    uint8_t dyi;

    /* No screen_w/screen_h locals -- sccz80 (the z88dk default compiler)
     * allocates every local on the stack with no register allocation, so
     * each avoidable local is real, measured stack-depth margin in a call
     * chain deep enough to otherwise corrupt this program's own BSS
     * globals (docs/picodiver-notes.md) -- reference the globals directly
     * instead of caching them. */
    for (dyi = 0; dyi < sh; dyi++) {
        int16_t py = dy + (int16_t)dyi;
        uint16_t v = flip_y ? (uint16_t)(sh - 1 - dyi) : dyi;
        const uint8_t *srcp;
        int8_t step;
        uint8_t *dstp;
        int16_t px;
        uint8_t dxi;

        if (py < 0 || py >= (int16_t)pico_screen_h) continue;

        srcp = sheet + (uint16_t)(sy + v) * sheet_w + sx;
        if (flip_x) { srcp += (sw - 1); step = -1; } else { step = 1; }

        px = dx;
        dstp = 0; /* resolved lazily at the first in-bounds pixel */
        for (dxi = 0; dxi < sw; dxi++, px++, srcp += step) {
            uint8_t color;
            if (px < 0) continue;
            if (px >= (int16_t)pico_screen_w) break; /* rest of row is off-screen too */
            color = *srcp;
            if (dstp) dstp++;
            else dstp = pico_video_batch_row((uint8_t)px, (uint8_t)py);
            if (color < 16 && (transparent_mask & (1u << color))) continue;
            *dstp = pico_video_remap(color);
        }
    }

    pico_video_batch_end(prev_page);
}

void pico_sspr(uint8_t sx, uint8_t sy, uint8_t sw, uint8_t sh,
               int16_t dx, int16_t dy, uint8_t dw, uint8_t dh,
               uint8_t flip_x, uint8_t flip_y) {
    uint16_t dxi, dyi, u, v;
    uint8_t color, prev_page;

    if (!sheet || dw == 0 || dh == 0 || sw == 0 || sh == 0) return;

    if (sw == dw && sh == dh) {
        blit_unscaled(sx, sy, sw, sh, dx, dy, flip_x, flip_y);
        return;
    }

    /* General scaled path -- needs a 32-bit multiply+divide per pixel to
     * map a dest offset back to a source offset (real z88dk/Z80 software
     * division, not a single instruction). Not hit by pico_spr() or any
     * map() tile (both always call through 1:1, see above); kept simple
     * since no reference game's sspr() call currently needs it fast. */
    prev_page = pico_video_batch_begin();

    for (dyi = 0; dyi < dh; dyi++) {
        v = (uint16_t)((uint32_t)dyi * sh / dh);
        if (flip_y) v = sh - 1 - v;
        for (dxi = 0; dxi < dw; dxi++) {
            u = (uint16_t)((uint32_t)dxi * sw / dw);
            if (flip_x) u = sw - 1 - u;

            color = sheet_pixel(sx + u, sy + v);
            if (color < 16 && (transparent_mask & (1u << color))) continue;

            batch_pset_safe(dx + (int16_t)dxi, dy + (int16_t)dyi, color);
        }
    }

    pico_video_batch_end(prev_page);
}

uint8_t pico_sget(uint8_t x, uint8_t y) {
    if (!sheet || x >= sheet_w || y >= sheet_h) return 0;
    return sheet_pixel(x, y);
}

uint8_t pico_spr_collide(uint8_t n1, int16_t x1, int16_t y1, uint8_t w1, uint8_t h1,
                          uint8_t n2, int16_t x2, int16_t y2, uint8_t w2, uint8_t h2) {
    uint16_t cells_per_row;
    uint8_t sx1, sy1, sx2, sy2, pw1, ph1, pw2, ph2;
    int16_t i, j, x, y;

    if (!sheet || sheet_w < 8) return 0;
    cells_per_row = sheet_w / 8;
    sx1 = (uint8_t)((n1 % cells_per_row) * 8);
    sy1 = (uint8_t)((n1 / cells_per_row) * 8);
    sx2 = (uint8_t)((n2 % cells_per_row) * 8);
    sy2 = (uint8_t)((n2 / cells_per_row) * 8);
    pw1 = (uint8_t)(w1 * 8); ph1 = (uint8_t)(h1 * 8);
    pw2 = (uint8_t)(w2 * 8); ph2 = (uint8_t)(h2 * 8);

    /* Cheap bounding-box reject before paying for the pixel-by-pixel
     * check below -- see this function's own header comment. */
    if (x1 >= (int16_t)(x2 + pw2) || x2 >= (int16_t)(x1 + pw1) ||
        y1 >= (int16_t)(y2 + ph2) || y2 >= (int16_t)(y1 + ph1)) return 0;

    /* Walk the smaller sprite's pixels against the larger one -- same
     * optimisation dw817's original algorithm uses. */
    if ((uint16_t)pw1 * ph1 > (uint16_t)pw2 * ph2) {
        uint8_t t8;
        int16_t t16;
        t8 = sx1; sx1 = sx2; sx2 = t8;
        t8 = sy1; sy1 = sy2; sy2 = t8;
        t8 = pw1; pw1 = pw2; pw2 = t8;
        t8 = ph1; ph1 = ph2; ph2 = t8;
        t16 = x1; x1 = x2; x2 = t16;
        t16 = y1; y1 = y2; y2 = t16;
    }

    for (i = 0; i < (int16_t)ph1; i++) {
        for (j = 0; j < (int16_t)pw1; j++) {
            x = (int16_t)(j + x1 - x2);
            y = (int16_t)(i + y1 - y2);
            if (x >= 0 && x < (int16_t)pw2 && y >= 0 && y < (int16_t)ph2 &&
                sheet_pixel((uint16_t)(sx1 + j), (uint16_t)(sy1 + i)) > 0 &&
                sheet_pixel((uint16_t)(sx2 + x), (uint16_t)(sy2 + y)) > 0) {
                return 1;
            }
        }
    }
    return 0;
}

/* Calls blit_unscaled() directly instead of going through pico_sspr() --
 * every pico_spr() call is 1:1 by construction (dw==sw, dh==sh always),
 * so the hop through pico_sspr() just to re-discover "not scaled" and
 * dispatch back out cost a whole extra stack frame for nothing. This
 * matters beyond raw speed: `map_draw()` (pico_map.c) calls pico_spr()
 * once per tile, and the real-game stack-depth investigation
 * (docs/picodiver-notes.md) found this call chain deep enough that SP
 * dipped into addresses this program's own BSS globals (`cab`, `mode`)
 * live at, corrupting them during ordinary play -- every removed call
 * level is real, measured safety margin, not just a speed tweak. */
void pico_spr(uint8_t n, int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t flip_x, uint8_t flip_y) {
    uint16_t cells_per_row, cell_col, cell_row, sx, sy;

    if (!sheet || sheet_w < 8) return;
    cells_per_row = sheet_w / 8;
    cell_col = n % cells_per_row;
    cell_row = n / cells_per_row;
    sx = cell_col * 8;
    sy = cell_row * 8;

    blit_unscaled((uint8_t)sx, (uint8_t)sy, (uint8_t)(w * 8), (uint8_t)(h * 8), x, y, flip_x, flip_y);
}
