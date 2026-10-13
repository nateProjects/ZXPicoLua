#include "pico_music_banked.h"
#include "pico_bank.h"

extern void pico_music_banked_init_asm(const void *song, uint8_t subsong) __z88dk_callee;
extern void pico_music_banked_play_asm(void) __z88dk_callee;
extern void pico_music_banked_stop_asm(void) __z88dk_callee;

void pico_music_banked_init(uint8_t bank_page, const void *song, uint8_t subsong) {
    uint8_t prev = pico_bank_page_in(bank_page);
    pico_music_banked_init_asm(song, subsong);
    pico_bank_page_out(prev);
}

void pico_music_banked_play_frame(uint8_t bank_page) {
    uint8_t prev = pico_bank_page_in(bank_page);
    pico_music_banked_play_asm();
    pico_bank_page_out(prev);
}

void pico_music_banked_stop(uint8_t bank_page) {
    uint8_t prev = pico_bank_page_in(bank_page);
    pico_music_banked_stop_asm();
    pico_bank_page_out(prev);
}
