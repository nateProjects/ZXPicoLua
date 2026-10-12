#ifndef BANKED_FN_H
#define BANKED_FN_H

#include <stdint.h>

/* Compiled as a banked module (see the Makefile's own GAME=bankcalltest
 * block) -- only callable while its physical page is paged into
 * pico_bank.h's window (pico_bank_page_in(BANKED_FN_PHYSICAL_PAGE)
 * first, pico_bank_page_out() after). Ordinary multiplication, chosen
 * purely so a wrong result (garbage from an unpaged/miscalculated call)
 * is obvious and not confusable with a coincidentally-plausible value. */
#define BANKED_FN_PHYSICAL_PAGE 8

int16_t banked_multiply(int16_t a, int16_t b);

#endif
