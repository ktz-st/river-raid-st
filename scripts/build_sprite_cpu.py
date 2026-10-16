"""Generate constant-colour planar CPU row handlers, never per-pixel code."""
from build_paths import GENERATED
lines=['''        section text
        xdef RR_MonoSprite
        xref rrMonoShifts32
; a0 = [colour*64, opaque16] rows, a1 = aligned destination group.
; d0.w = shift 0..15, d1.w = clipped height. Caller guarantees X bounds.
; Four planes, fixed 160-byte canvas stride, no BLiTTER or pixel loop.
RR_MonoSprite:
        tst.w d1
        beq .empty
        movem.l d2-d5/a2-a3,-(sp)
        subq.w #1,d1
        tst.w d0
        beq .aligned
        lea .shift_table(pc),a2
.shift_row:
        move.w (a0)+,d4
        lea (a2,d4.w),a3
        moveq #0,d2
        move.w (a0)+,d2
        beq .blank_shift
        swap d2
        lsr.l d0,d2
        move.l d2,d3
        swap d2
        move.w d2,d4
        not.w d4
        move.w d3,d5
        not.w d5
        jmp (a3)
.next_shift:
        lea 144(a1),a1
        dbra d1,.shift_row
        bra .done
.blank_shift:
        lea 16(a1),a1
        bra .next_shift
.aligned:
        lea .aligned_table(pc),a2
.aligned_row:
        move.w (a0)+,d4
        lea (a2,d4.w),a3
        move.w (a0)+,d2
        beq .blank_aligned
        move.w d2,d4
        not.w d4
        jmp (a3)
.next_aligned:
        lea 152(a1),a1
        dbra d1,.aligned_row
        bra .done
.blank_aligned:
        addq.l #8,a1
        bra .next_aligned
.done:
        movem.l (sp)+,d2-d5/a2-a3
.empty:
        rts''']
for mode in ('shift','aligned'):
    lines.append('.%s_table:'%mode)
    for ink in range(16):
        lines.append('.%s_%d:'%(mode,ink))
        for plane in range(4):
            op='or' if ink&(1<<plane) else 'and'
            reg='d2' if op=='or' else 'd4'
            lines.append('        %s.w %s,(a1)+'%(op,reg))
        if mode=='shift':
            for plane in range(4):
                op='or' if ink&(1<<plane) else 'and'
                reg='d3' if op=='or' else 'd5'
                lines.append('        %s.w %s,(a1)+'%(op,reg))
        lines.append('        bra.w .next_%s'%mode)
        lines.append('        ds.b 64-(*-.%s_%d)'%(mode,ink))
lines.append('''
        xdef RR_MonoSprite32
; Same ABI, [colour*64, shared mask offset, padding] rows, 32 source pixels.
RR_MonoSprite32:
        tst.w d1
        beq .empty32
        movem.l d2-d7/a2-a4,-(sp)
        move.w d0,d2
        lsl.w #2,d2
        lea rrMonoShifts32,a4
        movea.l (a4,d2.w),a4
        subq.w #1,d1
        tst.w d0
        beq .aligned32
        lea .shift32_table(pc),a2
.shift32_row:
        move.w (a0)+,d4
        lea (a2,d4.w),a3
        move.w (a0)+,d2
        addq.l #2,a0
        move.l (a4,d2.w),d3
        move.w 4(a4,d2.w),d4
        move.l d3,d2
        swap d2
        move.w d2,d5
        not.w d5
        move.w d3,d6
        not.w d6
        move.w d4,d7
        not.w d7
        jmp (a3)
.next_shift32:
        lea 136(a1),a1
        dbra d1,.shift32_row
        bra .done32
.aligned32:
        lea .aligned32_table(pc),a2
.aligned32_row:
        move.w (a0)+,d4
        lea (a2,d4.w),a3
        move.w (a0)+,d2
        addq.l #2,a0
        move.l (a4,d2.w),d2
        beq .blank_aligned32
        move.l d2,d3
        swap d2
        move.w d2,d5
        not.w d5
        move.w d3,d6
        not.w d6
        jmp (a3)
.next_aligned32:
        lea 144(a1),a1
        dbra d1,.aligned32_row
        bra .done32
.blank_aligned32:
        lea 16(a1),a1
        bra .next_aligned32
.done32:
        movem.l (sp)+,d2-d7/a2-a4
.empty32:
        rts''')
for mode,groups in (('shift32',3),('aligned32',2)):
    lines.append('.%s_table:'%mode)
    for ink in range(16):
        lines.append('.%s_%d:'%(mode,ink))
        for group in range(groups):
            for plane in range(4):
                op='or' if ink&(1<<plane) else 'and'
                reg='d%d'%((2 if op=='or' else 5)+group)
                lines.append('        %s.w %s,(a1)+'%(op,reg))
        lines.append('        bra.w .next_%s'%mode)
        lines.append('        ds.b 64-(*-.%s_%d)'%(mode,ink))
(GENERATED/'sprite_cpu.s').write_text('\n'.join(lines)+'\n')
