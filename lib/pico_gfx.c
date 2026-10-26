#include <stdint.h>
#include "pico_gfx.h"
#include "pico_video.h"
#include "pico_font.h"

/* --- current colour state (pico_gfx.h's own top comment) ------------------ */

static uint8_t current_color = 6; /* Pico-8's own default pen colour at cart start */

void pico_color(uint8_t c) {
    current_color = c;
}

/* Resolves PICO_CURRENT_COLOR to the real current colour, a no-op for
 * any genuine 0-15 (or dither-encoded >15, for the callers that still
 * pass those straight through) value -- called once at the top of each
 * of the 4 functions that accept the sentinel (pico_gfx.h's own list). */
static uint8_t resolve_color(uint8_t color) {
    return (color == PICO_CURRENT_COLOR) ? current_color : color;
}

/* --- fillp dither state -------------------------------------------------- */

static uint16_t fill_pattern = 0;
static uint8_t fill_pattern_transparent = 0;

void pico_fillp(uint16_t pattern, uint8_t transparent) {
    fill_pattern = pattern;
    fill_pattern_transparent = transparent;
}

void pico_fillp_fix(pico_fix_t p) {
    uint16_t pattern = (uint16_t)((uint32_t)p >> 16);
    uint8_t transparent = (uint8_t)(((uint32_t)p >> 15) & 1);
    pico_fillp(pattern, transparent);
}

static uint8_t fillp_bit_on(uint8_t x, uint8_t y) {
    uint8_t cell_x, cell_y, bit_index;
    if (fill_pattern == 0) return 0;
    cell_x = x & 3;
    cell_y = y & 3;
    bit_index = 15 - (cell_y * 4 + cell_x);
    return (uint8_t)((fill_pattern >> bit_index) & 1);
}

/* color > 15 encodes a two-colour dither (low nibble = primary/"off",
 * high nibble = secondary/"on") -- Pico-8's own convention, ported from
 * PicoJS's resolveFillColours(). */
static void resolve_fill_colours(uint8_t color, uint8_t *primary, uint8_t *secondary, uint8_t *has_secondary) {
    if (color > 15) {
        *primary = color & 0x0F;
        *secondary = (color >> 4) & 0x0F;
        *has_secondary = 1;
    } else {
        *primary = color;
        *secondary = 0;
        *has_secondary = 0;
    }
}

/* --- camera() state -------------------------------------------------------
 *
 * Applied inside pset_safe()/fill_run() below -- the two chokepoints every
 * shape primitive in this file already routes all of its actual pixel
 * writes through, so offsetting there makes rect/line/circ/oval (and their
 * fill variants) camera-aware with no change to any of those functions
 * themselves. print_impl() is the one exception -- see its own comment. */

static int16_t camera_x = 0;
static int16_t camera_y = 0;

void pico_camera(int16_t x, int16_t y) {
    camera_x = x;
    camera_y = y;
}

/* --- clip() state -----------------------------------------------------
 *
 * Same scope as camera() just above, for the same reason: applied inside
 * pset_safe()/fill_run()/draw_glyph()'s own call site, not pico_sprite.c's
 * spr()/sspr() or pico_video.c's raw pset/cls -- no reference game needs
 * sprite/raw-pset clipping narrower than the logical canvas yet, and this
 * stays a cheap intersection against bounds pset_safe()/fill_run() were
 * already computing for the screen-edge case, not a new per-pixel cost.
 * Defaults (INT16_MAX bounds) make every check below a no-op until a game
 * actually calls pico_clip() -- the existing screen-edge clipping is
 * untouched either way, this ANDs a second, narrower rectangle on top. */
static int16_t clip_x0 = 0;
static int16_t clip_y0 = 0;
static int16_t clip_x1 = 32767; /* INT16_MAX -- avoids pulling in <stdint.h>'s own macro for one constant */
static int16_t clip_y1 = 32767;

void pico_clip(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    clip_x0 = x0;
    clip_y0 = y0;
    clip_x1 = x1;
    clip_y1 = y1;
}

void pico_clip_reset(void) {
    clip_x0 = 0;
    clip_y0 = 0;
    clip_x1 = 32767;
    clip_y1 = 32767;
}

/* --- bounds-checked plotting ---------------------------------------------
 *
 * pico_pset()/pico_pget() (pico_video.h) take unchecked uint8_t coordinates
 * by design (a hot path games can also call directly). Every composite
 * shape here, though, derives coordinates via subtraction (a circle's
 * cx-r, an ellipse's cx-x, ...) that can legitimately go negative for a
 * shape that's simply partly off-screen -- e.g. circfill(2, 2, 10, ...) is
 * an ordinary small circle near a corner, not a misuse, and its leftmost
 * column is cx-r = -8. Casting a negative value straight to uint8_t would
 * wrap (-8 -> 248) and corrupt the framebuffer at a wildly wrong position
 * instead of simply clipping, the way Pico-8/PicoJS's own renderer does.
 * So every primitive below computes in signed int16_t and funnels all
 * actual pixel writes through pset_safe()/fill_run(), which clip to the
 * logical canvas instead of wrapping. */

static void pset_safe(int16_t x, int16_t y, uint8_t color) {
    x -= camera_x;
    y -= camera_y;
    if (x < 0 || y < 0 || x >= (int16_t)pico_screen_w || y >= (int16_t)pico_screen_h) return;
    if (x < clip_x0 || y < clip_y0 || x > clip_x1 || y > clip_y1) return;
    pico_pset((uint8_t)x, (uint8_t)y, color);
}

/* Fills the horizontal run [x0, x1] (inclusive, clipped) at row y,
 * honouring the current fillp() pattern if one is active -- ported from
 * PicoJS's fillRun(). */
static void fill_run(int16_t x0, int16_t x1, int16_t y, uint8_t primary, uint8_t secondary, uint8_t has_secondary) {
    uint8_t x;
    int16_t t;
    x0 -= camera_x;
    x1 -= camera_x;
    y -= camera_y;
    if (x1 < x0) { t = x0; x0 = x1; x1 = t; }
    if (y < 0 || y >= (int16_t)pico_screen_h) return;
    if (y < clip_y0 || y > clip_y1) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= (int16_t)pico_screen_w) x1 = (int16_t)pico_screen_w - 1;
    if (x0 < clip_x0) x0 = clip_x0;
    if (x1 > clip_x1) x1 = clip_x1;
    if (x0 > x1) return; /* fully clipped away */

    x = (uint8_t)x0;
    for (;;) {
        if (fill_pattern == 0) {
            pico_pset(x, (uint8_t)y, primary);
        } else if (fillp_bit_on(x, (uint8_t)y)) {
            if (!fill_pattern_transparent && has_secondary) pico_pset(x, (uint8_t)y, secondary);
        } else {
            pico_pset(x, (uint8_t)y, primary);
        }
        if (x == (uint8_t)x1) break;
        x++;
    }
}

/* --- rect / rectfill ------------------------------------------------------ */

void pico_rectfill(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color) {
    uint8_t tmp, primary, secondary, has_secondary, y;
    if (x1 < x0) { tmp = x0; x0 = x1; x1 = tmp; }
    if (y1 < y0) { tmp = y0; y0 = y1; y1 = tmp; }
    resolve_fill_colours(color, &primary, &secondary, &has_secondary);

    y = y0;
    for (;;) {
        fill_run(x0, x1, y, primary, secondary, has_secondary);
        if (y == y1) break;
        y++;
    }
}

void pico_rect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color) {
    uint8_t tmp, x, y;
    color = resolve_color(color);
    if (x1 < x0) { tmp = x0; x0 = x1; x1 = tmp; }
    if (y1 < y0) { tmp = y0; y0 = y1; y1 = tmp; }

    x = x0;
    for (;;) {
        pset_safe(x, y0, color);
        pset_safe(x, y1, color);
        if (x == x1) break;
        x++;
    }
    y = y0;
    for (;;) {
        pset_safe(x0, y, color);
        pset_safe(x1, y, color);
        if (y == y1) break;
        y++;
    }
}

/* --- line (Bresenham, ported from PicoJS's line()) ------------------------ */

void pico_line(uint8_t ax0, uint8_t ay0, uint8_t ax1, uint8_t ay1, uint8_t color) {
    int16_t x0 = ax0, y0 = ay0, x1 = ax1, y1 = ay1;
    color = resolve_color(color);
    int16_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int16_t dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int8_t sx = (x0 < x1) ? 1 : -1;
    int8_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t e2;

    for (;;) {
        pset_safe(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

/* --- circle (midpoint circle algorithm, ported from PicoJS's plotCircle) -- */

static void circle_points(uint8_t cx, uint8_t cy, int16_t ox, int16_t oy, uint8_t fill,
                           uint8_t primary, uint8_t secondary, uint8_t has_secondary) {
    int16_t icx = cx, icy = cy;
    if (fill) {
        fill_run(icx - ox, icx + ox, icy + oy, primary, secondary, has_secondary);
        fill_run(icx - ox, icx + ox, icy - oy, primary, secondary, has_secondary);
        fill_run(icx - oy, icx + oy, icy + ox, primary, secondary, has_secondary);
        fill_run(icx - oy, icx + oy, icy - ox, primary, secondary, has_secondary);
    } else {
        pset_safe(icx + ox, icy + oy, primary);
        pset_safe(icx - ox, icy + oy, primary);
        pset_safe(icx + ox, icy - oy, primary);
        pset_safe(icx - ox, icy - oy, primary);
        pset_safe(icx + oy, icy + ox, primary);
        pset_safe(icx - oy, icy + ox, primary);
        pset_safe(icx + oy, icy - ox, primary);
        pset_safe(icx - oy, icy - ox, primary);
    }
}

static void plot_circle(uint8_t cx, uint8_t cy, uint8_t r, uint8_t fill, uint8_t color) {
    int16_t x = r, y = 0, d = (int16_t)(1 - (int16_t)r);
    uint8_t primary, secondary, has_secondary;
    resolve_fill_colours(color, &primary, &secondary, &has_secondary);

    while (x >= y) {
        circle_points(cx, cy, x, y, fill, primary, secondary, has_secondary);
        y++;
        if (d <= 0) {
            d += 2 * y + 1;
        } else {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

void pico_circ(uint8_t cx, uint8_t cy, uint8_t r, uint8_t color) {
    plot_circle(cx, cy, r, 0, color);
}

void pico_circfill(uint8_t cx, uint8_t cy, uint8_t r, uint8_t color) {
    plot_circle(cx, cy, r, 1, color);
}

/* --- oval (midpoint ellipse algorithm -- integer only, no sqrt/float,
 * unlike PicoJS's own float-based plotOval(); see docs/gfx-notes.md) ----- */

static void ellipse_points(int16_t cx, int16_t cy, int16_t ox, int16_t oy, uint8_t fill,
                            uint8_t primary, uint8_t secondary, uint8_t has_secondary) {
    if (fill) {
        fill_run(cx - ox, cx + ox, cy + oy, primary, secondary, has_secondary);
        fill_run(cx - ox, cx + ox, cy - oy, primary, secondary, has_secondary);
    } else {
        pset_safe(cx + ox, cy + oy, primary);
        pset_safe(cx - ox, cy + oy, primary);
        pset_safe(cx + ox, cy - oy, primary);
        pset_safe(cx - ox, cy - oy, primary);
    }
}

static void plot_oval(uint8_t ax0, uint8_t ay0, uint8_t ax1, uint8_t ay1, uint8_t fill, uint8_t color) {
    int16_t x0 = ax0, y0 = ay0, x1 = ax1, y1 = ay1, t;
    int16_t rx, ry, cx, cy, x, y;
    int32_t rx2, ry2, dx, dy, d1, d2;
    uint8_t primary, secondary, has_secondary;

    if (x1 < x0) { t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { t = y0; y0 = y1; y1 = t; }

    rx = (x1 - x0) / 2;
    ry = (y1 - y0) / 2;
    cx = x0 + rx;
    cy = y0 + ry;
    resolve_fill_colours(color, &primary, &secondary, &has_secondary);

    if (rx <= 0 || ry <= 0) {
        /* degenerate bounding box (a line or a point) -- matches PicoJS's
         * own fallback for this case. */
        if (fill) {
            y = y0;
            for (;;) {
                fill_run(x0, x1, y, primary, secondary, has_secondary);
                if (y == y1) break;
                y++;
            }
        } else {
            x = x0;
            for (;;) { pset_safe(x, y0, primary); pset_safe(x, y1, primary); if (x == x1) break; x++; }
            y = y0;
            for (;;) { pset_safe(x0, y, primary); pset_safe(x1, y, primary); if (y == y1) break; y++; }
        }
        return;
    }

    rx2 = (int32_t)rx * rx;
    ry2 = (int32_t)ry * ry;
    x = 0;
    y = ry;
    dx = 0;
    dy = 2 * rx2 * y;

    ellipse_points(cx, cy, x, y, fill, primary, secondary, has_secondary);

    /* Region 1: |slope| < 1 */
    d1 = ry2 - rx2 * ry + rx2 / 4;
    while (dx < dy) {
        x++;
        dx += 2 * ry2;
        if (d1 < 0) {
            d1 += dx + ry2;
        } else {
            y--;
            dy -= 2 * rx2;
            d1 += dx - dy + ry2;
        }
        ellipse_points(cx, cy, x, y, fill, primary, secondary, has_secondary);
    }

    /* Region 2: |slope| >= 1 */
    d2 = ry2 * (2 * x + 1) * (2 * x + 1) / 4 + rx2 * (y - 1) * (y - 1) - rx2 * ry2;
    while (y > 0) {
        y--;
        dy -= 2 * rx2;
        if (d2 > 0) {
            d2 += rx2 - dy;
        } else {
            x++;
            dx += 2 * ry2;
            d2 += dx - dy + rx2;
        }
        ellipse_points(cx, cy, x, y, fill, primary, secondary, has_secondary);
    }
}

void pico_oval(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color) {
    plot_oval(x0, y0, x1, y1, 0, color);
}

void pico_ovalfill(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color) {
    plot_oval(x0, y0, x1, y1, 1, color);
}

/* --- text ------------------------------------------------------------------
 *
 * Uses the built-in Pico-8 4x6 font (pico_font.h/.c). The range check below
 * is deliberately "ch - FIRST >= NUM_GLYPHS" (an offset compared against a
 * count), not "ch >= FIRST && ch < FIRST + NUM_GLYPHS" (a sum) --
 * dark-room-next's docs/tilemap-notes.md hit a real bug from the latter
 * form: z88dk's 8-bit codegen can evaluate "FIRST + NUM_GLYPHS" (32+95=127,
 * harmless here, but the general pattern bit elsewhere) in 8-bit arithmetic
 * and wrap, silently breaking the upper-bound check. The offset form never
 * computes that sum at all. */

static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;

void pico_cursor(uint8_t x, uint8_t y) {
    cursor_x = x;
    cursor_y = y;
}

/* Writes within an ALREADY-OPEN batch session (see print_impl() below) --
 * profiling picodiver's title screen (7 lines, ~150 glyphs) found the
 * original per-pixel pset_safe()/pico_pset() path paid a full DI/EI plus
 * an MMU page-select port write for every single SET bit in the font
 * bitmap, the same per-pixel-call pathology pico_sprite.c's blit had
 * (see pico_video.c/pico_sprite.c's own comments) -- several seconds just
 * to draw the title screen's text, on top of the sprite/map fixes. Here
 * the caller opens one batch for the WHOLE string, so DI/EI and the MMU
 * page selection (pico_video_batch_row(), which only re-writes the page
 * when it actually changes) are paid at most a handful of times for an
 * entire multi-line print(), not once per lit pixel. */
static void draw_glyph(uint8_t ch, int16_t px, int16_t py, uint8_t remapped_color) {
    uint8_t glyph_index, row, col, bits;
    const uint8_t *rows;

    if (ch < PICO_FONT_FIRST_CHAR || (uint8_t)(ch - PICO_FONT_FIRST_CHAR) >= PICO_FONT_NUM_GLYPHS) {
        ch = '?'; /* fallback glyph, matches PicoJS */
    }
    glyph_index = (uint8_t)(ch - PICO_FONT_FIRST_CHAR);
    rows = &pico_font_data[(uint16_t)glyph_index * PICO_FONT_CELL_H];

    for (row = 0; row < PICO_FONT_CELL_H; row++) {
        int16_t py_row = py + (int16_t)row;
        if (py_row < 0 || py_row >= (int16_t)pico_screen_h) continue;
        if (py_row < clip_y0 || py_row > clip_y1) continue;
        bits = rows[row];
        for (col = 0; col < PICO_FONT_CELL_W; col++) {
            int16_t px_col = px + (int16_t)col;
            if ((bits >> (PICO_FONT_CELL_W - 1 - col)) & 1 &&
                px_col >= 0 && px_col < (int16_t)pico_screen_w &&
                px_col >= clip_x0 && px_col <= clip_x1) {
                *pico_video_batch_row((uint8_t)px_col, (uint8_t)py_row) = remapped_color;
            }
        }
    }
}

/* Shared by pico_print()/pico_print_at_cursor() -- draws `text` starting at
 * (x,y), breaking to a new line at embedded '\n' (resetting back to this
 * call's own starting x, advancing by the font's cell height, matching
 * PicoJS's print()), and reports where drawing ended via *end_x/*end_y. */
static void print_impl(const char *text, int16_t x, int16_t y, uint8_t color, int16_t *end_x, int16_t *end_y) {
    int16_t px = x, py = y, start_x = x;
    uint8_t ch;
    uint8_t remapped_color = pico_video_remap(color);
    uint8_t prev_page = pico_video_batch_begin();

    while ((ch = (uint8_t)*text) != 0) {
        text++;
        if (ch == '\n') {
            py += PICO_FONT_CELL_H;
            px = start_x;
            continue;
        }
        /* Camera offset applied ONLY here, not to px/py themselves --
         * *end_x/*end_y below feed pico_cursor()'s chaining for
         * print_at_cursor(), and offsetting px/py directly would double-
         * apply the camera shift on every subsequent chained call. */
        draw_glyph(ch, (int16_t)(px - camera_x), (int16_t)(py - camera_y), remapped_color);
        px += PICO_FONT_CELL_W;
    }

    pico_video_batch_end(prev_page);
    *end_x = start_x;
    *end_y = py;
}

void pico_print(const char *text, uint8_t x, uint8_t y, uint8_t color) {
    int16_t ex, ey;
    print_impl(text, x, y, resolve_color(color), &ex, &ey);
}

void pico_print_at_cursor(const char *text, uint8_t color) {
    int16_t ex, ey;
    print_impl(text, cursor_x, cursor_y, resolve_color(color), &ex, &ey);
    cursor_x = (uint8_t)ex;
    cursor_y = (uint8_t)ey;
}
