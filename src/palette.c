#include "palette.h"
#include <godlib/vbl/vbl.h>
#include <godlib/video/video.h>

U16 gRRPalette[16];
volatile U16 gPaletteStep,gPaletteFading;
static const U16 black[16]={0};

static U16 Intensity(U16 nibble)
{
    return ((nibble&7)<<1)|((nibble>>3)&1);
}

void RRPalette_Set(const U16 *palette)
{
    U16 i;
    for (i=0;i<16;++i) gRRPalette[i]=palette[i];
    Video_SetPalST(gRRPalette);
}

static void Fade(const U16 *target)
{
    U16 from[16],next[16],i,step;
    for (i=0;i<16;++i) from[i]=gRRPalette[i];
    gPaletteFading=1;
    for (step=1;step<=16;++step) {
        for (i=0;i<16;++i) {
            U16 shift,colour=0;
            /* STE stores the least significant DAC bit at bit 3. */
            for (shift=0;shift<=8;shift+=4) {
                U16 value=(Intensity(from[i]>>shift)*(16-step)+
                           Intensity(target[i]>>shift)*step)>>4;
                colour|=((value>>1)|((value&1)<<3))<<shift;
            }
            next[i]=colour;
        }
        Vbl_WaitVbl();
        gPaletteStep=step;
        RRPalette_Set(next);
    }
    /* Keep the final palette visible for a complete frame before drawing. */
    Vbl_WaitVbl();
    gPaletteFading=0;
}

void RRPalette_Black(void)
{
    Vbl_WaitVbl();
    RRPalette_Set(black);
}

void RRPalette_FadeOut(void)
{
    U16 i;
    for (i=0;i<16;++i) if (gRRPalette[i]) { Fade(black);return; }
}

void RRPalette_FadeIn(const U16 *target)
{
    Fade(target);
}
