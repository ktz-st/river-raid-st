#include "cold_init.h"
#include <string.h>
#include "native_assets.h"

void a8_generator_cold_init(A8Generator *g, const uint8_t rom[8192])
{
    uint8_t *m = g->memory;
#ifdef RR_GENERATOR_FULL
    uint8_t *dl;
#endif
    unsigned i;
    memset(g, 0, sizeof(*g));
#ifdef RR_GENERATOR_FULL
    memcpy(m+0xa000, rom, 8192);
    m[0xae94]=m[0xae96]=0x60;
    dl=m+0x3f00;
    /* Historical reference backend only: ANTIC display-list initialization. */
    memcpy(dl, rom+0x1776, 4);
    memset(dl+6, 0x0e, 0xa3);
    memcpy(dl+0xa9, rom+0x1789, 15);
    dl[4]=0; dl[5]=0x20; dl[6]=0x4e; dl[7]=0x30; dl[8]=0x20;
    dl[0x57]=0x4e; dl[0x58]=0; dl[0x59]=0x30;
    m[0xa5]=2; m[0xa6]=0x53; m[0xa7]=0x0e;
#endif
    m[0xcb]=0x2f;

    /* B0F1 / AF80: default one-player scene, no hardware/PMG buffer copies. */
    /* The historical signature is retained for test callers only. */
    (void)rom;
    memcpy(m+0x4d, rrSceneDefaults, sizeof(rrSceneDefaults));
    for (i=0; i<4; ++i) m[0x66+i]=m[0x71+i]=rrGameDefaults[i];
    m[0x60]=m[0x6b]=rrGameDefaults[4];
    m[0x62]=m[0x6d]=rrGameDefaults[5];
    m[0x17]=255; m[0x32]=23; m[0x515]=14; m[0x16]=0x4a;
    m[0x4d]=m[0x4e]=10;
    m[0x0b]=(m[0x60]&1) ? 0xd6 : 0xd2;
    m[0x4c]=m[0x23]=m[0x0b]&0x84;

    /* A3C7..A3E3: deterministic ROM-derived generator seed and launch X. */
    memcpy(m+0xaa, m+0x66, 3);
    m[0x52]=m[0x69];
    m[0x57]=(uint8_t)(m[0x69]+0x26+m[0x54]);
    g->x=g->sp=255;
    g->actor_scheduler=1;
}
