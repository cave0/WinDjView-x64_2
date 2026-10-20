//	WinDjView
//	Copyright (C) 2004-2012 Andrew Zhezherun
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along
//	with this program; if not, write to the Free Software Foundation, Inc.,
//	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//	http://www.gnu.org/copyleft/gpl.html
//
//	Uses pnmscalefixed scaling algorithm from netpbm project
//	Copyright (C) 1989, 1991 by Jef Poskanzer
//
//	Uses a modification of pnmscalefixed with subpixel scaling
//	written by Larry Davis.

#include "stdafx.h"
#include "Scaling.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define FULLSCALE  4096
#define SCALESHIFT 12
#define HALFSCALE  2048

// Interleaved accumulator for optimal cache spatial locality
struct PixelAcc
{
	UINT r;
	UINT g;
	UINT b;
};

// Functors for subpixel channel scaling
struct GetRedGPixel   { UINT operator()(const GPixel* p) const { return p->r; } };
struct SetRedGPixel   { void operator()(GPixel* p, BYTE v) const { p->r = v; } };

struct GetGreenGPixel { UINT operator()(const GPixel* p) const { return p->g; } };
struct SetGreenGPixel { void operator()(GPixel* p, BYTE v) const { p->g = v; } };

struct GetBlueGPixel  { UINT operator()(const GPixel* p) const { return p->b; } };
struct SetBlueGPixel  { void operator()(GPixel* p, BYTE v) const { p->b = v; } };

struct GetByte        { UINT operator()(const BYTE* p) const { return *p; } };

void horizontal_scale(const GPixel* inputxelrow, GPixel* newxelrow, UINT cols, UINT newcols, UINT sxscale)
{
	if (cols == 0 || newcols == 0)
		return;

	UINT r = 0, g = 0, b = 0;
	UINT newcol = 0;
	UINT fraccoltofill = FULLSCALE;

	const GPixel* piP = inputxelrow;
	GPixel* piN = newxelrow;

	for (UINT col = 0; col < cols; ++col, ++piP)
	{
		UINT fraccolleft = sxscale;
		const UINT pr = piP->r;
		const UINT pg = piP->g;
		const UINT pb = piP->b;

		while (fraccolleft >= fraccoltofill)
		{
			r += fraccoltofill * pr;
			g += fraccoltofill * pg;
			b += fraccoltofill * pb;
			piN->r = (BYTE) (r >> SCALESHIFT);
			piN->g = (BYTE) (g >> SCALESHIFT);
			piN->b = (BYTE) (b >> SCALESHIFT);
			r = g = b = 0;

			++piN;
			++newcol;
			fraccolleft -= fraccoltofill;
			fraccoltofill = FULLSCALE;
		}

		if (fraccolleft > 0)
		{
			r += fraccolleft * pr;
			g += fraccolleft * pg;
			b += fraccolleft * pb;
			fraccoltofill -= fraccolleft;
		}
	}

	if (cols > 0)
	{
		const GPixel* pLast = piP - 1;
		const UINT pr = pLast->r;
		const UINT pg = pLast->g;
		const UINT pb = pLast->b;

		while (newcol < newcols)
		{
			r += fraccoltofill * pr + HALFSCALE;
			g += fraccoltofill * pg + HALFSCALE;
			b += fraccoltofill * pb + HALFSCALE;

			piN->r = (BYTE) (r >> SCALESHIFT);
			piN->g = (BYTE) (g >> SCALESHIFT);
			piN->b = (BYTE) (b >> SCALESHIFT);
			r = g = b = 0;

			++piN;
			++newcol;
			fraccoltofill = FULLSCALE;
		}
	}
}

void horizontal_scale(const BYTE* inputxelrow, BYTE* newxelrow, UINT cols, UINT newcols, UINT sxscale)
{
	if (cols == 0 || newcols == 0)
		return;

	UINT newcol = 0;
	UINT fraccoltofill = FULLSCALE;
	UINT r = 0;
	const BYTE* piP = inputxelrow;
	BYTE* piN = newxelrow;

	for (UINT col = 0; col < cols; ++col, ++piP)
	{
		UINT fraccolleft = sxscale;
		const UINT val = *piP;

		while (fraccolleft >= fraccoltofill)
		{
			r += fraccoltofill * val;
			*piN = (BYTE) (r >> SCALESHIFT);
			r = 0;

			++piN;
			++newcol;
			fraccolleft -= fraccoltofill;
			fraccoltofill = FULLSCALE;
		}

		if (fraccolleft > 0)
		{
			r += fraccolleft * val;
			fraccoltofill -= fraccolleft;
		}
	}

	if (cols > 0)
	{
		const UINT lastVal = *(piP - 1);
		while (newcol < newcols)
		{
			r += fraccoltofill * lastVal + HALFSCALE;
			*piN = (BYTE) (r >> SCALESHIFT);
			r = 0;

			++piN;
			++newcol;
			fraccoltofill = FULLSCALE;
		}
	}
}

template<typename SourcePtr, typename GetVal, typename SetVal>
inline void scale_subpix_channel(SourcePtr inputxelrow, GPixel* newxelrow, UINT cols, UINT newcols,
	UINT sxscale, UINT initialFrac, GetVal getVal, SetVal setVal, int boostType)
{
	if (cols == 0 || newcols == 0)
		return;

	UINT acc = 0;
	UINT newcol = 0;
	UINT fraccoltofill = initialFrac;

	SourcePtr piP = inputxelrow;
	GPixel* piN = newxelrow;

	for (UINT col = 0; col < cols; ++col, ++piP)
	{
		UINT fraccolleft = sxscale;
		const UINT val = getVal(piP);

		while (fraccolleft >= fraccoltofill)
		{
			acc += fraccoltofill * val;
			setVal(piN, (BYTE) (acc >> SCALESHIFT));
			acc = 0;

			++piN;
			++newcol;
			fraccolleft -= fraccoltofill;
			fraccoltofill = FULLSCALE;
		}

		if (fraccolleft > 0)
		{
			acc += fraccolleft * val;
			fraccoltofill -= fraccolleft;
		}
	}

	if (cols > 0)
	{
		const UINT lastVal = getVal(piP - 1);
		while (newcol < newcols)
		{
			acc += fraccoltofill * lastVal + HALFSCALE;
			setVal(piN, (BYTE) (acc >> SCALESHIFT));
			acc = 0;

			++piN;
			++newcol;
			fraccoltofill = FULLSCALE;
		}
	}

	if (newcols > 0)
	{
		if (boostType == 1) // Red boost: normalize 1/3 starting weight
		{
			UINT v = (UINT) newxelrow->r * 3;
			newxelrow->r = (BYTE) (v > 255 ? 255 : v);
		}
		else if (boostType == 2) // Green boost: normalize 2/3 starting weight
		{
			UINT v = (UINT) newxelrow->g + (newxelrow->g / 2);
			newxelrow->g = (BYTE) (v > 255 ? 255 : v);
		}
	}
}

void horizontal_scale_subpix(const GPixel* inputxelrow, GPixel* newxelrow, UINT cols, UINT newcols, UINT sxscale)
{
	// Red (starts with 1/3 column weight)
	scale_subpix_channel(inputxelrow, newxelrow, cols, newcols, sxscale,
		FULLSCALE / 3, GetRedGPixel(), SetRedGPixel(), 1);

	// Green (starts with 2/3 column weight)
	scale_subpix_channel(inputxelrow, newxelrow, cols, newcols, sxscale,
		(2 * FULLSCALE) / 3, GetGreenGPixel(), SetGreenGPixel(), 2);

	// Blue (starts with full column weight)
	scale_subpix_channel(inputxelrow, newxelrow, cols, newcols, sxscale,
		FULLSCALE, GetBlueGPixel(), SetBlueGPixel(), 0);
}

void horizontal_scale_subpix(const BYTE* inputxelrow, GPixel* newxelrow, UINT cols, UINT newcols, UINT sxscale)
{
	// Red
	scale_subpix_channel(inputxelrow, newxelrow, cols, newcols, sxscale,
		FULLSCALE / 3, GetByte(), SetRedGPixel(), 1);

	// Green
	scale_subpix_channel(inputxelrow, newxelrow, cols, newcols, sxscale,
		(2 * FULLSCALE) / 3, GetByte(), SetGreenGPixel(), 2);

	// Blue
	scale_subpix_channel(inputxelrow, newxelrow, cols, newcols, sxscale,
		FULLSCALE, GetByte(), SetBlueGPixel(), 0);
}

GP<GPixmap> RescalePnm(GP<GPixmap> pSrc, UINT nWidth, UINT nHeight)
{
	if (pSrc == NULL || nWidth == 0 || nHeight == 0)
		return GP<GPixmap>();

	UINT cols = pSrc->columns();
	UINT rows = pSrc->rows();
	if (cols == 0 || rows == 0)
		return GP<GPixmap>();

	UINT newcols = nWidth;
	UINT newrows = nHeight;

	// Identity fast path: bypass all fixed-point math and buffers
	if (newcols == cols && newrows == rows)
	{
		GP<GPixmap> pGPixmap = GPixmap::create(rows, cols);
		for (UINT r = 0; r < rows; ++r)
			memcpy((*pGPixmap)[r], (*pSrc)[r], cols * sizeof(GPixel));
		return pGPixmap;
	}

	GP<GPixmap> pGPixmap = GPixmap::create(newrows, newcols);
	if (pGPixmap == NULL)
		return GP<GPixmap>();

	UINT sxscale = FULLSCALE * newcols / cols;
	UINT syscale = FULLSCALE * newrows / rows;

	GPixel* tempxelrow = (GPixel*) malloc(cols * sizeof(GPixel));
	PixelAcc* acc = (PixelAcc*) malloc(cols * sizeof(PixelAcc));
	if (tempxelrow == NULL || acc == NULL)
	{
		if (tempxelrow != NULL) free(tempxelrow);
		if (acc != NULL) free(acc);
		return GP<GPixmap>();
	}

	UINT rowsread = 0;
	UINT fracrowleft = syscale;
	UINT needtoreadrow = 1;
	UINT fracrowtofill = FULLSCALE;

	for (UINT col = 0; col < cols; ++col)
	{
		acc[col].r = HALFSCALE;
		acc[col].g = HALFSCALE;
		acc[col].b = HALFSCALE;
	}

	GPixel* xelrow = 0;
	const GPixel* xP;
	GPixel* nxP;

	for (UINT row = 0; row < newrows; ++row)
	{
		if (newrows == rows)
		{
			xelrow = (*pSrc)[rowsread];
			++rowsread;
		}
		else
		{
			while (fracrowleft < fracrowtofill)
			{
				if (needtoreadrow)
				{
					if (rowsread < rows)
					{
						xelrow = (*pSrc)[rowsread];
						++rowsread;
					}
				}

				PixelAcc* pAcc = acc;
				for (UINT col = 0, xP = xelrow; col < cols; ++col, ++xP, ++pAcc)
				{
					pAcc->r += fracrowleft * xP->r;
					pAcc->g += fracrowleft * xP->g;
					pAcc->b += fracrowleft * xP->b;
				}

				fracrowtofill -= fracrowleft;
				fracrowleft = syscale;
				needtoreadrow = 1;
			}

			if (needtoreadrow)
			{
				if (rowsread < rows)
				{
					xelrow = (*pSrc)[rowsread];
					++rowsread;
					needtoreadrow = 0;
				}
			}

			PixelAcc* pAcc = acc;
			for (UINT col = 0, xP = xelrow, nxP = tempxelrow; col < cols; ++col, ++xP, ++nxP, ++pAcc)
			{
				UINT r = pAcc->r + fracrowtofill * xP->r;
				UINT g = pAcc->g + fracrowtofill * xP->g;
				UINT b = pAcc->b + fracrowtofill * xP->b;
				nxP->r = (BYTE) (r >> SCALESHIFT);
				nxP->g = (BYTE) (g >> SCALESHIFT);
				nxP->b = (BYTE) (b >> SCALESHIFT);
				pAcc->r = HALFSCALE;
				pAcc->g = HALFSCALE;
				pAcc->b = HALFSCALE;
			}

			fracrowleft -= fracrowtofill;
			if (fracrowleft == 0)
			{
				fracrowleft = syscale;
				needtoreadrow = 1;
			}
			fracrowtofill = FULLSCALE;
		}

		// Ensure horizontal pass consumes the correct source row
		const GPixel* rowToScaleX = (newrows == rows) ? xelrow : tempxelrow;
		if (newcols == cols)
		{
			memcpy((*pGPixmap)[row], rowToScaleX, newcols * sizeof(GPixel));
		}
		else
		{
			horizontal_scale(rowToScaleX, (*pGPixmap)[row], cols, newcols, sxscale);
		}
	}

	free(tempxelrow);
	free(acc);

	return pGPixmap;
}

GP<GPixmap> RescalePnm_subpix(GP<GPixmap> pSrc, UINT nWidth, UINT nHeight)
{
	if (pSrc == NULL || nWidth == 0 || nHeight == 0)
		return GP<GPixmap>();

	UINT cols = pSrc->columns();
	UINT rows = pSrc->rows();
	if (cols == 0 || rows == 0)
		return GP<GPixmap>();

	UINT newcols = nWidth;
	UINT newrows = nHeight;

	GP<GPixmap> pGPixmap = GPixmap::create(newrows, newcols);
	if (pGPixmap == NULL)
		return GP<GPixmap>();

	UINT sxscale = (FULLSCALE * newcols - 2 * FULLSCALE / 3 - 1) / cols;
	UINT syscale = FULLSCALE * newrows / rows;

	GPixel* tempxelrow = (GPixel*) malloc(cols * sizeof(GPixel));
	PixelAcc* acc = (PixelAcc*) malloc(cols * sizeof(PixelAcc));
	if (tempxelrow == NULL || acc == NULL)
	{
		if (tempxelrow != NULL) free(tempxelrow);
		if (acc != NULL) free(acc);
		return GP<GPixmap>();
	}

	UINT rowsread = 0;
	UINT fracrowleft = syscale;
	UINT needtoreadrow = 1;
	UINT fracrowtofill = FULLSCALE;

	for (UINT col = 0; col < cols; ++col)
	{
		acc[col].r = HALFSCALE;
		acc[col].g = HALFSCALE;
		acc[col].b = HALFSCALE;
	}

	GPixel* xelrow = 0;
	const GPixel* xP;
	GPixel* nxP;

	for (UINT row = 0; row < newrows; ++row)
	{
		if (newrows == rows)
		{
			xelrow = (*pSrc)[rowsread];
			++rowsread;
		}
		else
		{
			while (fracrowleft < fracrowtofill)
			{
				if (needtoreadrow)
				{
					if (rowsread < rows)
					{
						xelrow = (*pSrc)[rowsread];
						++rowsread;
					}
				}

				PixelAcc* pAcc = acc;
				for (UINT col = 0, xP = xelrow; col < cols; ++col, ++xP, ++pAcc)
				{
					pAcc->r += fracrowleft * xP->r;
					pAcc->g += fracrowleft * xP->g;
					pAcc->b += fracrowleft * xP->b;
				}

				fracrowtofill -= fracrowleft;
				fracrowleft = syscale;
				needtoreadrow = 1;
			}

			if (needtoreadrow)
			{
				if (rowsread < rows)
				{
					xelrow = (*pSrc)[rowsread];
					++rowsread;
					needtoreadrow = 0;
				}
			}

			PixelAcc* pAcc = acc;
			for (UINT col = 0, xP = xelrow, nxP = tempxelrow; col < cols; ++col, ++xP, ++nxP, ++pAcc)
			{
				UINT r = pAcc->r + fracrowtofill * xP->r;
				UINT g = pAcc->g + fracrowtofill * xP->g;
				UINT b = pAcc->b + fracrowtofill * xP->b;
				nxP->r = (BYTE) (r >> SCALESHIFT);
				nxP->g = (BYTE) (g >> SCALESHIFT);
				nxP->b = (BYTE) (b >> SCALESHIFT);
				pAcc->r = HALFSCALE;
				pAcc->g = HALFSCALE;
				pAcc->b = HALFSCALE;
			}

			fracrowleft -= fracrowtofill;
			if (fracrowleft == 0)
			{
				fracrowleft = syscale;
				needtoreadrow = 1;
			}
			fracrowtofill = FULLSCALE;
		}

		const GPixel* rowToScaleX = (newrows == rows) ? xelrow : tempxelrow;
		horizontal_scale_subpix(rowToScaleX, (*pGPixmap)[row], cols, newcols, sxscale);
	}

	free(tempxelrow);
	free(acc);

	return pGPixmap;
}

GP<GBitmap> RescalePnm(GP<GBitmap> pSrc, UINT nWidth, UINT nHeight)
{
	if (pSrc == NULL || nWidth == 0 || nHeight == 0)
		return GP<GBitmap>();

	UINT cols = pSrc->columns();
	UINT rows = pSrc->rows();
	if (cols == 0 || rows == 0)
		return GP<GBitmap>();

	const int align = 4;
	int border = ((nWidth + align - 1) & ~(align - 1)) - nWidth;
	GP<GBitmap> pGBitmap = GBitmap::create(nHeight, nWidth, border);
	if (pGBitmap == NULL)
		return GP<GBitmap>();

	pGBitmap->change_grays(256);

	UINT newcols = nWidth;
	UINT newrows = nHeight;

	// Clamp palette entries to [2, 256] to prevent division by zero or buffer overflow
	UINT nPaletteEntries = pSrc->get_grays();
	if (nPaletteEntries < 2)
		nPaletteEntries = 2;
	else if (nPaletteEntries > 256)
		nPaletteEntries = 256;

	BYTE grays[256];
	int color = 0xff0000;
	int decrement = color / (nPaletteEntries - 1);
	for (int i = (int) nPaletteEntries - 1; i >= 0; --i)
	{
		grays[i] = (BYTE) (color >> 16);
		color -= decrement;
	}

	UINT sxscale = FULLSCALE * newcols / cols;
	UINT syscale = FULLSCALE * newrows / rows;

	BYTE* tempxelrow = (BYTE*) malloc(cols * sizeof(BYTE));
	UINT* rs = (UINT*) malloc(cols * sizeof(UINT));
	if (tempxelrow == NULL || rs == NULL)
	{
		if (tempxelrow != NULL) free(tempxelrow);
		if (rs != NULL) free(rs);
		return GP<GBitmap>();
	}

	UINT rowsread = 0;
	UINT fracrowleft = syscale;
	UINT needtoreadrow = 1;
	UINT fracrowtofill = FULLSCALE;

	for (UINT col = 0; col < cols; ++col)
		rs[col] = HALFSCALE;

	BYTE* xelrow = 0;
	const BYTE* xP;
	BYTE* nxP;

	for (UINT row = 0; row < newrows; ++row)
	{
		if (newrows == rows)
		{
			xelrow = (*pSrc)[rowsread];
			++rowsread;
		}
		else
		{
			while (fracrowleft < fracrowtofill)
			{
				if (needtoreadrow)
				{
					if (rowsread < rows)
					{
						xelrow = (*pSrc)[rowsread];
						++rowsread;
					}
				}

				UINT* prs = rs;
				for (UINT col = 0, xP = xelrow; col < cols; ++col, ++xP, ++prs)
				{
					*prs += fracrowleft * grays[*xP];
				}

				fracrowtofill -= fracrowleft;
				fracrowleft = syscale;
				needtoreadrow = 1;
			}

			if (needtoreadrow)
			{
				if (rowsread < rows)
				{
					xelrow = (*pSrc)[rowsread];
					++rowsread;
					needtoreadrow = 0;
				}
			}

			UINT* prs = rs;
			for (UINT col = 0, xP = xelrow, nxP = tempxelrow; col < cols; ++col, ++xP, ++nxP, ++prs)
			{
				UINT r = *prs + fracrowtofill * grays[*xP];
				*nxP = (BYTE) (r >> SCALESHIFT);
				*prs = HALFSCALE;
			}

			fracrowleft -= fracrowtofill;
			if (fracrowleft == 0)
			{
				fracrowleft = syscale;
				needtoreadrow = 1;
			}
			fracrowtofill = FULLSCALE;
		}

		// Ensure horizontal pass consumes the correct source row
		const BYTE* rowToScaleX = (newrows == rows) ? xelrow : tempxelrow;
		if (newcols == cols)
		{
			memcpy((*pGBitmap)[row], rowToScaleX, newcols * sizeof(BYTE));
		}
		else
		{
			horizontal_scale(rowToScaleX, (*pGBitmap)[row], cols, newcols, sxscale);
		}
	}

	free(tempxelrow);
	free(rs);

	return pGBitmap;
}

GP<GPixmap> RescalePnm_subpix(GP<GBitmap> pSrc, UINT nWidth, UINT nHeight)
{
	if (pSrc == NULL || nWidth == 0 || nHeight == 0)
		return GP<GPixmap>();

	UINT cols = pSrc->columns();
	UINT rows = pSrc->rows();
	if (cols == 0 || rows == 0)
		return GP<GPixmap>();

	GP<GPixmap> pGPixmap = GPixmap::create(nHeight, nWidth);
	if (pGPixmap == NULL)
		return GP<GPixmap>();

	UINT newcols = nWidth;
	UINT newrows = nHeight;

	// Clamp palette entries to [2, 256] to prevent division by zero or buffer overflow
	UINT nPaletteEntries = pSrc->get_grays();
	if (nPaletteEntries < 2)
		nPaletteEntries = 2;
	else if (nPaletteEntries > 256)
		nPaletteEntries = 256;

	BYTE grays[256];
	int color = 0xff0000;
	int decrement = color / (nPaletteEntries - 1);
	for (int i = (int) nPaletteEntries - 1; i >= 0; --i)
	{
		grays[nPaletteEntries - i - 1] = (BYTE) (color >> 16);
		color -= decrement;
	}

	UINT sxscale = (FULLSCALE * newcols - 2 * FULLSCALE / 3 - 1) / cols;
	UINT syscale = FULLSCALE * newrows / rows;

	BYTE* tempxelrow = (BYTE*) malloc(cols * sizeof(BYTE));
	UINT* rs = (UINT*) malloc(cols * sizeof(UINT));
	if (tempxelrow == NULL || rs == NULL)
	{
		if (tempxelrow != NULL) free(tempxelrow);
		if (rs != NULL) free(rs);
		return GP<GPixmap>();
	}

	UINT rowsread = 0;
	UINT fracrowleft = syscale;
	UINT needtoreadrow = 1;
	UINT fracrowtofill = FULLSCALE;

	for (UINT col = 0; col < cols; ++col)
		rs[col] = HALFSCALE;

	BYTE* xelrow = 0;
	const BYTE* xP;
	BYTE* nxP;

	for (UINT row = 0; row < newrows; ++row)
	{
		if (newrows == rows)
		{
			xelrow = (*pSrc)[rowsread];
			++rowsread;
		}
		else
		{
			while (fracrowleft < fracrowtofill)
			{
				if (needtoreadrow)
				{
					if (rowsread < rows)
					{
						xelrow = (*pSrc)[rowsread];
						++rowsread;
					}
				}

				UINT* prs = rs;
				for (UINT col = 0, xP = xelrow; col < cols; ++col, ++xP, ++prs)
				{
					*prs += fracrowleft * grays[*xP];
				}

				fracrowtofill -= fracrowleft;
				fracrowleft = syscale;
				needtoreadrow = 1;
			}

			if (needtoreadrow)
			{
				if (rowsread < rows)
				{
					xelrow = (*pSrc)[rowsread];
					++rowsread;
					needtoreadrow = 0;
				}
			}

			UINT* prs = rs;
			for (UINT col = 0, xP = xelrow, nxP = tempxelrow; col < cols; ++col, ++xP, ++nxP, ++prs)
			{
				UINT r = *prs + fracrowtofill * grays[*xP];
				*nxP = (BYTE) (r >> SCALESHIFT);
				*prs = HALFSCALE;
			}

			fracrowleft -= fracrowtofill;
			if (fracrowleft == 0)
			{
				fracrowleft = syscale;
				needtoreadrow = 1;
			}
			fracrowtofill = FULLSCALE;
		}

		const BYTE* rowToScaleX = (newrows == rows) ? xelrow : tempxelrow;
		horizontal_scale_subpix(rowToScaleX, (*pGPixmap)[row], cols, newcols, sxscale);
	}

	free(tempxelrow);
	free(rs);

	return pGPixmap;
}
