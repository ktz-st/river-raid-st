;*************************************************************************************
;* Handler TimerB splitu ekranu
;*
;* Counter writes are synchronized to the right border of the last terrain
;* row. All three address bytes must arrive before fetching the first HUD row.
;*************************************************************************************

		xdef	RRSplit_TimerB
		xref	gRRSplitPanel
		xref	gRRSplitTimerCMask
		xref	gRRSplitReload

		section text

RRSplit_TimerB:
		movem.l	d0-d1,-(sp)

;	Bliter trzyma magistrale i potrafilby rozjechac zapis licznika
;	w trakcie pobierania linii - wstrzymujemy go na czas zapisu.
		move.w	$ffff8a3c.w,d1
		bclr	#7,$ffff8a3c.w

; TimerB entered at the end of terrain row 158 and reloaded 159.
; Its next decrement marks the end of row 159. Do all three counter
; writes in that horizontal blank, before the HUD row starts fetching.
		move.b	gRRSplitReload,d0
.wait_de:
		cmp.b	$fffffa21.w,d0
		beq.s	.wait_de
		move.b	gRRSplitPanel+1,$ffff8205.w
		move.b	gRRSplitPanel+2,$ffff8207.w
		move.b	gRRSplitPanel+3,$ffff8209.w
		clr.b	$fffffa1b.w

		move.w	d1,$ffff8a3c.w			; wznow bliter

;	oddaj TimerC TOS-u, odlozony na czas klatki przez RRSplit_Video
		tst.w	gRRSplitTimerCMask
		beq.s	.ack
		bset	#5,$fffffa15.w

.ack:
		bclr	#0,$fffffa0f.w			; ISRA: kasowanie bitu zapisem zera
		movem.l	(sp)+,d0-d1
		rte
