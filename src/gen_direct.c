#include "gen_direct.h"
#include "compact_line.h"

void a8_generator_init(A8Generator *g, const uint8_t snapshot[65536],
                       const uint8_t rom[8192])
{
    a8_generator_compact_init(g, snapshot, rom);
}

int a8_generator_line(A8Generator *g, uint8_t output[48])
{
    uint8_t record[RR_LINE_DIRECT_MAX];
    if (!a8_generator_compact_line(g, record)) return 0;
    /* The native generator bounds every command and requires exactly 48
     * output bytes before success, so this record needs no second validator. */
    RR_LineExpand(record, output);
    return 1;
}
