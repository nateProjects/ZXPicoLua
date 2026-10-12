#include <stdint.h>
#include "pico_map.h"
#include "pico_sprite.h"

static uint8_t *map_data = 0;
static uint16_t map_w = 0;
static uint16_t map_h = 0;
/* fget()/fset()/map_layer() support -- opt out with -DPICO_NO_MAP_FLAGS
 * (per-game, in that game's own build) when a game doesn't use any of
 * them, to recover this array's 256 bytes. A real, measured need: this
 * project's own z88dk classic builds have essentially zero spare bytes
 * under the 64K resident limit (docs/picodiver-notes.md), and picodiver
 * itself never calls any of these (confirmed by grep, not assumed --
 * it branches on raw sprite-index ranges instead, see its own notes). */
#ifndef PICO_NO_MAP_FLAGS
static uint8_t sprite_flags[256];
#endif

void pico_map_init(uint8_t *data, uint16_t w, uint16_t h) {
    map_data = data;
    map_w = w;
    map_h = h;
}

uint8_t pico_mget(uint8_t x, uint8_t y) {
    if (!map_data || x >= map_w || y >= map_h) return 0;
    return map_data[(uint16_t)y * map_w + x];
}

void pico_mset(uint8_t x, uint8_t y, uint8_t v) {
    if (!map_data || x >= map_w || y >= map_h) return;
    map_data[(uint16_t)y * map_w + x] = v;
}

#ifndef PICO_NO_MAP_FLAGS
uint8_t pico_fget(uint8_t n, uint8_t f) {
    if (f >= 8) return 0;
    return (uint8_t)((sprite_flags[n] >> f) & 1);
}

void pico_fset(uint8_t n, uint8_t f, uint8_t v) {
    if (f >= 8) return;
    if (v) sprite_flags[n] |= (uint8_t)(1u << f);
    else sprite_flags[n] &= (uint8_t)~(1u << f);
}

uint8_t pico_fget_all(uint8_t n) {
    return sprite_flags[n];
}

void pico_fset_all(uint8_t n, uint8_t flags) {
    sprite_flags[n] = flags;
}
#endif

static void map_draw(uint8_t cel_x, uint8_t cel_y, int16_t sx, int16_t sy,
                      uint8_t cel_w, uint8_t cel_h, uint8_t layer_filter, uint8_t layer_mask) {
    uint8_t dx, dy, tile;
    uint16_t tx, ty;

    if (!map_data || cel_w == 0 || cel_h == 0) return;

    dy = 0;
    for (;;) {
        ty = (uint16_t)cel_y + dy;
        if (ty < map_h) {
            dx = 0;
            for (;;) {
                tx = (uint16_t)cel_x + dx;
                if (tx < map_w) {
                    tile = map_data[ty * map_w + tx];
                    /* Pico-8 convention: tile 0 is always "empty", never
                     * drawn -- ported from PicoJS's own picoJS.map. */
#ifndef PICO_NO_MAP_FLAGS
                    if (tile != 0 && (!layer_filter || (sprite_flags[tile] & layer_mask) != 0)) {
#else
                    if (tile != 0) {
#endif
                        pico_spr(tile, (int16_t)(sx + dx * 8), (int16_t)(sy + dy * 8), 1, 1, 0, 0);
                    }
                }
                if (dx == cel_w - 1) break;
                dx++;
            }
        }
        if (dy == cel_h - 1) break;
        dy++;
    }
}

void pico_map(uint8_t cel_x, uint8_t cel_y, int16_t sx, int16_t sy, uint8_t cel_w, uint8_t cel_h) {
    map_draw(cel_x, cel_y, sx, sy, cel_w, cel_h, 0, 0);
}

#ifndef PICO_NO_MAP_FLAGS
void pico_map_layer(uint8_t cel_x, uint8_t cel_y, int16_t sx, int16_t sy, uint8_t cel_w, uint8_t cel_h, uint8_t layer_mask) {
    map_draw(cel_x, cel_y, sx, sy, cel_w, cel_h, 1, layer_mask);
}

void pico_map_build_colmask(uint8_t *out_mask, uint8_t cel_x, uint8_t cel_y,
                             uint8_t cel_w, uint8_t cel_h, uint8_t flag_mask) {
    uint8_t dx, dy, tile;

    if (cel_w == 0 || cel_h == 0) return;

    dy = 0;
    for (;;) {
        dx = 0;
        for (;;) {
            tile = pico_mget((uint8_t)(cel_x + dx), (uint8_t)(cel_y + dy));
            out_mask[(uint16_t)dy * cel_w + dx] = (uint8_t)(pico_fget_all(tile) & flag_mask);
            if (dx == cel_w - 1) break;
            dx++;
        }
        if (dy == cel_h - 1) break;
        dy++;
    }
}

uint8_t pico_colbox_at(const uint8_t *mask, uint8_t mask_w, uint8_t mask_h,
                        int16_t px, int16_t py, uint8_t pw, uint8_t ph) {
    int16_t x0 = px >> 3;
    int16_t y0 = py >> 3;
    int16_t x1 = (int16_t)(px + pw - 1) >> 3;
    int16_t y1 = (int16_t)(py + ph - 1) >> 3;
    int16_t x, y;
    uint8_t result = 0;

    if (x1 < 0 || y1 < 0 || x0 >= (int16_t)mask_w || y0 >= (int16_t)mask_h) return 0;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= (int16_t)mask_w) x1 = (int16_t)mask_w - 1;
    if (y1 >= (int16_t)mask_h) y1 = (int16_t)mask_h - 1;

    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            result |= mask[(uint16_t)y * mask_w + x];
    return result;
}
#endif
