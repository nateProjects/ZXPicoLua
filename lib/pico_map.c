#include <stdint.h>
#include "pico_map.h"
#include "pico_sprite.h"

static uint8_t *map_data = 0;
static uint16_t map_w = 0;
static uint16_t map_h = 0;
static uint8_t sprite_flags[256];

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

static void map_draw(uint8_t cel_x, uint8_t cel_y, uint8_t sx, uint8_t sy,
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
                    if (tile != 0 && (!layer_filter || (sprite_flags[tile] & layer_mask) != 0)) {
                        pico_spr(tile, (uint8_t)(sx + dx * 8), (uint8_t)(sy + dy * 8), 1, 1, 0, 0);
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

void pico_map(uint8_t cel_x, uint8_t cel_y, uint8_t sx, uint8_t sy, uint8_t cel_w, uint8_t cel_h) {
    map_draw(cel_x, cel_y, sx, sy, cel_w, cel_h, 0, 0);
}

void pico_map_layer(uint8_t cel_x, uint8_t cel_y, uint8_t sx, uint8_t sy, uint8_t cel_w, uint8_t cel_h, uint8_t layer_mask) {
    map_draw(cel_x, cel_y, sx, sy, cel_w, cel_h, 1, layer_mask);
}
