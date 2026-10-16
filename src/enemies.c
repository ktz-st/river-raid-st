#include <godlib/graphic/graphic.h>
#include <godlib/screen/screen.h>
#include <godlib/sprite/sprite.h>
#include <godlib/ikbd/ikbd.h>
#include "enemies.h"
#include "actors.h"
#include "player.h"
#include "fuel_rules.h"
#include "restore.h"
#include "bank.h"
#include "bridge_tanks.h"
#include <string.h>
#include "native_assets.h"
#include "mask_overlap.h"
#include "audio.h"
#include "sprite_cpu.h"
#include "shot_terrain.h"

#define COUNT 24
#define TANK_COUNT 4
#define DIRTY_COUNT (COUNT+TANK_COUNT+2)
typedef struct { S32 world; A8Actor actor; } Enemy;
typedef struct {
    S32 world;
    S16 x;
    A8BridgeTank state;
    U16 used, stationary, fired, broken;
} Tank;
typedef struct {
    S32 world;
    S16 x, y, dx;
    U16 active, age, pose, owner;
} EnemyShot;
static Tank tanks[TANK_COUNT];
static EnemyShot enemyShots[2];
U32 gEnemyShots, gEnemyShotHits;
typedef struct { S16 x, y; U16 groups, height; S16 spriteX; U16 pose, skip; } Dirty;
static Enemy enemies[COUNT];
static Dirty dirty[2][DIRTY_COUNT];
static U16 dirtyCount[2], ticks;
static S32 distance, bridgeWorld;
static U16 bridgePending;
static S16 bridgeX;
S16 gLaunchX=152;
static struct {
    Enemy actors[COUNT];
    Tank tanks[TANK_COUNT];
    S32 distance, bridgeWorld;
    U32 randomState;
    U16 ticks, bridgePending, bridge;
    S16 bridgeX, launchX;
} checkpoint;
static U32 randomState;
static sSprite sprites[129];
#ifdef RR_LEGACY_ROM_ASSETS
/* Reference converter, compiled only by verify-native-assets. */
static U16 masks[84][20], gfx[84][80];
static U16 fuelMasks[6][24], fuelGfx[6][96];
static U16 bridgeMasks[6][80], bridgeGfx[6][320];
static U16 tankMasks[8][11], tankGfx[8][44];
static U16 shellMasks[7][16], shellGfx[7][64];
#endif
U16 gFuel=65535, gRefueling, gFuelEnabled=1;
#ifdef RR_SHOT_BLITTER
static sSprite shotSprite;
static U16 shotMask[8], shotGfx[32];
#endif
static Dirty shotDirty[2];
static U16 shotSaved[2];
static U16 spritesReady;
S16 gShotX, gShotY;
U16 gShotActive;
U32 gScore, gHighScore, gShots, gHits;
U16 gEnemiesEnabled=1, gEnemyBridge;
U16 gBridgeCrashAdvance;
U32 gEnemyMoves, gEnemyBounces, gEnemyOverflow;

static U16 ExplosionBase(U16 kind)
{
    if (kind==7) return 88;
    if (kind==12) return 98;
    return 32+(kind==13 ? 4 : kind-8)*10;
}

static U16 Supported(U16 kind)
{
    return kind>=7 && kind<=15;
}

static U16 Pose(const A8Actor *a)
{
    if (a->kind==15) return a->frame==15 ? 82 : 83+a->frame-2;
    if (a->kind==14) return a->frame==14 ? 108 : 109+a->frame-2;
    if (a->frame>=2 && a->frame<=6) {
        return ExplosionBase(a->kind)+((a->flags&8) ? 5 : 0)+a->frame-2;
    }
    return a8_actor_pose(a);
}

void RREnemies_Init(const U8 *rom, const U8 *inks)
{
#ifdef RR_LEGACY_ROM_ASSETS
    U16 n, direction, index=0, phase;
    static const U8 kinds[7]={8,9,10,11,13,7,12};
    if (spritesReady) return;
    for (phase=0; phase<6; ++phase)
    for (direction=0; direction<2; ++direction) for (n=0; n<7; ++n,++index) {
        U16 kind=kinds[n], frame=phase ? phase+1 : kind;
        U16 pose=phase ? ExplosionBase(kind)+direction*5+phase-1 : kind+direction*16;
        U16 height=rom[0x1b60+kind]+1;
        U16 scale=kind==13 ? 4 : 2, groups=scale/2;
        U16 address=0x1900+rom[0x1b40+frame+direction*16];
        U16 colour=0x1700+rom[0x1b30+kind];
        U16 y, bit, group, plane;
        for (y=0; y<height; ++y) {
            U32 bits=0;
            U16 ink=inks[rom[colour+height-1-y]];
            for (bit=0; bit<8; ++bit)
                if (rom[address+height-1-y] & (128>>bit))
                    bits |= (scale==4 ? 0xf0000000UL : 0xc0000000UL)>>(bit*scale);
            for (group=0; group<groups; ++group) {
                U16 word=(U16)(bits>>(16-group*16));
                masks[index][y*groups+group]=(U16)~word;
                for (plane=0; plane<4; ++plane)
                    gfx[index][(y*groups+group)*4+plane]=(ink&(1<<plane)) ? word : 0;
            }
        }
        sprites[pose].mpGfx=gfx[index];
        sprites[pose].mpMask=masks[index];
        sprites[pose].mWidth=scale*8;
        sprites[pose].mHeight=height;
        sprites[pose].mGfxPlaneCount=sprites[pose].mMaskPlaneCount=4;
    }
    /* FUEL uses the same PMG ROM tables, but is 24 rows tall and never moves. */
    for (phase=0; phase<6; ++phase) {
        U16 y, bit, plane, frame=phase ? phase+1 : 15;
        U16 address=0x1900+rom[0x1b40+frame];
        U16 colour=0x1700+rom[0x1b30+15];
        sSprite *s=&sprites[82+phase];
        for (y=0; y<24; ++y) {
            U16 bits=0, ink=inks[rom[colour+23-y]];
            for (bit=0; bit<8; ++bit)
                if (rom[address+23-y] & (128>>bit)) bits|=0xc000>>(bit*2);
            fuelMasks[phase][y]=(U16)~bits;
            for (plane=0; plane<4; ++plane)
                fuelGfx[phase][y*4+plane]=(ink&(1<<plane)) ? bits : 0;
        }
        s->mpGfx=fuelGfx[phase]; s->mpMask=fuelMasks[phase];
        s->mWidth=16; s->mHeight=24;
        s->mGfxPlaneCount=s->mMaskPlaneCount=4;
    }
    /* P3 quadruple-width bridge: keep all 64 bits, including blast masks. */
    for (phase=0; phase<6; ++phase) {
        U16 y, group, plane, frame=phase ? phase+1 : 14;
        U16 address=0x1900+rom[0x1b40+frame];
        U16 colour=0x1700+rom[0x1b30+14];
        sSprite *s=&sprites[108+phase];
        for (y=0; y<20; ++y) for (group=0; group<4; ++group) {
            U16 bits=0, ink=inks[rom[colour+19-y]];
            if (rom[address+19-y] & (128>>(group*2))) bits|=0xff00;
            if (rom[address+19-y] & (64>>(group*2))) bits|=0x00ff;
            bridgeMasks[phase][y*4+group]=(U16)~bits;
            for (plane=0; plane<4; ++plane)
                bridgeGfx[phase][y*16+group*4+plane]=(ink&(1<<plane)) ? bits : 0;
        }
        s->mpGfx=bridgeGfx[phase]; s->mpMask=bridgeMasks[phase];
        s->mWidth=64; s->mHeight=20;
        s->mGfxPlaneCount=s->mMaskPlaneCount=4;
    }
    for (phase=0; phase<15; ++phase) {
        U16 tank=phase<8, frame=tank ? phase : phase-8;
        U16 table=(tank ? 0xb4d1 : 0xbab7)+frame*2;
        U16 ptr=rom[table-0xa000]|((U16)rom[table+1-0xa000]<<8);
        U16 height=tank ? 11 : 16, ink=tank ? 13 : frame ? 15 : 12;
        U16 *mask=tank ? tankMasks[frame] : shellMasks[frame];
        U16 *graphic=tank ? tankGfx[frame] : shellGfx[frame];
        U16 y, bit, plane;
        sSprite *s=&sprites[114+phase];
        for (y=0; y<height; ++y) {
            U16 bits=0;
            if (frame) for (bit=0; bit<8; ++bit)
                if (rom[ptr+y-0xa000] & (128>>bit)) bits|=0xc000>>(bit*2);
            if (!tank && !frame && !y) bits=0xf000;
            mask[y]=(U16)~bits;
            for (plane=0; plane<4; ++plane)
                graphic[y*4+plane]=(ink&(1<<plane)) ? bits : 0;
        }
        s->mpGfx=graphic; s->mpMask=mask;
        s->mWidth=16; s->mHeight=height;
        s->mGfxPlaneCount=s->mMaskPlaneCount=4;
    }
#else
    (void)rom; (void)inks;
    if (spritesReady) return;
    memcpy(sprites, rrEnemySprites, sizeof(sprites));
#endif
#ifdef RR_SHOT_BLITTER
    for (U16 n=0; n<8; ++n) {
        shotMask[n]=0x3fff;
        shotGfx[n*4]=shotGfx[n*4+1]=0;
        shotGfx[n*4+2]=shotGfx[n*4+3]=0xc000;
    }
    shotSprite.mpGfx=shotGfx; shotSprite.mpMask=shotMask;
    shotSprite.mWidth=16; shotSprite.mHeight=8;
    shotSprite.mGfxPlaneCount=shotSprite.mMaskPlaneCount=4;
#endif
    spritesReady=1;
}

void RREnemies_Reset(void)
{
    U16 i;
    for (i=0; i<COUNT; ++i) enemies[i].actor.kind=0;
    memset(tanks,0,sizeof(tanks));
    memset(enemyShots,0,sizeof(enemyShots));
    gEnemyShots=gEnemyShotHits=0;
    dirtyCount[0]=dirtyCount[1]=ticks=bridgePending=0;
    gBridgeCrashAdvance=0;
    gShotActive=shotSaved[0]=shotSaved[1]=0;
    gFuel=65535; gRefueling=0;
    distance=0;
    gEnemyBridge=1;
    randomState=0x52495652UL;
    gEnemyMoves=gEnemyBounces=gEnemyOverflow=0;
}

void RREnemies_Checkpoint(U16 restore)
{
    if (!restore) {
        if (!bridgePending) gBridgeCrashAdvance=0;
        memcpy(checkpoint.actors,enemies,sizeof(enemies));
        memcpy(checkpoint.tanks,tanks,sizeof(tanks));
        checkpoint.distance=distance; checkpoint.bridgeWorld=bridgeWorld;
        checkpoint.randomState=randomState; checkpoint.ticks=ticks;
        checkpoint.bridgePending=bridgePending; checkpoint.bridge=gEnemyBridge;
        checkpoint.bridgeX=bridgeX; checkpoint.launchX=gLaunchX;
    } else {
        U16 i;
        gBridgeCrashAdvance=0;
        memcpy(enemies,checkpoint.actors,sizeof(enemies));
        /* A newly unlocked section must not freeze a bridge explosion in READY. */
        for (i=0;i<COUNT;++i)
            if (enemies[i].actor.kind==14 && enemies[i].actor.frame<7)
                enemies[i].actor.kind=0;
        memcpy(tanks,checkpoint.tanks,sizeof(tanks));
        /* Destruction is saved immediately, before its animation finishes.
         * Do not resurrect a tank blast as an idle tread pose in READY. */
        for (i=0;i<TANK_COUNT;++i)
            if (!tanks[i].stationary &&
                ((tanks[i].state.frame&128) || tanks[i].state.frame<4))
                tanks[i].used=0;
        memset(enemyShots,0,sizeof(enemyShots));
        distance=checkpoint.distance; bridgeWorld=checkpoint.bridgeWorld;
        randomState=checkpoint.randomState; ticks=checkpoint.ticks;
        bridgePending=checkpoint.bridgePending; gEnemyBridge=checkpoint.bridge;
        bridgeX=checkpoint.bridgeX; gLaunchX=checkpoint.launchX;
        dirtyCount[0]=dirtyCount[1]=shotSaved[0]=shotSaved[1]=gShotActive=0;
        gFuel=65535; gRefueling=0;
    }
}

void RREnemies_Initial(const A8Generator *g)
{
    U16 i, n=0;
    for (i=0; i<11; ++i) {
        U16 kind=g->memory[0x50b+i], frame=g->memory[0x500+i];
        Enemy *e;
        if (!Supported(kind) || kind==14 || frame<7) continue;
        e=&enemies[n++];
        e->world=(S16)g->memory[0xc+i]-0x5d;
        e->actor.x=((S16)g->memory[0x516+i]-48)*2;
        e->actor.top=(S16)e->world;
        e->actor.kind=kind; e->actor.frame=frame;
        e->actor.flags=g->memory[0x42+i];
    }
    gLaunchX=((S16)g->memory[0x57]-48)*2;
}

void RREnemies_Spawn(const A8Generator *g)
{
    U16 i, kind=g->spawn_type;
    S32 world;
    if (g->tank_spawned) {
        for (i=0; i<TANK_COUNT; ++i) if (!tanks[i].used) {
            Tank *t=&tanks[i];
            memset(t,0,sizeof(*t));
            t->world=-(S32)g->rows+(S16)g->tank_y-0x1d;
            t->x=((S16)g->tank_x-48)*2;
            t->used=1; t->stationary=(g->memory[0x41]&128)!=0;
            t->state.start_x=t->x;
            t->state.initial_frame=t->state.frame=g->tank_frame;
            t->state.active=!t->stationary;
            break;
        }
        if (i==TANK_COUNT) ++gEnemyOverflow;
    }
    if (!g->spawned) return;
    world=-(S32)g->rows+(S16)g->spawn_y-0x5d;
    if (world+distance>=160) return;
    if (kind==14 && (g->spawn_flags&128)) {
        bridgeWorld=world; bridgePending=1;
        bridgeX=((S16)g->spawn_x-48)*2+24;
    }
    if (kind==14 && !(g->spawn_flags&128)) return;
    if (!Supported(kind)) return;
    for (i=0; i<COUNT; ++i) if (!enemies[i].actor.kind) {
        Enemy *e=&enemies[i];
        e->world=world;
        e->actor.x=((S16)g->spawn_x-48)*2;
        e->actor.top=0;
        e->actor.kind=kind;
        e->actor.frame=g->spawn_frame;
        e->actor.flags=g->spawn_flags;
        return;
    }
    ++gEnemyOverflow;
}

void RREnemies_Scroll(void)
{
    U16 i;
    ++distance;
    for (i=0; i<TANK_COUNT; ++i)
        if (tanks[i].used && tanks[i].world+distance>=160) tanks[i].used=0;
    for (i=0; i<COUNT; ++i)
        if (enemies[i].actor.kind && enemies[i].world+distance>=160)
            enemies[i].actor.kind=0;
    if (bridgePending && bridgeWorld+distance>=141) {
        gLaunchX=bridgeX;
        bridgePending=0;
        gBridgeCrashAdvance=0;
    }
}

U16 RREnemies_BridgeCrashRows(void)
{
    S32 rows=141-(bridgeWorld+distance);
    return gBridgeCrashAdvance && bridgePending && rows>0 ? (U16)rows : 0;
}

/* Test original opaque PMG bits against COLPF0, not the bounding box or
 * brown bank outline. Each shifted word touches at most two ST groups. */
static U16 Bank(const A8Actor *a, S16 scroll)
{
    const sSprite *s=&sprites[a8_actor_pose(a)];
    RRBankQuery q;
    q.x=a->x; q.y=a->top; q.scroll=scroll;
    q.groups=s->mWidth/16; q.height=s->mHeight;
    return RR_Bank(&q,s->mpMask,Screen_GetpBack());
}

void RREnemies_Update(S16 scroll)
{
    U16 i, visible[COUNT], count=0;
    if (gEnemiesEnabled && gPlayerState==RR_PLAYER_EXPLODING) {
        for (i=0;i<TANK_COUNT;++i) {
            Tank *t=&tanks[i];
            if (t->used && !t->stationary && gPlayerDeathTicks &&
                ((t->state.frame&128) || t->state.frame<4))
                a8_bridge_tank_step(&t->state,&t->x,gPlayerDeathTicks,
                    (S16)(t->world+distance),t->broken);
        }
        for (i=0;i<COUNT;++i) {
            A8Actor *a=&enemies[i].actor;
            if (gPlayerDeathTicks && a->kind==14 && a->frame>=2 && a->frame<7) {
                a8_actor_step(a,gPlayerDeathTicks,0,0,0);
                if (!a->frame) a->kind=0;
            }
        }
        return;
    }
    if (!gEnemiesEnabled || gPlayerState!=RR_PLAYER_FLYING) return;
    ++ticks;
    for (i=0; i<TANK_COUNT; ++i) if (tanks[i].used && !tanks[i].stationary)
        a8_bridge_tank_step(&tanks[i].state,&tanks[i].x,ticks,
            (S16)(tanks[i].world+distance),tanks[i].broken);
    for (i=0; i<COUNT; ++i) if (enemies[i].actor.kind) {
        enemies[i].actor.top=(S16)(enemies[i].world+distance);
        if (enemies[i].actor.top+24>0 && enemies[i].actor.top<160) visible[count++]=i;
    }
    if (!(ticks&15)) {
        U16 slot, alive=0, first;
        for (i=0; i<count; ++i) {
            U16 frame=enemies[visible[i]].actor.frame;
            if (frame>=7 && frame<14) ++alive;
        }
        first=alive<11 ? 11-alive : 0;
        /* POKEY RANDOM substitute, retaining idle selections in the 11 slots. */
        randomState^=randomState<<13; randomState^=randomState>>17; randomState^=randomState<<5;
        slot=randomState&15;
        if (slot>=11) slot-=5;
        if (slot>=first) {
            slot-=first;
            for (i=count; i-- >0;) {
                A8Actor *a=&enemies[visible[i]].actor;
                if (a->frame>=7 && a->frame<14 && !slot--) { a->flags|=64; break; }
            }
        }
    }
    for (i=0; i<count; ++i) {
        U16 index=visible[i];
        A8Actor *a=&enemies[index].actor;
        S16 oldX=a->x;
        U16 direction=a->flags&8;
        U16 bank=0;
        /* A271: planes ignore banks; balloons move only one tick in four. */
        if (a->frame>=8 && a->frame<14 && gEnemyBridge>=2 &&
            (a->flags&64) && !((ticks^index)&1) &&
            !(a->frame==12 && (ticks&2))) bank=Bank(a,scroll);
        a8_actor_step(a,ticks,gEnemyBridge,index&1,bank);
        if (!a->frame) a->kind=0;
        if (oldX!=a->x) ++gEnemyMoves;
        if (direction!=(a->flags&8)) ++gEnemyBounces;
    }
}

#ifdef RR_COLLISION_REFERENCE
static U32 Bits(const sSprite *s, U16 row)
{
    U16 groups=s->mWidth/16;
    U32 bits=(U32)(U16)~s->mpMask[row*groups]<<16;
    if (groups==2) bits|=(U16)~s->mpMask[row*groups+1];
    return bits;
}

static U16 Overlap(U32 a, U32 b, S16 dx)
{
    if (dx<=-32 || dx>=32) return 0;
    return dx<0 ? (a & (b<<-dx))!=0 : (a & (b>>dx))!=0;
}

static U16 SpriteOverlap(const sSprite *s, U16 row, U32 bits, S16 dx)
{
    U16 group;
    if (s->mWidth<=32) return Overlap(Bits(s,row),bits,dx);
    for (group=0; group<s->mWidth/16; ++group)
        if (Overlap((U32)(U16)~s->mpMask[row*4+group]<<16,bits,dx-group*16)) return 1;
    return 0;
}
#endif

/* Shared hit transaction: ramming breaks/scores the span just like a shot. */
static void DestroyEnemy(U16 index,U16 frame)
{
    Enemy *e=&enemies[index];
    A8Actor *a=&e->actor;
    U16 i;
    if (a->kind==14) {
        ++gEnemyBridge;
        for (i=0;i<TANK_COUNT;++i) {
            Tank *t=&tanks[i];
            if (t->used && !t->stationary && t->world==e->world+1) {
                t->broken=1;
                gScore+=a8_bridge_tank_hit(&t->state,t->x,a->x,
                    gEnemyBridge);
            }
        }
        /* A death before crossing must still start the newly unlocked section. */
        if (bridgeWorld==e->world && bridgePending) gBridgeCrashAdvance=1;
    }
    gScore+=a->kind==14 ? 500 : a->kind==15 ? 80 : a->kind==13 ? 30 : a->kind==7 ? 100 :
        (a->kind==10 || a->kind==11) ? 150 : 60;
    if (gScore>gHighScore) gHighScore=gScore;
    a->frame=frame;
    RRAudio_Event(RR_SFX_HIT);
}

void RREnemies_CheckPlayer(void)
{
    U16 i;
    if (!gEnemiesEnabled || !gPlayerCollisionEnabled || gPlayerState!=RR_PLAYER_FLYING) return;
    for (i=0; i<COUNT; ++i) {
        const A8Actor *a=&enemies[i].actor;
        const sSprite *s;
        S16 y;
        if (!a->kind || a->kind==15 || a->frame<7 || a->x>=gPlayerX+16) continue;
        y=(S16)(enemies[i].world+distance);
        /* All collidable PMG poses are <=32 rows. Reject distant objects
         * before pose lookup, repeated for every consumed scroll row. */
        if (y>=RR_PLAYER_Y+RR_PLAYER_HEIGHT || y+32<=RR_PLAYER_Y) continue;
        s=&sprites[Pose(a)];
        if (a->x+(S16)s->mWidth<=gPlayerX) continue;
        if (y>=RR_PLAYER_Y+RR_PLAYER_HEIGHT || y+(S16)s->mHeight<=RR_PLAYER_Y) continue;
#ifdef RR_COLLISION_REFERENCE
        {
        U16 row;
        for (row=0; row<s->mHeight; ++row) {
            S16 py=y+row-RR_PLAYER_Y;
            if (py>=0 && py<RR_PLAYER_HEIGHT &&
                SpriteOverlap(s,row,(U32)RRPlayer_Bits(py)<<16,gPlayerX-a->x)) {
                if (a->kind==14) DestroyEnemy(i,3);
                RRPlayer_Kill();
                if (a->kind==14) RRPlayer_RewardScore(gScore);
                gShotActive=0; return;
            }
        }
        }
#else
        {
            S16 first=RR_PLAYER_Y-y, last=RR_PLAYER_Y+RR_PLAYER_HEIGHT-y;
            U16 groups=s->mWidth/16;
            if (first<0) first=0;
            if (last>(S16)s->mHeight) last=s->mHeight;
            if (RR_MaskOverlap(s->mpMask+first*groups,
                    gPlayerMasks[gPlayerFrame]+(y+first-RR_PLAYER_Y),
                    groups,last-first,gPlayerX-a->x)) {
                if (a->kind==14) DestroyEnemy(i,3);
                RRPlayer_Kill();
                if (a->kind==14) RRPlayer_RewardScore(gScore);
                gShotActive=0; return;
            }
        }
#endif
    }
}

void RRFuel_Update(void)
{
    U16 i;
    gRefueling=0;
    if (!gFuelEnabled || gPlayerState!=RR_PLAYER_FLYING) return;
    if (gEnemiesEnabled) for (i=0; i<COUNT && !gRefueling; ++i) {
        const A8Actor *a=&enemies[i].actor;
        S16 y;
        if (a->kind!=15 || a->frame!=15 || a->x>=gPlayerX+16 || a->x+16<=gPlayerX) continue;
        y=(S16)(enemies[i].world+distance);
        if (y>=RR_PLAYER_Y+RR_PLAYER_HEIGHT || y+24<=RR_PLAYER_Y) continue;
#ifdef RR_COLLISION_REFERENCE
        {
        U16 row;
        for (row=0; row<24; ++row) {
            S16 py=y+row-RR_PLAYER_Y;
            if (py>=0 && py<RR_PLAYER_HEIGHT &&
                Overlap(Bits(&sprites[82],row),(U32)RRPlayer_Bits(py)<<16,gPlayerX-a->x)) {
                gRefueling=1; break;
            }
        }
        }
#else
        {
            S16 first=RR_PLAYER_Y-y, last=RR_PLAYER_Y+RR_PLAYER_HEIGHT-y;
            if (first<0) first=0;
            if (last>24) last=24;
            if (RR_MaskOverlap(sprites[82].mpMask+first,
                    gPlayerMasks[gPlayerFrame]+(y+first-RR_PLAYER_Y),
                    1,last-first,gPlayerX-a->x)) gRefueling=1;
        }
#endif
    }
    gFuel=rr_fuel_step(gFuel,gRefueling);
    if (!gFuel) { RRPlayer_Kill(); gShotActive=0; }
}

void RREnemies_Combat(U8 joy,S16 scroll)
{
    U16 i;
    S16 bottom, closest=-32768, target=-1;
    if (gPlayerState!=RR_PLAYER_FLYING) { gShotActive=0; return; }
    if (!gShotActive) {
        if (!(joy&128) && !IKBD_GetKeyStatus(eIKBDSCAN_LEFTSHIFT) &&
            !IKBD_GetKeyStatus(eIKBDSCAN_RIGHTSHIFT)) return;
        gShotActive=1; gShotX=gPlayerX+7; gShotY=RR_PLAYER_Y-8; ++gShots;
        RRAudio_Event(RR_SFX_SHOT);
    }
    bottom=gShotY+8;
    gShotY-=8;
    if (gEnemiesEnabled) for (i=0; i<COUNT; ++i) {
        const A8Actor *a=&enemies[i].actor;
        const sSprite *s;
        S16 y;
        if (!a->kind || a->frame<7 || a->x>=gShotX+2) continue;
        y=(S16)(enemies[i].world+distance);
#ifndef RR_COLLISION_REFERENCE
        if (y>=bottom || y+32<=0 || y+32<=gShotY || y+32<=closest+1) continue;
#endif
        s=&sprites[Pose(a)];
        if (a->x+(S16)s->mWidth<=gShotX) continue;
#ifdef RR_COLLISION_REFERENCE
        {
        U16 row;
        for (row=0; row<s->mHeight; ++row) {
            S16 hit=y+row;
            if (hit>=0 && hit>=gShotY && hit<bottom && hit>closest &&
                SpriteOverlap(s,row,0xc0000000UL,gShotX-a->x)) {
                closest=hit; target=i;
            }
        }
        }
#else
        {
            static const U16 shotMask=0x3fff;
            S16 low=gShotY,first,last,row;
            U16 groups=s->mWidth/16;
            if (low<0) low=0;
            if (low<=closest) low=closest+1;
            first=low-y; last=bottom-y;
            if (first<0) first=0;
            if (last>(S16)s->mHeight) last=s->mHeight;
            /* Descending rows find this actor's nearest swept hit first. */
            for (row=last-1;row>=first;--row)
                if (RR_MaskOverlap(s->mpMask+row*groups,&shotMask,groups,1,gShotX-a->x)) {
                    closest=y+row; target=i; break;
                }
        }
#endif
    }
    if (target>=0) {
        DestroyEnemy((U16)target,2);
        RRPlayer_RewardScore(gScore);
        gShotActive=0; ++gHits;
    } else {
        S16 first=gShotY,last=bottom;
        /* A8 $A4B6 checks object hits before $A58D's terrain latch.
         * Sweep the old/new eight-row shot, including its previous tail. */
        if (first<0) first=0;
        if (last>160) last=160;
        if (first<last && gShotX>=0 && gShotX<=318) {
            U32 offset=(U32)(U16)(scroll+first)*160UL+(U16)(gShotX&~15)/2;
            const U16 *line=(const U16*)((const U8*)Screen_GetpBack()+offset);
            if (RR_ShotTerrainCPU(line,(U16)gShotX&15,(U16)(last-first)))
                gShotActive=0;
        }
    }
    if (gShotY+8<=0) gShotActive=0;
}

/* One M0 helicopter shot and one P1 tank shell, as in A5CA..A671. */
void RREnemies_Weapons(void)
{
    U16 i, which;
    EnemyShot *s=&enemyShots[0];
    if (!gEnemiesEnabled || gPlayerState!=RR_PLAYER_FLYING) return;
    if (s->active && (s->owner>=COUNT ||
        !enemies[s->owner].actor.kind || enemies[s->owner].actor.frame<10 ||
        enemies[s->owner].actor.frame>11)) s->active=0;
    if (!s->active) for (i=0; i<COUNT; ++i) {
        const Enemy *e=&enemies[i];
        S16 y=(S16)(e->world+distance);
        if ((e->actor.kind!=10 && e->actor.kind!=11) ||
            e->actor.frame<10 || e->actor.frame>11 || y<0 || y+10>=160) continue;
        s->world=e->world+3; s->y=y+3; s->owner=i;
        s->dx=(e->actor.flags&8) ? -4 : 4;
        s->x=e->actor.x+((e->actor.flags&8) ? -6 : 14);
        s->age=s->pose=0; s->active=1; ++gEnemyShots;
        RRAudio_Event(RR_SFX_SHOT);
        break;
    }
    s=&enemyShots[1];
    if (!s->active) for (i=0; i<TANK_COUNT; ++i) {
        Tank *t=&tanks[i];
        S16 y=(S16)(t->world+distance);
        if (!t->used) continue;
        if (t->stationary) {
            if (t->fired || y<90 || y>=144) continue;
        } else if (!t->state.armed || t->state.frame<4 || t->state.frame>=8 || y< -4 || y>=159) continue;
        s->dx=t->state.initial_frame>=6 ? -3 : 3;
        s->x=t->x+(s->dx<0 ? -8 : 16);
        s->world=t->world-6; s->y=y-6; s->owner=i;
        s->age=0; s->pose=s->active=1;
        t->fired=1; ++gEnemyShots;
        RRAudio_Event(RR_SFX_ENEMY);
        break;
    }
    for (which=0; which<2; ++which) {
        S16 oldX,oldY,steps,n,dx,dy;
        s=&enemyShots[which];
        if (!s->active) continue;
        oldX=s->x; oldY=s->y;
        if (!which || s->age<16) s->x+=s->dx;
        if (which) {
            static const U8 fall[16]={0,0,0,0,1,0,1,0,1,0,1,0,1,1,1,1};
            if (s->age<16) s->world+=fall[s->age];
            else s->pose=2+(s->age-16)/8;
        }
        s->y=(S16)(s->world+distance);
        ++s->age;
        if ((which && s->age>56) || s->x< -16 || s->x>=320 || s->y+16<=0 || s->y>=160) {
            s->active=0; continue;
        }
        if (!gPlayerCollisionEnabled) continue;
        /* The interpolation stays inside the endpoint rectangle. */
        if ((oldX>s->x ? oldX : s->x)+16<=gPlayerX ||
            (oldX<s->x ? oldX : s->x)>=gPlayerX+16 ||
            (oldY>s->y ? oldY : s->y)+16<=RR_PLAYER_Y ||
            (oldY<s->y ? oldY : s->y)>=RR_PLAYER_Y+RR_PLAYER_HEIGHT) continue;
        dx=s->x-oldX; dy=s->y-oldY;
        steps=dx<0 ? -dx : dx;
        if (dy>steps) steps=dy;
        if (!steps) steps=1;
        for (n=0; n<=steps; ++n) {
            S16 x=oldX+dx*n/steps, y=oldY+dy*n/steps;
            if (x+16<=gPlayerX || x>=gPlayerX+16 || y+16<=RR_PLAYER_Y || y>=RR_PLAYER_Y+14) continue;
#ifdef RR_COLLISION_REFERENCE
            {
            U16 row;
            for (row=0; row<16; ++row) {
                S16 py=y+row-RR_PLAYER_Y;
                if (py>=0 && py<14 && Overlap(
                    (U32)(U16)~sprites[122+s->pose].mpMask[row]<<16,
                    (U32)RRPlayer_Bits(py)<<16,gPlayerX-x)) {
                    ++gEnemyShotHits; enemyShots[0].active=enemyShots[1].active=0;
                    gShotActive=0; RRPlayer_Kill(); return;
                }
            }
            }
#else
            {
                S16 first=RR_PLAYER_Y-y,last=RR_PLAYER_Y+RR_PLAYER_HEIGHT-y;
                if (first<0) first=0;
                if (last>16) last=16;
                if (RR_MaskOverlap(sprites[122+s->pose].mpMask+first,
                        gPlayerMasks[gPlayerFrame]+(y+first-RR_PLAYER_Y),
                        1,last-first,gPlayerX-x)) {
                    ++gEnemyShotHits; enemyShots[0].active=enemyShots[1].active=0;
                    gShotActive=0; RRPlayer_Kill(); return;
                }
            }
#endif
        }
    }
}

static void Restore(const Dirty *d)
{
    U32 offset=(U32)d->y*160UL+(U16)d->x/2;
    U32 *dst=(U32 *)((U8 *)Screen_GetpLogic()+offset);
    const U32 *src=(const U32 *)((const U8 *)Screen_GetpBack()+offset);
    /* A shifted 64-pixel bridge spans five groups; reuse the fast kernels. */
    if (d->groups>3) {
        RR_Restore(src,dst,3,d->height);
        RR_Restore(src+6,dst+6,d->groups-3,d->height);
    } else RR_Restore(src,dst,d->groups,d->height);
}

void RREnemies_Restore(void)
{
    U16 page=Screen_GetLogicIndex(), i;
    for (i=0; i<dirtyCount[page]; ++i) {
        const Dirty *d=&dirty[page][i];
        Restore(d);
    }
    dirtyCount[page]=0;
    if (shotSaved[page]) Restore(&shotDirty[page]);
    shotSaved[page]=0;
}

extern void RR_ShotCPU(void *dst, U16 shift, U16 height);
static U8 *drawLogic;
static U16 drawPage;

static void DrawOne(U16 pose,S16 x,S16 y,S16 scroll)
{
        U16 skip=0, height;
        sSprite *s=&sprites[pose];
        sSprite clipped;
        sGraphicPos pos;
        S16 x0=x, x1=x+s->mWidth;
        Dirty *d;
        if (y>=160 || y+(S16)s->mHeight<=0 || x1<=0 || x0>=320) return;
        height=s->mHeight;
        if (y<0) {
            skip=-y;
            height-=skip; y=0;
        }
        if (y+height>160) height=160-y;
        pos.mX=x; pos.mY=scroll+y;
        if (x0>=0 && x1<=320) {
#ifndef RR_SPRITE_BLITTER
            if (rrEnemyMono[pose]) {
                void *dst=drawLogic+(U32)pos.mY*160UL+(U16)(x&~15)/2;
                if (s->mWidth==16) RR_MonoSprite(rrEnemyMono[pose]+skip*2,dst,(U16)x&15,height);
                else RR_MonoSprite32(rrEnemyMono[pose]+skip*3,dst,(U16)x&15,height);
            }
            else
#endif
            {
                clipped=*s;
                clipped.mpGfx+=skip*(s->mWidth/16)*4;
                clipped.mpMask+=skip*(s->mWidth/16);
                clipped.mHeight=height;
                Screen_Logic_DrawSprite(&pos,&clipped);
            }
        }
        else {
            clipped=*s;
            clipped.mpGfx+=skip*(s->mWidth/16)*4;
            clipped.mpMask+=skip*(s->mWidth/16);
            clipped.mHeight=height;
            Screen_Logic_DrawSprite_Clip(&pos,&clipped);
        }
        if (x0<0) x0=0;
        if (x1>320) x1=320;
        d=&dirty[drawPage][dirtyCount[drawPage]++];
        d->x=x0&~15; d->y=pos.mY;
        d->groups=((x1+15)/16)-(d->x/16); d->height=height;
        d->spriteX=x; d->pose=pose;
        d->skip=skip;
}

void RREnemies_Draw(S16 scroll)
{
    U16 i, pass, page=Screen_GetLogicIndex();
    drawLogic=(U8*)Screen_GetpLogic(); drawPage=page;
    /* Static PMG below moving enemies; tanks and shells are separate slots. */
    for (pass=0; pass<2; ++pass)
    for (i=0; gEnemiesEnabled && i<COUNT; ++i) {
        const A8Actor *a=&enemies[i].actor;
        if (a->kind && pass==(a->kind<14))
            DrawOne(Pose(a),a->x,(S16)(enemies[i].world+distance),scroll);
    }
    for (i=0; gEnemiesEnabled && i<TANK_COUNT; ++i) {
        const Tank *t=&tanks[i];
        if (t->used && t->state.frame)
            DrawOne(114+(t->state.frame&7),t->x,(S16)(t->world+distance),scroll);
    }
    for (i=0; gEnemiesEnabled && i<2; ++i) if (enemyShots[i].active) {
        const EnemyShot *s=&enemyShots[i];
        DrawOne(122+s->pose,s->x,(S16)(s->world+distance),scroll);
    }
    if (gShotActive) {
#ifdef RR_SHOT_BLITTER
        sSprite *s=&shotSprite;
        sSprite clipped;
        sGraphicPos pos;
#endif
        Dirty *d=&shotDirty[page];
        S16 y=gShotY;
        U16 height=8;
        if (y<0) {
#ifdef RR_SHOT_BLITTER
            clipped=shotSprite;
            clipped.mpMask-=y; clipped.mpGfx-=y*4; clipped.mHeight+=y;
            s=&clipped;
#endif
            height+=y; y=0;
        }
#ifdef RR_SHOT_BLITTER
        pos.mX=gShotX; pos.mY=scroll+y;
        if (gShotX>=0 && gShotX<=304) Screen_Logic_DrawSprite(&pos,s);
        else Screen_Logic_DrawSprite_Clip(&pos,s);
#else
        RR_ShotCPU((U8 *)Screen_GetpLogic()+(U32)(scroll+y)*160UL+
            (U16)(gShotX&~15)/2,(U16)gShotX&15,height);
#endif
        d->x=gShotX&~15; d->y=scroll+y;
        d->height=height; d->groups=((gShotX&15)+17)/16;
        d->spriteX=gShotX; d->skip=y-gShotY;
        shotSaved[page]=1;
    }
}
