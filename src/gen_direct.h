#ifndef RR_GEN_DIRECT_H
#define RR_GEN_DIRECT_H

#include "generator.h"

void a8_generator_compact_init(A8Generator *g, const uint8_t *snapshot,
                               const uint8_t *rom);
/* Caller supplies RR_LINE_DIRECT_MAX bytes; returns record length or zero. */
uint16_t a8_generator_compact_line(A8Generator *g, uint8_t *record);

#endif
