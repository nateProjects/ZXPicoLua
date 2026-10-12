#include "pico_bank.h"
#include <arch/zxn.h>

uint8_t pico_bank_page_in(uint8_t physical_page) {
    uint8_t prev = ZXN_READ_REG(PICO_BANK_WINDOW_REG);
    ZXN_WRITE_REG(PICO_BANK_WINDOW_REG, physical_page);
    return prev;
}

void pico_bank_page_out(uint8_t previous_page) {
    ZXN_WRITE_REG(PICO_BANK_WINDOW_REG, previous_page);
}
