#include "title.h"
#include "title_assets.h"

U8 RRTitle_Draw(void *screen)
{
    const U8 *source=rrTitlePacked,*end=source+rrTitlePackedBytes;
    U8 *out=(U8*)screen,*limit=out+32000UL,*start=out;
    while (out<limit) {
        U16 flags,bit;
        if (source==end) return 0;
        flags=*source++;
        for (bit=1;bit<256 && out<limit;bit<<=1) {
            if (flags&bit) {
                U16 code,distance,count;
                U8 *copy;
                if (end-source<2) return 0;
                code=(U16)*source++<<8;
                code|=*source++;
                distance=(code>>4)+1;count=(code&15)+3;
                if ((U32)(out-start)<distance || (U32)(limit-out)<count) return 0;
                copy=out-distance;
                while (count--) *out++=*copy++;
            } else {
                if (source==end) return 0;
                *out++=*source++;
            }
        }
    }
    if (source!=end) return 0;
    return 1;
}
