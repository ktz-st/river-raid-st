#ifndef RR_RESTORE_H
#define RR_RESTORE_H
#include <godlib/base/base.h>

/* Even, disjoint buffers, stride 160; groups 1..3, height 0..416. */
void RR_RestoreCPU(const U32 *src, U32 *dst, U16 groups, U16 height);
void RR_RestoreC(const U32 *src, U32 *dst, U16 groups, U16 height);
void RR_RestorePlayerC(const U32 *src, U32 *dst, U16 groups, U16 height);
#ifdef RR_RESTORE_USE_C
#define RR_Restore RR_RestoreC
#define RR_RestorePlayer RR_RestorePlayerC
#else
#define RR_Restore RR_RestoreCPU
#define RR_RestorePlayer RR_RestoreCPU
#endif
#endif
