        section text
        xdef RR_HudCell
        xref rrHudMasks
        xref rrHudCellOffsets,rrHudNativeOffsets,rrHudNativePlanes
; a0=320px panel, d0.w=cell 0..59, d1.w=character/ink code.
; Fully opaque glyph: write four planar words, including background colour 5.
RR_HudCell:
        movem.l d2-d4,-(sp)
        move.w d0,d2
        add.w d2,d2
        lea rrHudCellOffsets,a1
        move.w (a1,d2.w),d2
        adda.w d2,a0
        move.w d1,d0
        andi.w #63,d0
        beq .blank
        add.w d0,d0
        lea rrHudNativeOffsets,a1
        move.w (a1,d0.w),d0
        bmi .mask
        lea rrHudNativePlanes,a1
        adda.w d0,a1
        lsr.w #6,d1
        cmpi.w #1,d1
        beq .native_blue
        cmpi.w #3,d1
        beq .native_yellow
        move.l (a1)+,(a0)
        move.l (a1)+,4(a0)
rrHudOfs set 160
        rept 7
        move.l (a1)+,rrHudOfs(a0)
        move.l (a1)+,rrHudOfs+4(a0)
rrHudOfs set rrHudOfs+160
        endr
        bra .done
.native_blue:
        move.l (a1)+,d2
        move.l (a1)+,(a0)
        move.l d2,4(a0)
rrHudOfs set 160
        rept 7
        move.l (a1)+,d2
        move.l (a1)+,rrHudOfs(a0)
        move.l d2,rrHudOfs+4(a0)
rrHudOfs set rrHudOfs+160
        endr
        bra .done
.native_yellow:
        move.l #$ffff0000,d3
        move.l d3,(a0)
        move.l 4(a1),4(a0)
rrHudOfs set 160
rrHudSrc set 12
        rept 7
        move.l d3,rrHudOfs(a0)
        move.l rrHudSrc(a1),rrHudOfs+4(a0)
rrHudOfs set rrHudOfs+160
rrHudSrc set rrHudSrc+8
        endr
        bra .done
.blank:
        move.l #$ffff0000,d2
        move.l d2,(a0)
        move.l d2,4(a0)
rrHudOfs set 160
        rept 7
        move.l d2,rrHudOfs(a0)
        move.l d2,rrHudOfs+4(a0)
rrHudOfs set rrHudOfs+160
        endr
        bra .done
.mask:
        andi.w #$7fff,d0
        lea rrHudMasks,a1
        adda.w d0,a1
        lsr.w #6,d1
        moveq #7,d4
        cmpi.w #1,d1
        beq.s .blue
        cmpi.w #3,d1
        beq.s .yellow
.pale:
        moveq #0,d3
        move.w (a1)+,d3
        move.l d3,d2
        not.w d2
        swap d2
        clr.w d2
        move.l d2,(a0)
        ori.l #$ffff0000,d3
        move.l d3,4(a0)
        lea 160(a0),a0
        dbra d4,.pale
        bra.s .done
.blue:
        moveq #0,d3
        move.w (a1)+,d3
        move.l d3,d2
        ori.l #$ffff0000,d3
        move.l d3,(a0)
        not.w d2
        swap d2
        clr.w d2
        move.l d2,4(a0)
        lea 160(a0),a0
        dbra d4,.blue
        bra.s .done
.yellow:
        moveq #0,d3
        move.w (a1)+,d3
        ori.l #$ffff0000,d3
        move.l #$ffff0000,(a0)
        move.l d3,4(a0)
        lea 160(a0),a0
        dbra d4,.yellow
.done:
        movem.l (sp)+,d2-d4
        rts

        xdef RR_HudScorePair
; A0 panel, D0 score cell 2..8, D1 blank or digit code 16..25.
; The equal high-score digit is ten cells (+80 bytes) to the right.
; Valid HUD required: score P1/P2 and record P0/P1/P2 are invariant.
RR_HudScorePair:
        move.l d2,-(sp)
        move.w d0,d2
        add.w d2,d2
        lea rrHudCellOffsets,a1
        move.w (a1,d2.w),d2
        adda.w d2,a0
        move.w d1,d0
        lsl.w #4,d0
        lea rrHudMasks,a1
        adda.w d0,a1
rrHudPairOfs set 0
        rept 8
        move.w (a1)+,d2
        move.w d2,rrHudPairOfs+6(a0)
        move.w d2,rrHudPairOfs+86(a0)
        not.w d2
        move.w d2,rrHudPairOfs(a0)
rrHudPairOfs set rrHudPairOfs+160
        endr
.done:
        move.l (sp)+,d2
        rts
