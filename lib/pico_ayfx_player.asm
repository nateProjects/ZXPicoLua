;;
;; AYFX "Minimal ayFX player v0.15 06.05.06" engine, vendored from
;; Shiru's AYFX format/player, via the user's own Next port (read
;; directly: /Users/nat/Code/Games/ayfxResources/next-ayfx-main/src/
;; ayfx.asm -- MIT licensed, see that repo's own README) -- the real
;; Z80 engine routines only (AFXINIT/AFXPLAY/AFXFRAME), NOT that port's
;; own NextBASIC driver wrapper (the `.DRV`/dot-command API, its own
;; bank-switching for NextBASIC's BANK command, backup_bank/restore_bank
;; etc) -- none of that applies here, this project links the engine
;; directly into a C program the same way lib/pico_music_player.asm
;; vendors the Arkos player. See docs/sfx-convert-notes.md for why this
;; exists (pico_sfx_synth.c and the Arkos music player can't safely
;; share one AY chip; this plays on a second, separate chip instead).
;;
;; Two real adaptations from the original source, both marked below:
;; 1. AFXPLAY's own `START_OF_FX_BANK equ $c000` assumed the NextBASIC
;;    driver had already paged an effects bank into MMU6 ($C000) before
;;    calling it -- replaced with an EXTERN reference to this project's
;;    own linked effects-bank symbol instead (no bank-paging needed,
;;    it's just resident data here).
;; 2. `ay_chip_select` (read by AFXINIT/AFXFRAME) must be set by the
;;    caller before use -- lib/pico_ayfx.c sets it to the literal value
;;    confirmed correct for the Next's Turbosound chip-select protocol
;;    ($FE, not z88dk's own apparently-wrong IT_SELECT_PSG_1 symbol --
;;    see lib/pico_sfx_synth.c's own comment on this).

SECTION code_user

EXTERN _pico_ayfx_bank_data

PUBLIC _pico_ayfx_init_asm
PUBLIC _pico_ayfx_play_asm
PUBLIC _pico_ayfx_frame_asm
PUBLIC _ay_chip_select

; C-callable trampolines (added here, not part of the original file) --
; __z88dk_callee convention, same shape as lib/pico_music_player.asm's
; own trampolines.

_pico_ayfx_init_asm:
        jp      AFXINIT

_pico_ayfx_play_asm:
        pop     bc              ; BC = retaddr
        pop     de              ; E = effect number
        ld      a, e
        push    bc              ; restore retaddr
        jp      AFXPLAY

_pico_ayfx_frame_asm:
        jp      AFXFRAME

;-Minimal ayFX player v0.15 06.05.06---------------------------;
;                                                              ;
; The simplest player for effects. Plays effects on one AY,    ;
; without music in the background. Channel selection priority: ;
; if available; free channels, one of them is selected. If free;
; no channels, the longest sounding one is selected. Procedure ;
; playback uses registers AF, BC, DE, HL, IX.                  ;
;                                                              ;
; Initialization:                                              ;
;   ld hl, effect bank address                                 ;
;   call AFXINIT                                               ;
;                                                              ;
; Effects playback:                                            ;
;   ld a, effect number (0..255)                               ;
;   call AFXPLAY                                               ;
;                                                              ;
; In the interrupt handler:                                    ;
;   call AFXFRAME                                              ;
;                                                              ;
;--------------------------------------------------------------;



; ------------------------------------------------- -------------;
; Effects player initialization.                                 ;
; Turns off all channels, sets variables.                        ;
; Input: ay_chip_select must already be set by the caller         ;
; ------------------------------------------------- -------------;

AFXINIT:
        ld hl,afxChDesc		        ; mark all channels as empty
        ld de,$00ff
        ld bc,$03fd			; B is used for the loop, C is used as part of the PORT select
afxInit0:
        ld (hl),d
        inc hl
        ld (hl),d
        inc hl
        ld (hl),e
        inc hl
        ld (hl),e
        inc hl
        djnz afxInit0

        ld 	hl,$ffbf		; initialise AY
        ld	a,(_ay_chip_select)	; select the AY chip the user provided
        ld 	b,h
        out 	(c),a			; remember `out (c)` is actually out (bc)
					; now that the AY chip is selected,
					; future `out` calls use the same chip.
        ld 	e,$15
afxInit1: ; runs 15 times (from E register)
        dec e
        ld b,h
        out (c),e
        ld b,l
        out (c),d
        jr nz,afxInit1
        ld (afxNseMix+1),de             ; reset the player variables
        ret



; --------------------------------------------------------------;
; Play the current frame.                                       ;
; Has no parameters.                                            ;
; --------------------------------------------------------------;

AFXFRAME:
        ld 	bc,$fffd
        ld	a,(_ay_chip_select)	; select the AY chip the user provided
        out 	(c),a
        ld 	bc,$03fd
        ld ix,afxChDesc

afxFrame0:
        push bc

        ld a,11
        ld h,(ix+1)			; high byte of address by <11
        cp h
        jr nc,afxFrame7		        ; channel is not playing, skip
        ld l,(ix+0)

        ld e,(hl)			; take the value of the byte
        inc hl

        sub b				; select the volume register:
        ld d,b				; (11-3=8, 11-2=9, 11-1=10)

        ld b,$ff			;load volume value
        out (c),a
        ld b,$bf
        ld a,e
        and $0f
        out (c),a

        bit 5,e				; will there be a tone change?
        jr z,afxFrame1		        ; tone didn't change

        ld a,3				; select the tone registers:
        sub d				; 3-3=0, 3-2=1, 3-1=2
        add a,a				; 0*2=0, 1*2=2, 2*2=4

        ld b,$ff			; load the tone value
        out (c),a
        ld b,$bf
        ld d,(hl)
        inc hl
        out (c),d
        ld b,$ff
        inc a
        out (c),a
        ld b,$bf
        ld d,(hl)
        inc hl
        out (c),d

afxFrame1:
        bit 6,e				; will there be a noise change?
        jr z,afxFrame3		        ; noise didn't change

        ld a,(hl)			; read noise word
        sub $20
        jr c,afxFrame2		        ; less than # 20, play on
        ld h,a				; otherwise end of the effect
        ld b,$ff
        ld b,c				; in BC we enter the longest time
        jr afxFrame6

afxFrame2:
        inc hl
        ld (afxNseMix+1),a	        ; store the noise value

afxFrame3:
        pop bc				; restore the loop value to B
        push bc
        inc b				; number of offsets for TN flags

        ld a,%01101111		        ; mask for TN flags
afxFrame4:
        rrc e				; move flags and mask
        rrca
        djnz afxFrame4
        ld d,a
        ld bc,afxNseMix+2	        ; save flag values
        ld a,(bc)
        xor e
        and d
        xor e				;E is masked with D
        ld (bc),a

afxFrame5:
        ld c,(ix+2)			; increase the time counter
        ld b,(ix+3)
        inc bc

afxFrame6:
        ld (ix+2),c
        ld (ix+3),b

        ld (ix+0),l			; save changed address
        ld (ix+1),h

afxFrame7:
        ld bc,4				; go to next channel
        add ix,bc
        pop bc
        djnz afxFrame0

        ld hl,$ffbf			; load noise and mixer values
afxNseMix:
        ld de,0				;+1(E)=noise, +2(D)=mixer
        ld a,6
        ld b,h
        out (c),a
        ld b,l
        out (c),e
        inc a
        ld b,h
        out (c),a
        ld b,l
        out (c),d

        ret



; ------------------------------------------------- -------------;
; Triggers the effect on a free channel. Without the most recent ;
; sounding channel is selected.
; Input: A = effect number 0..255;
; ------------------------------------------------- -------------;

AFXPLAY:
        ld de,0				; in DE, the longest search time
        ld h,e
        ld l,a
        add hl,hl
afxBnkAdr:
        ; ADAPTED: the original here was
        ;   ld bc,START_OF_FX_BANK+1  ($C000+1, a NextBASIC-paged bank)
        ; replaced with this project's own linked effects-bank symbol --
        ; see this file's own top comment, adaptation 1.
        ld bc,_pico_ayfx_bank_data+1
        add hl,bc
        ld c,(hl)
        inc hl
        ld b,(hl)
        add hl,bc			; effect address is held in HL
        push hl				; save the address of the effect on the stack
        ld hl,afxChDesc		        ; find empty channel
        ld b,3
afxPlay0:
        inc hl
        inc hl
        ld a,(hl)			; compare the channel time with the longest
        inc hl
        cp e
        jr c,afxPlay1
        ld c,a
        ld a,(hl)
        cp d
        jr c,afxPlay1
        ld e,c				; save the longest time
        ld d,a
        push hl				; save the channel address+3 in IX
        pop ix
afxPlay1:
        inc hl
        djnz afxPlay0

        pop de				; pop the effect address from the stack
        ld (ix-3),e			; put into the channel descriptor
        ld (ix-2),d
        ld (ix-1),b			; zero out the seconds
        ld (ix-0),b

        ret

; **************************************************************
; * data
; **************************************************************

; channel descriptors, 4 bytes per channel:
; +0 (2) current address (channel is free if high byte = # 00)
; +2 (2) effect sounding time
; ...

afxChDesc:	DEFS 3*4		; will be populated with 0x0000 0xffff (x 3 via B register)

; ADAPTED: the original here was `ay_chip_select: defb 0`, a private
; variable the NextBASIC driver's own API (callId1_load_bank) wrote
; before calling AFXINIT -- made PUBLIC instead, so lib/pico_ayfx.c can
; set it directly (adaptation 2, this file's own top comment).
_ay_chip_select:
        defb	0
