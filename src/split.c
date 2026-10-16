/*************************************************************************************
 * Split ekranu: 160 linii plynacej rzeki + 40 linii panelu.
 *
 * Wzorowane na pacmad.ste/src/split.c. Dwie rzeczy, ktorych brakowalo
 * w poprzednim, recznym podejsciu:
 *
 *  1. Przezbrajanie TimerB nalezy do VBL-a, nie do petli glownej. Gdy robila
 *     to petla, kazda zgubiona klatka trwale rozjezdzala faze, bo licznik
 *     niesie reszte impulsow DE na nastepna ramke.
 *
 *  2. TimerC TOS-u trzyma IPL 6 przez granice rastra i opoznia nasze
 *     zdarzenie - jest maskowany na czas klatki i oddawany w handlerze.
 *************************************************************************************/

#include <godlib/base/base.h>
#include <godlib/memory/memory.h>
#include <godlib/mfp/mfp.h>
#include <godlib/vbl/vbl.h>

#include "split.h"

/* ################################################################################ */

/* czytane przez handler w split_s.s */
volatile U32	gRRSplitPanel;
volatile U16	gRRSplitTimerCMask;
volatile U8	gRRSplitReload;

extern void		RRSplit_TimerB( void );

static void		(*gpOldVideoFunc)( void );
static void		(*gpOldTimerBFunc)( void );
static U8		gOldTimerBLine;
static sMfpTimer gOldTimer;
static U8		gInstalled;

/* ################################################################################ */

static void RRSplit_Video( void )
{
	/* TimerC TOS-u potrafi przetrzymac IPL 6 przez granice rastra.
	 * Odkladamy go na czas klatki; handler splitu go odda. */
	gRRSplitTimerCMask = (U16)( *(volatile U8*)0xfffffa15UL & 0x20 );
	*(volatile U8*)0xfffffa15UL &= (U8)~0x20;

	if( gpOldVideoFunc )
		gpOldVideoFunc();
}

/* ################################################################################ */

void RRSplit_Init( void * apPanel, U16 aLine )
{
	sMfpTimer lTimer;

	gRRSplitPanel = (U32)apPanel;
	/* Enter one DE event early, then wait for the next falling edge. */
	gRRSplitReload = (U8)(aLine - 1);

	gpOldVideoFunc  = Vbl_GetpVbl()->mfVideoFunc;
	gpOldTimerBFunc = Vbl_GetpVbl()->mfTimerBFunc;
	gOldTimerBLine  = Vbl_GetpVbl()->mTimerBScanLine;
	Mfp_GetTimerB( &gOldTimer );

	Memory_Clear( sizeof( lTimer ), &lTimer );
	lTimer.mfTimerFunc = RRSplit_TimerB;
	lTimer.mData       = gRRSplitReload;
	lTimer.mMask       = 1;
	lTimer.mEnable     = 1;

	Vbl_InstallTimerB( &lTimer );
	Vbl_SetVideoFunc( RRSplit_Video );

	gInstalled = 1;
}

/* ################################################################################ */

void RRSplit_DeInit( void )
{
	if( !gInstalled )
		return;

	*(volatile U8*)0xfffffa15UL |= (U8)gRRSplitTimerCMask;

	Vbl_SetVideoFunc( gpOldVideoFunc );
	Vbl_GetpVbl()->mfTimerBFunc    = gpOldTimerBFunc;
	Vbl_GetpVbl()->mTimerBScanLine = gOldTimerBLine;
	Mfp_InstallTimerB( &gOldTimer );

	gInstalled = 0;
}
