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
#include "pico_palette.h"

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

/* Clears the WHOLE physical 256x192 surface, letterbox border included --
 * used only by pico_video_init() to blank both buffers once at startup.
 * pico_cls() (below) deliberately does NOT do this every call -- see its
 * own comment. */
static void cls_physical_full(uint8_t color) {
    uint8_t base = layer2_base_page();
    uint8_t prev;
    uint8_t i;

    intrinsic_di();
    prev = mmu_window_page_in(base);
    memset(LAYER2_WINDOW_ADDR, color, LAYER2_WINDOW_SIZE);
    for (i = 1; i < LAYER2_PAGES; i++) {
        ZXN_WRITE_REG(MMU_SLOT_3_REG, base + i);
        memset(LAYER2_WINDOW_ADDR, color, LAYER2_WINDOW_SIZE);
    }
    mmu_window_page_out(prev);
    intrinsic_ei();
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
     * asked otherwise; every pico_pset() pays real per-pixel overhead
     * (DI/EI plus NextReg port accesses for the MMU page switch) that
     * made this matter in practice -- first as visible tearing at the
     * default speed (M2.5 human testing, docs/input-notes.md), then as
     * multi-second single-frame draws once M4's picodiver port exercised
     * map()/spr() at real scale (docs/picodiver-notes.md).
     *
     * Tried 28MHz (raw register value 3 -- this z88dk checkout's zxn.h
     * doesn't name a constant past RTM_14MHZ) at the user's suggestion,
     * with and without also disabling memory contention
     * (REG_PERIPHERAL_3/RP3_DISABLE_CONTENTION). Both variants broke
     * keyboard input -- a key held via ZRCP's deterministic
     * set-ui-io-ports (the same method that reliably worked throughout
     * M2.5) no longer registered, even held for several seconds, while
     * the exact same test against a 14MHz build transitioned correctly.
     * Narrowed to the speed change itself, not contention (both with and
     * without RP3_DISABLE_CONTENTION failed identically) -- root cause
     * not yet understood (pico_input.c reads the keyboard matrix via
     * in_KeyPressed(), an instantaneous read with no debounce timing that
     * should obviously depend on, so why raw CPU speed would affect it
     * isn't yet explained). Reverted to the proven-reliable 14MHz rather
     * than ship something with broken input -- see docs/picodiver-notes.md
     * for the full writeup; worth revisiting if more speed is needed. */
    ZXN_WRITE_REG(REG_TURBO_MODE, 0x03); /* 28MHz -- see comment above; re-confirmed keyboard input still works via ZRCP's set-ui-io-ports for mode transitions (X/O), though this session's own movement-key (LEFT/RIGHT/UP/DOWN) testing via the same tool proved unreliable at BOTH 14MHz and 28MHz, undermining confidence that the earlier 28MHz revert was really about clock speed rather than a ZRCP testing-tool limitation -- see docs/picodiver-notes.md. */

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
    cls_physical_full(0);
    pico_flip();
    cls_physical_full(0);

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

/* Clears a sub-rectangle of the logical canvas -- pico_cls() (below) is
 * just this called over the whole canvas, which is also why it needs no
 * separate implementation of its own. Added for picodiver's gameplay
 * screen, which needs to clear/redraw its "sky" strip (the scrolling
 * ship, y<32) every frame while leaving the ocean below untouched
 * (handled instead by targeted per-sprite erase, docs/picodiver-notes.md)
 * -- clearing the whole canvas every frame would wipe the ocean too,
 * forcing a full 192-tile redraw right back on top of it.
 *
 * A row (pico_screen_w bytes starting at origin_x) always fits inside one
 * physical page -- PICO_PHYS_W (256) divides LAYER2_WINDOW_SIZE (8192)
 * evenly -- so each row needs at most one MMU page select, same property
 * this exploited before being generalised from a canvas-only pico_cls(). */
void pico_cls_rect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color) {
    uint8_t prev;
    uint16_t y;
    uint16_t w; /* NOT uint8_t -- a full-width clear (x0=0, x1=255) needs
                 * w=256, which overflows uint8_t to 0, silently turning
                 * the whole call into a no-op memset(...,0) (confirmed
                 * directly, not guessed: this is exactly why Stage 10's
                 * full-screen clears -- the title->game one-time wipe,
                 * the icon HUD clear, the ship banner's own clear, all
                 * x0=0/x1=SCREEN_W-1 -- silently did nothing, leaving
                 * stale content on screen. Never surfaced before Stage
                 * 10 because no earlier game's logical canvas was ever
                 * wider than 128, so w never got anywhere near 256). */

    if (x1 >= pico_screen_w) x1 = (uint8_t)(pico_screen_w - 1);
    if (y1 >= pico_screen_h) y1 = (uint8_t)(pico_screen_h - 1);
    if (x0 > x1 || y0 > y1) return;
    w = (uint16_t)(x1 - x0 + 1);

    /* cls writes raw bytes via memset, not pico_pset() -- unlike every
     * other primitive, so it needs its own pal() remap applied here
     * explicitly (PicoJS's own cls() goes through the same draw-palette
     * remap as everything else, picoJS.js's resolveColour()). */
    if (color < 16) color = pal_table[color];

    /* Goes through the shared batch session (pico_video_batch_begin()/
     * pico_video_batch_row()/pico_video_batch_end()) instead of its own
     * separate DI/EI + raw MMU writes, for two reasons, both confirmed by
     * direct ZRCP frame-counter measurement, not assumed:
     * (1) pico_video_batch_row() already only re-selects the MMU page
     * when it actually changes between rows -- a physical page is 32
     * rows tall (LAYER2_WINDOW_SIZE / PICO_PHYS_W), so a full 128-row
     * clear only crosses a page boundary 4 times, not 128; this used to
     * write the page unconditionally every row, pure redundant NextReg
     * port I/O.
     * (2) going through the NESTABLE batch session (pico_video.h's own
     * comment) means a caller that wraps a whole frame's pico_cls() +
     * several pico_spr() calls in one outer pico_video_batch_begin()/
     * end() pair pays DI/EI once for the entire frame, not once for the
     * clear and again for every sprite -- picodiver's own creature/HUD-
     * icon-heavy frames were the case that made this matter in practice
     * (docs/picodiver-notes.md). */
    prev = pico_video_batch_begin();
    for (y = y0; y <= y1; y++) {
        memset(pico_video_batch_row(x0, (uint8_t)y), color, w);
    }
    pico_video_batch_end(prev);
}

void pico_cls(uint8_t color) {
    pico_cls_rect(0, 0, (uint8_t)(pico_screen_w - 1), (uint8_t)(pico_screen_h - 1), color);
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

/* --- batch-write session ---------------------------------------------
 *
 * Added for M4: profiling picodiver's real map()/spr()-heavy scenes
 * found pico_pset()'s per-pixel cost (DI/EI, a layer2_base_page() port
 * read, and an MMU page-select port write, ALL repeated for every single
 * pixel) was the dominant cost once pico_sspr()'s needless scaling
 * divide was already removed -- a 16x12-tile map() draw still took
 * multiple seconds. An 8x8 sprite/tile's destination pixels usually fall
 * in the SAME physical page (pages are 32 rows tall) -- re-selecting it
 * 64 times for one tile is pure waste. This session API resolves the
 * base page and disables interrupts ONCE for a whole batch of pixels
 * (pico_sprite.c's pico_sspr() is the first user), only re-writing the
 * MMU register when the page actually changes between pixels, which for
 * a single on-screen tile is rare (at most once, at a page boundary).
 *
 * Interrupts stay disabled for the WHOLE batch, not per pixel -- keep a
 * batch to one sprite/tile's worth of pixels (not, say, an entire frame)
 * for the same reason pico_pset()'s own critical section has to stay
 * brief (docs/layer2-notes.md's "RESOLVED" section: an interrupt handler
 * can corrupt whatever page is paged in if it fires mid-access; a long
 * batch means a long window where that can't be preempted at all).
 *
 * NESTING: begin/end calls nest (a plain depth counter) -- the outermost
 * begin() is the only one that actually does DI + resolves the base
 * page, and the outermost end() is the only one that actually restores
 * the page and re-enables interrupts; everything in between is a no-op
 * depth change. This lets a caller explicitly wrap SEVERAL pico_spr()
 * calls (each of which already calls begin/end once internally) in one
 * outer begin/end pair, turning N separate DI/EI pairs into 1 -- added
 * after direct measurement found picodiver's own per-frame cost scaling
 * with sprite-CALL count, not just total pixels (docs/picodiver-notes.md):
 * cab_show()'s own 2 small 8x8 blits cost nearly as much as pico_cls()'s
 * entire 128-row clear, which per-row page-skipping alone didn't explain.
 * Safe to nest arbitrarily deep -- unlike the single-sprite case this
 * deliberately extends the "interrupts off" window to a whole frame's
 * worth of sprites when a caller opts into it (same order of magnitude
 * pico_cls_rect()'s own already-single-DI-span 128 rows), not a new kind
 * of risk, just a larger version of one already accepted elsewhere. */
static uint8_t batch_base_page;
static uint8_t batch_current_page;
static uint8_t batch_depth;
static uint8_t batch_saved_page;

uint8_t pico_video_batch_begin(void) {
    if (batch_depth == 0) {
        intrinsic_di();
        batch_base_page = layer2_base_page();
        batch_current_page = 0xFF; /* sentinel: no page selected yet this batch */
        batch_saved_page = ZXN_READ_REG(MMU_SLOT_3_REG);
    }
    batch_depth++;
    return batch_saved_page;
}

void pico_video_batch_pset(uint8_t x, uint8_t y, uint8_t color) {
    uint16_t offset = (uint16_t)(origin_y + y) * PICO_PHYS_W + (origin_x + x);
    uint8_t page = batch_base_page + (offset >> 13);

    if (color < 16) color = pal_table[color];

    if (page != batch_current_page) {
        ZXN_WRITE_REG(MMU_SLOT_3_REG, page);
        batch_current_page = page;
    }
    *((uint8_t *)(0x6000 + (offset & 0x1FFF))) = color;
}

void pico_video_batch_end(uint8_t previous_page) {
    (void)previous_page; /* only meaningful at the outermost nesting level -- see batch_saved_page */
    if (batch_depth > 0) batch_depth--;
    if (batch_depth == 0) {
        ZXN_WRITE_REG(MMU_SLOT_3_REG, batch_saved_page);
        intrinsic_ei();
    }
}

/* Row-granular batch access, added when profiling picodiver's sprite/map
 * blits found pico_video_batch_pset()'s per-PIXEL cost was itself still
 * the bottleneck even after the page-resolution batching above -- not the
 * MMU logic, but sccz80's codegen for the two function calls (sheet_pixel()
 * + batch_pset_safe()) a naive per-pixel loop makes: every call re-spills
 * all locals through stack-relative addressing (confirmed by reading the
 * generated .asm, not assumed), dwarfing the handful of real T-states the
 * MMU page switch and pixel write actually need.
 *
 * A Layer 2 row is PICO_PHYS_W (256) bytes and a physical page is
 * LAYER2_WINDOW_SIZE (8192) bytes -- exactly 32 rows -- so EVERY possible
 * (origin_y+y) row lies entirely inside one page; incrementing the pointer
 * this returns can never cross a page boundary while y stays fixed. This
 * lets a blit loop resolve the destination page/pointer ONCE per row
 * (not once per pixel) and then walk a raw uint8_t* across the row with
 * plain pointer arithmetic -- no further calls, no further MMU writes. */
uint8_t *pico_video_batch_row(uint8_t x, uint8_t y) {
    uint16_t offset = (uint16_t)(origin_y + y) * PICO_PHYS_W + (origin_x + x);
    uint8_t page = batch_base_page + (offset >> 13);

    if (page != batch_current_page) {
        ZXN_WRITE_REG(MMU_SLOT_3_REG, page);
        batch_current_page = page;
    }
    return (uint8_t *)(0x6000 + (offset & 0x1FFF));
}

/* Exposes pal()'s remap table so a tight blit loop (pico_sprite.c) can
 * inline the lookup instead of paying a function-call-per-pixel for it. */
uint8_t pico_video_remap(uint8_t color) {
    return (color < 16) ? pal_table[color] : color;
}

/* Two writes per index, same full 9-bit RRRGGGBBB protocol as
 * pico_tilemap_set_color() (see that function's own comment for why the
 * simpler single-write REG_PALETTE_VALUE_8 path isn't good enough --
 * only 2-bit blue precision). RPC_SELECT_LAYER_2_PALETTE_0 is already
 * named in z88dk's own <arch/zxn.h>, unlike the tilemap's equivalent. */
void pico_video_load_pico8_palette(void) {
    uint8_t i;
    for (i = 0; i < 16; i++) {
        uint8_t r = pico8_palette[i][0];
        uint8_t g = pico8_palette[i][1];
        uint8_t b = pico8_palette[i][2];
        ZXN_WRITE_REG(REG_PALETTE_CONTROL, RPC_SELECT_LAYER_2_PALETTE_0);
        ZXN_WRITE_REG(REG_PALETTE_INDEX, i);
        ZXN_WRITE_REG(REG_PALETTE_VALUE_16, (uint8_t)((r << 5) | (g << 2) | (b >> 1)));
        ZXN_WRITE_REG(REG_PALETTE_VALUE_16, (uint8_t)(b & 1));
    }
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
