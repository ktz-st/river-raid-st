#ifndef RR_FUEL_RULES_H
#define RR_FUEL_RULES_H
#include <stdint.h>
/* A8 A4xx: add two to $5B on contact, then subtract $20 from $5C. */
static inline uint16_t rr_fuel_step(uint16_t fuel, int contact)
{
    uint32_t next=fuel;
    if (contact) { next+=512; if (next>65535) next=65535; }
    return next>=32 ? (uint16_t)(next-32) : 0;
}
#endif
