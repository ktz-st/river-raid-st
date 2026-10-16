#ifndef A8_BRIDGE_TANKS_H
#define A8_BRIDGE_TANKS_H
#include <stdint.h>

typedef struct {
    int16_t start_x, bridge, scenery;
    uint8_t frame, initial_frame, armed, active;
} A8BridgeTank;

/* ROM $A21B..$A26A: live tread poses tick every four frames, blasts every 16. */
static inline void a8_bridge_tank_step(A8BridgeTank *tank, int16_t *x,
                                      unsigned ticks, int relative_y, int broken)
{
    static const uint8_t next[8] = {0,0,1,2,5,4,7,6};
    if (!tank->active || relative_y < -18) return;
    if (relative_y >= 159) { tank->frame = tank->armed = 0; return; }
    unsigned gate = tank->frame >= 4 && tank->frame < 8 ? 3 : 15;
    if (ticks & gate) return;
    if (!broken && tank->frame >= 4 && tank->frame < 8) {
        int hpos = *x/2+48;
        hpos = (uint8_t)(hpos+(tank->initial_frame < 6 ? 1 : -1));
        *x = (hpos-48)*2;
    }
    tank->frame = tank->frame & 128 ? 3 : next[tank->frame & 7];
}

/* $A500..$A545 uses HPOS clocks, not the expanded sprite's bounding box.
   Return the extra 250 points: the combined bridge/tank award is 750. */
static inline unsigned a8_bridge_tank_hit(A8BridgeTank *tank, int x,
                                         int bridge_x, unsigned bridge_number)
{
    if (!tank->active || tank->frame < 4 || tank->frame >= 8) return 0;
    int hpos = x/2+48, span = bridge_x/2+48;
    if (hpos > span-8 && hpos <= span+31) {
        tank->frame |= 128;
        tank->armed = 0;
        return 250;
    }
    int distance = tank->initial_frame < 6 ? span-hpos : hpos-span;
    unsigned limit = tank->initial_frame < 6 ? 0x29 : 0x41;
    if (bridge_number >= 5 && (unsigned)distance < limit) tank->armed = 1;
    return 0;
}

static inline void a8_bridge_tank_reset(A8BridgeTank *tank, int16_t *x)
{
    if (!tank->active) return;
    *x = tank->start_x;
    tank->frame = tank->initial_frame;
    tank->armed = 0;
}
#endif
