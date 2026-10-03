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
 * off-screen sprite -- clip instead of letting pico_pset()'s raw uint8_t
 * cast wrap and corrupt the framebuffer. */
static void pset_safe(int16_t x, int16_t y, uint8_t color) {
    if (x < 0 || y < 0 || x >= (int16_t)pico_screen_w || y >= (int16_t)pico_screen_h) return;
    pico_pset((uint8_t)x, (uint8_t)y, color);
}

void pico_sspr(uint8_t sx, uint8_t sy, uint8_t sw, uint8_t sh,
               uint8_t dx, uint8_t dy, uint8_t dw, uint8_t dh,
               uint8_t flip_x, uint8_t flip_y) {
    uint16_t dxi, dyi, u, v;
    uint8_t color;

    if (!sheet || dw == 0 || dh == 0 || sw == 0 || sh == 0) return;

    for (dyi = 0; dyi < dh; dyi++) {
        v = (uint16_t)((uint32_t)dyi * sh / dh);
        if (flip_y) v = sh - 1 - v;
        for (dxi = 0; dxi < dw; dxi++) {
            u = (uint16_t)((uint32_t)dxi * sw / dw);
            if (flip_x) u = sw - 1 - u;

            color = sheet_pixel(sx + u, sy + v);
            if (color < 16 && (transparent_mask & (1u << color))) continue;

            pset_safe((int16_t)dx + dxi, (int16_t)dy + dyi, color);
        }
    }
}

void pico_spr(uint8_t n, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t flip_x, uint8_t flip_y) {
    uint16_t cells_per_row, cell_col, cell_row, sx, sy, sw, sh;

    if (!sheet || sheet_w < 8) return;
    cells_per_row = sheet_w / 8;
    cell_col = n % cells_per_row;
    cell_row = n / cells_per_row;
    sx = cell_col * 8;
    sy = cell_row * 8;
    sw = (uint16_t)w * 8;
    sh = (uint16_t)h * 8;

    pico_sspr((uint8_t)sx, (uint8_t)sy, (uint8_t)sw, (uint8_t)sh, x, y, (uint8_t)sw, (uint8_t)sh, flip_x, flip_y);
}
