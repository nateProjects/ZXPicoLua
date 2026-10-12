/* M0 spike for the Islander-as-Next-game plan (see /Users/nat/.claude/plans/
 * polished-churning-meerkat.md): throwaway, measure-don't-assume check of
 * two open questions before any real game code gets written --
 *
 * 1. Is hardware-sprite X 1:1 with this project's 40-column tilemap-pixel
 *    X, or scaled (dark-room-next measured a 2:1 scale against ITS OWN
 *    80-column/640px tilemap -- unverified whether that holds here, where
 *    the tilemap is 40-column/320px, matching the sprite layer's own fixed
 *    320x256 native surface width).
 * 2. Does a hardware sprite actually render in this project's existing
 *    init sequence (pico_video_init + pico_tilemap_init, which already
 *    enables RSLS_SPRITES_VISIBLE) with no extra palette setup -- matches
 *    dark-room-next's own finding that 8-bit sprite patterns render
 *    correctly against the default boot palette.
 *
 * Draws 3 known tilemap column markers (col 0, 20, 39, each an 8x8 solid
 * tile) and 2 hardware sprites at known nominal sprite-space X (0 and 160)
 * at the same Y, then halts forever -- a single ZRCP screenshot is enough
 * to measure both. Not part of the real game build; its own GAME= target. */

#include <arch/zxn.h>
#include <intrinsic.h>
#include <stdint.h>
#include "pico_video.h"
#include "pico_tilemap.h"

/* Verbatim from next-resources/code/zxnext_level9-master/src/sprite.c --
 * a proven, shipped reference implementation -- used as-is here to rule
 * out a subtle divergence in my own re-derived version as the cause of
 * the first 3 attempts' invisible sprites (everything else -- attribute
 * bytes, pattern data, clip window, layer-priority register -- already
 * confirmed correct via direct ZRCP hardware-state readback, not just
 * "what my code intended to write"). */
#define SPRITE_SLOT_MASK 0x3F
#define SPRITE_VISIBLE_MASK 0x80
#define SPRITE_EXTENDED_MASK 0x40
#define LSB(v) ((uint8_t)((v) & 0x00FF))
#define MSB(v) ((uint8_t)(((v) & 0x0100) >> 8))

#define sprite_select_slot(slot) (IO_SPRITE_SLOT = (slot) & SPRITE_SLOT_MASK)

static void sprite_set_pattern_solid(uint8_t color) {
    uint16_t i;
    for (i = 0; i < 256; i++) IO_SPRITE_PATTERN = color;
}

static void sprite_set_attributes(uint8_t pattern_slot, uint16_t x, uint16_t y, uint8_t visible) {
    uint8_t attr = (uint8_t)(pattern_slot & SPRITE_SLOT_MASK);
    attr |= SPRITE_EXTENDED_MASK;
    if (visible) attr |= SPRITE_VISIBLE_MASK;

    IO_SPRITE_ATTRIBUTE = LSB(x);
    IO_SPRITE_ATTRIBUTE = LSB(y);
    IO_SPRITE_ATTRIBUTE = MSB(x);
    IO_SPRITE_ATTRIBUTE = attr;
    IO_SPRITE_ATTRIBUTE = MSB(y);
}

static void sprite_set(uint8_t slot, uint16_t x, uint16_t y, uint8_t color) {
    sprite_select_slot(slot);
    sprite_set_pattern_solid(color);
    sprite_select_slot(slot);
    sprite_set_attributes(slot, x, y, 1);
}

int main(void) {
    static const uint8_t marker_tile[32] = {
        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11
    };

    intrinsic_ei();
    pico_video_init(320, 256);
    pico_tilemap_init();
    /* Disable Layer 2 visibility entirely for this test -- isolates
     * "are hardware sprites rendering at all" from "is Layer 2's
     * transparency letting them show through", which the first attempt
     * couldn't distinguish (Layer 2's own cleared black and the
     * tilemap's own blank black look identical either way). */
    IO_LAYER_2_CONFIG = 0;

    /* Explicit sprite clip-window reset -- nothing else in this codebase
     * has ever touched REG_CLIP_WINDOW_SPRITES, so it's whatever the cold
     * boot default is. Reset its own write-index then set it to the full
     * sprite-space range, ruling out "clipped to nothing" as the cause of
     * the first two attempts' invisible sprites. */
    ZXN_WRITE_REG(REG_CLIP_WINDOW_CONTROL, RCWC_RESET_SPRITE_CLIP_INDEX);
    ZXN_WRITE_REG(REG_CLIP_WINDOW_SPRITES, 0);
    ZXN_WRITE_REG(REG_CLIP_WINDOW_SPRITES, 255);
    ZXN_WRITE_REG(REG_CLIP_WINDOW_SPRITES, 0);
    ZXN_WRITE_REG(REG_CLIP_WINDOW_SPRITES, 255);

    /* Explicit sprite palette/transparency setup (don't rely on assumed
     * boot defaults) -- matches next-resources/zxnext_level9's own
     * sprite_set_default_palette(): identity palette (value N -> raw
     * RGB332 colour N) and an explicit transparency index that can't
     * collide with either marker colour below. */
    ZXN_WRITE_REG(REG_SPRITE_TRANSPARENCY_INDEX, 0xFF);
    {
        uint8_t prev_pal_ctrl = ZXN_READ_REG(REG_PALETTE_CONTROL);
        uint16_t i;
        ZXN_WRITE_REG(REG_PALETTE_CONTROL, (uint8_t)((prev_pal_ctrl & 0x8F) | RPC_SELECT_SPRITES_PALETTE_0));
        ZXN_WRITE_REG(REG_PALETTE_INDEX, 0);
        for (i = 0; i < 256; i++) ZXN_WRITE_REG(REG_PALETTE_VALUE_8, (uint8_t)i);
    }

    pico_tilemap_set_color(0, 0, 0, 0); /* black -- blank cells */
    pico_tilemap_set_color(1, 7, 7, 0); /* yellow -- marker tile colour */
    pico_tilemap_define_tile(1, marker_tile);
    pico_tilemap_fill(0, 0);
    pico_tilemap_set(0, 0, 1, 0);  /* column 0 */
    pico_tilemap_set(20, 0, 1, 0); /* column 20 */
    pico_tilemap_set(39, 0, 1, 0); /* column 39 (last) */

    /* Two hardware sprites, same Y, nominal screen-relative X 0 and 160
     * (160 sprite-space units apart, same nominal delta dark-room-next
     * used for its own measurement, so the result is directly
     * comparable) -- +32 on BOTH axes: zxnext_notes.pdf (Register $19
     * section) is explicit that "sprite position is always in sprite
     * coordinates with 32,32 corresponding to the top left corner of the
     * ULA screen", regardless of over-border mode. Without this offset
     * (the first 4 attempts' actual bug, found only by reading the spec
     * directly after exhausting every other hypothesis -- attributes,
     * pattern data, clip window and the layer-priority register were all
     * independently confirmed correct via ZRCP hardware-state readback)
     * sprite Y=16 sits 16px *above* the visible screen entirely. */
    sprite_set(0, 32 + 0, 32 + 16, 0xE0);   /* red */
    sprite_set(1, 32 + 160, 32 + 16, 0x1C); /* green */

    for (;;) intrinsic_halt();
}
