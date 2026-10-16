#ifndef A8_HUD_H
#define A8_HUD_H
#include <stdint.h>
#include "hud_assets.inc"
#define A8_HUD_HEIGHT 40

#ifdef A8_HUD_NUMBER
#define a8_hud_number A8_HUD_NUMBER
#else
static void a8_hud_number(unsigned char *row, unsigned first, unsigned last,
                          uint32_t value, unsigned colour)
{
    for (unsigned i=last+1; i-- > first;) {
        row[i] = (i == last || value) ? colour+16+value%10 : 0;
        value /= 10;
    }
}

#endif

static void a8_hud_rows(unsigned char *rows, uint32_t score, uint32_t high,
                        unsigned reserves, unsigned bridge)
{
    for (unsigned i=0; i<60; ++i) rows[i] = a8_hud_template[i];
    a8_hud_number(rows, 2, 8, score, 0);
    a8_hud_number(rows, 12, 18, high, 192);
    a8_hud_number(rows+20, 16, 18, bridge, 128);
    for (unsigned i=11; i<20; ++i) rows[40+i] = i-11 < reserves ? 0x9c : 0;
}

#ifdef A8_HUD_CELL
#define a8_hud_cell A8_HUD_CELL
#else
static void a8_hud_cell(uint16_t *panel, unsigned cell, unsigned code)
{
    const unsigned tops[3] = {6, 18, 26};
    const unsigned inks[4] = {12, 3, 12, 13};
    const unsigned ink = inks[code >> 6];
    for (unsigned y=0; y<8; ++y) {
        const unsigned bits = a8_hud_font[(code & 63)*8+y];
        unsigned mask = 0;
        for (unsigned bit=0; bit<8; ++bit)
            if (bits & (128 >> bit)) mask |= 0xc000u >> (bit*2);
        uint16_t *dst = panel+(tops[cell/20]+y)*80+(cell%20)*4;
        for (unsigned plane=0; plane<4; ++plane)
            dst[plane] = ((5 & (1u << plane)) ? (uint16_t)~mask : 0) |
                ((ink & (1u << plane)) ? mask : 0);
    }
}

#endif

static void a8_hud_draw(uint16_t *panel, uint32_t score, uint32_t high,
                        unsigned reserves, unsigned bridge)
{
    unsigned char rows[60];
    a8_hud_rows(rows, score, high, reserves, bridge);
#ifdef A8_HUD_FILL
    A8_HUD_FILL(panel,5,A8_HUD_HEIGHT);
#else
    /* Diagnostic reference only; the game supplies the ASM fill kernel. */
    for (unsigned y=0; y<A8_HUD_HEIGHT; ++y)
        for (unsigned word=0; word<20; ++word)
            for (unsigned plane=0; plane<4; ++plane)
                panel[y*80+word*4+plane] = (5 & (1u << plane)) ? 0xffff : 0;
#endif
    for (unsigned cell=0; cell<60; ++cell) a8_hud_cell(panel, cell, rows[cell]);
}

static unsigned a8_hud_update(uint16_t *panel, unsigned char *previous,
                              uint32_t score, uint32_t high,
                              unsigned reserves, unsigned bridge)
{
    unsigned char rows[60];
    unsigned changed = 0;
    a8_hud_rows(rows, score, high, reserves, bridge);
    for (unsigned cell=0; cell<60; ++cell)
        if (rows[cell] != previous[cell]) {
            // Replace only this glyph's planar words, never blank the panel.
            a8_hud_cell(panel, cell, rows[cell]);
            previous[cell] = rows[cell];
            ++changed;
        }
    return changed;
}

#ifdef A8_HUD_FUEL
#define a8_hud_fuel A8_HUD_FUEL
#else
static inline unsigned a8_hud_fuel(uint16_t *panel, const unsigned char *rows,
                            uint16_t fuel, unsigned *previous)
{
    const unsigned position = fuel >> 11;
    if (*previous == position) return 0;
    if (*previous < 32) {
        const unsigned old_x = 88+*previous*2;
        for (unsigned column=old_x/16; column<=(old_x+3)/16; ++column) {
            a8_hud_cell(panel, 20+column, rows[20+column]);
            a8_hud_cell(panel, 40+column, rows[40+column]);
        }
    }
    const unsigned x = 88+position*2;
    for (unsigned y=20; y<30; ++y)
        for (unsigned dx=0; dx<4; ++dx) {
            const unsigned bit = 0x8000u >> ((x+dx)&15);
            uint16_t *dst = panel+y*80+((x+dx)/16)*4;
            for (unsigned p=0; p<4; ++p)
                dst[p] = (12 & (1u<<p)) ? dst[p] | bit : dst[p] & ~bit;
        }
    *previous = position;
    return 1;
}
#endif
#endif
