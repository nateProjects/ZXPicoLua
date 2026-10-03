/* Layer 2 access: MMU slot 3 ($6000-$7FFF) is used as a temporary 8K
 * window onto Layer 2 RAM, paged in 8K steps -- the same mechanism proven
 * in two real shipped z88dk Next games (next-resources/code/
 * zxnext_level9-master/src/layer2.c and brahman-master-zxnext_magnetic's
 * layer2.c), adapted here from their 320x256 (10 pages) to this project's
 * classic 256x192 mode (6 pages = 256*192 bytes / 8192). See
 * docs/layer2-notes.md for the M0 spike that confirmed this against the
 * actual target resolution.
 *
 * zxn.h declares ZXN_WRITE_MMU2()/ZXN_READ_MMU2() helpers, but those
 * symbols are not present in this project's -clib=classic library build
 * (confirmed by a link failure -- "undefined symbol: ZXN_WRITE_MMU2";
 * the two reference games above both use -clib=sdcc_iy instead, which
 * does have them). dark-room-next's bank.c hit the same kind of gap and
 * worked around it by writing the MMU slot's NextReg directly via
 * ZXN_WRITE_REG/ZXN_READ_REG (proven working there) rather than a
 * slot-specific helper -- MMU slot N's NextReg is $50+N (bank.h confirms
 * slot 1 = $51). Same approach used here, for slot 3 ($53).
 *
 * Moved from slot 2 ($4000-$5FFF) to slot 3 ($6000-$7FFF) -- slot 2
 * overlaps the classic ZX ROM's system variables area ($5B00-$5CFF, the
 * same range dark-room-next's docs/tilemap-notes.md already flagged as
 * hazardous), clear of this project's own resident code too (which starts
 * at $8000 per z88dk classic clib convention, per bank.h's own comment).
 * This alone turned out NOT to be sufficient, though -- see the DI/EI
 * note below, and docs/layer2-notes.md's "stray pixels" postmortem for
 * the full investigation (confirmed, by reading the actual framebuffer
 * bytes at the affected coordinates, that a handful of stray, intermittent
 * single-pixel anomalies in M0/M1 screenshots were genuine data
 * corruption, not a ZEsarUX rendering artifact as first guessed).
 *
 * CRITICAL: every access through this window disables interrupts for its
 * duration (intrinsic_di()/intrinsic_ei() around mmu_window_page_in() ..
 * mmu_window_page_out()). Moving to slot 3 alone did not stop the
 * corruption in testing -- something (the ROM's 50Hz interrupt handler,
 * almost certainly, though its exact reach beyond $5B00-$5CFF wasn't
 * pinned down further) can still write into this window's address range
 * while a Layer 2 physical page is mapped there, regardless of which
 * slot/address range is used. Disabling interrupts for the critical
 * section is the actual, slot-independent fix -- confirmed via a full
 * 48KB memory dump of every physical page after a long stress run (see
 * docs/layer2-notes.md), not just a screenshot spot-check. Safe here
 * because this project runs with interrupts enabled throughout its whole
 * lifetime from one intrinsic_ei() at startup (main.c) -- these are brief,
 * non-nested critical sections, not a general-purpose DI/EI stack. */

#include <arch/zxn.h>
#include <intrinsic.h>
#include <string.h>
#include "pico_video.h"

#define MMU_SLOT_3_REG 0x53

#define LAYER2_WINDOW_ADDR ((uint8_t *)0x6000)
#define LAYER2_WINDOW_SIZE 0x2000
#define LAYER2_PAGES 6 /* 256*192 = 49152 bytes = 6 * 8192 */

uint16_t pico_screen_w;
uint16_t pico_screen_h;

/* Logical-canvas origin within the physical surface -- see pico_video.h's
 * "logical canvas" comment. Centred, so a pixel at logical (x,y) lands at
 * physical (origin_x+x, origin_y+y). Always fits a uint8_t: origin+logical
 * size never exceeds PICO_PHYS_W/H by construction (pico_video_init()
 * computes it as (PHYS - logical) / 2). */
static uint8_t origin_x;
static uint8_t origin_y;

/* pal()'s draw-time colour remap table -- see pico_pal()'s own comment.
 * Identity by default (pal_table[c] == c), reset in pico_video_init(). */
static uint8_t pal_table[16];

/* Double buffering: pico_pset()/pico_pget()/pico_cls() always act on the
 * SHADOW bank (REG_LAYER_2_SHADOW_RAM_BANK) -- never the displayed MAIN
 * bank (REG_LAYER_2_RAM_BANK) -- so drawing never touches memory the
 * display's own scanout is simultaneously reading. pico_flip() swaps the
 * two registers' values, presenting everything just drawn and handing
 * back a fresh (but two-flips-stale) buffer to draw into next. Added
 * after real human testing (see docs/input-notes.md's "double buffering"
 * section) showed a moving sprite visibly tearing even at 14MHz -- raw
 * speed only narrows the window a redraw can be caught mid-flight in,
 * double buffering is what actually closes it, the same way every real
 * framebuffer-based game (Pico-8 itself included) avoids this. */
static uint8_t layer2_base_page(void) {
    /* REG_LAYER_2_SHADOW_RAM_BANK is 16K-granular; a physical page is 8K. */
    return ZXN_READ_REG(REG_LAYER_2_SHADOW_RAM_BANK) << 1;
}

static uint8_t mmu_window_page_in(uint8_t physical_page) {
    uint8_t prev = ZXN_READ_REG(MMU_SLOT_3_REG);
    ZXN_WRITE_REG(MMU_SLOT_3_REG, physical_page);
    return prev;
}

static void mmu_window_page_out(uint8_t previous_page) {
    ZXN_WRITE_REG(MMU_SLOT_3_REG, previous_page);
}

void pico_video_init(uint16_t logical_w, uint16_t logical_h) {
    /* NextReg $70 (Layer 2 Control Register,
     * https://wiki.specnext.dev/Layer_2_Control_Register) bit 4 selects
     * the 320x256 8bpp mode -- confirmed by both
     * next-resources/code/operation-jeff-main/src/screen.c and
     * zxnext_level9-master/src/main.c, which each write 0x10 for that
     * mode. Left at its cold-boot default (0 = classic 256x192 8bpp)
     * here; confirmed via M0 spike (docs/layer2-notes.md) that default
     * holds under --tbblue-fast-boot-mode too. */

    /* The Next boots at classic-compatible 3.5MHz and stays there unless
     * asked otherwise. Every pico_pset() pays real per-pixel overhead
     * (an interrupt-disable/enable pair plus two NextReg port accesses
     * for the MMU page switch, on top of the actual byte write) --
     * observed (M2.5 human testing, see docs/input-notes.md) as visible
     * tearing on a moving sprite at the default speed, consistent with a
     * frame's worth of pixel writes still being in flight when the
     * display's own scanout catches up to them. Requesting the Next's
     * fastest documented-in-this-z88dk-checkout speed (14MHz, 4x
     * headroom) here, once, is the cheapest possible first thing to try
     * before restructuring the draw path itself. */
    ZXN_WRITE_REG(REG_TURBO_MODE, RTM_14MHZ);

    pico_screen_w = logical_w;
    pico_screen_h = logical_h;
    origin_x = (uint8_t)((PICO_PHYS_W - logical_w) / 2);
    origin_y = (uint8_t)((PICO_PHYS_H - logical_h) / 2);
    pico_pal_reset();

    /* Allocate the shadow bank explicitly, 3 banks (one full 256x192
     * buffer, LAYER2_PAGES/2 16K banks) past whatever the main bank
     * already is -- not relying on any default for it (unlike the main
     * bank, left at its own cold-boot default, confirmed safe to do in
     * the M0 spike). zxnext_level9-master/src/main.c is the proven
     * precedent for this exact pattern: it explicitly sets both
     * layer2_set_main_screen_bank()/layer2_set_shadow_screen_bank()
     * rather than trusting defaults for either. We only follow that for
     * the shadow bank, since the main bank's own default has already
     * been proven correct by every prior milestone -- changing it too
     * would be an untested, unnecessary risk alongside the real change
     * here. */
    {
        uint8_t main_bank_16k = ZXN_READ_REG(REG_LAYER_2_RAM_BANK);
        ZXN_WRITE_REG(REG_LAYER_2_SHADOW_RAM_BANK, main_bank_16k + (LAYER2_PAGES / 2));
    }

    /* Clear the shadow (back) buffer, flip it to the front so its clean
     * content is what's actually displayed, then clear the new shadow
     * (the old, never-cleared main buffer) too -- leaves both buffers
     * blank, with the ORIGINAL main bank back in front, matching every
     * prior milestone's behaviour (nothing about which physical bank
     * ends up visible changes here). Callers that want initial content
     * visible from the first frame must draw it, pico_flip(), then draw
     * it again into the new back buffer -- see games/demo/main.c for the
     * pattern; not automated here since not every game needs the same
     * initial content priming. */
    pico_cls(0);
    pico_flip();
    pico_cls(0);

    IO_LAYER_2_CONFIG = IL2C_SHOW_LAYER_2;
}

/* pal(c0, c1) -- remaps colour c0 to c1 for every subsequent draw call,
 * until reset. Ported from PicoJS's picoJS.pal (picoJS.js ~line 758),
 * minus the p=1 "screen palette" mode (a whole-framebuffer remap at flip
 * time) -- PicoJS itself doesn't implement that either (see its own
 * comment there), and no reference game needs it. Only remaps the
 * standard 0-15 Pico-8 colour range -- the Next's native 16-255 range
 * (this project's own extension beyond stock Pico-8, see the plan doc's
 * palette design) is left untouched, matching that c0/c1 are Pico-8
 * colour indices, not arbitrary Next ones.
 *
 * Applied centrally inside pico_pset() (below), not duplicated in every
 * caller -- every other primitive (pico_gfx.c's shapes, pico_sprite.c's
 * blits, pico_font.c's glyphs) already funnels every actual pixel write
 * through pico_pset(), so this one change point is all that's needed for
 * pal() to correctly affect sprites too, matching real Pico-8 (pal() is
 * often used for sprite palette-swap effects). */
void pico_pal(uint8_t c0, uint8_t c1) {
    if (c0 < 16 && c1 < 16) pal_table[c0] = c1;
}

void pico_pal_reset(void) {
    uint8_t i;
    for (i = 0; i < 16; i++) pal_table[i] = i;
}

void pico_flip(void) {
    uint8_t main_bank = ZXN_READ_REG(REG_LAYER_2_RAM_BANK);
    uint8_t shadow_bank = ZXN_READ_REG(REG_LAYER_2_SHADOW_RAM_BANK);
    ZXN_WRITE_REG(REG_LAYER_2_RAM_BANK, shadow_bank);
    ZXN_WRITE_REG(REG_LAYER_2_SHADOW_RAM_BANK, main_bank);
}

void pico_cls(uint8_t color) {
    uint8_t base = layer2_base_page();
    uint8_t prev;

    /* cls() writes raw bytes via memset, not pico_pset() -- unlike every
     * other primitive, so it needs its own pal() remap applied here
     * explicitly (PicoJS's own cls() goes through the same draw-palette
     * remap as everything else, picoJS.js's resolveColour()). */
    if (color < 16) color = pal_table[color];

    intrinsic_di();
    prev = mmu_window_page_in(base);
    memset(LAYER2_WINDOW_ADDR, color, LAYER2_WINDOW_SIZE);
    for (uint8_t i = 1; i < LAYER2_PAGES; i++) {
        ZXN_WRITE_REG(MMU_SLOT_3_REG, base + i);
        memset(LAYER2_WINDOW_ADDR, color, LAYER2_WINDOW_SIZE);
    }
    mmu_window_page_out(prev);
    intrinsic_ei();
}

void pico_pset(uint8_t x, uint8_t y, uint8_t color) {
    uint16_t offset = (uint16_t)(origin_y + y) * PICO_PHYS_W + (origin_x + x);
    uint8_t base = layer2_base_page();
    uint8_t prev;

    if (color < 16) color = pal_table[color];

    intrinsic_di();
    prev = mmu_window_page_in(base + (offset >> 13));
    *((uint8_t *)(0x6000 + (offset & 0x1FFF))) = color;
    mmu_window_page_out(prev);
    intrinsic_ei();
}

uint8_t pico_pget(uint8_t x, uint8_t y) {
    uint16_t offset = (uint16_t)(origin_y + y) * PICO_PHYS_W + (origin_x + x);
    uint8_t base = layer2_base_page();
    uint8_t prev;
    uint8_t value;

    intrinsic_di();
    prev = mmu_window_page_in(base + (offset >> 13));
    value = *((uint8_t *)(0x6000 + (offset & 0x1FFF)));
    mmu_window_page_out(prev);
    intrinsic_ei();

    return value;
}
