#include <arch/zxn.h>
#include "pico_hwsprite.h"
#include "pico_palette.h"

/* Two DIFFERENT 0-63 vs 0-127 ranges were conflated under one mask name
 * until Islander's M4 crafting menu put sprites past slot 63 on screen
 * and found they silently wrapped onto (and corrupted) low-numbered
 * slots still in active use (player/foragry/drops) -- "partial resource
 * sprites" flickering on adjacent islands, initially (and only partly
 * correctly) attributed to a per-frame sprite-write-count budget issue.
 * PATTERN ids genuinely are 6-bit (the attribute byte's own "N0-N5"
 * field, zxnext_notes.pdf 2.5.3) -- 0-63, SPRITE_PATTERN_MASK is right
 * for those. The ANCHOR/instance slot IO_SPRITE_SLOT selects between
 * (what the player/foragry/HUD/etc. each get their own distinct range
 * of) is a full 7 bits -- 0-127, the "128 hardware sprites available"
 * this project's own docs/hwsprite-notes.md already claimed -- needs
 * SPRITE_INDEX_MASK, not the pattern one, even though both happened to
 * be 0x3F here before this fix made them visibly different constants. */
#define SPRITE_PATTERN_MASK  0x3F
#define SPRITE_INDEX_MASK    0x7F
#define SPRITE_VISIBLE_MASK  0x80
#define SPRITE_EXTENDED_MASK 0x40
#define LSB(v) ((uint8_t)((v) & 0x00FF))
#define MSB(v) ((uint8_t)(((v) >> 8) & 0x01))

/* Transparency index for sprite patterns -- any pattern byte equal to
 * this is see-through. 0xE3, not 0xFF: Islander's own M1 testing found
 * 0xFF rendering as a visible opaque colour (a black box around the
 * player sprite, its 4px-per-side padding never actually transparent)
 * despite matching REG_SPRITE_TRANSPARENCY_INDEX exactly -- confirmed via
 * ZRCP that the register, the pattern bytes, and the index all agreed,
 * so 0xFF itself seems to carry some other meaning to the hardware/
 * emulator rather than behaving as a plain transparent value. 0xE3 is
 * what next-resources/code/zxnext_level9-master's own proven
 * sprite_set_default_palette() uses -- switched to match exactly rather
 * than re-guess a value, same reasoning as reusing that reference's
 * attribute-byte layout verbatim in docs/hwsprite-notes.md's own spike. */
#define TRANSPARENT_BYTE 0xE3

static uint8_t pico8_to_rgb332(uint8_t pico8_color) {
    uint8_t r = pico8_palette[pico8_color][0];
    uint8_t g = pico8_palette[pico8_color][1];
    uint8_t b = pico8_palette[pico8_color][2];
    return (uint8_t)((r << 5) | (g << 2) | (b >> 1));
}

void pico_hwsprite_init(void) {
    uint16_t i;

    ZXN_WRITE_REG(REG_SPRITE_TRANSPARENCY_INDEX, TRANSPARENT_BYTE);

    /* Sprites-over-border (NextReg $15 bit 1) -- without this, the sprite
     * clip window is specified in ULA-screen coordinates, which start 32
     * sprite-space pixels in from sprite coordinate (0,0) (zxnext_notes.pdf,
     * Register $19 section: "sprite position is always in sprite
     * coordinates with 32,32 corresponding to the top left corner of the
     * ULA screen" -- confirmed in docs/hwsprite-notes.md's own M0 spike).
     * A clip window of [0,255] in THAT space only covers sprite-space
     * 32-287 -- any sprite at a raw sprite-X or sprite-Y below 32 (e.g. a
     * player near the world's own left/top edge, using this project's
     * full-0-319-sprite-space viewport, not a narrower ULA-matching one)
     * gets silently clipped -- invisible, NOT mispositioned, which read
     * as "the character disappears" during Islander's own M1 testing,
     * not a collision bug (confirmed by reading back the sprite's actual
     * stored attributes via ZRCP -- correctly positioned and visible=1,
     * just clipped). Over-border mode moves the clip window's own origin
     * to the sprite coordinate origin directly (same section: "the
     * X coords are internally doubled and the clip window origin is
     * moved to the sprite origin inside the border"), so [0,255] below
     * covers sprite-space X 0-510 and Y 0-255 -- comfortably covers this
     * project's full tilemap-viewport range (320x256) either way. */
    {
        uint8_t prev = ZXN_READ_REG(REG_SPRITE_LAYER_SYSTEM);
        ZXN_WRITE_REG(REG_SPRITE_LAYER_SYSTEM, (uint8_t)(prev | RSLS_SPRITES_OVER_BORDER));
    }

    /* Full-range sprite clip window (not the cold-boot default, which
     * dark-room-next's docs/sprite-notes.md never needed resetting but
     * this project's own M0 spike reset defensively and kept -- cheap
     * insurance against a stale index from the tilemap's own clip-window
     * use, if any future code starts using that). */
    ZXN_WRITE_REG(REG_CLIP_WINDOW_CONTROL, RCWC_RESET_SPRITE_CLIP_INDEX);
    ZXN_WRITE_REG(REG_CLIP_WINDOW_SPRITES, 0);
    ZXN_WRITE_REG(REG_CLIP_WINDOW_SPRITES, 255);
    ZXN_WRITE_REG(REG_CLIP_WINDOW_SPRITES, 0);
    ZXN_WRITE_REG(REG_CLIP_WINDOW_SPRITES, 255);

    /* Identity sprite palette: slot N -> raw RGB332 byte N. Pattern bytes
     * written by pico_hwsprite_set_pattern() are themselves already real
     * RGB332 values (via pico8_to_rgb332() above), so this makes the
     * sprite's own palette indirection a pure passthrough -- same
     * approach next-resources/zxnext_level9's sprite_set_default_palette()
     * uses, confirmed necessary by direct M0 measurement (docs/
     * hwsprite-notes.md), not assumed from "no palette needed" claims
     * that turned out not to be the actual blocker there. */
    {
        uint8_t prev_pal_ctrl = ZXN_READ_REG(REG_PALETTE_CONTROL);
        ZXN_WRITE_REG(REG_PALETTE_CONTROL, (uint8_t)((prev_pal_ctrl & 0x8F) | RPC_SELECT_SPRITES_PALETTE_0));
        ZXN_WRITE_REG(REG_PALETTE_INDEX, 0);
        for (i = 0; i < 256; i++) ZXN_WRITE_REG(REG_PALETTE_VALUE_8, (uint8_t)i);
    }
}

/* Writes one pattern byte for a given Pico-8 colour index -- TRANSPARENT_BYTE
 * for colour 0 or `extra_transparent`, the real RGB332 colour otherwise.
 * Deliberately 3 independent if/else branches, NOT `(a == b || a == c) ?
 * x : y` -- found the hard way (2026-10-07, Islander's own 16x16 player
 * art): z88dk/sccz80 -O3 miscompiles that exact shape when one side of
 * the `||` is a direct zero-test (`and a`/`jp z`, which clears the carry
 * flag) and the other is a runtime equality call (`l_eq`, which sets
 * carry on a match) -- the generated code reuses a SHARED `jp nc,...`
 * off the end to decide the ternary, but the colour==0 path left the
 * carry flag in the zero-test's own leftover state (always clear, i.e.
 * "false") instead of the logical-OR's true result, silently writing
 * the REAL colour (0x00 for Pico-8 colour 0) instead of the transparent
 * byte. The colour==extra_transparent path happened to work (l_eq's own
 * carry state survives untouched to the final jump), which is exactly
 * why this went unnoticed until something actually needed colour-0
 * transparency WITHIN a sprite's own body, not just a margin. Confirmed
 * by direct disassembly (zcc -a) and a ZRCP sprite-pattern readback
 * (tbblue-get-pattern) showing literal 0x00 where TRANSPARENT_BYTE was
 * expected. Independent if/else branches each compile to their own
 * direct conditional jump with no shared/reused flag state, sidestepping
 * the bug entirely -- if future code hits the same symptom (a sprite's
 * background rendering as opaque black instead of transparent) from a
 * `(a==b||a==c)?x:y` shape, suspect this exact compiler issue again. */
static uint8_t transparent_or_rgb(uint8_t pico8_color, uint8_t extra_transparent) {
    if (pico8_color == 0) return TRANSPARENT_BYTE;
    if (pico8_color == extra_transparent) return TRANSPARENT_BYTE;
    return pico8_to_rgb332(pico8_color);
}

void pico_hwsprite_set_pattern(uint8_t slot, const uint8_t *sheet, uint16_t sheet_w,
                                uint16_t sheet_x, uint16_t sheet_y, uint8_t extra_transparent) {
    uint8_t row, col;

    IO_SPRITE_SLOT = (uint8_t)(slot & SPRITE_PATTERN_MASK);
    for (row = 0; row < 16; row++) {
        for (col = 0; col < 16; col++) {
            if (row < 4 || row >= 12 || col < 4 || col >= 12) {
                IO_SPRITE_PATTERN = TRANSPARENT_BYTE;
            } else {
                uint16_t sx = sheet_x + (col - 4);
                uint16_t sy = sheet_y + (row - 4);
                uint8_t pico8_color = sheet[(uint32_t)sy * sheet_w + sx];
                IO_SPRITE_PATTERN = transparent_or_rgb(pico8_color, extra_transparent);
            }
        }
    }
}

void pico_hwsprite_set_pattern16(uint8_t slot, const uint8_t *sheet, uint16_t sheet_w,
                                  uint16_t sheet_x, uint16_t sheet_y, uint8_t extra_transparent) {
    uint8_t row, col;

    IO_SPRITE_SLOT = (uint8_t)(slot & SPRITE_PATTERN_MASK);
    for (row = 0; row < 16; row++) {
        for (col = 0; col < 16; col++) {
            uint16_t sx = sheet_x + col;
            uint16_t sy = sheet_y + row;
            uint8_t pico8_color = sheet[(uint32_t)sy * sheet_w + sx];
            IO_SPRITE_PATTERN = transparent_or_rgb(pico8_color, extra_transparent);
        }
    }
}

void pico_hwsprite_set_pattern_solid(uint8_t slot, uint8_t pico8_color, uint8_t size) {
    uint8_t row, col, lo, hi;
    uint8_t rgb = pico8_to_rgb332(pico8_color);

    if (size > 16) size = 16;
    lo = (uint8_t)((16 - size) / 2);
    hi = (uint8_t)(lo + size);

    IO_SPRITE_SLOT = (uint8_t)(slot & SPRITE_PATTERN_MASK);
    for (row = 0; row < 16; row++) {
        for (col = 0; col < 16; col++) {
            IO_SPRITE_PATTERN = (row >= lo && row < hi && col >= lo && col < hi)
                ? rgb : TRANSPARENT_BYTE;
        }
    }
}

#define SPRITE_MIRROR_X 0x08 /* attribute byte 2 bit 3 (zxnext_notes.pdf 2.5.3: "P P P P XM YM R X8") */

#define SPRITE_MAG_MASK 0x03 /* attribute 4's XX/YY fields are each 2 bits */

static void hwsprite_set_impl(uint8_t slot, uint8_t pattern, int16_t x, int16_t y,
                               uint8_t visible, uint8_t flip_x, uint8_t margin, uint8_t mag) {
    uint8_t attr2, attr3, attr4;
    /* -margin,-margin: pico_hwsprite_set_pattern() centres the visible 8x8
     * content with a 4px margin inside the hardware's fixed 16x16 pattern,
     * so the pattern's own (0,0) -- what the raw attribute x/y below
     * addresses -- is 4px up-left of the visible graphic at 1x. Correcting
     * it HERE, once, means x/y means what every caller actually wants (the
     * visible content's own top-left, same convention as the tilemap's
     * col*8/row*8 and pico_spr()'s own x/y) rather than a footgun every
     * call site has to remember. Found the hard way in Islander's own M1:
     * the player's collision box was built against the logical
     * (uncorrected) position, so it sat 4px up-left of the sprite you
     * could actually see -- able to visually overlap obstacles when
     * moving down/right (collision trailed behind the graphic) and
     * stopping visually short when moving up/left (collision led ahead of
     * the graphic), exactly the asymmetry real play surfaced before this
     * fix existed. At mag>0 the hardware scales the WHOLE pattern
     * (margin included) outward from its own (0,0), so the margin's own
     * on-screen size scales by the same factor too -- callers that combine
     * a margin with mag>0 must pre-scale it themselves (pico_hwsprite_set_
     * mag() below does `4 << mag`); pico_hwsprite_set_pattern16()'s own
     * patterns fill the full 16x16 canvas, margin=0, unaffected by mag. */
    x = (int16_t)(x - margin);
    y = (int16_t)(y - margin);

    attr2 = MSB(x);
    if (flip_x) attr2 |= SPRITE_MIRROR_X;

    attr3 = (uint8_t)(pattern & SPRITE_PATTERN_MASK);
    attr3 |= SPRITE_EXTENDED_MASK;
    if (visible) attr3 |= SPRITE_VISIBLE_MASK;

    /* Attribute 4 (standard anchor sprite, zxnext_notes.pdf 2.5.4): H N6 T
     * XX YY Y8, with H/N6/T=0 (8-bit pattern, not a relative sprite). XX
     * and YY are each `mag` here -- no caller has needed independent X/Y
     * scaling yet. */
    attr4 = (uint8_t)(((mag & SPRITE_MAG_MASK) << 3) | ((mag & SPRITE_MAG_MASK) << 1) | MSB(y));

    IO_SPRITE_SLOT = (uint8_t)(slot & SPRITE_INDEX_MASK);
    IO_SPRITE_ATTRIBUTE = LSB(x);
    IO_SPRITE_ATTRIBUTE = LSB(y);
    IO_SPRITE_ATTRIBUTE = attr2;
    IO_SPRITE_ATTRIBUTE = attr3;
    IO_SPRITE_ATTRIBUTE = attr4;
}

void pico_hwsprite_set(uint8_t slot, uint8_t pattern, int16_t x, int16_t y,
                        uint8_t visible, uint8_t flip_x) {
    hwsprite_set_impl(slot, pattern, x, y, visible, flip_x, 4, 0);
}

void pico_hwsprite_set_mag(uint8_t slot, uint8_t pattern, int16_t x, int16_t y,
                            uint8_t visible, uint8_t flip_x, uint8_t mag) {
    hwsprite_set_impl(slot, pattern, x, y, visible, flip_x, (uint8_t)(4 << mag), mag);
}

void pico_hwsprite_set16(uint8_t slot, uint8_t pattern, int16_t x, int16_t y,
                          uint8_t visible, uint8_t flip_x) {
    hwsprite_set_impl(slot, pattern, x, y, visible, flip_x, 0, 0);
}

void pico_hwsprite_hide(uint8_t slot) {
    IO_SPRITE_SLOT = (uint8_t)(slot & SPRITE_INDEX_MASK);
    IO_SPRITE_ATTRIBUTE = 0;
    IO_SPRITE_ATTRIBUTE = 0;
    IO_SPRITE_ATTRIBUTE = 0;
    IO_SPRITE_ATTRIBUTE = (uint8_t)(SPRITE_EXTENDED_MASK | (slot & SPRITE_PATTERN_MASK)); /* visible bit clear */
    IO_SPRITE_ATTRIBUTE = 0;
}
