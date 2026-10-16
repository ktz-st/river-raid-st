#ifndef RR_RASTER_CPU_H
#define RR_RASTER_CPU_H
#include <godlib/base/base.h>
/* 320px ST4 rows; even destination. Zero rows is a no-op. */
void RR_FillRowsCPU(U16 *dst,U16 ink,U16 rows);
/* Four-pixel colour-12 marker, ten rows; dst is its first aligned group. */
void RR_FuelMarkerCPU(U16 *dst,U16 shift);
/* Eight rows, eight pixels, colour 15 on 0; preserve adjacent half-word. */
void RR_TextGlyphCPU(U16 *dst,const U8 *glyph,U16 half);
#endif
