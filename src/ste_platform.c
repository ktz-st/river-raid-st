/* Preserve GodLib's hardware lifecycle, without unused subsystems. */
#include <godlib/platform/platform.h>
#include <godlib/memory/memory.h>
#include <godlib/system/system.h>
#include <godlib/vbl/vbl.h>
#include <godlib/video/video.h>
#include <godlib/ikbd/ikbd.h>
#include <godlib/audio/audio.h>
#include <godlib/graphic/graphic.h>

static U16 hardwareActive;

void Platform_Init(void)
{
    Memory_Init();
    System_Init();
    /* The caller rejects other machines; do not touch STE-only registers. */
    if (System_GetVDO()!=VDO_STE) return;
    Vbl_Init();
    Video_Init();
    IKBD_Init();
    Audio_Init();
    System_DataCacheDisable();
    System_InstructionCacheDisable();
    System_SetCPUSpeed(16);
    IKBD_EnableJoysticks();
    Graphic_Init();
    hardwareActive=1;
}

void Platform_DeInit(void)
{
    if (hardwareActive) {
        Graphic_DeInit();
        Audio_DeInit();
        IKBD_EnableMouse();
        IKBD_DeInit();
        Video_DeInit();
        Vbl_DeInit();
    }
    System_DeInit();
    if (hardwareActive) {
        Vbl_WaitVbl();
        IKBD_FlushGemdos();
        Audio_SoundChipOff();
    }
    hardwareActive=0;
    Memory_DeInit();
}
