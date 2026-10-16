#ifndef RR_CONTROLS_H
#define RR_CONTROLS_H

enum { RR_CONTROL_PAUSE=1, RR_CONTROL_RESTART=2, RR_CONTROL_ESCAPE=4,
       RR_CONTROL_YES=8, RR_CONTROL_NO=16, RR_CONTROL_FIRE=32 };

/* Held keys must not repeat pause, restart or a modal decision every frame. */
static inline unsigned rr_control_edges(unsigned *previous, unsigned held)
{
    unsigned pressed=held & ~*previous;
    *previous=held;
    return pressed;
}
#endif
