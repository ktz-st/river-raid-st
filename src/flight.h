#ifndef RR_FLIGHT_H
#define RR_FLIGHT_H
#include <stdint.h>

typedef struct { uint16_t speed, fraction; } RRFlight;

static inline void rr_flight_reset(RRFlight *f)
{
    f->speed=0x40;
    f->fraction=0;
}

/* A8 $A497..$A4B6 and $A6FD..$A711. UP wins; neutral returns to $80. */
static inline unsigned rr_flight_step(RRFlight *f, int up, int down)
{
    unsigned target=up ? 0xfe : down ? 0x40 : 0x80;
    unsigned total;
    if (f->speed<target) f->speed+=2;
    else if (f->speed>target) f->speed-=2;
    total=f->fraction+(f->speed==0xfe ? 512 : f->speed*2);
    f->fraction=total&255;
    return total>>8;
}
#endif
