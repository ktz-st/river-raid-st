#ifndef RR_AUDIO_PSG_H
#define RR_AUDIO_PSG_H
#include <godlib/base/base.h>
typedef struct { U16 tone; U8 noise,volume,mix,pad; } RRPsgFrame;
typedef struct { const RRPsgFrame *frames; U16 count; } RRPsgEffect;
extern const RRPsgEffect rrPsgEffects[21];
U8 RRPsg_Init(void);
void RRPsg_DeInit(void);
U8 RRPsg_Play(U16 channel,U16 effect,U8 loop);
void RRPsg_Stop(U16 channel);
U8 RRPsg_IsPlaying(U16 channel);
void RRPsg_Vbl(void);
extern U8 gPsgRegisters[14];
extern U32 gPsgTicks;
/* Physical A=shots/life, B=fuel/warning, C=engine or explosion.
 * Logical voice 3 temporarily borrows C from engine voice 2. */
extern S16 gAudioExplosion;
#endif
