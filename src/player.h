#ifndef RR_PLAYER_H
#define RR_PLAYER_H

#include <godlib/base/base.h>

#define RR_PLAYER_Y 143
#define RR_PLAYER_HEIGHT 14

extern S16 gPlayerX;
extern U16 gPlayerFrame;
extern U16 gPlayerVisible;
enum { RR_PLAYER_FLYING, RR_PLAYER_EXPLODING, RR_PLAYER_WAIT, RR_PLAYER_GAME_OVER,
       RR_PLAYER_READY };
extern U16 gPlayerState, gPlayerLives, gPlayerDeathTicks, gPlayerCollisionEnabled;

void RRPlayer_Init(const U8 *rom);
U16 RRPlayer_Update(U8 joy);
void RRPlayer_Respawn(U16 newGame);
void RRPlayer_RewardScore(U32 score);
void RRPlayer_CheckBank(S16 scroll);
void RRPlayer_Kill(void);
U16 RRPlayer_Bits(U16 row);
void RRPlayer_Restore(void);
void RRPlayer_Draw(S16 scroll);

#endif
