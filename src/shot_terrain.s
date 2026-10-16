        section text
        xdef RR_ShotTerrainCPU
; a0=first aligned terrain group, d0=x&15, d1=rows (0..16).
; M2PF & 3: COLPF0/1, not COLBK or water. Road inks 4..6 are
; recoloured COLPF0. The clean background contains no sprite colours.
RR_ShotTerrainCPU:
        tst.w d1
        beq .empty
        move.l d3,-(sp)
        move.w #$c000,d2
        lsr.w d0,d2
        subq.w #1,d1
        cmpi.w #15,d0
        beq .spill
.loop:
        move.w (a0),d0
        move.w 2(a0),d3
        eor.w d3,d0
        or.w 4(a0),d0
        and.w d2,d0
        bne .hit
        lea 160(a0),a0
        dbra d1,.loop
        bra .miss
.spill:
        move.w (a0),d0
        move.w 2(a0),d3
        eor.w d3,d0
        or.w 4(a0),d0
        and.w d2,d0
        bne .hit
        move.w 8(a0),d0
        move.w 10(a0),d3
        eor.w d3,d0
        or.w 12(a0),d0
        andi.w #$8000,d0
        bne .hit
        lea 160(a0),a0
        dbra d1,.spill
.miss:
        moveq #0,d0
        bra .done
.hit:
        moveq #1,d0
.done:
        move.l (sp)+,d3
        rts
.empty:
        moveq #0,d0
        rts
