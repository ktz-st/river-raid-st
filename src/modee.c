#include "modee.h"
#include <stddef.h>

_Static_assert(offsetof(modee_lut_t, fill) == 1536, "RR_LinePlanar fill offset");

void modee_build_lut(modee_lut_t* _lut, const uint8_t _selmap[4])
{
	for (int b = 0; b < 256; ++b)
	{
		uint8_t v[4] = { 0, 0, 0, 0 };

		for (int s = 0; s < 4; ++s)
		{
			const uint8_t sel = (uint8_t)((b >> (6 - 2*s)) & 3);
			const uint8_t idx = _selmap[sel];

			/* piksel zrodlowy s zajmuje bity 7-2s i 6-2s wyniku */
			const uint8_t mask = (uint8_t)((1u << (7 - 2*s)) | (1u << (6 - 2*s)));

			for (int pl = 0; pl < 4; ++pl)
			{
				if ((idx >> pl) & 1)
					v[pl] |= mask;
			}
		}

		for (int pl = 0; pl < 4; ++pl)
			_lut->plane[b][pl] = v[pl];

		uint8_t solid = 0;

		for (int s = 0; s < 4; ++s)
		{
			const uint8_t sel = (uint8_t)((b >> (6 - 2*s)) & 3);

			if (sel != 3)		/* 3 = woda */
				solid |= (uint8_t)((1u << (7 - 2*s)) | (1u << (6 - 2*s)));
		}

		_lut->solid[b] = solid;

		uint8_t bank = 0;

		for (int s = 0; s < 4; ++s)
		{
			if (((b >> (6 - 2*s)) & 3) == 1)		/* 1 = goly lad */
				bank |= (uint8_t)((1u << (7 - 2*s)) | (1u << (6 - 2*s)));
		}

		_lut->bank[b] = bank;
	}

	for (int s = 0; s < 4; ++s)
		for (int pl = 0; pl < 4; ++pl)
			_lut->fill[s][pl] = (_selmap[s] & (1u << pl)) ? 0xffff : 0;
}

void modee_row_to_solid(const modee_lut_t* _lut, const uint8_t* _src, uint16_t* _out)
{
	const uint8_t* s = _src + MODEE_VIS_FIRST;

	for (int g = 0; g < MODEE_ST_GROUPS; ++g)
	{
		_out[g] = (uint16_t)((uint16_t)_lut->solid[s[0]] << 8 | _lut->solid[s[1]]);
		s += 2;
	}

	_out[MODEE_ST_GROUPS] = 0;		/* wartownik dla odczytu spod x niewyrownanego */
}

void modee_row_to_bank(const modee_lut_t* _lut, const uint8_t* _src, uint16_t* _out)
{
	const uint8_t* s = _src + MODEE_VIS_FIRST;

	for (int g = 0; g < MODEE_ST_GROUPS; ++g)
	{
		_out[g] = (uint16_t)((uint16_t)_lut->bank[s[0]] << 8 | _lut->bank[s[1]]);
		s += 2;
	}

	_out[MODEE_ST_GROUPS] = 0;
}

void modee_row_to_planar(const modee_lut_t* _lut, const uint8_t* _src, uint16_t* _out)
{
	const uint8_t* s = _src + MODEE_VIS_FIRST;

	for (int g = 0; g < MODEE_ST_GROUPS; ++g)
	{
		const uint8_t* la = _lut->plane[s[0]];
		const uint8_t* lb = _lut->plane[s[1]];
		s += 2;

		_out[0] = (uint16_t)((uint16_t)la[0] << 8 | lb[0]);
		_out[1] = (uint16_t)((uint16_t)la[1] << 8 | lb[1]);
		_out[2] = (uint16_t)((uint16_t)la[2] << 8 | lb[2]);
		_out[3] = (uint16_t)((uint16_t)la[3] << 8 | lb[3]);
		_out += 4;
	}
}

void modee_planar_to_indices(const uint16_t* _row, uint8_t* _px)
{
	for (int g = 0; g < MODEE_ST_GROUPS; ++g)
	{
		const uint16_t w0 = _row[g*4 + 0];
		const uint16_t w1 = _row[g*4 + 1];
		const uint16_t w2 = _row[g*4 + 2];
		const uint16_t w3 = _row[g*4 + 3];

		for (int bit = 0; bit < 16; ++bit)
		{
			const int sh = 15 - bit;
			uint8_t idx = 0;

			if ((w0 >> sh) & 1) idx |= 1;
			if ((w1 >> sh) & 1) idx |= 2;
			if ((w2 >> sh) & 1) idx |= 4;
			if ((w3 >> sh) & 1) idx |= 8;

			_px[g*16 + bit] = idx;
		}
	}
}

void modee_selmap_from_memory(const uint8_t* _mem, const uint8_t* _inks, uint8_t _selmap[4])
{
	/* cienie rejestrow koloru OS: COLOR0..COLOR4 = $2C4..$2C8 */
	const uint8_t a8[4] = {
		_mem[0x2C8],	/* selektor 0 = tlo      */
		_mem[0x2C4],	/* selektor 1 = COLPF0   */
		_mem[0x2C5],	/* selektor 2 = COLPF1   */
		_mem[0x2C6]		/* selektor 3 = COLPF2   */
	};

	for (int s = 0; s < 4; ++s)
		_selmap[s] = _inks ? _inks[a8[s]] : (uint8_t)s;
}
