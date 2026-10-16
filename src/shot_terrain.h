#ifndef RR_SHOT_TERRAIN_H
#define RR_SHOT_TERRAIN_H
#include <godlib/base/base.h>

/* Clean planar terrain only (inks 0..6), two-pixel shot, fixed 160-byte stride.
 * The caller clips rows and guarantees both horizontal pixels are on screen. */
U16 RR_ShotTerrainCPU(const U16 *line,U16 shift,U16 height);
#endif
