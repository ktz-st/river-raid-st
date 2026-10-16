#ifndef RR_AUDIO_H
#define RR_AUDIO_H
#include <godlib/base/base.h>
enum { RR_SFX_SHOT=1, RR_SFX_HIT=2, RR_SFX_CRASH=4,
       RR_SFX_EMPTY=8, RR_SFX_LIFE=16, RR_SFX_ENEMY=32 };
U8 RRAudio_Init(void);
void RRAudio_DeInit(void);
void RRAudio_Reset(void);
void RRAudio_Event(U16 event);
void RRAudio_Update(U16 speed);
extern U16 gAudioReady, gAudioEvents;
extern S16 gAudioEngine, gAudioAuxiliary, gAudioEffect;
extern U32 gAudioPlayed[21];
#endif
