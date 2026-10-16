#ifndef RR_SPRITE_CPU_H
#define RR_SPRITE_CPU_H
#include <godlib/base/base.h>
/* Native row streams, 160-byte stride. Caller clips Y and guarantees X bounds. */
void RR_MonoSprite(const U16 *rows,void *dst,U16 shift,U16 height);
void RR_MonoSprite32(const U16 *rows,void *dst,U16 shift,U16 height);
#endif
