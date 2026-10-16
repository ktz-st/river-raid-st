        section text
        xdef RR_LinePlanar

; a0 = trusted row commands, a1 = Mode E LUT, output pointer at 4(sp).
; Crop 4 packed bytes at each side; pair each logical pixel horizontally.
; a2 is a half-group output cursor, d5 its high/low phase, d6 visible bytes.
RR_LinePlanar:
        movem.l d2-d7/a2-a6,-(sp)
        movea.l 48(sp),a2
        lea 1536(a1),a4
        moveq #0,d5
        moveq #40,d6
        moveq #4,d7
        move.b (a0)+,d1
        btst #7,d1
        beq .token
        moveq #48,d2
        bra .literal
.token:
        moveq #0,d1
        move.b (a0)+,d1
        beq .literal_count
        move.w d1,d2
        andi.w #63,d2
        tst.w d7
        beq .fill_visible
        cmp.w d7,d2
        bhi .fill_prefix
        sub.w d2,d7
        bra .token
.fill_prefix:
        sub.w d7,d2
        moveq #0,d7
.fill_visible:
        cmp.w d6,d2
        bls .fill_clipped
        move.w d6,d2
.fill_clipped:
        andi.w #$c0,d1
        lsr.w #3,d1
        lea (a4,d1.w),a3
        move.l (a3),d3
        move.l 4(a3),d4
        tst.w d5
        beq .fill_pairs
        bsr .fill_half
        subq.w #1,d2
        subq.w #1,d6
        beq .done
.fill_pairs:
        move.w d2,d1
        andi.w #1,d2
        lsr.w #1,d1
        move.w d1,d0
        add.w d0,d0
        sub.w d0,d6
        subq.w #1,d1
        bmi .fill_tail
.groups:
        move.l d3,(a2)+
        move.l d4,(a2)+
        dbra d1,.groups
.fill_tail:
        tst.w d6
        beq .done
        tst.w d2
        beq .token
        bsr .fill_half
        subq.w #1,d6
        bne .token
.done:
        movem.l (sp)+,d2-d7/a2-a6
        rts
.literal_count:
        moveq #0,d2
        move.b (a0)+,d2
.literal:
        tst.w d7
        beq .literal_visible
        cmp.w d7,d2
        bhi .literal_prefix
        adda.w d2,a0
        sub.w d2,d7
        bra .token
.literal_prefix:
        adda.w d7,a0
        sub.w d7,d2
        moveq #0,d7
.literal_visible:
        cmp.w d6,d2
        bls .literal_loop
        move.w d6,d2
.literal_loop:
        moveq #0,d0
        move.b (a0)+,d0
        lsl.w #2,d0
        lea (a1,d0.w),a3
        move.b (a3)+,(a2)
        move.b (a3)+,2(a2)
        move.b (a3)+,4(a2)
        move.b (a3),6(a2)
        bsr .advance_half
        subq.w #1,d6
        beq .done
        subq.w #1,d2
        bne .literal_loop
        bra .token
.fill_half:
        move.b (a3),(a2)
        move.b 2(a3),2(a2)
        move.b 4(a3),4(a2)
        move.b 6(a3),6(a2)
.advance_half:
        eori.w #1,d5
        beq .low_done
        addq.l #1,a2
        rts
.low_done:
        addq.l #7,a2
        rts
