#ifndef RR_SPLIT_H
#define RR_SPLIT_H

#include <godlib/base/base.h>

/* Panel ma wlasny bufor poza plotnem i nie scrolluje: na linii podzialu
 * przerwanie TimerB przestawia LICZNIK adresu wideo na ten bufor.
 * Przezbrajanie co ramke robi VBL godliba, a nie petla glowna - to wlasnie
 * ta czesc rozjezdzala sie przy zgubionej klatce w poprzednim podejsciu. */

void	RRSplit_Init( void * apPanel, U16 aLine );
void	RRSplit_DeInit( void );

#endif
