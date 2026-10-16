        section text
        xdef RR_ShotCPU
; a0 = aligned destination, d0 = x&15, d1 = clipped height (0..8).
; Colour 12: clear planes 0/1, set planes 2/3. No background writes elsewhere.
RR_ShotCPU:
        tst.w d1
        beq .empty
        movem.l d3-d5,-(sp)
        lsl.w #3,d0
        lea .masks(pc),a1
        move.l (a1,d0.w),d2
        move.l 4(a1,d0.w),d4
        move.l d2,d3
        not.l d3
        subq.w #1,d1
        tst.l d4
        bne .spill
.loop:
        and.l d3,(a0)
        or.l d2,4(a0)
        lea 160(a0),a0
        dbra d1,.loop
        bra .done
.spill:
        move.l d4,d5
        not.l d5
.loop2:
        and.l d3,(a0)
        or.l d2,4(a0)
        and.l d5,8(a0)
        or.l d4,12(a0)
        lea 160(a0),a0
        dbra d1,.loop2
.done:
        movem.l (sp)+,d3-d5
.empty:
        rts
        even
.masks:
        dc.l $c000c000,$00000000
        dc.l $60006000,$00000000
        dc.l $30003000,$00000000
        dc.l $18001800,$00000000
        dc.l $0c000c00,$00000000
        dc.l $06000600,$00000000
        dc.l $03000300,$00000000
        dc.l $01800180,$00000000
        dc.l $00c000c0,$00000000
        dc.l $00600060,$00000000
        dc.l $00300030,$00000000
        dc.l $00180018,$00000000
        dc.l $000c000c,$00000000
        dc.l $00060006,$00000000
        dc.l $00030003,$00000000
        dc.l $00010001,$80008000
