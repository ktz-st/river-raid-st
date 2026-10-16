        section text
        xdef RR_CopyCheckpoint
; Fastcall a0=source, a1=destination, d0.l=bytes.
; Even disjoint buffers, size divisible by four, at most 4 MB.
; Preserve atomic checkpoint semantics; only volatile registers are changed.
RR_CopyCheckpoint:
        move.l d0,d1
        lsr.l #8,d1
        and.w #255,d0
        subq.w #1,d1
        bmi .tail
.block:
        rept 64
        move.l (a0)+,(a1)+
        endr
        dbra d1,.block
.tail:
        lsr.w #2,d0
        subq.w #1,d0
        bmi .done
.word:
        move.l (a0)+,(a1)+
        dbra d0,.word
.done:
        rts
