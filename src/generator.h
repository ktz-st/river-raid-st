#ifndef RR_STAGE5_GENERATOR_H
#define RR_STAGE5_GENERATOR_H

#include <stdint.h>
#define A8_GEN_RAM_BYTES 0x600UL
#define A8_GEN_STATE_OFFSET 0x600UL
typedef struct {
    uint8_t memory[A8_GEN_RAM_BYTES]; /* $0000..$05ff; no virtual display list */
    uint8_t a, x, y, sp;
    uint32_t rows;
    uint8_t spawned, spawn_type, spawn_frame, spawn_flags, spawn_y, spawn_x, spawn_size;
    uint8_t actor_scheduler, tank_spawned, tank_frame, tank_x, tank_y;
} A8Generator;
void a8_generator_init(A8Generator *g, const uint8_t snapshot[65536],
                       const uint8_t rom[8192]);
int a8_generator_line(A8Generator *g, uint8_t output[48]);
#endif
