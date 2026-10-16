/*************************************************************************************
 * River Raid STE - szkielet na godlibie
 *
 * Teren z oryginalnego generatora A8, panel na split screenie
 * i samolot gracza z oryginalnymi klatkami PMG.
 *
 * Czym to sie rozni od wersji na AGT:
 *  - scroll i podwojne buforowanie robi Screen_* godliba, nie wlasny ring
 *  - przezbrajanie TimerB splitu nalezy do VBL-a, nie do petli glownej
 *  - wejscie przez IKBD_*
 *************************************************************************************/

#include <godlib/gemdos/gemdos.h>
#include <godlib/graphic/graphic.h>
#include <godlib/ikbd/ikbd.h>
#include <godlib/memory/memory.h>
#include <godlib/platform/platform.h>
#include <godlib/screen/screen.h>
#include <godlib/system/system.h>
#include <godlib/vbl/vbl.h>
#include <godlib/video/video.h>

#include "generator.h"
#include "gen_direct.h"
#include "native_assets.h"
#include "audio.h"
#include "cold_init.h"
#include "compact_line.h"
#include "generator_abi.h"
#include "modee.h"
#include "split.h"
#include "road_cpu.h"
#include "controls.h"
#include "title.h"
#include "title_assets.h"
#include "palette.h"
#include "raster_cpu.h"
static void RR_HudNumber(U8 *row,U16 first,U16 last,U32 value,U16 colour)
{
    U16 i;
    for (i=last+1; i-- > first;) {
        U16 digit;
        if (!value && i!=last) { row[i]=0; continue; }
        if (value<=655359UL) {
            U32 divided=value;
            /* Quotient fits in 16 bits: one native DIVU gives both results. */
            __asm__ volatile ("divu.w #10,%0" : "+d"(divided) : : "cc");
            digit=divided>>16; value=(U16)divided;
        } else { digit=value%10UL; value/=10UL; }
        row[i]=colour+16+digit;
    }
}
extern void RR_HudCell(U16 *panel,U16 cell,U16 code);
extern void RR_HudScorePair(U16 *panel,U16 scoreCell,U16 digit);
static U16 RR_HudFuel(U16 *panel,const U8 *rows,U16 fuel,unsigned *previous);
#ifndef RR_HUD_REFERENCE
#define A8_HUD_CELL RR_HudCell
#define A8_HUD_NUMBER RR_HudNumber
#define A8_HUD_FUEL RR_HudFuel
#define A8_HUD_FILL RR_FillRowsCPU
#endif
#include "hud.h"
#include "player.h"
#include "enemies.h"
#include "flight.h"
#include <string.h>

static U16 RR_HudFuel(U16 *panel,const U8 *rows,U16 fuel,unsigned *previous)
{
    U16 position=fuel>>11,x;
    if (*previous==position) return 0;
    if (*previous<32) {
        U16 old=88+*previous*2,column;
        for (column=old/16; column<=(old+3)/16; ++column) {
            RR_HudCell(panel,20+column,rows[20+column]);
            RR_HudCell(panel,40+column,rows[40+column]);
        }
    }
    x=88+position*2;
    RR_FuelMarkerCPU(panel+20*80+(x/16)*4,x&15);
    *previous=position; return 1;
}

/* ################################################################################ */

#define dWIDTH			320
#define dSTRIDE			(dWIDTH / 2)		/* bajtow na linie, 4 plany */

_Static_assert(dSTRIDE == 160, "RR_CopyRow160 requires 160-byte rows");
extern void RR_CopyRow160(const void * apSrc, void * apDst);

#define dVIEW			160					/* linie rzeki                       */
#define dRING			256					/* okres ringu, potega dwojki        */

/* TimerB switches to the separate HUD after dVIEW rows. Match GodLib's
 * scroll clamp to that viewport; only those rows need a ring duplicate.
 * Screen_Init still adds its legacy 32-row allocation reserve. */
#define dDUP			dVIEW
#define dCANVAS			(dRING + dDUP)
#ifdef RR_LOW_MEMORY
/* Screen_Init adds 32 rows itself; the ring needs exactly dCANVAS rows. */
#define dSCREEN_HEIGHT (dCANVAS - 32)
#else
#define dSCREEN_HEIGHT dCANVAS
#endif

#define dMAX_STEP		2

/* Wiersze generujemy Z WYPRZEDZENIEM, poza widocznym oknem: nowy wiersz
 * powstaje dLOOKAHEAD linii NAD widokiem i dopiero po kilku klatkach wjezdza
 * na ekran. Dzieki temu zaden zapis terenu nie laduje w aktualnie widocznym
 * zakresie - nawet w buforze tylnym. Musi byc > dMAX_STEP, bo wyprzedzenie
 * zjada sie o step na klatke. */
#define dLOOKAHEAD		12


/* Panel dostaje zapas linii ponad swoje 40: split moze wypasc o linie nizej,
 * a wtedy shifter czyta za koniec bufora i widac smieci pod HUD-em. */
/* kolory znacznika linii zapasu - celowo jaskrawe i obce panelowi */
#define dHUD_MARK_A		2
#define dHUD_MARK_B		3

#define dHUD_SPARE		8
#define dHUD_LINES		( A8_HUD_HEIGHT + dHUD_SPARE )
#define dHUD_BYTES		( (U32)A8_HUD_HEIGHT * dSTRIDE )
/* +1 linia rozbiegu PRZED panelem: linia, w trakcie ktorej nastepuje zapis
 * licznika, jest pobierana sprzed bufora - musi byc nasza i wyczyszczona,
 * inaczej widac tam smieci. */
#define dHUD_STORE		( (U32)( dHUD_LINES + 1 ) * dSTRIDE )

/* ################################################################################ */

static A8Generator *	gpGen;
static modee_lut_t		gLut;

static U16 *			gpPanel;			/* bufor panelu, poza plotnem        */
static U8				gPanelStore[ dHUD_STORE + 256 ];

static unsigned char	gHudCells[ 60 ];
typedef struct { U32 score, high; U16 lives, bridge, valid; } RRHudValues;
static RRHudValues gHudValues;

static U16 RR_HudField(U16 *panel,U8 *cells,U16 first,U16 last,U32 value,U16 colour)
{
    U8 row[7];
    U16 i,changed=0;
    RR_HudNumber(row,0,last-first,value,colour);
    for (i=first;i<=last;++i) if (cells[i]!=row[i-first]) {
        cells[i]=row[i-first]; RR_HudCell(panel,i,cells[i]); ++changed;
    }
    return changed;
}

static U16 RR_HudUpdate(U16 *panel,U8 *cells,const RRHudValues *old,
                       U32 score,U32 high,U16 lives,U16 bridge)
{
    U16 changed=0,i;
    if (!old->valid) return a8_hud_update(panel,cells,score,high,lives,bridge);
    if (score==high && (old->score!=score || old->high!=high)) {
        U8 row[7];
        RR_HudNumber(row,0,6,score,0);
        for (i=0;i<7;++i) {
            U16 code=row[i],record=code ? code+192 : 0;
            U16 a=cells[2+i]!=code,b=cells[12+i]!=record;
            if (a && b) RR_HudScorePair(panel,2+i,code);
            else if (a) RR_HudCell(panel,2+i,code);
            else if (b) RR_HudCell(panel,12+i,record);
            cells[2+i]=code; cells[12+i]=record; changed+=a+b;
        }
    } else {
        if (old->score!=score) changed+=RR_HudField(panel,cells,2,8,score,0);
        if (old->high!=high) changed+=RR_HudField(panel,cells,12,18,high,192);
    }
    if (old->bridge!=bridge) changed+=RR_HudField(panel,cells,36,38,bridge,128);
    if (old->lives!=lives) for (i=0;i<9;++i) {
        U16 code=i<lives ? 0x9c : 0;
        if (cells[51+i]!=code) {
            cells[51+i]=code; RR_HudCell(panel,51+i,code); ++changed;
        }
    }
    return changed;
}
static unsigned gFuelMark=65535;

/* Only the modal's glyphs: avoid linking GodLib's complete 12K font. */
static void RR_ConfirmTextOn(U16 *target,const char *text,U16 x,U16 y)
{
    static const char chars[]="QUITOS?Y/ENR:C";
    static const U8 blank[8]={0};
    static const U8 glyphs[][8]={
        {0x38,0x44,0x44,0x44,0x54,0x48,0x34,0},
        {0x44,0x44,0x44,0x44,0x44,0x44,0x38,0},
        {0x38,0x10,0x10,0x10,0x10,0x10,0x38,0},
        {0x7c,0x10,0x10,0x10,0x10,0x10,0x10,0},
        {0x38,0x44,0x44,0x44,0x44,0x44,0x38,0},
        {0x3c,0x40,0x40,0x38,0x04,0x04,0x78,0},
        {0x38,0x44,0x04,0x08,0x10,0,0x10,0},
        {0x44,0x44,0x28,0x10,0x10,0x10,0x10,0},
        {0x04,0x04,0x08,0x10,0x20,0x40,0x40,0},
        {0x7c,0x40,0x40,0x78,0x40,0x40,0x7c,0},
        {0x44,0x64,0x64,0x54,0x4c,0x4c,0x44,0},
        {0x78,0x44,0x44,0x78,0x50,0x48,0x44,0},
        {0,0x10,0x10,0,0x10,0x10,0,0},
        {0x38,0x44,0x40,0x40,0x40,0x44,0x38,0}
    };
    while (*text) {
        U16 i;
        for (i=0;chars[i] && chars[i]!=*text;++i) {}
        RR_TextGlyphCPU(target+(U32)y*(dSTRIDE/2)+(x>>4)*4,
            chars[i] ? glyphs[i] : blank,x&8);
        ++text; x+=8;
    }
}

static void RR_ConfirmText(const char *text,U16 x,U16 y)
{
    RR_ConfirmTextOn(gpPanel,text,x,y);
}

U16 gTitleActive;
volatile U16 gGameOverDelay;

static U8 RR_ShowTitle(void)
{
    unsigned previous=~0u;
    U16 confirm=0,redraw=1,first=1;
    U8 start=0;
    RRAudio_Reset();
    RRPalette_FadeOut();
    RRSplit_DeInit();
    gScreenClass.mViewportHeight=200;
    Screen_SetScrollY(0);
    gTitleActive=1;
    for (;;) {
        unsigned controls;
        U8 joy;
        if (redraw) {
            if (!RRTitle_Draw(Screen_GetpLogic())) break;
            if (confirm) {
                RR_ConfirmTextOn(Screen_GetpLogic(),"QUIT TO TOS?",112,80);
                RR_ConfirmTextOn(Screen_GetpLogic(),"Y/ENTER: YES   N/ESC: NO",64,104);
            }
            Screen_Update();
            if (first) { RRPalette_FadeIn(rrTitlePalette);first=0; }
            redraw=0;
        }
        IKBD_Update();
        joy=IKBD_GetJoy1Packet();
        while (IKBD_GetKbdBytesWaiting()) (void)IKBD_PopKbdByte();
        controls=rr_control_edges(&previous,
            (IKBD_GetKeyStatus(eIKBDSCAN_ESC) ? RR_CONTROL_ESCAPE : 0) |
            ((IKBD_GetKeyStatus(eIKBDSCAN_Y) || IKBD_GetKeyStatus(eIKBDSCAN_RETURN)) ? RR_CONTROL_YES : 0) |
            (IKBD_GetKeyStatus(eIKBDSCAN_N) ? RR_CONTROL_NO : 0) |
            ((IKBD_GetKeyStatus(eIKBDSCAN_LEFTSHIFT) ||
              IKBD_GetKeyStatus(eIKBDSCAN_RIGHTSHIFT) || (joy&128)) ? RR_CONTROL_FIRE : 0));
        if (confirm) {
            if (controls&RR_CONTROL_YES) break;
            if (controls&(RR_CONTROL_ESCAPE|RR_CONTROL_NO)) { confirm=0;redraw=1; }
        } else if (controls&RR_CONTROL_ESCAPE) { confirm=1;redraw=1; }
        else if (controls&RR_CONTROL_FIRE) { start=1;break; }
        Vbl_WaitVbl();
    }
    RRPalette_FadeOut();
    gTitleActive=0;
    gScreenClass.mViewportHeight=dVIEW;
    return start;
}

static void RR_GameOverPause(void)
{
    U32 began=Vbl_GetCounter();
    RRAudio_Reset();
    /* Freeze the last frame for 1.5 seconds after the final explosion. */
    gGameOverDelay=75;
    while (gGameOverDelay) {
        U32 elapsed;
        Vbl_WaitVbl();
        IKBD_Update();
        while (IKBD_GetKbdBytesWaiting()) (void)IKBD_PopKbdByte();
        elapsed=Vbl_GetCounter()-began;
        gGameOverDelay=elapsed>=75 ? 0 : 75-(U16)elapsed;
    }
}

static S16				gScroll;
static S16				gShownScroll;
static S16				gFill;			/* najnizszy wygenerowany wiersz */
static S16				gAhead;			/* ile wierszy gotowych nad widokiem */
#ifdef dAUDIT
static U32				gAuditPhysic;
static U32				gAuditVisible;
#endif			/* wiersz ringu na gorze widoku      */
static S16				gStep = 1;
static RRFlight gFlight;
U16 gPaused;
U16 gQuitConfirm;
static S32 gRoadTop;
#ifdef RR_FRAME_TRACE
/* Diagnostic build only: one record per presented frame, no per-row probes. */
typedef struct { U32 start,ready,shown; U16 scroll,speed,step,bridge; } RRFrameTrace;
RRFrameTrace gFrameTrace[1024];
U16 gFrameTraceFlags[1024];
U32 gFrameTraceScores[1024];
U16 gFrameTraceCount,gFrameTraceArmed;
#endif
/* Only writable generator RAM is retained; ROM and unused address space
 * are shared. No per-frame framebuffer copies and no disk IO on respawn. */
typedef struct {
    U8 ram[A8_GEN_RAM_BYTES], tail[sizeof(A8Generator)-A8_GEN_STATE_OFFSET];
    U16 canvas[dRING*(dSTRIDE/2)];
    S16 scroll, fill, ahead;
    S32 road;
} RRCheckpoint;
static RRCheckpoint gCheckpoint;
static U16 gCheckpointRowsLeft,gCheckpointRow;
static U16 gCheckpointSeeded;
static U16 gCheckpointBridgeAdvance;
_Static_assert(offsetof(RRCheckpoint, canvas) == 0x614, "checkpoint canvas ABI");
_Static_assert(sizeof(RRCheckpoint) == 42526UL, "checkpoint size");

/* Snapshot rows in the same descending order as future terrain writes. Copy
 * eight per frame while generation consumes at most two. No extra bitmap. */
static void RR_CheckpointCopy(U16 budget)
{
    extern void RR_CopyCheckpoint(const void *src,void *dst,U32 bytes);
    while (budget && gCheckpointRowsLeft) {
        U16 n=budget,first;
        if (n>gCheckpointRowsLeft) n=gCheckpointRowsLeft;
        if (n>gCheckpointRow+1) n=gCheckpointRow+1;
        first=gCheckpointRow+1-n;
        RR_CopyCheckpoint((const U8*)Screen_GetpBack()+(U32)first*dSTRIDE,
            (U8*)gCheckpoint.canvas+(U32)first*dSTRIDE,(U32)n*dSTRIDE);
        gCheckpointRow=(gCheckpointRow-n)&(dRING-1);
        gCheckpointRowsLeft-=n; budget-=n;
    }
}

static void RR_Checkpoint(U16 restore)
{
    extern void RR_CopyCheckpoint(const void *src,void *dst,U32 bytes);
    U8 *tail=(U8*)gpGen+A8_GEN_STATE_OFFSET;
    if (!restore) {
        gCheckpointBridgeAdvance=gBridgeCrashAdvance;
        if (!gCheckpointSeeded) {
            RR_CopyCheckpoint(gpGen->memory,gCheckpoint.ram,sizeof(gCheckpoint.ram));
            gCheckpointSeeded=1;
        } else {
            /* Native generator never writes $100..$4ff; checked by its builder.
             * Preserve the initial immutable image, refresh only mutable pages. */
            RR_CopyCheckpoint(gpGen->memory,gCheckpoint.ram,0x100);
            RR_CopyCheckpoint(gpGen->memory+0x500,gCheckpoint.ram+0x500,0x100);
        }
        RR_CopyCheckpoint(tail,gCheckpoint.tail,sizeof(gCheckpoint.tail));
        gCheckpointRow=(gFill-1)&(dRING-1);
        gCheckpointRowsLeft=dRING;
        gCheckpoint.scroll=gScroll; gCheckpoint.fill=gFill;
        gCheckpoint.ahead=gAhead; gCheckpoint.road=gRoadTop;
    } else {
        /* Usually complete during the 40-frame death animation. Also support
         * an immediate restore before the background snapshot has drained. */
        RR_CheckpointCopy(dRING);
        RR_CopyCheckpoint(gCheckpoint.ram,gpGen->memory,sizeof(gCheckpoint.ram));
        RR_CopyCheckpoint(gCheckpoint.tail,tail,sizeof(gCheckpoint.tail));
        RR_CopyCheckpoint(gCheckpoint.canvas,Screen_GetpBack(),(U32)dRING*dSTRIDE);
        RR_CopyCheckpoint(Screen_GetpBack(),
            (U8*)Screen_GetpBack()+(U32)dRING*dSTRIDE,(U32)dDUP*dSTRIDE);
        gScroll=gCheckpoint.scroll; gFill=gCheckpoint.fill;
        gAhead=gCheckpoint.ahead; gRoadTop=gCheckpoint.road;
    }
    RREnemies_Checkpoint(restore);
    if (restore) gBridgeCrashAdvance=gCheckpointBridgeAdvance;
}

static void RR_RoadRow(U16 *dst, S32 world)
{
    S32 roadY=world-gRoadTop;
    if (roadY>=0 && roadY<20) {
        RR_RoadCPU(dst,rrRoadInks[(U16)roadY]);
    }
}

static void RR_PaintRow(const U8 *record, S16 row, S32 world)
{
    U16 *dst=(U16*)((U8*)Screen_GetpBack()+(U32)row*dSTRIDE);
    RR_LinePlanar(record,&gLut,dst);
    RR_RoadRow(dst,world);
}

/* ################################################################################ */

/* Konwersja pisze PROSTO w czyste plotno. Stamtad rozprowadzamy wiersz do
 * buforow wyswietlania - ale NIGDY do tego, ktory wlasnie jest na ekranie.
 *
 * Bufory sie zamieniaja, wiec kazdy nowy wiersz trzeba dociagnac dwa razy:
 * w klatce, w ktorej powstal, i w nastepnej, gdy drugi bufor stanie sie
 * logicznym. Zapisywanie od razu do obu (czyli takze do fizycznego) dawalo
 * widoczne rysowanie nowej linii tuz nad panelem.
 */

#define dPENDING_MAX	( dMAX_STEP * 2 )

static S16	gPending[ dPENDING_MAX ];
static U16	gPendingCount;

static void RR_CopyRowToLogic( S16 aRow )
{
	const U32	lOffset = (U32)aRow * dSTRIDE;
	const U8 *	lpSrc   = (const U8*)Screen_GetpBack() + lOffset;

#ifdef dAUDIT
	/* 1. czy cel zapisu to przypadkiem bufor wlasnie wyswietlany? */
	if( ( Screen_GetpLogic() == Screen_GetpPhysic() ) ||
		( Screen_GetpBack()  == Screen_GetpPhysic() ) )
		gAuditPhysic++;

	/* 2. czy zapisywany wiersz wpada w okno, ktore wlasnie jest na ekranie?
	 *    Wyswietlany jest scroll z POPRZEDNIEJ klatki, nie biezacy. */
	{
		S16 lLo = gShownScroll;
		S16 lHi = (S16)( gShownScroll + dVIEW - 1 );

		if( ( aRow >= lLo ) && ( aRow <= lHi ) )
			gAuditVisible++;
		if( ( aRow < dDUP ) &&
			( (S16)( aRow + dRING ) >= lLo ) && ( (S16)( aRow + dRING ) <= lHi ) )
			gAuditVisible++;
	}
#endif

	RR_CopyRow160( lpSrc, (U8*)Screen_GetpLogic() + lOffset );

	if( aRow < dDUP )
	{
		const U32 lDup = (U32)( aRow + dRING ) * dSTRIDE;

		RR_CopyRow160( lpSrc, (U8*)Screen_GetpLogic() + lDup );
	}
}

/* Dociaga do biezacego bufora logicznego wiersze z poprzedniej klatki. */
static void RR_FlushPending( void )
{
	U16 i;

	for( i = 0; i < gPendingCount; i++ )
		RR_CopyRowToLogic( gPending[ i ] );

	gPendingCount = 0;
}

static void RR_SpreadRow( S16 aRow )
{
	/* Back's ring duplicate changes once, not again when the other page flushes. */
	if (aRow<dDUP) {
		U8 *src=(U8*)Screen_GetpBack()+(U32)aRow*dSTRIDE;
		RR_CopyRow160(src,src+(U32)dRING*dSTRIDE);
	}
	RR_CopyRowToLogic( aRow );

	if( gPendingCount < dPENDING_MAX )
		gPending[ gPendingCount++ ] = aRow;
}

/* ################################################################################ */

static U8 RR_GenRow( S16 aRow )
{
	U8 lRecord[ RR_LINE_DIRECT_MAX ];
	/* Protect the oldest pending snapshot row before it is overwritten, even
	 * if future code asks for more than the normal two generated rows/tick. */
	if (gCheckpointRowsLeft && (U16)aRow==gCheckpointRow) RR_CheckpointCopy(1);

	if( !a8_generator_compact_line( gpGen, lRecord ) )
		return 0;
	RREnemies_Spawn( gpGen );
	if (gpGen->spawned && gpGen->spawn_type==14 && (gpGen->spawn_flags&128))
		gRoadTop=-(S32)gpGen->rows+(S16)gpGen->spawn_y-0x5d;
	RR_PaintRow(lRecord,aRow,-(S32)gpGen->rows);

	RR_SpreadRow( aRow );

	return 1;
}

static U8 RR_FillAhead(void)
{
    while (gAhead<dLOOKAHEAD) {
        gFill=(gFill-1)&(dRING-1);
        if (!RR_GenRow(gFill)) return 0;
        ++gAhead;
    }
    return 1;
}

/* The snapshot was committed on destruction. On respawn only, position that
 * saved section at road-top=141 without changing its checkpoint or live view. */
static U8 RR_BridgeCrashCheckpoint(void)
{
    U16 rows;
    if (!gBridgeCrashAdvance) return 1;
    rows=RREnemies_BridgeCrashRows();
    while (rows--) {
        gScroll=(gScroll-1)&(dRING-1);
        --gAhead;
        RREnemies_Scroll();
        if (!RR_FillAhead()) return 0;
    }
    gBridgeCrashAdvance=0;
    return 1;
}

/* ################################################################################ */

static U8 RR_ResetRiver(void);

static U8 RR_Load( void )
{
	U8		lSelMap[ 4 ];
	U8 result;
	RRAudio_Reset();
	gCheckpointRowsLeft=0;

	if( !gpGen ) gpGen = (A8Generator*)mMEMALLOC( (U32)sizeof( A8Generator ) );
	if( !gpGen ) return 0;
	a8_generator_cold_init( gpGen, 0 );
	gpGen->actor_scheduler = 1;
	RREnemies_Init( 0, 0 );
	RRPlayer_Init( 0 );

	/* Teren NIE idzie przez tablice inkow - selektor Mode E jest wprost
	 * indeksem palety ST (1 = zielen, 2 = brzeg, 3 = woda). */
	modee_selmap_from_memory( gpGen->memory, 0, lSelMap );
	modee_build_lut( &gLut, lSelMap );

	result=RR_ResetRiver();

	return result;
}

static void RR_PublishRiver(void)
{
    Memory_Copy((U32)dDUP*dSTRIDE,Screen_GetpBack(),
        (U8*)Screen_GetpBack()+(U32)dRING*dSTRIDE);
    Memory_Copy((U32)dSTRIDE*dCANVAS,Screen_GetpBack(),Screen_GetpLogic());
    Screen_SetScrollY(gScroll);
    Screen_Update();
    Memory_Copy((U32)dSTRIDE*dCANVAS,Screen_GetpBack(),Screen_GetpLogic());
    gShownScroll=gScroll;
    gPendingCount=0;
    rr_flight_reset(&gFlight);
    gStep=0;
}

static U8 RR_ResetRiver(void)
{
	S16 k;
	U8 record[RR_LINE_DIRECT_MAX];
	gCheckpointSeeded=0;
	RREnemies_Reset();
	gRoadTop=(S16)gpGen->memory[0x16]-0x5d;
	Memory_Clear((U32)dSTRIDE*dCANVAS,Screen_GetpBack());
	/* Original scene starts with bridge Y=$4a; 160 rows put it at
	 * $ea, i.e. ST row 141. Render newest rows above older rows. */
	for (k=0; k<dVIEW; ++k) {
		if (!a8_generator_compact_line(gpGen,record)) return 0;
		RR_PaintRow(record,dVIEW-1-k,-(S32)gpGen->rows);
	}
	/* Adopt the prepared actor list once; prefix events are not live spawns. */
	RREnemies_Initial(gpGen);
	gRoadTop=(S16)gpGen->memory[0x16]-0x5d;
	if (gRoadTop!=141) return 0;
	gpGen->rows=0;
	gScroll = gShownScroll = gFill = gAhead = 0;
	gPendingCount = 0;
	while( gAhead < dLOOKAHEAD )
	{
		gFill = (S16)( ( gFill - 1 ) & ( dRING - 1 ) );
		if( !RR_GenRow( gFill ) ) return 0;
		gAhead++;
	}
	/* Never rebuild the page being displayed. Flip to the complete new
	 * landscape before refreshing the formerly physical page. */
	RR_PublishRiver();
	RR_Checkpoint(0);
	return 1;
}

static U8 RR_StartTitleGame(void)
{
    if (!RR_ShowTitle()) return 0;
    gPaused=0;
    gScore=0;
    if (!RR_Load()) return 0;
    gPlayerX=gLaunchX;
    gPlayerState=RR_PLAYER_FLYING;
    gHudValues.valid=0;
    a8_hud_draw(gpPanel,gScore,gHighScore,gPlayerLives,gEnemyBridge);
    a8_hud_rows(gHudCells,gScore,gHighScore,gPlayerLives,gEnemyBridge);
    Memory_Clear(dSTRIDE,gpPanel);
    gFuelMark=65535;
    a8_hud_fuel(gpPanel,gHudCells,gFuel,&gFuelMark);
    RRSplit_Init(gpPanel,dVIEW);
    RREnemies_Draw(gScroll);
    RRPlayer_Draw(gScroll);
    Screen_Update();
    RRPalette_FadeIn(rrPalette);
    return 1;
}

/* ################################################################################ */

S16 GodLib_Game_Main( S16 aArgCount, char * apArgs[] )
{
	void *	lpSsp;
	U8		lQuit = 0;
	S16		k;
#ifdef dRUN_FRAMES
	U32		lFrames = 0;
#endif
	unsigned previousControls=0;

	(void)aArgCount;
	(void)apArgs;

	lpSsp = GemDos_Super( 0 );
	Platform_Init();

	if( System_GetVDO() != VDO_STE )
	{
		Platform_DeInit();
		return 1;
	}

	RRPalette_Black();
	Graphic_SetBlitterEnable( 1 );
	Screen_Init( dWIDTH, dSCREEN_HEIGHT, eGRAPHIC_COLOURMODE_4PLANE, eSCREEN_SCROLL_V );
	if( !gScreenClass.mpMemBase )
	{
		Platform_DeInit();
		return 1;
	}

	/* Hardware stays ST-low 320x200; TimerB supplies the separate 40-row HUD. */
	gScreenClass.mViewportHeight = dVIEW;

	IKBD_EnableJoysticks();
	/* Native YM2149 via one VBL callback; unavailable audio stays silent. */
	RRAudio_Init();

	/* ---------------------------------------------------------------- */
	/*	wstepne wypelnienie ringu - najnowszy wiersz na indeksie 0       */


	/* ---------------------------------------------------------------- */
	/*	panel + split                                                    */

	Memory_Clear( sizeof( gPanelStore ), gPanelStore );

	/* panel zaczyna sie JEDNA LINIE dalej - przed nim zostaje czysty rozbieg */
	gpPanel = (U16*)( ( ( (U32)gPanelStore + 255UL ) & ~255UL ) + dSTRIDE );

	a8_hud_draw( gpPanel, 0, 0, 3, 1 );
	a8_hud_rows( gHudCells, 0, 0, 3, 1 );
	a8_hud_fuel( gpPanel, gHudCells, gFuel, &gFuelMark );

	/* Split completes before fetching HUD row 0: one black separator row. */
	RR_FillRowsCPU(gpPanel,0,1);

	/* Linie zapasu pod panelem malujemy KONTRASTOWO, nie tlem panelu.
	 * Dzieki temu natychmiast widac, czy split odpalil za wczesnie: kazda
	 * widoczna linia znacznika to jedna linia spoznienia. Gdyby zapas byl
	 * w kolorze panelu, problem bylby niewidoczny zamiast naprawiony. */
	{
		U16 l;
		for( l = A8_HUD_HEIGHT; l < dHUD_LINES; l++ )
		{
			const U16 lInk = ( l & 1 ) ? dHUD_MARK_A : dHUD_MARK_B;
			U16 * lpLine = gpPanel + (U32)l * MODEE_ST_WORDS;

			RR_FillRowsCPU(lpLine,lInk,1);
		}
	}

	lQuit=!RR_StartTitleGame();

	/* ---------------------------------------------------------------- */

#ifdef dAUDIT
	/* Wstepne wypelnienie liczy sie do audytu, choc nic jeszcze nie jest
	 * wyswietlane - zerujemy liczniki dopiero tutaj. */
	gAuditPhysic  = 0;
	gAuditVisible = 0;
#endif

	while( !lQuit )
	{
		U8 lJoy;
		U16 hudChanged;
		U16 bridgeBefore;
		unsigned controls;
#ifdef RR_FRAME_TRACE
		U32 traceStart=Vbl_GetCounter(),traceReady;
		U16 traceFlags=0,traceFuel=gFuelMark;
#endif
#ifdef dRUN_FRAMES
		if( lFrames >= dRUN_FRAMES )
			lQuit = 1;
#endif
		IKBD_Update();
		lJoy = IKBD_GetJoy1Packet();
		while( IKBD_GetKbdBytesWaiting() )
		{
			(void)IKBD_PopKbdByte();
		}
		if (lQuit) break;
		controls=rr_control_edges(&previousControls,
			(IKBD_GetKeyStatus(eIKBDSCAN_SPACE) ? RR_CONTROL_PAUSE : 0) |
			(IKBD_GetKeyStatus(eIKBDSCAN_F1) ? RR_CONTROL_RESTART : 0) |
			(IKBD_GetKeyStatus(eIKBDSCAN_ESC) ? RR_CONTROL_ESCAPE : 0) |
			((IKBD_GetKeyStatus(eIKBDSCAN_Y) || IKBD_GetKeyStatus(eIKBDSCAN_RETURN)) ? RR_CONTROL_YES : 0) |
			(IKBD_GetKeyStatus(eIKBDSCAN_N) ? RR_CONTROL_NO : 0) |
			((IKBD_GetKeyStatus(eIKBDSCAN_LEFTSHIFT) ||
			  IKBD_GetKeyStatus(eIKBDSCAN_RIGHTSHIFT) || (lJoy&128)) ? RR_CONTROL_FIRE : 0));
		if (gQuitConfirm) {
			if (controls&RR_CONTROL_YES) { lQuit=1; break; }
			if (controls&(RR_CONTROL_ESCAPE|RR_CONTROL_NO)) {
				gQuitConfirm=0;
				Vbl_WaitVbl();
				a8_hud_draw(gpPanel,gScore,gHighScore,gPlayerLives,gEnemyBridge);
				Memory_Clear(dSTRIDE,gpPanel);
				gFuelMark=65535;
				a8_hud_fuel(gpPanel,gHudCells,gFuel,&gFuelMark);
			}
			Vbl_WaitVbl();
			continue;
		}
		if (controls&RR_CONTROL_ESCAPE) {
			gQuitConfirm=1;
			RRAudio_Reset();
			Vbl_WaitVbl();
			Memory_Clear(dHUD_BYTES,gpPanel);
			RR_ConfirmText("QUIT TO TOS?",112,8);
			RR_ConfirmText("Y/ENTER: YES   N/ESC: NO",64,24);
			continue;
		}
		if (controls&RR_CONTROL_RESTART) {
			gPaused=0;
			gScore=0;
			if (!RR_Load()) { lQuit=1; break; }
			gPlayerX=gLaunchX;
		} else {
			if (controls&RR_CONTROL_PAUSE) {
				gPaused^=1;
				if (gPaused) RRAudio_Reset();
			} else if (gPaused && (controls&RR_CONTROL_FIRE)) gPaused=0;
			if (gPaused) {
				/* Do not flip an undrawn page or advance gameplay while paused. */
				Vbl_WaitVbl();
				continue;
			}
			if( RRPlayer_Update( lJoy ) )
			{
				U16 newGame = gPlayerState == RR_PLAYER_GAME_OVER;
				if (newGame) {
					RR_GameOverPause();
					if (!RR_StartTitleGame()) { lQuit=1; break; }
				} else {
					RR_Checkpoint(1);
					if (!RR_BridgeCrashCheckpoint()) { lQuit=1; break; }
					RR_PublishRiver();
					RRPlayer_Respawn(0);
				}
				RRAudio_Reset();
				gPlayerX=gLaunchX;
			}
		}

		gStep=gPlayerState==RR_PLAYER_FLYING ? rr_flight_step(&gFlight,
			IKBD_GetKeyStatus(eIKBDSCAN_UPARROW) || (lJoy&1),
			IKBD_GetKeyStatus(eIKBDSCAN_DOWNARROW) || (lJoy&2)) : 0;

		bridgeBefore=gEnemyBridge;
		RRPlayer_Restore();
		RREnemies_Restore();
		RR_FlushPending();

		/* scroll konsumuje zapas... */
		RRPlayer_CheckBank( gScroll );
		RREnemies_CheckPlayer();
		for( k = 0; k < gStep && gPlayerState == RR_PLAYER_FLYING; k++ )
		{
			gScroll = (S16)( ( gScroll - 1 ) & ( dRING - 1 ) );
			gAhead--;
			RREnemies_Scroll();
			/* Test each consumed row, including at maximum scroll speed. */
			RRPlayer_CheckBank( gScroll );
			RREnemies_CheckPlayer();
		}

		/* ...a generowanie go odtwarza, zawsze POZA widocznym oknem */
		if (!RR_FillAhead()) lQuit=1;

		/* Kolejnosc jak na ST: rysuj -> zamien strony -> czekaj na ramke.
		 * Screen_Update robi dwa ostatnie kroki sam: najpierw przestawia
		 * bufor i programuje sprzet, potem czeka na VBL (screen.c:200).
		 * Wlasny Vbl_WaitVbl PRZED nim odwracal to - programowanie bazy
		 * ladowalo tuz po VBL, czyli za pozno na zatrzask poczatku ramki. */
		RREnemies_Update( gScroll );
		RREnemies_CheckPlayer();
		RREnemies_Combat( lJoy, gScroll );
		/* Both bullets and ramming commit in the destruction frame, even if
		 * the player has just died. Crossing an intact span never saves. */
		if (gEnemyBridge!=bridgeBefore) {
			RR_Checkpoint(0);
#ifdef RR_FRAME_TRACE
			traceFlags|=4;
#endif
		}
		RREnemies_Weapons();
		RRFuel_Update();
		RRAudio_Update( gFlight.speed );
#ifdef RR_FRAME_TRACE
		if (Vbl_GetCounter()!=traceStart) traceFlags|=16;
#endif
		hudChanged=!gHudValues.valid || gHudValues.score!=gScore || gHudValues.high!=gHighScore ||
			gHudValues.lives!=gPlayerLives || gHudValues.bridge!=gEnemyBridge;
		if (hudChanged) {
#ifdef RR_FRAME_TRACE
			traceFlags|=1;
#endif
			RR_HudUpdate( gpPanel, gHudCells, &gHudValues, gScore, gHighScore, gPlayerLives, gEnemyBridge );
			gHudValues.score=gScore; gHudValues.high=gHighScore;
			gHudValues.lives=gPlayerLives; gHudValues.bridge=gEnemyBridge; gHudValues.valid=1;
		}
		a8_hud_fuel( gpPanel, gHudCells, gFuel, &gFuelMark );
		/* Do not stack the amortized bitmap copy on top of a score/bridge HUD
		 * update. RR_GenRow protects snapshot rows even if this work is deferred. */
		if (!hudChanged) RR_CheckpointCopy(8);
#ifdef RR_FRAME_TRACE
		if (Vbl_GetCounter()!=traceStart) traceFlags|=32;
		if (traceFuel!=gFuelMark) traceFlags|=2;
#endif
		RREnemies_Draw( gScroll );
		RRPlayer_Draw( gScroll );
		Screen_SetScrollY( gScroll );
#ifdef RR_FRAME_TRACE
		traceReady=Vbl_GetCounter();
#endif
		Screen_Update();
#ifdef RR_FRAME_TRACE
		if (gFrameTraceArmed && gFrameTraceCount<1024) {
			gFrameTraceFlags[gFrameTraceCount]=traceFlags;
			gFrameTraceScores[gFrameTraceCount]=gScore;
			RRFrameTrace *t=&gFrameTrace[gFrameTraceCount++];
			t->start=traceStart; t->ready=traceReady; t->shown=Vbl_GetCounter();
			t->scroll=gScroll; t->speed=gFlight.speed; t->step=gStep; t->bridge=gEnemyBridge;
		}
#endif

		gShownScroll = gScroll;		/* od teraz TO jest na ekranie */

#ifdef dRUN_FRAMES
		lFrames++;
#endif
	}

	RRAudio_DeInit();
	RRSplit_DeInit();
	Screen_DeInit();

	Platform_DeInit();
	GemDos_Super( lpSsp );

	return 0;
}
