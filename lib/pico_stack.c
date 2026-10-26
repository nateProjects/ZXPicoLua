#include "pico_stack.h"

/* __BSS_END_tail is z88dk classic crt0's own linker symbol -- the first
 * byte above this program's BSS segment, i.e. the lowest address real
 * stack growth is allowed to reach before it starts corrupting a static
 * global. It's a label, not a variable with real storage, so this is
 * declared only to take its address, never read/written as a value.
 * Declared with only ONE leading underscore here, not two -- sccz80
 * silently prepends its own leading underscore to every C symbol, so a
 * C name of `_BSS_END_tail` is what actually resolves to the linker's
 * real `__BSS_END_tail` (confirmed directly: declaring two underscores
 * here looked for a THIRD-underscore symbol at link time and failed). */
extern uint8_t _BSS_END_tail;

uint16_t pico_stack_margin(void) {
    uint16_t sp = pico_stack_sp();
    uint16_t bss_end = (uint16_t)&_BSS_END_tail;
    /* Signed subtraction, not unsigned: if SP has already fallen to or
     * below bss_end (a real collision, already happened), an unsigned
     * (sp - bss_end) would wrap around to a huge positive number --
     * exactly backwards, reporting a crisis as abundant headroom. */
    int16_t margin = (int16_t)(sp - bss_end);
    return (margin > 0) ? (uint16_t)margin : 0;
}

uint8_t pico_stack_low(uint16_t min_margin) {
    return pico_stack_margin() <= min_margin;
}
