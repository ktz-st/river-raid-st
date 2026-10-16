#include "audio.h"
#include "audio_psg.h"
#include "player.h"
#include "enemies.h"

U16 gAudioReady, gAudioEvents;
S16 gAudioEngine=-1, gAudioAuxiliary=-1, gAudioEffect=-1;
U32 gAudioPlayed[21];
S16 gAudioExplosion=-1;
static U16 sPreviousFuel=65535, sFullTicks;

static void Play(U16 channel,U16 effect,U8 loop)
{
    if (RRPsg_Play(channel,effect,loop)) ++gAudioPlayed[effect];
}

void RRAudio_Reset(void)
{
    U16 channel;
    if (gAudioReady) for (channel=0;channel<4;++channel) RRPsg_Stop(channel);
    gAudioExplosion=-1;
    gAudioEvents=0;
    gAudioEngine=gAudioAuxiliary=gAudioEffect=-1;
    sFullTicks=0; sPreviousFuel=65535;
}

U8 RRAudio_Init(void)
{
    gAudioReady=RRPsg_Init();
    RRAudio_Reset();
    return gAudioReady;
}

void RRAudio_DeInit(void)
{
    if (gAudioReady) {
        RRAudio_Reset();
        RRPsg_DeInit();
    }
    gAudioReady=0;
}

void RRAudio_Event(U16 event) { gAudioEvents|=event; }

void RRAudio_Update(U16 speed)
{
    S16 engine=-1,auxiliary=-1,effect=-1;
    U16 events=gAudioEvents;
    gAudioEvents=0;
    if (!gAudioReady) return;
    if (gPlayerState==RR_PLAYER_FLYING) {
        if (gRefueling && gFuel>=65503 && sPreviousFuel<65503) sFullTicks=8;
        if (sFullTicks) { auxiliary=13; --sFullTicks; }
        else if (gRefueling) auxiliary=12;
        else {
            engine=(S16)(speed>>4)-4;
            if (engine<0) engine=0;
            if (engine>11) engine=11;
            if (gFuel<0x4000) auxiliary=19;
        }
    } else sFullTicks=0;
    sPreviousFuel=gFuel;
    /* Do not restart effects every frame: register sequences must advance. */
    if (gAudioEngine!=engine) {
        RRPsg_Stop(2); gAudioEngine=engine;
        if (engine>=0) Play(2,(U16)engine,1);
    }
    if (gAudioAuxiliary!=auxiliary) {
        RRPsg_Stop(1); gAudioAuxiliary=auxiliary;
        if (auxiliary>=0) Play(1,(U16)auxiliary,auxiliary!=13);
    }
    if (gPlayerState!=RR_PLAYER_FLYING && gPlayerState!=RR_PLAYER_EXPLODING) {
        RRPsg_Stop(0); gAudioEffect=-1;
        RRPsg_Stop(3); gAudioExplosion=-1;
        return;
    }
    /* A remains available for shots while an explosion borrows engine C.
     * Logical voice 2 retains the desired pitch; the backend resumes it. */
    if ((events&RR_SFX_SHOT) &&
        !(gAudioEffect==18 && RRPsg_IsPlaying(0))) {
        Play(0,14,0); gAudioEffect=14;
    }
    if (events&RR_SFX_LIFE) { Play(0,18,0); gAudioEffect=18; }
    if (events&RR_SFX_ENEMY) effect=20;
    if (events&RR_SFX_HIT) effect=15;
    if (events&RR_SFX_CRASH) effect=16;
    if (events&RR_SFX_EMPTY) effect=17;
    if (effect>=0) { Play(3,(U16)effect,0); gAudioExplosion=effect; }
    if (!RRPsg_IsPlaying(0)) gAudioEffect=-1;
    if (!RRPsg_IsPlaying(3)) gAudioExplosion=-1;
}
