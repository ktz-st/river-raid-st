#include "audio_psg.h"
#include <godlib/vbl/vbl.h>

typedef struct { U16 id,index; U8 active,loop; } Voice;
static volatile Voice sVoices[4];
static U8 sSaved[14];
static volatile U8 sEnabled;
_Static_assert(sizeof(RRPsgFrame)==6,"PSG frame ABI");
_Static_assert(sizeof(RRPsgEffect)==6,"PSG effect ABI");
U8 gPsgRegisters[14];
U32 gPsgTicks;
extern U16 RRPsg_Lock(void);
extern void RRPsg_Unlock(U16 sr);

#ifndef RR_PSG_TEST
static U8 Read(U16 reg)
{
    volatile U8 *psg=(volatile U8 *)0xffff8800UL;
    psg[0]=reg; return psg[0];
}
static void Write(U16 reg,U8 value)
{
    volatile U8 *psg=(volatile U8 *)0xffff8800UL;
    psg[0]=reg; psg[2]=value;
}
#else
extern U8 Read(U16 reg);
extern void Write(U16 reg,U8 value);
#endif

/* Only this callback owns the sound registers while enabled. Main-thread
   voice changes are protected against VBL; R14/R15 are never accessed. */
void RRPsg_Vbl(void)
{
    const RRPsgFrame *f[3];
    U16 c,r;
    S16 noiseOwner=-1;
    U8 mixer=sSaved[7]|63;
    if (!sEnabled) return;
    ++gPsgTicks;
    for (c=0;c<3;++c) {
        U16 voice=(c==2 && sVoices[3].active) ? 3 : c;
        volatile Voice *v=&sVoices[voice];
        f[c]=0;
        if (v->active) {
            const RRPsgEffect *e=&rrPsgEffects[v->id];
            f[c]=&e->frames[v->index];
            if (++v->index==e->count) {
                if (v->loop) v->index=0;
                else v->active=0;
            }
            /* An explosion borrowing C owns noise; tonal A/B keep playing. */
            if ((noiseOwner<0 || voice==3) && (f[c]->mix&2) && f[c]->volume)
                noiseOwner=c;
        }
    }
    for (c=0;c<3;++c) {
        const RRPsgFrame *p=f[c];
        U8 volume=p?p->volume:0;
        if (p) {
            gPsgRegisters[c*2]=p->tone;
            gPsgRegisters[c*2+1]=p->tone>>8;
            if (p->mix&1) mixer&=~(1<<c);
            if ((p->mix&2) && noiseOwner==(S16)c) mixer&=~(8<<c);
            else if (!(p->mix&1)) volume=0;
        }
        gPsgRegisters[8+c]=volume;
    }
    if (noiseOwner>=0) gPsgRegisters[6]=f[noiseOwner]->noise;
    gPsgRegisters[7]=mixer;
    /* Eleven bounded byte writes, no divisions, PCM or hardware envelopes. */
    for (r=0;r<11;++r) Write(r,gPsgRegisters[r]);
}

U8 RRPsg_Init(void)
{
    U16 r,sr=RRPsg_Lock();
    if (sEnabled) { RRPsg_Unlock(sr); return 1; }
    for (r=0;r<14;++r) sSaved[r]=gPsgRegisters[r]=Read(r);
    for (r=0;r<4;++r) sVoices[r].active=0;
    if (!Vbl_AddCall(RRPsg_Vbl)) { RRPsg_Unlock(sr); return 0; }
    sEnabled=1;
    RRPsg_Vbl();
    RRPsg_Unlock(sr);
    return 1;
}
void RRPsg_DeInit(void)
{
    U16 r,sr=RRPsg_Lock();
    if (sEnabled) {
        sEnabled=0; Vbl_RemoveCall(RRPsg_Vbl);
        for (r=0;r<14;++r) Write(r,sSaved[r]);
    }
    RRPsg_Unlock(sr);
}
U8 RRPsg_Play(U16 channel,U16 effect,U8 loop)
{
    U16 sr;
    if (!sEnabled || channel>=4 || effect>=21) return 0;
    sr=RRPsg_Lock();
    sVoices[channel].active=0;
    sVoices[channel].id=effect; sVoices[channel].index=0;
    sVoices[channel].loop=loop; sVoices[channel].active=1;
    RRPsg_Unlock(sr); return 1;
}
void RRPsg_Stop(U16 channel)
{
    U16 sr=RRPsg_Lock();
    if (channel<4) sVoices[channel].active=0;
    RRPsg_Unlock(sr);
}
U8 RRPsg_IsPlaying(U16 channel)
{
    return sEnabled && channel<4 && sVoices[channel].active;
}
