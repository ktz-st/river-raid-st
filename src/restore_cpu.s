        section text
        xdef RR_RestoreCPU
; GCC fastcall: a0 source, a1 destination, d0.w groups, d1.w height.
; Only volatile registers changed. No pixel masking: copy all four planes.
RR_RestoreCPU:
        tst.w d1
        beq .done
        cmp.w #1,d0
        beq .width1
        cmp.w #2,d0
        beq .width2
        cmp.w #3,d0
        beq .width3
.done:
        rts
.width1:
        cmp.w #4,d1
        blo .tail1
.loop1:
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        lea 152(a0),a0
        lea 152(a1),a1
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        lea 152(a0),a0
        lea 152(a1),a1
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        lea 152(a0),a0
        lea 152(a1),a1
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        lea 152(a0),a0
        lea 152(a1),a1
        subq.w #4,d1
        cmp.w #4,d1
        bhs .loop1
.tail1:
        subq.w #1,d1
        bmi .done1
.row1:
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        lea 152(a0),a0
        lea 152(a1),a1
        dbra d1,.row1
.done1:
        rts
.width2:
        cmp.w #4,d1
        blo .tail2
.loop2:
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        lea 144(a0),a0
        lea 144(a1),a1
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        lea 144(a0),a0
        lea 144(a1),a1
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        lea 144(a0),a0
        lea 144(a1),a1
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        lea 144(a0),a0
        lea 144(a1),a1
        subq.w #4,d1
        cmp.w #4,d1
        bhs .loop2
.tail2:
        subq.w #1,d1
        bmi .done2
.row2:
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        lea 144(a0),a0
        lea 144(a1),a1
        dbra d1,.row2
.done2:
        rts
.width3:
        cmp.w #4,d1
        blo .tail3
.loop3:
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        lea 136(a0),a0
        lea 136(a1),a1
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        lea 136(a0),a0
        lea 136(a1),a1
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        lea 136(a0),a0
        lea 136(a1),a1
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        lea 136(a0),a0
        lea 136(a1),a1
        subq.w #4,d1
        cmp.w #4,d1
        bhs .loop3
.tail3:
        subq.w #1,d1
        bmi .done3
.row3:
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        move.l (a0)+,(a1)+
        lea 136(a0),a0
        lea 136(a1),a1
        dbra d1,.row3
.done3:
        rts
