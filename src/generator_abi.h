#ifndef RR_GENERATOR_ABI_H
#define RR_GENERATOR_ABI_H
#include <stddef.h>
#include "generator.h"

/* Native generator metadata follows mutable low RAM; tables are separate const C. */
#define RR_GEN_OFFSET(field, offset) \
    _Static_assert(offsetof(A8Generator, field) == A8_GEN_STATE_OFFSET+(offset), "generator ASM offset: " #field)
RR_GEN_OFFSET(a, 0);
RR_GEN_OFFSET(x, 1);
RR_GEN_OFFSET(y, 2);
RR_GEN_OFFSET(sp, 3);
RR_GEN_OFFSET(rows, 4);
RR_GEN_OFFSET(spawned, 8);
RR_GEN_OFFSET(spawn_type, 9);
RR_GEN_OFFSET(spawn_frame, 10);
RR_GEN_OFFSET(spawn_flags, 11);
RR_GEN_OFFSET(spawn_y, 12);
RR_GEN_OFFSET(spawn_x, 13);
RR_GEN_OFFSET(spawn_size, 14);
RR_GEN_OFFSET(actor_scheduler, 15);
RR_GEN_OFFSET(tank_spawned, 16);
RR_GEN_OFFSET(tank_frame, 17);
RR_GEN_OFFSET(tank_x, 18);
RR_GEN_OFFSET(tank_y, 19);
_Static_assert(sizeof(A8Generator) == A8_GEN_STATE_OFFSET+20UL, "generator ASM size");
#undef RR_GEN_OFFSET
#endif
