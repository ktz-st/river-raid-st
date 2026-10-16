    section text
    xdef RRPsg_Lock,RRPsg_Unlock
RRPsg_Lock:
    move.w sr,d0
    ori.w #$0700,sr
    rts
RRPsg_Unlock:
    move.w d0,sr
    rts
