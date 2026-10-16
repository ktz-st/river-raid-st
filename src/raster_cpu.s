        section text
        xdef RR_FillRowsCPU,RR_FuelMarkerCPU,RR_TextGlyphCPU

; A0 destination, D0 ink, D1 row count. 20 ST4 groups per 160-byte row.
RR_FillRowsCPU:
        tst.w d1
        beq .empty
        movem.l d2-d3,-(sp)
        moveq #0,d2
        moveq #0,d3
        btst #0,d0
        beq.s .plane1
        move.w #$ffff,d2
.plane1:
        swap d2
        btst #1,d0
        beq.s .plane2
        move.w #$ffff,d2
.plane2:
        btst #2,d0
        beq.s .plane3
        move.w #$ffff,d3
.plane3:
        swap d3
        btst #3,d0
        beq.s .rows
        move.w #$ffff,d3
.rows:
        subq.w #1,d1
.row:
        rept 20
        move.l d2,(a0)+
        move.l d3,(a0)+
        endr
        dbra d1,.row
        movem.l (sp)+,d2-d3
.empty:
        rts

; A0 first aligned group on marker row 20, D0 x&15.
; Duplicate each 16-bit mask across two planes; colour 12 clears P0/P1.
RR_FuelMarkerCPU:
        movem.l d3-d5,-(sp)
        lsl.w #3,d0
        lea .masks(pc),a1
        move.l (a1,d0.w),d2
        move.l 4(a1,d0.w),d4
        move.l d2,d3
        not.l d3
        tst.l d4
        bne .spill
        rept 10
        and.l d3,(a0)
        or.l d2,4(a0)
        lea 160(a0),a0
        endr
        bra .done
.spill:
        move.l d4,d5
        not.l d5
        rept 10
        and.l d3,(a0)
        or.l d2,4(a0)
        and.l d5,8(a0)
        or.l d4,12(a0)
        lea 160(a0),a0
        endr
.done:
        movem.l (sp)+,d3-d5
        rts
        even
.masks:
        dc.l $f000f000,$00000000
        dc.l $78007800,$00000000
        dc.l $3c003c00,$00000000
        dc.l $1e001e00,$00000000
        dc.l $0f000f00,$00000000
        dc.l $07800780,$00000000
        dc.l $03c003c0,$00000000
        dc.l $01e001e0,$00000000
        dc.l $00f000f0,$00000000
        dc.l $00780078,$00000000
        dc.l $003c003c,$00000000
        dc.l $001e001e,$00000000
        dc.l $000f000f,$00000000
        dc.l $00070007,$80008000
        dc.l $00030003,$c000c000
        dc.l $00010001,$e000e000

; A0 aligned ST4 group, A1 eight source bytes (may be odd), D0 x&8.
; Copy an opaque 8px glyph into one half of each planar word.
RR_TextGlyphCPU:
        movem.l d3-d4,-(sp)
        move.l #$00ff00ff,d3
        tst.w d0
        beq.s .start
        not.l d3
.start:
        moveq #7,d4
.row:
        moveq #0,d1
        move.b (a1)+,d1
        tst.w d0
        bne.s .low
        lsl.w #8,d1
.low:
        move.w d1,d2
        swap d1
        move.w d2,d1
        and.l d3,(a0)
        or.l d1,(a0)
        and.l d3,4(a0)
        or.l d1,4(a0)
        lea 160(a0),a0
        dbra d4,.row
        movem.l (sp)+,d3-d4
        rts
