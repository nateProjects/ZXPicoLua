#ifndef PICO_BANK_H
#define PICO_BANK_H

#include <stdint.h>

/* Next MMU slot 1 ($2000-$3FFF, NextReg $51) as a dedicated banked-data
 * window -- lets a module's large const tables (a sprite sheet, a map)
 * live OUTSIDE the resident 64K z88dk classic mode caps everything else
 * to, paged in only while actually being read. Ported verbatim from
 * dark-room-next's src/bank.c/.h (that project's own "Root cause, found"
 * investigation in z88dk-banking-notes.md is the reason this exists in
 * this exact form: `#pragma bank N` + CRT_ORG_BANK_N_L looks like it
 * should do the same thing but silently computes the WRONG address,
 * producing byte-correct-but-garbled reads; the Makefile's own
 * PAGE_<n>_<name> section-rename trick, not this header, is what
 * actually avoids that -- see the Makefile comment next to CBANK_*).
 *
 * A single 8K physical page is the hard ceiling per bank_page_in() call
 * through this window -- a module that outgrows 8192 bytes needs a
 * second physical page and a second explicit page-in, not a bigger
 * single one (dark-room-next hit this planning its own City/Executioner
 * content). Nothing else in ZXPicoLua or its games uses NextReg $51 or
 * $2000-$3FFF (pico_video.c's Layer 2 window is MMU slot 3/$6000-$7FFF;
 * pico_tilemap.c's tile defs live in fixed Spectrum "Bank 5"/$4000-$7FFF;
 * pico_hwsprite.c is I/O-port-only) -- safe to claim for this.
 *
 * PHYSICAL PAGE CHOICE: avoid 18-29. Confirmed directly in THIS project's
 * own pico_video.c, not just inherited secondhand -- pico_video_init()
 * leaves Layer 2's main bank at its cold-boot default (bank 9 = pages
 * 18-23, LAYER2_PAGES=6) and allocates the shadow bank 3 16K-banks
 * further (bank 12 = pages 24-29), then clears both. A banked module
 * placed on any page in that range gets silently zeroed by that startup
 * clear -- a byte-correct .nex file, all-zero at runtime, the exact
 * failure mode a sibling project (Harvester/Islander) hit and spent real
 * effort root-causing (dark-room-next's own banking convention happened
 * to start at page 18, which collided with this project's own Layer 2
 * range the moment it was reused here). 8/12/14 (used by picodiver,
 * bankcalltest) and 16/17/30 are confirmed clear of this; page 10 is
 * also a poor choice (it's Spectrum "Bank 5"'s own half, the
 * tilemap/screen area pico_tilemap.c uses). Still confirm any NEW page
 * number via ZRCP (page it in, read known bytes back) before trusting
 * it -- this explains the known-bad range, it doesn't guarantee every
 * other page is safe on every future z88dk/ZEsarUX combination. */
#define PICO_BANK_WINDOW_REG 0x51

/* Pages `physical_page` into the shared window and returns whatever page
 * was there before (pass that to pico_bank_page_out() to restore it). */
uint8_t pico_bank_page_in(uint8_t physical_page);
void pico_bank_page_out(uint8_t previous_page);

/* --- Banking CODE, not just data --------------------------------------
 *
 * The Makefile's own PAGE_<n>_<name> mechanism (the pattern rule next to
 * CBANK_* in the Makefile) isn't data-specific -- it substitutes
 * code_compiler/rodata_compiler/data_compiler/bss_compiler identically,
 * so a .c file whose only content is a function bodies the same way a
 * data module does, linked at a real, CALLable address while its page is
 * active. Confirmed directly (games/bankcalltest/, docs/bank-call-notes.md)
 * -- not just inferred from the mechanism's own description. No special
 * calling convention is needed beyond the obvious:
 *
 *   uint8_t prev = pico_bank_page_in(SOME_PHYSICAL_PAGE);
 *   result = some_banked_function(args...);   // an ordinary call
 *   pico_bank_page_out(prev);
 *
 * -- an ordinary forward declaration (in the banked module's own header)
 * plus a plain C call, exactly like any other function. Useful when
 * banked DATA alone doesn't buy back enough resident-code headroom under
 * z88dk classic's 64K ceiling; add the module to BANKED_CSRCS/CBANK_* in
 * the Makefile the same way a banked data file already is.
 *
 * HAZARD (found the hard way in a sibling project, Harvester, not by
 * bankcalltest's own trivially simple test function): any `static const`
 * table inside a banked file MUST be at file scope, not function-local.
 * A function-local one gets emitted INLINE in the function's own code by
 * sccz80 -- this mechanism's section-rename script maps the whole file's
 * sections into one, so that data ends up sitting in the middle of the
 * banked code, and execution falling through it (or a branch landing
 * past where you think the function ends) runs the data bytes as
 * instructions. String literals are fine (emitted at the file's end, not
 * inline) -- only const arrays/structs declared inside a function body
 * are the hazard. See docs/bank-call-notes.md for the full incident. */

#endif
