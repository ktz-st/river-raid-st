        section text
        xdef RR_MaskOverlap
; a0 enemy transparency, a1 player transparency, d0 groups, d1 rows, d2 dx.
; Only the one or two enemy words underneath the player can overlap.
RR_MaskOverlap:
        tst.w d1
        beq .empty
        movem.l d2-d7,-(sp)
        move.w d0,d7
        add.w d7,d7
        subq.w #1,d1
        tst.w d2
        bmi .left
        move.w d2,d3
        lsr.w #4,d3
        cmp.w d0,d3
        bcc .miss
        add.w d3,d3
        adda.w d3,a0
        move.w d7,d6
        sub.w d3,d6
        andi.w #15,d2
        beq .aligned
        cmpi.w #4,d6
        bcs .right
        moveq #16,d6
        sub.w d2,d6
.two:
        move.w (a1)+,d3
        not.w d3
        move.w d3,d4
        lsr.w d2,d3
        move.w (a0),d5
        not.w d5
        and.w d3,d5
        bne .hit
        lsl.w d6,d4
        move.w 2(a0),d5
        not.w d5
        and.w d4,d5
        bne .hit
        adda.w d7,a0
        dbra d1,.two
        bra .miss
.left:
        cmpi.w #-16,d2
        ble .miss
        neg.w d2
.left_row:
        move.w (a1)+,d3
        not.w d3
        lsl.w d2,d3
        move.w (a0),d4
        not.w d4
        and.w d3,d4
        bne .hit
        adda.w d7,a0
        dbra d1,.left_row
        bra .miss
.right:
        move.w (a1)+,d3
        not.w d3
        lsr.w d2,d3
        move.w (a0),d4
        not.w d4
        and.w d3,d4
        bne .hit
        adda.w d7,a0
        dbra d1,.right
        bra .miss
.aligned:
        move.w (a1)+,d3
        not.w d3
        move.w (a0),d4
        not.w d4
        and.w d3,d4
        bne .hit
        adda.w d7,a0
        dbra d1,.aligned
.miss:
        moveq #0,d0
        bra .done
.hit:
        moveq #1,d0
.done:
        movem.l (sp)+,d2-d7
        rts
.empty:
        moveq #0,d0
        rts
