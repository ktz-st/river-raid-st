#ifndef RR_ROAD_CPU_H
#define RR_ROAD_CPU_H
#include <godlib/base/base.h>
/* Recolour land (plane0 & ~plane1) in one 320px, four-plane row. */
void RR_RoadCPU(U16 *dst,U16 ink);
#endif
