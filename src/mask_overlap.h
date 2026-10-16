#ifndef RR_MASK_OVERLAP_H
#define RR_MASK_OVERLAP_H
#include <godlib/base/base.h>
/* Transparent masks. Player is 16px, dx is playerX - enemyX; caller clips Y. */
U16 RR_MaskOverlap(const U16 *enemy,const U16 *player,U16 groups,U16 rows,S16 dx);
#endif
