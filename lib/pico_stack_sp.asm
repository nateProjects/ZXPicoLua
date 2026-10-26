; pico_stack_sp.asm -- the one instruction pico_stack.c's margin check
; needs and C can't express directly: reading the live SP into a normal
; return value. `ADD HL,SP` is a real Z80 opcode (no `LD HL,SP`), so this
; is the standard idiom (`LD HL,0` / `ADD HL,SP`) rather than anything
; exotic. Zero arguments, so there's nothing calling-convention-specific
; about this entry point -- the usual sccz80 __z88dk_callee/fastcall
; distinctions only affect how ARGUMENTS arrive, and there are none here.
;
; Note the result is the SP *at the point this instruction executes*,
; which is already 2 bytes below the caller's own SP (the CALL into this
; function pushed a return address first) -- a small, fixed, KNOWN
; offset in the conservative direction: it makes the reported margin
; very slightly smaller than the caller's true headroom, never larger,
; so it can't mask a real problem.

SECTION code_compiler
PUBLIC _pico_stack_sp

_pico_stack_sp:
        ld      hl, 0
        add     hl, sp
        ret
