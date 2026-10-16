#ifndef RR_ENEMIES_H
#define RR_ENEMIES_H
#include <godlib/base/base.h>
#include "generator.h"

extern U16 gEnemiesEnabled, gEnemyBridge;
extern U16 gBridgeCrashAdvance;
extern U32 gEnemyMoves, gEnemyBounces, gEnemyOverflow;
void RREnemies_Init(const U8 *rom, const U8 *inks);
void RREnemies_Reset(void);
void RREnemies_Initial(const A8Generator *g);
void RREnemies_Checkpoint(U16 restore);
extern S16 gLaunchX;
void RREnemies_Spawn(const A8Generator *g);
/* Crossing sets the launch position, but never scores or saves a checkpoint. */
void RREnemies_Scroll(void);
U16 RREnemies_BridgeCrashRows(void);
void RREnemies_Update(S16 scroll);
void RREnemies_Combat(U8 joy,S16 scroll);
void RREnemies_Weapons(void);
void RREnemies_CheckPlayer(void);
extern U32 gScore, gHighScore, gShots, gHits;
extern U16 gFuel, gRefueling, gFuelEnabled;
void RRFuel_Update(void);
void RREnemies_Restore(void);
void RREnemies_Draw(S16 scroll);
#endif
