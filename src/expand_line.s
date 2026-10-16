        section text
        xdef RR_LineExpand

; Trusted complete compact record in a0, 48-byte output in a1.
; Accepts odd addresses. Validate external records with RR_LineDecode first.
RR_LineExpand:
        movem.l d2-d3,-(sp)
        moveq #0,d0
        move.b (a0)+,d0
        btst #7,d0
        bne .raw
.token:
        moveq #0,d1
        move.b (a0)+,d1
        subq.w #1,d0
        tst.b d1
        beq .literal
        move.w d1,d2
        andi.w #63,d2
        lsr.w #4,d1
        andi.w #12,d1
        move.l .patterns(pc,d1.w),d3
        move.l a1,d1
        btst #0,d1
        beq .aligned
        move.b d3,(a1)+
        subq.w #1,d2
.aligned:
        move.w d2,d1
        andi.w #3,d1
        lsr.w #2,d2
        subq.w #1,d2
        bmi .tail
.longs:
        move.l d3,(a1)+
        dbra d2,.longs
.tail:
        subq.w #1,d1
        bmi .next
.bytes:
        move.b d3,(a1)+
        dbra d1,.bytes
.next:
        tst.w d0
        bne .token
        movem.l (sp)+,d2-d3
        rts
.literal:
        moveq #0,d2
        move.b (a0)+,d2
        subq.w #1,d0
        sub.w d2,d0
        subq.w #1,d2
.copy:
        move.b (a0)+,(a1)+
        dbra d2,.copy
        bra .next
.raw:
        moveq #47,d2
        clr.w d0
        bra .copy
        even
.patterns:
        dc.l $00000000,$55555555,$aaaaaaaa,$ffffffff
