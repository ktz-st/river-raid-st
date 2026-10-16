/*
 * modee - konwersja wiersza ANTIC Mode E na wiersz planarny ST (4 bitplany).
 *
 * Zrodlo: 48 bajtow wide-DMA Mode E = 192 logicznych pikseli.
 * Widoczne jest centralne 160 pikseli (bajty 4..43), bo A8 w trybie wide
 * ma po 4 bajty overscanu z kazdej strony. Kazdy logiczny piksel dublujemy
 * poziomo: 160 * 2 = 320 pikseli ST.
 *
 * Jeden bajt Mode E (4 piksele) daje 8 pikseli ST = 8 bitow na plan, czyli
 * polowe slowa. Dwa bajty zrodla skladaja sie wiec dokladnie w jedna grupe
 * 16 pikseli = 4 slowa planarne. Stad tablica LUT bajt -> 4 bajty planow
 * i konwersja bez zadnych operacji na bitach w petli.
 */

#ifndef MODEE_H
#define MODEE_H

#include <stdint.h>

#define MODEE_SRC_BYTES		48		/* pelny wiersz wide-DMA        */
#define MODEE_VIS_FIRST		4		/* pierwszy widoczny bajt       */
#define MODEE_VIS_BYTES		40		/* widocznych bajtow            */

#define MODEE_ST_WIDTH		320		/* pikseli ST w wierszu         */
#define MODEE_ST_GROUPS		20		/* grup po 16 pikseli           */
#define MODEE_ST_WORDS		80		/* 20 grup * 4 plany            */
#define MODEE_ST_BYTES		160		/* bajtow na wiersz ekranu ST   */

#define MODEE_SOLID_WORDS	(21)	/* 20 slow na 320 px + 1 slowo wartownika */

typedef struct modee_lut
{
	/* plane[bajt zrodla][numer planu] = 8 bitow planu dla 8 pikseli ST */
	uint8_t plane[256][4];

	/* solid[bajt zrodla] = 8 bitow "to nie woda" dla tych samych 8 pikseli ST.
	 * Selektor 3 to woda; wszystko inne (lad, brzeg, cien gory) jest stale. */
	uint8_t solid[256];

	/* bank[bajt zrodla] = 8 bitow "to goly lad" (selektor 1).
	 * Oryginal odbija wrogow wlasnie od selektora 1, nie od linii brzegowej. */
	uint8_t bank[256];

	/* One 16-pixel planar group for each uniform Mode E selector. */
	uint16_t fill[4][4];
} modee_lut_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Buduje LUT dla danego odwzorowania selektora Mode E (0..3) na indeks ST (0..15). */
void modee_build_lut(modee_lut_t* _lut, const uint8_t _selmap[4]);

/* Konwertuje jeden wiersz zrodlowy (48 bajtow) na 80 slow planarnych ST. */
#ifdef RR_MODEE_REFERENCE
void modee_row_to_planar(const modee_lut_t* _lut, const uint8_t* _src, uint16_t* _out);
#endif

/* Trusted compact row directly to 320 planar pixels. Record may be odd;
 * LUT and output must be word-aligned. No intermediate packed bitmap. */
void RR_LinePlanar(const uint8_t* record, const modee_lut_t* lut, uint16_t* out);

/* Maska kolizji wiersza: 20 slow po 16 pikseli + slowo wartownika na koncu,
 * zeby odczyt 16 bitow spod dowolnego x nie wyszedl poza tablice. */
#ifdef RR_MODEE_REFERENCE
void modee_row_to_solid(const modee_lut_t* _lut, const uint8_t* _src, uint16_t* _out);

/* Maska "goly lad" w tym samym ukladzie co modee_row_to_solid. */
void modee_row_to_bank(const modee_lut_t* _lut, const uint8_t* _src, uint16_t* _out);

/* Tylko do weryfikacji/podgladu: rozklada 80 slow planarnych na 320 indeksow. */
void modee_planar_to_indices(const uint16_t* _row, uint8_t* _px);
#endif

/* Odczytuje rejestry koloru Mode E z obrazu pamieci A8 i mapuje je na indeksy ST.
 * Selektor 0 = COLBK ($2C8), 1 = COLPF0 ($2C4), 2 = COLPF1 ($2C5), 3 = COLPF2 ($2C6).
 * _inks moze byc NULL - wtedy selektory mapuja sie na indeksy 0..3. */
void modee_selmap_from_memory(const uint8_t* _mem, const uint8_t* _inks, uint8_t _selmap[4]);

#ifdef __cplusplus
}
#endif

#endif
