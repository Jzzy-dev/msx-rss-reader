;==============================================================================
; rssw.asm - RSSW.ROM: the MSX RSS reader as a 16 KB ROM for the "WIFI+Telnet"
; mode of the MSX Pico+ menu (v2.13): the menu loads a file of exactly 16 KB
; that contains the text UNAPI at 8000h, next to the ESP8266P TCP/IP UNAPI
; BIOS in page 1 of the same sub-slot (as it runs MSXTT and Telnet).
;
; RSS.ROM (25 KB of code at 4000h) is in here packed with ZX0.  At H.STKE:
;   1. the packed data goes to VRAM (the ROM itself is in page 2, which is
;      switched to RAM halfway),
;   2. a small routine in page 3 switches pages 1 and 2 to RAM and unpacks
;      RSS there, reading the packed data from VRAM,
;   3. RSS's own INIT runs (it finds the RAM as "its slot") and then its
;      H.STKE routine: RSS runs as RSS.ROM does,
;   4. when RSS ends: an empty BASIC program again (page 2 was BASIC's RAM),
;      page 1 and page 2 as they were, back to the BIOS.
;
; INIT sets USRTAB, so that Nextor starts Disk BASIC 2 (FAT16) before
; H.STKE instead of MSX-DOS 1 mode.
;
;   zx0 rss_img.bin rss_img.zx0     (rss_img.bin = RSS.ROM up to its code end)
;   sjasmplus --raw=RSSW.ROM rssw.asm
;
; ZX0 decoder: "standard" version by Einar Saukas & Urusergi (BSD-3), here
; reading its input from the VDP data port.
;
; This work is licensed under a "Creative Commons Attribution-NonCommercial-
; ShareAlike 4.0 International License".
;==============================================================================

ENASLT  equ #0024
DISSCR  equ #0041
CHPUT   equ #00A2
MSXVER  equ #002D

USRTAB  equ #F39A
FCERR   equ #475A
HIMEM   equ #FC4A
EXPTBL  equ #FCC1
H_STKE  equ #FEDA

RSS_VARS_END equ #D7DD          ; RSS.ROM's variables: C000h up to here
TRAMP   equ #D800               ; our page 3 routine, after RSS's variables

        org #8000
        db "AB"
        dw linit
        dw 0,0,0
        ds 6
        db "RSS reader for WIFI+Telnet, needs a TCP/IP UNAPI",0

;--- INIT: hook H.STKE (our slot is the page 2 slot), USRTAB for Disk BASIC 2
linit:  call slot_p2
        ld c,a
        ld hl,FCERR+1
        ld (USRTAB),hl
        ld hl,H_STKE
        ld (hl),#F7             ; RST 30h, our slot, lstart
        inc hl
        ld (hl),c
        inc hl
        ld (hl),low lstart
        inc hl
        ld (hl),high lstart
        inc hl
        ld (hl),#C9
        ret

;--- H.STKE (page 2 = us)
lstart: ld a,(H_STKE+1)
        push af
        ld hl,H_STKE
        ld b,5
.c:     ld (hl),#C9
        inc hl
        djnz .c
        ld hl,FCERR
        ld (USRTAB),hl
        ld hl,(HIMEM)           ; room for the page 3 routine (RSS itself
        ld de,TRAMP+tramp_len+800 ; wants 768 bytes under HIMEM after it)
        or a
        sbc hl,de
        jr c,.small
        ld hl,tramp_src
        ld de,TRAMP
        ld bc,tramp_len
        ldir
        pop af
        ld (t_rom),a
        call slot_p1
        ld (t_p1),a
        call slot_p3
        ld (t_ram),a
        ; the packed RSS to VRAM 0
        call DISSCR
        di
        call vdp_r14
        xor a
        out (#99),a
        ld a,#40                ; write, address 0
        out (#99),a
        ld hl,packed
        ld bc,PACKED_LEN
.v:     ld a,(hl)
        out (#98),a
        inc hl
        dec bc
        ld a,b
        or c
        jr nz,.v
        jp TRAMP                ; (returns to H.STKE's caller)
.small: pop af
        ld hl,txt_small
.m:     ld a,(hl)
        or a
        ret z
        call CHPUT
        inc hl
        jr .m

; R#14 = 0 on an MSX2 or later (the TMS9918 has no R#14)
vdp_r14:
        ld a,(MSXVER)
        or a
        ret z
        xor a
        out (#99),a
        ld a,#8E
        out (#99),a
        ret

; slot ids (bit 7 expanded, bits 3-2 sub-slot, bits 1-0 primary) of pages 1-3
slot_p1:
        in a,(#A8)
        rrca
        rrca
        ld b,0                  ; (sub-slot bits 3-2 of SLTTBL: as they are)
        jr slot_x
slot_p2:
        in a,(#A8)
        rrca
        rrca
        rrca
        rrca
        ld b,1                  ; (bits 5-4: two to the right)
        jr slot_x
slot_p3:
        in a,(#A8)
        rlca
        rlca
        ld b,2                  ; (bits 7-6: four to the right)
slot_x: and 3
        ld c,a
        push bc
        ld b,0
        ld hl,EXPTBL
        add hl,bc
        ld a,(hl)
        pop bc
        and #80
        jr z,.ne
        or c
        ld c,a
        inc hl
        inc hl
        inc hl
        inc hl
        ld a,(hl)               ; SLTTBL of that slot
        inc b
.s:     dec b
        jr z,.d
        rrca
        rrca
        jr .s
.d:     and #0C
        or c
        ret
.ne:    ld a,c
        ret

txt_small: db "RSS: not enough memory under HIMEM",13,10,0

;==============================================================================
; the page 3 routine
;==============================================================================
tramp_src:
        DISP TRAMP
        ld a,(t_ram)            ; pages 1 and 2: RAM
        ld h,#40
        call ENASLT
        ld a,(t_ram)
        ld h,#80
        call ENASLT
        di
        call t_r14
        xor a
        out (#99),a
        out (#99),a             ; read, address 0
        ld de,#4000
        call dzx0v              ; RSS at 4000h
        ld hl,(#4002)           ; RSS's INIT: H.STKE with its slot (the RAM)
        call .jhl
        ld hl,(H_STKE+2)        ; and its H.STKE routine: RSS runs
        call .jhl
        xor a                   ; an empty BASIC program again
        ld (#8000),a
        ld (#8001),a
        ld (#8002),a
        ld a,(t_p1)             ; page 1 and page 2 as they were
        ld h,#40
        call ENASLT
        ld a,(t_rom)
        ld h,#80
        call ENASLT
        ei
        ret
.jhl:   jp (hl)

t_r14:  ld a,(MSXVER)
        or a
        ret z
        xor a
        out (#99),a
        ld a,#8E
        out (#99),a
        ret

; ZX0 "standard" decoder, the input from the VDP (port 98h), DE = output
dzx0v:  ld bc,#ffff             ; preserve default offset 1
        push bc
        inc bc
        ld a,#80
.lit:   call .elias             ; obtain length
        push af                 ; copy literals
.ll:    in a,(#98)
        ld (de),a
        inc de
        dec bc
        ld a,b
        or c
        jr nz,.ll
        pop af
        add a,a                 ; copy from last offset or new offset?
        jr c,.new
        call .elias             ; obtain length
.copy:  ex (sp),hl              ; preserve source, restore offset
        push hl                 ; preserve offset
        add hl,de               ; calculate destination - offset
        ldir                    ; copy from offset
        pop hl                  ; restore offset
        ex (sp),hl              ; preserve offset, restore source
        add a,a                 ; copy from literals or new offset?
        jr nc,.lit
.new:   pop bc                  ; discard last offset
        ld c,#fe                ; prepare negative offset
        call .eloop             ; obtain offset MSB
        inc c
        ret z                   ; check end marker
        ld b,c
        push af
        in a,(#98)              ; obtain offset LSB
        ld c,a
        pop af
        rr b                    ; last offset bit becomes first length bit
        rr c
        push bc                 ; preserve new offset
        ld bc,1                 ; obtain length
        call nc,.eback
        inc bc
        jr .copy
.elias: inc c                   ; interlaced Elias gamma coding
.eloop: add a,a
        jr nz,.eskip
        in a,(#98)              ; load another group of 8 bits
        rla
.eskip: ret c
.eback: add a,a
        rl c
        rl b
        jr .eloop

t_ram:  db 0
t_p1:   db 0
t_rom:  db 0
        ENT
tramp_len equ $-tramp_src
        ASSERT TRAMP > RSS_VARS_END+32

packed: incbin "rss_img.zx0"
PACKED_LEN equ $-packed
code_end:
        ASSERT code_end <= #C000, does not fit in 16 KB
        ds #C000-$,#FF
