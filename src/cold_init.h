#ifndef RR_COLD_INIT_H
#define RR_COLD_INIT_H
#include "generator.h"
/* rom is a historical test-ABI argument; production passes NULL. */
void a8_generator_cold_init(A8Generator *g, const uint8_t rom[8192]);
#endif
