#ifndef A8_ACTORS_H
#define A8_ACTORS_H
#include <stdint.h>
typedef struct {
    int16_t x, top;
    uint8_t kind, frame, flags;
} A8Actor;

/* A271..A2ED: horizontal PMG clocks, animation and bank-contact latch. */
static inline void a8_actor_step(A8Actor *actor, unsigned ticks, unsigned difficulty,
                          unsigned phase, int bank_contact)
{
    static const unsigned char next[12] = {0,1,3,4,5,6,0,7,9,8,11,10};
    if (actor->frame < 12 && !(ticks & (actor->frame >= 7 ? 1 : 7)))
        actor->frame = next[actor->frame];
    if (difficulty < 2 || actor->frame < 7 || actor->frame >= 14) return;
    if (actor->frame != 7) {
        if (actor->frame == 12 && (ticks & 2)) return;
        if ((ticks ^ phase) & 1) return;
        if (!(actor->flags & 64)) return;
        if (bank_contact) {
            if (!(actor->flags & 16)) actor->flags ^= 24;
        } else actor->flags &= ~16;
    }
    if (actor->flags & 8) {
        actor->x -= 2;
        if (actor->x < -8) actor->x = 328;
    } else {
        actor->x += 2;
        if (actor->x >= 330) actor->x = -8;
    }
}

static unsigned a8_actor_pose(const A8Actor *actor)
{
    if (actor->kind == 13 && actor->frame >= 2 && actor->frame <= 6)
        return 32+actor->frame-2+((actor->flags & 8) ? 5 : 0);
    return actor->frame + ((actor->flags & 8) ? 16 : 0);
}
#endif
