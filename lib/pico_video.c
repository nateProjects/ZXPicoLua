/* Layer 2 access: MMU slot 2 ($4000-$5FFF) is used as a temporary 8K
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
 * slot 1 = $51), so slot 2 is $52. Same approach used here. */

#include <arch/zxn.h>
#include <string.h>
#include "pico_video.h"

#define MMU_SLOT_2_REG 0x52

#define LAYER2_WINDOW_ADDR ((uint8_t *)0x4000)
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

static uint8_t layer2_base_page(void) {
    /* REG_LAYER_2_RAM_BANK is 16K-granular; a physical page is 8K. */
    return ZXN_READ_REG(REG_LAYER_2_RAM_BANK) << 1;
}

static uint8_t mmu2_page_in(uint8_t physical_page) {
    uint8_t prev = ZXN_READ_REG(MMU_SLOT_2_REG);
    ZXN_WRITE_REG(MMU_SLOT_2_REG, physical_page);
    return prev;
}

static void mmu2_page_out(uint8_t previous_page) {
    ZXN_WRITE_REG(MMU_SLOT_2_REG, previous_page);
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

    pico_screen_w = logical_w;
    pico_screen_h = logical_h;
    origin_x = (uint8_t)((PICO_PHYS_W - logical_w) / 2);
    origin_y = (uint8_t)((PICO_PHYS_H - logical_h) / 2);

    pico_cls(0);

    IO_LAYER_2_CONFIG = IL2C_SHOW_LAYER_2;
}

void pico_cls(uint8_t color) {
    uint8_t base = layer2_base_page();
    uint8_t prev = mmu2_page_in(base);

    memset(LAYER2_WINDOW_ADDR, color, LAYER2_WINDOW_SIZE);
    for (uint8_t i = 1; i < LAYER2_PAGES; i++) {
        ZXN_WRITE_REG(MMU_SLOT_2_REG, base + i);
        memset(LAYER2_WINDOW_ADDR, color, LAYER2_WINDOW_SIZE);
    }

    mmu2_page_out(prev);
}

void pico_pset(uint8_t x, uint8_t y, uint8_t color) {
    uint16_t offset = (uint16_t)(origin_y + y) * PICO_PHYS_W + (origin_x + x);
    uint8_t base = layer2_base_page();
    uint8_t prev = mmu2_page_in(base + (offset >> 13));

    *((uint8_t *)(0x4000 + (offset & 0x1FFF))) = color;

    mmu2_page_out(prev);
}

uint8_t pico_pget(uint8_t x, uint8_t y) {
    uint16_t offset = (uint16_t)(origin_y + y) * PICO_PHYS_W + (origin_x + x);
    uint8_t base = layer2_base_page();
    uint8_t prev = mmu2_page_in(base + (offset >> 13));
    uint8_t value;

    value = *((uint8_t *)(0x4000 + (offset & 0x1FFF)));

    mmu2_page_out(prev);
    return value;
}
