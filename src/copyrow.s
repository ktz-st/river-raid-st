        section text
        xdef RR_CopyRow160

; GCC fastcall: a0 = source, a1 = destination. Even, disjoint 160-byte rows.
; REPT expands at assembly time: 40 MOVE.L instructions, no runtime loop.
; Only volatile a0/a1 and CCR change. 800 cycles + 16 for RTS on 68000.
RR_CopyRow160:
        rept 40
        move.l (a0)+,(a1)+
        endr
        rts
