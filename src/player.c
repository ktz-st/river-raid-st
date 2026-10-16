#include <godlib/graphic/graphic.h>
#include <godlib/ikbd/ikbd.h>
#include <godlib/screen/screen.h>
#include <godlib/sprite/sprite.h>
#include "player.h"
#include "restore.h"
#include <string.h>
#include "native_assets.h"
#include "audio.h"
#include "bank.h"
#include "enemies.h"
#include "sprite_cpu.h"

S16 gPlayerX;
U16 gPlayerFrame;
U16 gPlayerVisible;
U16 gPlayerState, gPlayerLives, gPlayerDeathTicks, gPlayerCollisionEnabled;
static U16 gPreviousFire;
static U32 gNextLifeScore=10000UL;

static U16 FireHeld(U8 joy)
{
    return IKBD_GetKeyStatus(eIKBDSCAN_LEFTSHIFT) ||
        IKBD_GetKeyStatus(eIKBDSCAN_RIGHTSHIFT) || (joy & 128);
}

typedef struct {
    S16 x, y;
    U16 frame, valid;
} RRPlayerSaved;

RRPlayerSaved gPlayerSaved[2];
static sSprite gPlayerSprites[7];
#ifdef RR_LEGACY_ROM_ASSETS
static U16 rrLegacyPlayerMasks[7][RR_PLAYER_HEIGHT];
#define gPlayerMasks rrLegacyPlayerMasks
static U16 gPlayerGfx[7][RR_PLAYER_HEIGHT * 4];
#endif

void RRPlayer_Init(const U8 *rom)
{
#ifdef RR_LEGACY_ROM_ASSETS
    U16 frame, y, x, plane;
    for (frame=0; frame<7; ++frame) {
        /* A8 PMG states: neutral 9, right 8, left 7; each bit is 2 ST pixels. */
        U16 state=frame<3 ? 9-frame : frame;
        U16 table=0xbacd+state*2;
        U16 address=(U16)rom[table-0xa000] | ((U16)rom[table+1-0xa000]<<8);
        for (y=0; y<RR_PLAYER_HEIGHT; ++y) {
            U16 bits=0;
            for (x=0; x<8; ++x)
                if (rom[address+y-0xa000] & (0x80>>x)) bits |= 0xc000>>(x*2);
            gPlayerMasks[frame][y]=(U16)~bits;
            for (plane=0; plane<4; ++plane)
                gPlayerGfx[frame][y*4+plane]=(12 & (1<<plane)) ? bits : 0;
        }
        gPlayerSprites[frame].mpGfx=gPlayerGfx[frame];
        gPlayerSprites[frame].mpMask=gPlayerMasks[frame];
        gPlayerSprites[frame].mWidth=16;
        gPlayerSprites[frame].mHeight=RR_PLAYER_HEIGHT;
        gPlayerSprites[frame].mGfxPlaneCount=4;
        gPlayerSprites[frame].mMaskPlaneCount=4;
    }
#else
    (void)rom;
    memcpy(gPlayerSprites, rrPlayerSprites, sizeof(gPlayerSprites));
#endif
    gPreviousFire=0;
    gPlayerCollisionEnabled=1;
    RRPlayer_Respawn(1);
}

void RRPlayer_Respawn(U16 newGame)
{
    if (newGame) { gPlayerLives=3; gNextLifeScore=10000UL; }
    gPlayerState=RR_PLAYER_READY;
    gPlayerDeathTicks=0;
    gPlayerX=152;
    gPlayerFrame=0;
    gPlayerVisible=1;
    gPlayerSaved[0].valid=gPlayerSaved[1].valid=0;
    /* A held trigger from the previous life must not launch the new one. */
    gPreviousFire=FireHeld(IKBD_GetJoy1Packet());
}

void RRPlayer_RewardScore(U32 score)
{
    /* A8 $A574: consume each 10000-point boundary, even at the nine-life cap. */
    while (gNextLifeScore && score>=gNextLifeScore) {
        gNextLifeScore+=10000UL;
        if (gNextLifeScore<10000UL) gNextLifeScore=0;
        if (gPlayerLives<9) {
            ++gPlayerLives;
            RRAudio_Event(RR_SFX_LIFE);
        }
    }
}

U16 RRPlayer_Update(U8 joy)
{
    U16 left=IKBD_GetKeyStatus(eIKBDSCAN_LEFTARROW) || (joy & 4);
    U16 right=IKBD_GetKeyStatus(eIKBDSCAN_RIGHTARROW) || (joy & 8);
    U16 fire=FireHeld(joy);
    U16 pressed=fire && !gPreviousFire;
    gPreviousFire=fire;
    if (gPlayerState==RR_PLAYER_EXPLODING) {
        if (++gPlayerDeathTicks>=40) {
            gPlayerVisible=0;
            gPlayerState=gPlayerLives ? RR_PLAYER_WAIT : RR_PLAYER_GAME_OVER;
            return 1;
        } else gPlayerFrame=3+gPlayerDeathTicks/10;
        return 0;
    }
    if (gPlayerState==RR_PLAYER_READY) {
        if (!pressed) return 0;
        gPlayerState=RR_PLAYER_FLYING;
    }
    if (gPlayerState!=RR_PLAYER_FLYING) return pressed;
    gPlayerFrame=0;
    if (left != right) {
        gPlayerFrame=left ? 2 : 1;
        gPlayerX+=left ? -2 : 2;
        if (gPlayerX<0) gPlayerX=0;
        if (gPlayerX>304) gPlayerX=304;
    }
    return 0;
}

void RRPlayer_CheckBank(S16 scroll)
{
    U16 shift=(U16)gPlayerX & 15;
    const U16 *src;
    if (!gPlayerCollisionEnabled || !gPlayerVisible || gPlayerState!=RR_PLAYER_FLYING) return;
    src=(const U16 *)((const U8 *)Screen_GetpBack()
        +(U32)(scroll+RR_PLAYER_Y)*160UL+(U16)(gPlayerX & ~15)/2);
    /* Same COLPF0 mask test as before, using the already verified enemy-bank
     * assembly kernel. Player bounds guarantee that clipping is unnecessary. */
    if (RR_BankCPU(gPlayerMasks[gPlayerFrame],src,shift,RR_PLAYER_HEIGHT,1))
        RRPlayer_Kill();
}

U16 RRPlayer_Bits(U16 row)
{
    return row<RR_PLAYER_HEIGHT ? (U16)~gPlayerMasks[gPlayerFrame][row] : 0;
}

void RRPlayer_Kill(void)
{
    if (gPlayerState!=RR_PLAYER_FLYING) return;
    RRAudio_Event(gFuel ? RR_SFX_CRASH : RR_SFX_EMPTY);
    if (gPlayerLives) --gPlayerLives;
    gPlayerState=RR_PLAYER_EXPLODING;
    gPlayerDeathTicks=0;
    gPlayerFrame=3;
}

void RRPlayer_Restore(void)
{
    RRPlayerSaved *saved=&gPlayerSaved[Screen_GetLogicIndex()];
    U16 groups;
    U32 offset;
    U32 *dst;
    const U32 *src;
    if (!saved->valid) return;
    /* Restore whole 16-pixel groups at the OLD absolute canvas Y. The clean
     * page never contains sprites; U32 offsets also cover rows above 64 KiB. */
    offset=(U32)saved->y*160UL + (U16)(saved->x & ~15)/2;
    groups=(saved->x & 15) ? 2 : 1;
    src=(const U32 *)((const U8 *)Screen_GetpBack()+offset);
    dst=(U32 *)((U8 *)Screen_GetpLogic()+offset);
    RR_RestorePlayer(src,dst,groups,RR_PLAYER_HEIGHT);
    saved->valid=0;
}

void RRPlayer_Draw(S16 scroll)
{
    sGraphicPos pos;
    RRPlayerSaved *saved=&gPlayerSaved[Screen_GetLogicIndex()];
    if (!gPlayerVisible) return;
    pos.mX=gPlayerX;
    pos.mY=scroll+RR_PLAYER_Y;
#ifdef RR_SPRITE_BLITTER
    Screen_Logic_DrawSprite(&pos, &gPlayerSprites[gPlayerFrame]);
#else
    RR_MonoSprite(rrPlayerMono[gPlayerFrame],
        (U8*)Screen_GetpLogic()+(U32)pos.mY*160UL+(U16)(pos.mX&~15)/2,
        (U16)pos.mX&15,RR_PLAYER_HEIGHT);
#endif
    saved->x=pos.mX; saved->y=pos.mY;
    saved->frame=gPlayerFrame; saved->valid=1;
}
