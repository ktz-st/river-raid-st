/* River Raid uses only four-plane canvases and the original blitter sprites. */
#include <godlib/graphic/graphic.h>
#include <godlib/memory/memory.h>

extern void Graphic_4BP_DrawSprite_BLT(sGraphicCanvas *,sGraphicPos *,void *);
extern void Graphic_4BP_DrawSprite_Clip_BLT(sGraphicCanvas *,sGraphicPos *,void *);

sGraphicFuncs gGraphicFuncs[eGRAPHIC_COLOURMODE_LIMIT];
sGraphicFuncs gGraphicFuncsClip[eGRAPHIC_COLOURMODE_LIMIT];
U8 gGraphicBlitterEnableFlag;

void Graphic_SetBlitterEnable(U8 flag)
{
    (void)flag;
    /* This build requires an STE. No software fallback dispatch is linked. */
    gGraphicBlitterEnableFlag=1;
    gGraphicFuncs[eGRAPHIC_COLOURMODE_4PLANE].DrawSprite=Graphic_4BP_DrawSprite_BLT;
    gGraphicFuncsClip[eGRAPHIC_COLOURMODE_4PLANE].DrawSprite=Graphic_4BP_DrawSprite_Clip_BLT;
}

void Graphic_Init(void) { Graphic_SetBlitterEnable(1); }
void Graphic_DeInit(void) {}

void GraphicCanvas_Init(sGraphicCanvas *canvas,U16 mode,U16 width,U16 height)
{
    U32 row,offset=0,stride=((U32)(width+15)&0xfff0UL)>>1;
    canvas->mColourMode=mode;
    canvas->mWidth=width;
    canvas->mHeight=height;
    canvas->mpLineOffsets=(U32*)mMEMALLOC(((U32)height+1)*sizeof(U32));
    for (row=0;row<=height;++row) {
        canvas->mpLineOffsets[row]=offset;
        offset+=stride;
    }
    canvas->mpFuncs=&gGraphicFuncs[mode];
    canvas->mpClipFuncs=&gGraphicFuncsClip[mode];
    canvas->mClipBox.mX0=canvas->mClipBox.mY0=0;
    canvas->mClipBox.mX1=width;
    canvas->mClipBox.mY1=height;
}

void GraphicCanvas_DeInit(sGraphicCanvas *canvas)
{
    if (canvas->mpLineOffsets) {
        mMEMFREE(canvas->mpLineOffsets);
        canvas->mpLineOffsets=0;
    }
}
