        section text
        xdef RR_BankCPU
; GCC fastcall: a0 mask, a1 first terrain group; d0 shift, d1 rows, d2 groups.
; Caller clips vertically and ensures the whole 16/32px sprite is in screen.
; Collision is exactly COLPF0: plane0 AND NOT plane1, not the brown outline.
RR_BankCPU:
        tst.w d1
        beq .empty
        movem.l d3-d6,-(sp)
        subq.w #1,d1
        moveq #16,d6
        sub.w d0,d6
        cmp.w #1,d2
        bne .wide
        tst.w d0
        beq .aligned1
        bra .shifted1
.wide:
        tst.w d0
        beq .aligned2
        bra .shifted2
.empty:
        moveq #0,d0
        rts
.aligned1:
        move.w (a0)+,d3
        not.w d3
        move.w 2(a1),d5
        not.w d5
        and.w (a1),d5
        and.w d3,d5
        bne .hit
        lea 160(a1),a1
        dbra d1,.aligned1
        moveq #0,d0
        bra .return
.shifted1:
        move.w (a0)+,d3
        not.w d3
        move.w d3,d4
        lsr.w d0,d4
        move.w 2(a1),d5
        not.w d5
        and.w (a1),d5
        and.w d4,d5
        bne .hit
        lsl.w d6,d3
        move.w 10(a1),d5
        not.w d5
        and.w 8(a1),d5
        and.w d3,d5
        bne .hit
        lea 160(a1),a1
        dbra d1,.shifted1
        moveq #0,d0
        bra .return
.aligned2:
        move.w (a0)+,d3
        not.w d3
        move.w 2(a1),d5
        not.w d5
        and.w (a1),d5
        and.w d3,d5
        bne .hit
        move.w (a0)+,d3
        not.w d3
        move.w 10(a1),d5
        not.w d5
        and.w 8(a1),d5
        and.w d3,d5
        bne .hit
        lea 160(a1),a1
        dbra d1,.aligned2
        moveq #0,d0
        bra .return
.shifted2:
        move.w (a0)+,d3
        not.w d3
        move.w d3,d4
        lsr.w d0,d4
        move.w 2(a1),d5
        not.w d5
        and.w (a1),d5
        and.w d4,d5
        bne .hit
        lsl.w d6,d3
        move.w 10(a1),d5
        not.w d5
        and.w 8(a1),d5
        and.w d3,d5
        bne .hit
        move.w (a0)+,d3
        not.w d3
        move.w d3,d4
        lsr.w d0,d4
        move.w 10(a1),d5
        not.w d5
        and.w 8(a1),d5
        and.w d4,d5
        bne .hit
        lsl.w d6,d3
        move.w 18(a1),d5
        not.w d5
        and.w 16(a1),d5
        and.w d3,d5
        bne .hit
        lea 160(a1),a1
        dbra d1,.shifted2
        moveq #0,d0
        bra .return
.hit:
        moveq #1,d0
.return:
        movem.l (sp)+,d3-d6
        rts
