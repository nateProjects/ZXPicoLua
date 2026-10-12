; pico_mul_z80n.asm -- hardware-accelerated 16x16->16 unsigned multiply,
; overriding z88dk classic clib's own generic (software shift-add) l_mult/
; l_mult_u/l_mult_0 with the Next's `mul de` opcode (ED 30, D*E->DE
; unsigned, 8 T-states -- cpu-z80n skill, z88dk-master/.agents/skills/
; cpu-z80n/SKILL.md).
;
; 2026-10-12 finding: z88dk's OWN z80n-optimized l_mult exists
; (libsrc/_DEVELOPMENT/l/sccz80/4-z80n/l_mult.asm) but classic clib's own
; prebuilt archives don't appear to select it for this project's build --
; confirmed by disassembling games/islander/main.c (zcc -a): every one of
; its 13 compiler-generated multiplies compiles to `call l_mult`/`call
; l_mult_u`/`call l_mult_0`, and the opcode `mul` never appears ANYWHERE
; in that output, meaning the generic software routine is what's actually
; linked despite building `-mz80n` for a Next-only binary.
;
; This file is the exact z80n routine from z88dk's own tree (proven,
; not reinvented) -- PUBLIC symbol names match EXACTLY what sccz80 emits
; for its own internal multiply operator, so this requires zero call-site
; changes anywhere: every existing `a * b` on two 16-bit values in this
; whole project (both games, and any future one built from this same
; lib/) silently starts using hardware multiply the moment this file is
; linked in. Standard link-resolution order (this file's own object is
; passed directly on the command line, never pulled from an archive)
; means these definitions satisfy the symbols before the classic clib
; archive's own generic versions are ever consulted -- confirmed by a
; clean build with no duplicate-symbol error.
;
; Entry (sccz80's own internal multiply-operator calling convention,
; NOT a general C ABI -- don't call these directly from C, write a
; normal C-callable wrapper if a hand-written asm entry point is ever
; needed instead):
;   l_mult_0: bc = value1, de = value2
;   l_mult / l_mult_u: hl = value1, de = value2
; Exit: hl = value1 * value2 (low 16 bits of the product either way --
; signed and unsigned multiplication produce the same bit pattern in the
; low 16 bits of a 16x16->16 result, which is why sccz80 shares one
; routine for both l_mult and l_mult_u).

SECTION code_clib
SECTION code_l_sccz80
PUBLIC  l_mult
PUBLIC  l_mult_u
PUBLIC  l_mult_0

.l_mult_0
    ld hl,bc

.l_mult
.l_mult_u
    ld a,d                      ; a = xh
    ld d,h                      ; d = yh
    ld h,a                      ; h = xh
    ld c,e                      ; c = xl
    ld b,l                      ; b = yl
    mul de                      ; yh * yl
    ex de,hl
    mul de                      ; xh * yl
    add hl,de                   ; add cross products
    ld de,bc
    mul de                      ; yl * xl
    ld a,l                      ; cross products lsb
    add a,d                     ; add to msb final
    ld h,a
    ld l,e                      ; hl = final
    ret
