#include "bank.h"

/* Original per-row/group test, retained for horizontally clipped objects. */
U16 RR_BankReference(const RRBankQuery *q, const U16 *mask, const U16 *screen)
{
    U16 row, group, shift=(U16)q->x&15;
    S16 start=q->x&~15;
    for (row=0; row<q->height; ++row) {
        S16 y=q->y+row;
        const U16 *line;
        if (y<0 || y>=160) continue;
        line=(const U16 *)((const U8 *)screen+(U32)(q->scroll+y)*160UL);
        for (group=0; group<q->groups; ++group) {
            S16 x=start+group*16;
            U16 bits=(U16)~mask[row*q->groups+group];
            if (x>=0 && x<320 && ((line[x/4] & ~line[x/4+1]) & (bits>>shift))) return 1;
            x+=16;
            if (shift && x>=0 && x<320 &&
                ((line[x/4] & ~line[x/4+1]) & (U16)(bits<<(16-shift)))) return 1;
        }
    }
    return 0;
}

U16 RR_Bank(const RRBankQuery *q, const U16 *mask, const U16 *screen)
{
    S16 y=q->y;
    U16 height=q->height;
    U32 offset;
    if (q->groups<1 || q->groups>2 || q->x<0 ||
        q->x+(S16)(q->groups*16)>320)
        return RR_BankReference(q,mask,screen);
    if (y>=160 || y+(S16)height<=0 || !height) return 0;
    if (y<0) {
        U16 skip=(U16)-y;
        mask+=skip*q->groups;
        height-=skip;
        y=0;
    }
    if (height>160-y) height=160-y;
    offset=(U32)(U16)(q->scroll+y)*160UL+(U16)(q->x&~15)/2;
    return RR_BankCPU(mask,(const U16 *)((const U8 *)screen+offset),
        (U16)q->x&15,height,q->groups);
}
