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
 * pico_hwsprite.c is I/O-port-only) -- safe to claim for this. */
#define PICO_BANK_WINDOW_REG 0x51

/* Pages `physical_page` into the shared window and returns whatever page
 * was there before (pass that to pico_bank_page_out() to restore it). */
uint8_t pico_bank_page_in(uint8_t physical_page);
void pico_bank_page_out(uint8_t previous_page);

#endif
