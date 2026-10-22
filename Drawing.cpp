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

#include "stdafx.h"
#include "WinDjView.h"
#include "Drawing.h"

#include "PrintDlg.h"
#include "ProgressDlg.h"
#include "DjVuDoc.h"
#include "DjVuView.h"
#include "MyGdiPlus.h"
#include <emmintrin.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


void BuildInvertTable(BYTE* table)
{
	for (int i = 0; i < 256; ++i)
		table[i] = 255 - table[i];
}

void BuildBrightnessTable(int nBrightness, BYTE* table)
{
	for (int i = 0; i < 256; ++i)
	{
		double color = table[i] * (100.0 + nBrightness) / 100.0;
		table[i] = max(0, min(255, static_cast<int>(color + 0.5)));
	}
}

void BuildContrastTable(int nContrast, BYTE* table)
{
	for (int i = 0; i < 256; ++i)
	{
		double color = 128.0 + (table[i] - 128.0) * (100 + nContrast) / 100.0;
		table[i] = max(0, min(255, static_cast<int>(color + 0.5)));
	}
}

void BuildGammaTable(double fGamma, BYTE* table)
{
	double exponent = 1 / fGamma;
	for (int i = 0; i < 256; ++i)
	{
		double color = pow(table[i] / 255.0, exponent) * 255.0;
		table[i] = max(0, min(255, static_cast<int>(color + 0.5)));
	}
}

void BuildTransparencyTable(double fTransparency, BYTE c, BYTE* table)
{
	int nAlpha = max(0, min(255, static_cast<int>(fTransparency * 255.0 + 0.5)));
	for (int i = 0; i < 256; ++i)
	{
		table[i] = static_cast<BYTE>((table[i] * nAlpha + c * (255 - nAlpha) + 128) / 255);
	}
}

CDIB* RenderPixmap(GPixmap& pm, const CDisplaySettings& displaySettings)
{
	return RenderPixmap(pm, CRect(0, 0, pm.columns(), pm.rows()), displaySettings);
}

CDIB* RenderPixmap(GPixmap& pm, const CRect& rcClip, const CDisplaySettings& displaySettings)
{
	ASSERT(0 <= rcClip.left && 0 <= rcClip.top);
	ASSERT((int)pm.columns() >= rcClip.right && (int)pm.rows() >= rcClip.bottom);

	BITMAPINFOHEADER bmih;
	ZeroMemory(&bmih, sizeof(bmih));

	bmih.biSize = sizeof(bmih);
	bmih.biWidth = rcClip.Width();
	bmih.biHeight = rcClip.Height();
	bmih.biBitCount = 24;
	bmih.biCompression = BI_RGB;
	bmih.biClrUsed = 0;

	CDIB* pBitmap = CDIB::CreateDIB((BITMAPINFO*)&bmih);
	if (pBitmap->m_hObject == NULL)
		return pBitmap;

	int nRowLength = CalculateDIBPitch(rcClip.Width(), 24);
	int nRowBytes = rcClip.Width() * 3;
	LPBYTE pBits = pBitmap->GetBits();

	int nBrightness = displaySettings.GetBrightness();
	int nContrast = displaySettings.GetContrast();
	double fGamma = displaySettings.GetGamma();
	if (fGamma != 1.0 || nBrightness != 0 || nContrast != 0 || displaySettings.bInvertColors)
	{
		// 128-bit SIMD vector inversion (Night Mode) when no other color adjustments are active
		if (displaySettings.bInvertColors && fGamma == 1.0 && nBrightness == 0 && nContrast == 0)
		{
			const __m128i vInvert = _mm_set1_epi32(-1);
			for (int y = rcClip.top; y < rcClip.bottom; ++y, pBits += nRowLength)
			{
				const BYTE* pSrc = reinterpret_cast<const BYTE*>(pm[y] + rcClip.left);
				int x = 0;
				while (x + 16 <= nRowBytes)
				{
					__m128i val = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pSrc + x));
					val = _mm_xor_si128(val, vInvert);
					_mm_storeu_si128(reinterpret_cast<__m128i*>(pBits + x), val);
					x += 16;
				}
				while (x < nRowBytes)
				{
					pBits[x] = static_cast<BYTE>(~pSrc[x]);
					++x;
				}
			}
		}
		else
		{
			BYTE table[256];
			for (int i = 0; i < 256; ++i)
				table[i] = static_cast<BYTE>(i);

			if (displaySettings.bInvertColors)
				BuildInvertTable(table);
			if (nBrightness != 0)
				BuildBrightnessTable(nBrightness, table);
			if (nContrast != 0)
				BuildContrastTable(nContrast, table);
			if (fGamma != 1.0)
				BuildGammaTable(fGamma, table);

			// 8-way unrolled table mapping loop
			for (int y = rcClip.top; y < rcClip.bottom; ++y, pBits += nRowLength)
			{
				const BYTE* pSrc = reinterpret_cast<const BYTE*>(pm[y] + rcClip.left);
				int x = 0;
				while (x + 8 <= nRowBytes)
				{
					pBits[x + 0] = table[pSrc[x + 0]];
					pBits[x + 1] = table[pSrc[x + 1]];
					pBits[x + 2] = table[pSrc[x + 2]];
					pBits[x + 3] = table[pSrc[x + 3]];
					pBits[x + 4] = table[pSrc[x + 4]];
					pBits[x + 5] = table[pSrc[x + 5]];
					pBits[x + 6] = table[pSrc[x + 6]];
					pBits[x + 7] = table[pSrc[x + 7]];
					x += 8;
				}
				while (x < nRowBytes)
				{
					pBits[x] = table[pSrc[x]];
					++x;
				}
			}
		}
	}
	else
	{
		for (int y = rcClip.top; y < rcClip.bottom; ++y, pBits += nRowLength)
			memcpy(pBits, pm[y] + rcClip.left, nRowBytes);
	}

	return pBitmap;
}

CDIB* RenderBitmap(GBitmap& bm, const CDisplaySettings& displaySettings)
{
	return RenderBitmap(bm, CRect(0, 0, bm.columns(), bm.rows()), displaySettings);
}

CDIB* RenderBitmap(GBitmap& bm, const CRect& rcClip, const CDisplaySettings& displaySettings)
{
	ASSERT(0 <= rcClip.left && 0 <= rcClip.top);
	ASSERT((int)bm.columns() >= rcClip.right && (int)bm.rows() >= rcClip.bottom);

	LPBITMAPINFO pBMI;

	int nPaletteEntries = bm.get_grays();
	int nSize = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD)*nPaletteEntries;

	pBMI = (LPBITMAPINFO) malloc(nSize);
	ZeroMemory(pBMI, nSize);

	BITMAPINFOHEADER& bmih = pBMI->bmiHeader;
	bmih.biSize = sizeof(bmih);
	bmih.biWidth = rcClip.Width();
	bmih.biHeight = rcClip.Height();
	bmih.biBitCount = 8;
	bmih.biCompression = BI_RGB;
	bmih.biClrUsed = nPaletteEntries;

	// Create palette for the bitmap
	int color = 0xff0000;
	int decrement = color/(nPaletteEntries - 1);
	for (int i = 0; i < nPaletteEntries; ++i)
	{
		int level = color >> 16;
		pBMI->bmiColors[i].rgbBlue = level;
		pBMI->bmiColors[i].rgbGreen = level;
		pBMI->bmiColors[i].rgbRed = level;
		color -= decrement;
	}

	int nBrightness = displaySettings.GetBrightness();
	int nContrast = displaySettings.GetContrast();
	double fGamma = displaySettings.GetGamma();
	if (fGamma != 1.0 || nBrightness != 0 || nContrast != 0 || displaySettings.bInvertColors)
	{
		BYTE table[256];
		for (int i = 0; i < 256; ++i)
			table[i] = static_cast<BYTE>(i);

		if (displaySettings.bInvertColors)
			BuildInvertTable(table);
		if (nBrightness != 0)
			BuildBrightnessTable(nBrightness, table);
		if (nContrast != 0)
			BuildContrastTable(nContrast, table);
		if (fGamma != 1.0)
			BuildGammaTable(fGamma, table);

		for (int j = 0; j < nPaletteEntries; ++j)
		{
			pBMI->bmiColors[j].rgbBlue = table[pBMI->bmiColors[j].rgbBlue];
			pBMI->bmiColors[j].rgbGreen = table[pBMI->bmiColors[j].rgbGreen];
			pBMI->bmiColors[j].rgbRed = table[pBMI->bmiColors[j].rgbRed];
		}
	}

	CDIB* pBitmap = CDIB::CreateDIB(pBMI);
	free(pBMI);

	if (pBitmap->m_hObject == NULL)
		return pBitmap;

	int nRowLength = CalculateDIBPitch(rcClip.Width(), 8);

	LPBYTE pBits = pBitmap->GetBits();
	for (int y = rcClip.top; y < rcClip.bottom; ++y, pBits += nRowLength)
		memcpy(pBits, bm[y] + rcClip.left, rcClip.Width());

	return pBitmap;
}

CDIB* RenderEmpty(const CSize& szBitmap, const CDisplaySettings& displaySettings)
{
	int nSize = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD);
	LPBITMAPINFO pBMI = (LPBITMAPINFO) malloc(nSize);
	ZeroMemory(pBMI, nSize);

	BITMAPINFOHEADER& bmih = pBMI->bmiHeader;
	bmih.biSize = sizeof(BITMAPINFOHEADER);
	bmih.biWidth = szBitmap.cx;
	bmih.biHeight = szBitmap.cy;
	bmih.biBitCount = 1;
	bmih.biCompression = BI_RGB;
	bmih.biClrUsed = 1;

	// Create palette for the bitmap
	pBMI->bmiColors[0].rgbBlue = 0xff;
	pBMI->bmiColors[0].rgbGreen = 0xff;
	pBMI->bmiColors[0].rgbRed = 0xff;

	int nBrightness = displaySettings.GetBrightness();
	int nContrast = displaySettings.GetContrast();
	double fGamma = displaySettings.GetGamma();
	if (fGamma != 1.0 || nBrightness != 0 || nContrast != 0 || displaySettings.bInvertColors)
	{
		BYTE table[256];
		for (int i = 0; i < 256; ++i)
			table[i] = static_cast<BYTE>(i);

		if (displaySettings.bInvertColors)
			BuildInvertTable(table);
		if (nBrightness != 0)
			BuildBrightnessTable(nBrightness, table);
		if (nContrast != 0)
			BuildContrastTable(nContrast, table);
		if (fGamma != 1.0)
			BuildGammaTable(fGamma, table);

		pBMI->bmiColors[0].rgbBlue = table[pBMI->bmiColors[0].rgbBlue];
		pBMI->bmiColors[0].rgbGreen = table[pBMI->bmiColors[0].rgbGreen];
		pBMI->bmiColors[0].rgbRed = table[pBMI->bmiColors[0].rgbRed];
	}

	CDIB* pBitmap = CDIB::CreateDIB(pBMI);
	free(pBMI);

	if (pBitmap->m_hObject == NULL)
		return pBitmap;

	int nRowLength = CalculateDIBPitch(szBitmap.cx, 1);

	LPBYTE pBits = pBitmap->GetBits();
	ZeroMemory(pBits, nRowLength*szBitmap.cy);

	return pBitmap;
}


// CDIB

CDIB::~CDIB()
{
	DeleteObject();
	free(m_pBMI);

	if (m_hSection != NULL)
		CloseHandle(m_hSection);
	if (m_hFile != NULL)
		CloseHandle(m_hFile);
}

void CDIB::Create(const BITMAPINFO* pBMI)
{
	ASSERT(m_hObject == NULL);

	UINT nSize = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD)*pBMI->bmiHeader.biClrUsed;
	m_pBMI = (LPBITMAPINFO)malloc(nSize);
	memcpy(m_pBMI, pBMI, nSize);

	BITMAPINFOHEADER& bmih = m_pBMI->bmiHeader;
	bmih.biPlanes = 1;
	bmih.biSizeImage = 0;

	HBITMAP hBitmap = ::CreateDIBSection(NULL,
		m_pBMI, DIB_RGB_COLORS, (VOID**)&m_pBits, NULL, 0);

	if (hBitmap == NULL)
	{
		CString strTempPath;
		if (::GetTempPath(_MAX_PATH, strTempPath.GetBuffer(_MAX_PATH)) != 0)
			strTempPath.ReleaseBuffer();
		else
			strTempPath = _T(".");

		CString strTempFile;
		if (::GetTempFileName(strTempPath, _T("DJV"), 0, strTempFile.GetBuffer(_MAX_PATH)) != 0)
		{
			DWORD nLineLength = CalculateDIBPitch(pBMI->bmiHeader.biWidth, pBMI->bmiHeader.biBitCount);
			DWORD nBitsSize = nLineLength*pBMI->bmiHeader.biHeight;

			// Try to create a mapped file section
			m_hFile = ::CreateFile(strTempFile, GENERIC_READ | GENERIC_WRITE,
				0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
			if (m_hFile != NULL && m_hFile != INVALID_HANDLE_VALUE)
			{
				m_hSection = ::CreateFileMapping(m_hFile, NULL,
					PAGE_READWRITE, 0, nBitsSize, NULL);
				if (m_hSection != NULL)
				{
					hBitmap = ::CreateDIBSection(NULL, m_pBMI, DIB_RGB_COLORS,
							(VOID**)&m_pBits, m_hSection, 0);
				}
			}

			if (hBitmap == NULL)
			{
				if (m_hSection != NULL)
					::CloseHandle(m_hSection);
				if (m_hFile != NULL && m_hFile != INVALID_HANDLE_VALUE)
					::CloseHandle(m_hFile);
				m_hSection = NULL;
				m_hFile = NULL;
			}
		}
	}

	Attach(hBitmap);
}

CDIB* CDIB::CreateDIB(const BITMAPINFO* pBMI)
{
	CDIB* pDIB = new CDIB();
	pDIB->Create(pBMI);
	return pDIB;
}

void CDIB::Create(CDIB* pSource, int nBitCount)
{
	ASSERT(pSource->m_pBits != NULL);
	ASSERT(nBitCount == -1 || nBitCount == 1 || nBitCount == 4 || nBitCount == 8 ||
		   nBitCount == 16 || nBitCount == 24 || nBitCount == 32);
	if (nBitCount == -1)
		nBitCount = pSource->m_pBMI->bmiHeader.biBitCount;

	int nPaletteEntries = pSource->m_pBMI->bmiHeader.biClrUsed;
	if (nBitCount <= 8)
		nPaletteEntries = min(1 << nBitCount, nPaletteEntries);
	else
		nPaletteEntries = 0;

	int nSize = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD)*nPaletteEntries;
	LPBITMAPINFO pBMI = (LPBITMAPINFO) malloc(nSize);
	ZeroMemory(pBMI, nSize);

	BITMAPINFOHEADER& bmih = pBMI->bmiHeader;
	bmih.biSize = sizeof(BITMAPINFOHEADER);
	bmih.biWidth = pSource->m_pBMI->bmiHeader.biWidth;
	bmih.biHeight = pSource->m_pBMI->bmiHeader.biHeight;
	bmih.biBitCount = nBitCount;
	bmih.biCompression = BI_RGB;
	bmih.biClrUsed = nPaletteEntries;
	bmih.biXPelsPerMeter = pSource->m_pBMI->bmiHeader.biXPelsPerMeter;
	bmih.biYPelsPerMeter = pSource->m_pBMI->bmiHeader.biYPelsPerMeter;

	memcpy(&pBMI->bmiColors[0], &pSource->m_pBMI->bmiColors[0], nPaletteEntries*sizeof(RGBQUAD));

	Create(pBMI);
	free(pBMI);

	if (m_hObject == NULL)
		return;

	CScreenDC dcScreen;

	CDC dcDest;
	dcDest.CreateCompatibleDC(&dcScreen);
	CBitmap* pOldBmp = dcDest.SelectObject(this);

	pSource->DrawDC(&dcDest, CPoint(0, 0));

	dcDest.SelectObject(pOldBmp);
}

CDIB* CDIB::CreateDIB(CDIB* pSource, int nBitCount)
{
	CDIB* pDIB = new CDIB();
	pDIB->Create(pSource, nBitCount);
	return pDIB;
}

CDIB* CDIB::CreateDIB(int nWidth, int nHeight, int nBitCount)
{
	BITMAPINFOHEADER bmih;
	ZeroMemory(&bmih, sizeof(bmih));

	bmih.biSize = sizeof(BITMAPINFOHEADER);
	bmih.biWidth = nWidth;
	bmih.biHeight = nHeight;
	bmih.biBitCount = nBitCount;
	bmih.biCompression = BI_RGB;
	bmih.biClrUsed = 0;

	return CreateDIB((BITMAPINFO*)&bmih);
}

CDIB* CDIB::ReduceColors()
{
	if (m_pBMI->bmiHeader.biBitCount > 8)
		return NULL;

	if (m_pBMI->bmiHeader.biClrUsed <= 2)
		return CDIB::CreateDIB(this, 1);

	if (m_pBMI->bmiHeader.biClrUsed <= 16)
		return CDIB::CreateDIB(this, 4);

	return NULL;
}

CDIB* CDIB::Crop(const CRect& rcCrop)
{
	ASSERT(m_pBits != NULL);

	UINT nSize = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD)*m_pBMI->bmiHeader.biClrUsed;
	LPBITMAPINFO pBMI = (LPBITMAPINFO) malloc(nSize);
	memcpy(pBMI, m_pBMI, nSize);

	BITMAPINFOHEADER& bmih = pBMI->bmiHeader;
	bmih.biWidth = rcCrop.Width();
	bmih.biHeight = rcCrop.Height();

	CDIB* pDIB = CreateDIB(pBMI);
	free(pBMI);

	if (pDIB->m_hObject == NULL)
		return pDIB;

	int nBpp = m_pBMI->bmiHeader.biBitCount;
	if (nBpp >= 8)
	{
		int nBytesPerPixel = nBpp / 8;
		int nSrcRowLength = CalculateDIBPitch(m_pBMI->bmiHeader.biWidth, nBpp);
		int nDstRowLength = CalculateDIBPitch(rcCrop.Width(), nBpp);

		LPBYTE pSrcBits = m_pBits;
		LPBYTE pDstBits = pDIB->GetBits();

		bool bBottomUp = (m_pBMI->bmiHeader.biHeight > 0);
		int nCropHeight = rcCrop.Height();
		int nCopyBytes = rcCrop.Width() * nBytesPerPixel;

		for (int y = 0; y < nCropHeight; ++y)
		{
			int nSrcY = bBottomUp ? (m_pBMI->bmiHeader.biHeight - 1 - (rcCrop.top + y)) : (rcCrop.top + y);
			int nDstY = bBottomUp ? (nCropHeight - 1 - y) : y;

			memcpy(pDstBits + nDstY * nDstRowLength,
			       pSrcBits + nSrcY * nSrcRowLength + rcCrop.left * nBytesPerPixel,
			       nCopyBytes);
		}
		return pDIB;
	}

	CScreenDC dcScreen;

	CDC dcDest;
	dcDest.CreateCompatibleDC(&dcScreen);
	CBitmap* pOldBmp = dcDest.SelectObject(pDIB);

	DrawDC(&dcDest, CPoint(0, 0), rcCrop);

	dcDest.SelectObject(pOldBmp);
	return pDIB;
}

void CDIB::Draw(CDC* pDC, const CPoint& ptOffset)
{
	ASSERT(pDC != NULL && m_pBits != NULL);
	::StretchDIBits(pDC->m_hDC,
		ptOffset.x, ptOffset.y, m_pBMI->bmiHeader.biWidth, m_pBMI->bmiHeader.biHeight,
		0, 0, m_pBMI->bmiHeader.biWidth, m_pBMI->bmiHeader.biHeight,
		m_pBits, m_pBMI, DIB_RGB_COLORS, SRCCOPY);
}

void CDIB::Draw(CDC* pDC, const CPoint& ptOffset, const CSize& szScaled)
{
	Draw(pDC, ptOffset, szScaled, CRect(ptOffset, szScaled));
}

void CDIB::Draw(CDC* pDC, const CPoint& ptOffset, const CSize& szScaled, const CRect& rcClip)
{
	if (szScaled.cx <= 0 || szScaled.cy <= 0)
		return;

	if (szScaled.cx == m_pBMI->bmiHeader.biWidth && szScaled.cy == m_pBMI->bmiHeader.biHeight)
	{
		Draw(pDC, ptOffset);
		return;
	}

	ASSERT(pDC != NULL && m_pBits != NULL);

	CRect rcDest(ptOffset, szScaled);
	CRect rcVisible;
	if (!rcVisible.IntersectRect(rcDest, rcClip))
		return;

	int srcW = m_pBMI->bmiHeader.biWidth;
	int srcH = abs(m_pBMI->bmiHeader.biHeight);

	int srcX = static_cast<int>(1LL * (rcVisible.left - rcDest.left) * srcW / szScaled.cx);
	int srcY = static_cast<int>(1LL * (rcVisible.top - rcDest.top) * srcH / szScaled.cy);
	int srcSubW = static_cast<int>(1LL * rcVisible.Width() * srcW / szScaled.cx) + 1;
	int srcSubH = static_cast<int>(1LL * rcVisible.Height() * srcH / szScaled.cy) + 1;

	srcX = max(0, min(srcX, srcW - 1));
	srcY = max(0, min(srcY, srcH - 1));
	srcSubW = min(srcSubW, srcW - srcX);
	srcSubH = min(srcSubH, srcH - srcY);

	int gdiSrcY = (m_pBMI->bmiHeader.biHeight > 0) ? (srcH - (srcY + srcSubH)) : srcY;

	pDC->SetStretchBltMode(COLORONCOLOR);
	::StretchDIBits(pDC->m_hDC,
		rcVisible.left, rcVisible.top, rcVisible.Width(), rcVisible.Height(),
		srcX, gdiSrcY, srcSubW, srcSubH,
		m_pBits, m_pBMI, DIB_RGB_COLORS, SRCCOPY);
}

void CDIB::DrawDC(CDC* pDC, const CPoint& ptOffset)
{
	ASSERT(pDC != NULL && m_hObject != NULL);

	static thread_local HDC s_hSharedSrcDC = NULL;
	if (s_hSharedSrcDC == NULL)
		s_hSharedSrcDC = ::CreateCompatibleDC(NULL);

	HGDIOBJ hOldBmpSrc = ::SelectObject(s_hSharedSrcDC, m_hObject);

	::BitBlt(pDC->m_hDC, ptOffset.x, ptOffset.y,
		m_pBMI->bmiHeader.biWidth, m_pBMI->bmiHeader.biHeight,
		s_hSharedSrcDC, 0, 0, SRCCOPY);

	::SelectObject(s_hSharedSrcDC, hOldBmpSrc);
}

void CDIB::DrawDC(CDC* pDC, const CPoint& ptOffset, const CRect& rcPart)
{
	ASSERT(pDC != NULL && m_hObject != NULL);
	ASSERT(rcPart.left >= 0 && rcPart.top >= 0 &&
		   rcPart.right <= m_pBMI->bmiHeader.biWidth &&
		   rcPart.bottom <= m_pBMI->bmiHeader.biHeight);

	static thread_local HDC s_hSharedSrcDC = NULL;
	if (s_hSharedSrcDC == NULL)
		s_hSharedSrcDC = ::CreateCompatibleDC(NULL);

	HGDIOBJ hOldBmpSrc = ::SelectObject(s_hSharedSrcDC, m_hObject);

	::BitBlt(pDC->m_hDC, ptOffset.x, ptOffset.y, rcPart.Width(), rcPart.Height(),
		s_hSharedSrcDC, rcPart.left, rcPart.top, SRCCOPY);

	::SelectObject(s_hSharedSrcDC, hOldBmpSrc);
}

void CDIB::SetDPI(int nDPI)
{
	ASSERT(m_pBMI != NULL);
	m_pBMI->bmiHeader.biXPelsPerMeter = static_cast<int>(nDPI/0.0254);
	m_pBMI->bmiHeader.biYPelsPerMeter = static_cast<int>(nDPI/0.0254);
}

bool CDIB::Save(LPCTSTR pszPathName, ImageFormat nFormat) const
{
	if (nFormat >= FormatPNG)
	{
		if (!Gdip::IsLoaded())
			return false;

		bool bInvertTiff = (nFormat == FormatTIF && GetBitsPerPixel() <= 8 && GetColorCount() <= 2);
		DWORD nLineLength = CalculateDIBPitch(GetWidth(), GetBitsPerPixel());
		DWORD nDwordCount = (GetHeight() * nLineLength) / 4;

		if (bInvertTiff)
		{
			for (DWORD* pData = (DWORD*)m_pBits; pData < (DWORD*)m_pBits + nDwordCount; ++pData)
				*pData = ~*pData;
		}

		Gdip::Bitmap* pGdipBitmap = NULL;
		Gdip::Status status = Gdip::CreateBitmapFromGdiDib(m_pBMI, m_pBits, &pGdipBitmap);

		if (bInvertTiff)
		{
			for (DWORD* pData = (DWORD*)m_pBits; pData < (DWORD*)m_pBits + nDwordCount; ++pData)
				*pData = ~*pData;
		}

		if (status != Gdip::Ok)
			return false;

		Gdip::BitmapSetResolution(pGdipBitmap,
				static_cast<int>(m_pBMI->bmiHeader.biXPelsPerMeter*0.0254 + 0.5),
				static_cast<int>(m_pBMI->bmiHeader.biYPelsPerMeter*0.0254 + 0.5));

		const wchar_t* szEncoder = L"image/bmp";
		switch (nFormat)
		{
		case FormatPNG:
			szEncoder = L"image/png";
			break;
		case FormatGIF:
			szEncoder = L"image/gif";
			break;
		case FormatTIF:
			szEncoder = L"image/tiff";
			break;
		case FormatJPG:
			szEncoder = L"image/jpeg";
			break;
		}

		CLSID clsid;
		if (!Gdip::GetEncoderClsid(szEncoder, &clsid))
		{
			Gdip::DisposeImage(pGdipBitmap);
			return false;
		}

		vector<char> buf(sizeof(Gdip::EncoderParameters) + 2*sizeof(Gdip::EncoderParameter));
		Gdip::EncoderParameters* parameters = (Gdip::EncoderParameters*) &buf[0];
		parameters->Count = 0;

		LONG nTiffEncoding = Gdip::EncoderValueCompressionLZW;
		LONG nTiffColorDepth = 24;
		LONG nJpegQuality = 80;
		switch (nFormat)
		{
		case FormatTIF:
			parameters->Count = 2;
			parameters->Parameter[0].Guid = Gdip::EncoderCompression;
			parameters->Parameter[0].NumberOfValues = 1;
			parameters->Parameter[0].Type = Gdip::EncoderParameterValueTypeLong;
			parameters->Parameter[0].Value = &nTiffEncoding;
			parameters->Parameter[1].Guid = Gdip::EncoderColorDepth;
			parameters->Parameter[1].NumberOfValues = 1;
			parameters->Parameter[1].Type = Gdip::EncoderParameterValueTypeLong;
			parameters->Parameter[1].Value = &nTiffColorDepth;

			if (GetBitsPerPixel() <= 8)
			{
				if (GetColorCount() <= 2)
				{
					nTiffEncoding = Gdip::EncoderValueCompressionCCITT4;
					nTiffColorDepth = 1;
				}
				else
					nTiffColorDepth = 8;
			}
			break;

		case FormatJPG:
			parameters->Count = 1;
			parameters->Parameter[0].Guid = Gdip::EncoderQuality;
			parameters->Parameter[0].NumberOfValues = 1;
			parameters->Parameter[0].Type = Gdip::EncoderParameterValueTypeLong;
			parameters->Parameter[0].Value = &nJpegQuality;
			break;
		}

		wstring wstrFileName;
		MakeWString(CString(pszPathName), wstrFileName);
		bool bResult = (Gdip::SaveImageToFile(pGdipBitmap, wstrFileName.c_str(), &clsid,
				parameters->Count > 0 ? parameters : NULL) == Gdip::Ok);

		Gdip::DisposeImage(pGdipBitmap);
		return bResult;
	}
	else
	{
		CFile file;
		if (!file.Open(pszPathName, CFile::modeCreate | CFile::modeWrite | CFile::shareExclusive))
			return false;

		BITMAPFILEHEADER hdr;
		hdr.bfType = 'MB';
		hdr.bfReserved1 = 0;
		hdr.bfReserved2 = 0;

		DWORD dwHeaderSize = sizeof(BITMAPINFOHEADER) +
			sizeof(RGBQUAD)*m_pBMI->bmiHeader.biClrUsed;
		DWORD dwDataSize = CalculateDIBPitch(m_pBMI->bmiHeader.biWidth, m_pBMI->bmiHeader.biBitCount) * m_pBMI->bmiHeader.biHeight;

		hdr.bfOffBits = sizeof(BITMAPFILEHEADER) + dwHeaderSize;
		hdr.bfSize = hdr.bfOffBits + dwDataSize;

		file.Write(&hdr, sizeof(hdr));
		file.Write(m_pBMI, dwHeaderSize);
		file.Write(m_pBits, dwDataSize);

		file.Close();
		return true;
	}
}

HGLOBAL CDIB::SaveToMemory() const
{
	DWORD dwHeaderSize = sizeof(BITMAPINFOHEADER) +
		sizeof(RGBQUAD)*m_pBMI->bmiHeader.biClrUsed;
	DWORD dwDataSize = CalculateDIBPitch(m_pBMI->bmiHeader.biWidth, m_pBMI->bmiHeader.biBitCount) * m_pBMI->bmiHeader.biHeight;

	HGLOBAL hData = ::GlobalAlloc(GMEM_MOVEABLE, dwHeaderSize + dwDataSize);
	if (hData != NULL)
	{
		LPBYTE pData = (LPBYTE) ::GlobalLock(hData);
		memmove(pData, m_pBMI, dwHeaderSize);
		memmove(pData + dwHeaderSize, m_pBits, dwDataSize);
		::GlobalUnlock(hData);
	}

	return hData;
}

// CLightweightDIB

CLightweightDIB* CLightweightDIB::Create(CDIB* pSrc)
{
	CLightweightDIB* pDIB = new CLightweightDIB();

	ASSERT(pSrc != NULL);
	if (pSrc->GetBitsPerPixel() == 24)
	{
		const BITMAPINFO* pSrcBMI = pSrc->GetBitmapInfo();
		UINT nSize = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * pSrc->GetColorCount();
		pDIB->m_pBMI = (LPBITMAPINFO)malloc(nSize);
		memcpy(pDIB->m_pBMI, pSrcBMI, nSize);

		int nRowLength = CalculateDIBPitch(pDIB->GetWidth(), 24);
		DWORD dwByteCount = nRowLength * pDIB->GetHeight();
		pDIB->m_pBits = new BYTE[dwByteCount];
		memcpy(pDIB->m_pBits, pSrc->GetBits(), dwByteCount);
		return pDIB;
	}

	pDIB->CDIB::Create(pSrc, 24);

	if (pDIB->m_hObject == NULL)
	{
		pDIB->m_pBits = NULL;
		return pDIB;
	}

	DWORD dwByteCount = CalculateDIBPitch(pDIB->GetWidth(), pDIB->GetBitsPerPixel()) * pDIB->GetHeight();

	LPBYTE pBits = new BYTE[dwByteCount];
	memcpy(pBits, pDIB->m_pBits, dwByteCount);

	pDIB->DeleteObject();
	pDIB->m_pBits = pBits;

	return pDIB;
}

CLightweightDIB::~CLightweightDIB()
{
	delete[] m_pBits;
}


// Drawing

void FrameRect(CDC* pDC, const CRect& rect, COLORREF color)
{
	CRect rcHorzLine(rect.TopLeft(), CSize(rect.Width(), 1));
	pDC->FillSolidRect(rcHorzLine, color);
	rcHorzLine.OffsetRect(0, rect.Height() - 1);
	pDC->FillSolidRect(rcHorzLine, color);

	CRect rcVertLine(rect.TopLeft(), CSize(1, rect.Height()));
	pDC->FillSolidRect(rcVertLine, color);
	rcVertLine.OffsetRect(rect.Width() - 1, 0);
	pDC->FillSolidRect(rcVertLine, color);
}

void InvertFrame(CDC* pDC, const CRect& rect)
{
	CRect rcHorzLine(rect.TopLeft(), CSize(rect.Width(), 1));
	pDC->InvertRect(rcHorzLine);
	rcHorzLine.OffsetRect(0, rect.Height() - 1);
	pDC->InvertRect(rcHorzLine);

	CRect rcVertLine(CPoint(rect.left, rect.top + 1), CSize(1, rect.Height() - 2));
	pDC->InvertRect(rcVertLine);
	rcVertLine.OffsetRect(rect.Width() - 1, 0);
	pDC->InvertRect(rcVertLine);
}

void FrameOval(CDC* pDC, const CRect& bounds, COLORREF color)
{
	CPen penSolid(PS_SOLID, 1, color);
	CPen* pOldPen = pDC->SelectObject(&penSolid);
	CBrush* pOldBrush = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);

	pDC->Ellipse(bounds);

	pDC->SelectObject(pOldBrush);
	pDC->SelectObject(pOldPen);
}

void InvertOvalFrame(CDC* pDC, const CRect& bounds)
{
	int nOldROP = pDC->SetROP2(R2_NOT);
	CPen penSolid(PS_SOLID, 1, RGB(0, 0, 0));
	CPen* pOldPen = pDC->SelectObject(&penSolid);
	CBrush* pOldBrush = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);

	pDC->Ellipse(bounds);

	pDC->SelectObject(pOldBrush);
	pDC->SelectObject(pOldPen);
	pDC->SetROP2(nOldROP);
}

void FramePoly(CDC* pDC, const POINT* points, int nCount, COLORREF color)
{
	CPen penSolid(PS_SOLID, 1, color);
	CPen* pOldPen = pDC->SelectObject(&penSolid);

	pDC->Polyline((POINT*) points, nCount);

	pDC->SelectObject(pOldPen);
}

void InvertPolyFrame(CDC* pDC, const POINT* points, int nCount)
{
	if (nCount < 2)
		return;

	int nOldROP = pDC->SetROP2(R2_NOT);
	CPen penSolid(PS_SOLID, 1, RGB(0, 0, 0));
	CPen* pOldPen = pDC->SelectObject(&penSolid);

	pDC->Polyline((POINT*)points, nCount);

	pDC->SelectObject(pOldPen);
	pDC->SetROP2(nOldROP);
}

void DrawDottedLine(CDC* pDC, const CPoint& ptStart, const CPoint& ptEnd, COLORREF color)
{
	if (ptStart.x != ptEnd.x && ptStart.y != ptEnd.y)
	{
		ASSERT(false);
		return;
	}

	LOGBRUSH lb;
	lb.lbStyle = BS_SOLID;
	lb.lbColor = color;
	lb.lbHatch = 0;

	// 1 pixel ON, 1 pixel OFF
	DWORD dwStyle[2] = { 1, 1 };
	HPEN hPen = ::ExtCreatePen(PS_COSMETIC | PS_USERSTYLE, 1, &lb, 2, dwStyle);
	if (hPen != NULL)
	{
		HGDIOBJ hOldPen = ::SelectObject(pDC->m_hDC, hPen);

		// Guarantee transparent gaps matching the old SetPixel behavior
		int nOldBkMode = ::SetBkMode(pDC->m_hDC, TRANSPARENT);

		::MoveToEx(pDC->m_hDC, ptStart.x, ptStart.y, NULL);
		::LineTo(pDC->m_hDC, ptEnd.x, ptEnd.y);

		::SetBkMode(pDC->m_hDC, nOldBkMode);
		::SelectObject(pDC->m_hDC, hOldPen);
		::DeleteObject(hPen);
	}
}

void DrawDottedRect(CDC* pDC, const CRect& rect, COLORREF color)
{
	LOGBRUSH lb;
	lb.lbStyle = BS_SOLID;
	lb.lbColor = color;
	lb.lbHatch = 0;

	DWORD dwStyle[2] = { 1, 1 };
	HPEN hPen = ::ExtCreatePen(PS_COSMETIC | PS_USERSTYLE, 1, &lb, 2, dwStyle);
	if (hPen != NULL)
	{
		HGDIOBJ hOldPen = ::SelectObject(pDC->m_hDC, hPen);
		int nOldBkMode = ::SetBkMode(pDC->m_hDC, TRANSPARENT);

		// Top
		::MoveToEx(pDC->m_hDC, rect.left, rect.top, NULL);
		::LineTo(pDC->m_hDC, rect.right, rect.top);

		// Right
		::MoveToEx(pDC->m_hDC, rect.right - 1, rect.top + (rect.Width() + 1) % 2, NULL);
		::LineTo(pDC->m_hDC, rect.right - 1, rect.bottom);

		// Left
		::MoveToEx(pDC->m_hDC, rect.left, rect.top, NULL);
		::LineTo(pDC->m_hDC, rect.left, rect.bottom);

		// Bottom
		::MoveToEx(pDC->m_hDC, rect.left + (rect.Height() + 1) % 2, rect.bottom - 1, NULL);
		::LineTo(pDC->m_hDC, rect.right, rect.bottom - 1);

		::SetBkMode(pDC->m_hDC, nOldBkMode);
		::SelectObject(pDC->m_hDC, hOldPen);
		::DeleteObject(hPen);
	}
}

void HighlightRect(CDC* pDC, const CRect& rect, COLORREF color, double fTransparency)
{
	CRect rcClip;
	pDC->GetClipBox(rcClip);
	if (!rcClip.IntersectRect(CRect(rcClip), rect))
		return;

	int nAlpha = max(0, min(255, static_cast<int>(fTransparency * 255.0 + 0.5)));
	BYTE nSrcAlpha = static_cast<BYTE>(255 - nAlpha);
	if (nSrcAlpha == 0)
		return;

	// Persistent 1x1 32-bit DIB holding the solid highlight color
	static HDC s_hMemDC = NULL;
	static HBITMAP s_hBmp = NULL;
	static COLORREF s_crLast = CLR_INVALID;
	static DWORD* s_pPixel = NULL;

	if (s_hMemDC == NULL)
	{
		s_hMemDC = ::CreateCompatibleDC(NULL);
		BITMAPINFO bi;
		ZeroMemory(&bi, sizeof(bi));
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = 1;
		bi.bmiHeader.biHeight = 1;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;
		s_hBmp = ::CreateDIBSection(s_hMemDC, &bi, DIB_RGB_COLORS, (void**)&s_pPixel, NULL, 0);
		::SelectObject(s_hMemDC, s_hBmp);
	}

	if (s_pPixel != NULL && s_crLast != color)
	{
		BYTE* p = reinterpret_cast<BYTE*>(s_pPixel);
		p[0] = GetBValue(color);
		p[1] = GetGValue(color);
		p[2] = GetRValue(color);
		p[3] = 0;
		s_crLast = color;
	}

	BLENDFUNCTION bf;
	bf.BlendOp = AC_SRC_OVER;
	bf.BlendFlags = 0;
	bf.SourceConstantAlpha = nSrcAlpha;
	bf.AlphaFormat = 0;

	::GdiAlphaBlend(pDC->m_hDC, rcClip.left, rcClip.top, rcClip.Width(), rcClip.Height(),
					s_hMemDC, 0, 0, 1, 1, bf);
}

void HighlightPolygon(CDC* pDC, const POINT* points, int nCount, COLORREF color, double fTransparency)
{
	if (nCount < 2)
		return;

	CRect rcBounds = CRect(points[0], CSize(1, 1));
	for (int nPoint = 0; nPoint < nCount; ++nPoint)
		rcBounds.UnionRect(CRect(rcBounds), CRect(points[nPoint], CSize(1, 1)));

	CRect rcClip;
	pDC->GetClipBox(rcClip);
	if (!rcClip.IntersectRect(CRect(rcClip), rcBounds))
		return;

	HRGN hRgn = ::CreatePolygonRgn(points, nCount, WINDING);
	if (hRgn != NULL)
	{
		int nSaved = pDC->SaveDC();
		::ExtSelectClipRgn(pDC->m_hDC, hRgn, RGN_AND);
		HighlightRect(pDC, rcClip, color, fTransparency);
		pDC->RestoreDC(nSaved);
		::DeleteObject(hRgn);
	}
}

void DrawArrow(CDC* pDC, int nArrowType, const CRect& rect, COLORREF color)
{
	POINT points[3];

	switch (nArrowType)
	{
	case ARR_LEFT:
		points[0].x = rect.left;
		points[0].y = rect.CenterPoint().y;
		points[1].x = rect.right;
		points[1].y = rect.top;
		points[2].x = rect.right;
		points[2].y = rect.bottom;
		break;

	case ARR_RIGHT:
		points[0].x = rect.left;
		points[0].y = rect.top;
		points[1].x = rect.right;
		points[1].y = rect.CenterPoint().y;
		points[2].x = rect.left;
		points[2].y = rect.bottom;
		break;

	case ARR_UP:
		points[0].x = rect.CenterPoint().x;
		points[0].y = rect.top;
		points[1].x = rect.right;
		points[1].y = rect.bottom;
		points[2].x = rect.left;
		points[2].y = rect.bottom;
		break;

	case ARR_DOWN:
	default:
		points[0].x = rect.left;
		points[0].y = rect.top;
		points[1].x = rect.right;
		points[1].y = rect.top;
		points[2].x = rect.CenterPoint().x;
		points[2].y = rect.bottom;
		break;
	}
	
	CBrush brush(color);
	CPen pen(PS_SOLID, 1, color);

	CBrush* pOldBrush = pDC->SelectObject(&brush);
	CPen* pOldPen = pDC->SelectObject(&pen);
	pDC->SetPolyFillMode(WINDING);

	pDC->Polygon(points, 3);

	pDC->SelectObject(pOldPen);
	pDC->SelectObject(pOldBrush);
}


// COffscreenDC

COffscreenDC::COffscreenDC()
	: m_pBitmap(NULL), m_pOldBitmap(NULL), m_szBitmap(0, 0)
{
}

COffscreenDC::~COffscreenDC()
{
	if (m_hDC != NULL)
		Release();

	delete m_pBitmap;
}

bool COffscreenDC::Create(CDC* pDC, CSize size, int nBitCount)
{
	if (m_pBitmap == NULL || m_szBitmap.cx < size.cx || m_szBitmap.cy < size.cy || m_pBitmap->GetBitsPerPixel() != nBitCount)
	{
		m_szBitmap.cx = max(m_szBitmap.cx, static_cast<int>(size.cx*1.1 + 0.5));
		m_szBitmap.cy = max(m_szBitmap.cy, static_cast<int>(size.cy*1.1 + 0.5));

		delete m_pBitmap;
		m_pBitmap = CDIB::CreateDIB(m_szBitmap.cx, -m_szBitmap.cy, nBitCount); // Top-down DIB
	}

	ASSERT(m_hDC == NULL);
	if (m_hDC != NULL)
		Release();

	if (!CreateCompatibleDC(pDC))
		return false;

	SetViewportOrg(CPoint(0, 0));

	m_pOldBitmap = SelectObject(m_pBitmap);
	return true;
}

void COffscreenDC::Release()
{
	ASSERT(m_hDC != NULL);

	if (m_hDC != NULL)
	{
		SelectObject(m_pOldBitmap);
		m_pOldBitmap = NULL;

		DeleteDC();
	}
}


// CScreenDC

CScreenDC::CScreenDC()
{
	CreateDC(_T("DISPLAY"), NULL, NULL, NULL);
}


// Printing

CRect FindContentRect(GP<DjVuImage> pImage)
{
	CRect rcResult(0, 0, 0, 0);
	if (pImage == NULL)
		return rcResult;

	CSize szImage(pImage->get_width(), pImage->get_height());
	if (szImage.cx <= 0 || szImage.cy <= 0)
		return rcResult;

	// Subsample large images down to ~1024 max dimension for margin detection.
	// This avoids decoding full 35MB-100MB bitmaps just to find outer white borders.
	int nSub = max(1, max(szImage.cx, szImage.cy) / 1024);
	CSize szSub(max(1, szImage.cx / nSub), max(1, szImage.cy / nSub));
	GRect rectSub(0, 0, szSub.cx, szSub.cy);

	int yMin = 0, yMax = szSub.cy;
	int xMin = 0, xMax = szSub.cx;

	if (pImage->is_legal_photo() || pImage->is_legal_compound())
	{
		GP<GPixmap> pm = pImage->get_pixmap(rectSub, rectSub);
		if (pm == NULL)
			return rcResult;

		int nRows = pm->rows();
		int nCols = pm->columns();

		// In DjVu, y=0 is the bottom row. Find bottom margin (yMin)
		int y;
		for (y = 0; y < nRows; ++y)
		{
			const GPixel* row = (*pm)[y];
			int x;
			for (x = 0; x < nCols; ++x)
			{
				if (row[x].r < 250 || row[x].g < 250 || row[x].b < 250)
					break;
			}
			if (x < nCols)
				break;
		}
		yMin = y;

		// Find top margin (yMax)
		for (y = nRows - 1; y >= yMin; --y)
		{
			const GPixel* row = (*pm)[y];
			int x;
			for (x = 0; x < nCols; ++x)
			{
				if (row[x].r < 250 || row[x].g < 250 || row[x].b < 250)
					break;
			}
			if (x < nCols)
				break;
		}
		yMax = y + 1;

		if (yMax <= yMin)
			return CRect(0, 0, 0, 0);

		// Find left margin
		xMin = nCols;
		for (y = yMin; y < yMax; ++y)
		{
			const GPixel* row = (*pm)[y];
			for (int x = 0; x < xMin; ++x)
			{
				if (row[x].r < 250 || row[x].g < 250 || row[x].b < 250)
				{
					xMin = x;
					break;
				}
			}
		}

		// Find right margin
		xMax = xMin;
		for (y = yMin; y < yMax; ++y)
		{
			const GPixel* row = (*pm)[y];
			for (int x = nCols - 1; x >= xMax; --x)
			{
				if (row[x].r < 250 || row[x].g < 250 || row[x].b < 250)
				{
					xMax = x + 1;
					break;
				}
			}
		}
	}
	else // Bilevel / B&W
	{
		GP<GBitmap> bm = pImage->get_bitmap(rectSub, rectSub, 1);
		if (bm == NULL)
			return rcResult;

		int nRows = bm->rows();
		int nCols = bm->columns();

		// In GBitmap, 0 is white/background
		int y;
		for (y = 0; y < nRows; ++y)
		{
			const unsigned char* row = (*bm)[y];
			int x;
			for (x = 0; x < nCols; ++x)
			{
				if (row[x] != 0)
					break;
			}
			if (x < nCols)
				break;
		}
		yMin = y;

		for (y = nRows - 1; y >= yMin; --y)
		{
			const unsigned char* row = (*bm)[y];
			int x;
			for (x = 0; x < nCols; ++x)
			{
				if (row[x] != 0)
					break;
			}
			if (x < nCols)
				break;
		}
		yMax = y + 1;

		if (yMax <= yMin)
			return CRect(0, 0, 0, 0);

		xMin = nCols;
		for (y = yMin; y < yMax; ++y)
		{
			const unsigned char* row = (*bm)[y];
			for (int x = 0; x < xMin; ++x)
			{
				if (row[x] != 0)
				{
					xMin = x;
					break;
				}
			}
		}

		xMax = xMin;
		for (y = yMin; y < yMax; ++y)
		{
			const unsigned char* row = (*bm)[y];
			for (int x = nCols - 1; x >= xMax; --x)
			{
				if (row[x] != 0)
				{
					xMax = x + 1;
					break;
				}
			}
		}
	}

	// Scale back to native resolution and preserve DjVu coordinate convention:
	// top = yMin (bottom offset), bottom = yMax (top offset)
	rcResult.top = yMin * nSub;
	rcResult.bottom = yMax * nSub;
	rcResult.left = xMin * nSub;
	rcResult.right = xMax * nSub;

	// Clamp to native image bounds
	rcResult.left = max(0, min(szImage.cx, (int)rcResult.left));
	rcResult.right = max((int)rcResult.left, min(szImage.cx, (int)rcResult.right));
	rcResult.top = max(0, min(szImage.cy, (int)rcResult.top));
	rcResult.bottom = max((int)rcResult.top, min(szImage.cy, (int)rcResult.bottom));

	return rcResult;
}

void PrintPage(CDC* pDC, GP<DjVuImage> pImage, int nRotate, int nMode, const CRect& rcFullPage,
	double fPrinterMMx, double fPrinterMMy, CPrintSettings& settings, GRect* pSelRect, bool bPreview)
{
	if (pImage == NULL)
		return;

	CSize szImage(pImage->get_width(), pImage->get_height());
	if (szImage.cx <= 0 || szImage.cy <= 0)
		return;

	CPoint ptSrcOffset(0, 0);
	CSize szDjVuPage(szImage);

	if (pSelRect != NULL)
	{
		ptSrcOffset = CPoint(pSelRect->xmin, pSelRect->ymin);
		szDjVuPage = CSize(pSelRect->width(), pSelRect->height());
	}
	else if (settings.bClipContent)
	{
		CRect rcContent = FindContentRect(pImage);
		ptSrcOffset = rcContent.TopLeft();

		szDjVuPage = rcContent.Size();
	}

	int nTotalRotate = GetTotalRotate(pImage, nRotate);
	if (nTotalRotate % 2 != 0)
		swap(szDjVuPage.cx, szDjVuPage.cy);

	CRect rcPage = rcFullPage;

	// Subtract margins
	rcPage.DeflateRect(static_cast<int>(settings.fMarginLeft*fPrinterMMx),
					   static_cast<int>(settings.fMarginTop*fPrinterMMy),
					   static_cast<int>(settings.fMarginRight*fPrinterMMx),
					   static_cast<int>(settings.fMarginBottom*fPrinterMMy));

	if (settings.bAutoRotate && (rcPage.Width() - rcPage.Height())*(szDjVuPage.cx - szDjVuPage.cy) < 0)
	{
		nTotalRotate = (nTotalRotate + 3) % 4;
		swap(szDjVuPage.cx, szDjVuPage.cy);
	}

	double fSourceMM = pImage->get_dpi() / 25.4;

	CSize szScaled;
	CPoint ptOffset;

	if (!settings.bCenterImage)
	{
		ptOffset.x = static_cast<int>(settings.fPosLeft * fPrinterMMx);
		ptOffset.y = static_cast<int>(settings.fPosTop * fPrinterMMy);
	}

	if (!settings.bScaleToFit)
	{
		szScaled.cx = static_cast<int>(szDjVuPage.cx * fPrinterMMx * settings.fScale * 0.01 / fSourceMM);
		szScaled.cy = static_cast<int>(szDjVuPage.cy * fPrinterMMy * settings.fScale * 0.01 / fSourceMM);

		if (settings.bCenterImage)
		{
			ptOffset.x = (rcPage.Width() - szScaled.cx) / 2;
			ptOffset.y = (rcPage.Height() - szScaled.cy) / 2;
		}
	}

	if (settings.bScaleToFit || settings.bShrinkOversized && (szScaled.cx > rcPage.Width() || szScaled.cy > rcPage.Height()))
	{
		szScaled.cx = rcPage.Width();
		szScaled.cy = szScaled.cx * szDjVuPage.cy / szDjVuPage.cx;
		if (szScaled.cy > rcPage.Height())
		{
			szScaled.cy = rcPage.Height();
			szScaled.cx = szScaled.cy * szDjVuPage.cx / szDjVuPage.cy;
		}

		if (settings.bCenterImage)
		{
			ptOffset.x = (rcPage.Width() - szScaled.cx) / 2;
			ptOffset.y = (rcPage.Height() - szScaled.cy) / 2;
		}
	}

	CRect rcResult(ptOffset + rcPage.TopLeft(), szScaled);
	rcResult &= rcFullPage;
	if (rcResult.IsRectEmpty())
		return;

	GRect rect, rectAll;
	if (settings.bClipContent || pSelRect != NULL || !bPreview)
	{
		if (nTotalRotate % 2 == 0)
			rect = GRect(ptSrcOffset.x, ptSrcOffset.y, szDjVuPage.cx, szDjVuPage.cy);
		else
			rect = GRect(ptSrcOffset.x, ptSrcOffset.y, szDjVuPage.cy, szDjVuPage.cx);

		rectAll = GRect(0, 0, szImage.cx, szImage.cy);
	}
	else // if (bPreview)
	{
		rect = GRect(0, 0, szScaled.cx, szScaled.cy);
		if (nTotalRotate % 2 != 0)
			swap(rect.xmax, rect.ymax);

		rectAll = rect;
	}

	CDIB* pBmpPage = NULL;
	GP<GPixmap> pm;
	GP<GBitmap> bm;

	try
	{
		switch (nMode)
		{
		case CDjVuView::BlackAndWhite:
			bm = pImage->get_bitmap(rect, rectAll, 4);
			break;

		case CDjVuView::Foreground:
			pm = pImage->get_fg_pixmap(rect, rectAll);
			if (pm == NULL)
				bm = pImage->get_bitmap(rect, rectAll, 4);
			break;

		case CDjVuView::Background:
			pm = pImage->get_bg_pixmap(rect, rectAll);
			break;

		case CDjVuView::Color:
		default:
			pm = pImage->get_pixmap(rect, rectAll);
			if (pm == NULL)
				bm = pImage->get_bitmap(rect, rectAll, 4);
		}
	}
	catch (GException&)
	{
	}
	catch (...)
	{
		theApp.ReportFatalError();
	}

	CDisplaySettings defaultSettings;
	CDisplaySettings& displaySettings = (settings.bAdjustPrinting ? *theApp.GetDisplaySettings() : defaultSettings);

	if (pm != NULL)
	{
		if (nTotalRotate != 0)
			pm = pm->rotate(nTotalRotate);

		if ((settings.bClipContent || pSelRect != NULL) && bPreview)
		{
			// Scale the pixmap to needed size
			GRect rcDest(0, 0, szScaled.cx, szScaled.cy);
			GRect rcSrc(0, 0, szDjVuPage.cx, szDjVuPage.cy);

			GP<GPixmapScaler> ps = GPixmapScaler::create();
			ps->set_input_size(szDjVuPage.cx, szDjVuPage.cy);
			ps->set_output_size(szScaled.cx, szScaled.cy);
			ps->set_horz_ratio(szScaled.cx, szDjVuPage.cx);
			ps->set_vert_ratio(szScaled.cy, szDjVuPage.cy);

			GP<GPixmap> pmStretched = GPixmap::create();
			ps->scale(rcSrc, *pm, rcDest, *pmStretched);
			pm = pmStretched;
		}

		pBmpPage = RenderPixmap(*pm, displaySettings);
	}
	else if (bm != NULL)
	{
		if (nTotalRotate != 0)
			bm = bm->rotate(nTotalRotate);

		if ((settings.bClipContent || pSelRect != NULL) && bPreview)
		{
			// Scale the bitmap to needed size
			GRect rcDest(0, 0, szScaled.cx, szScaled.cy);
			GRect rcSrc(0, 0, szDjVuPage.cx, szDjVuPage.cy);

			GP<GBitmapScaler> bs = GBitmapScaler::create();
			bs->set_input_size(szDjVuPage.cx, szDjVuPage.cy);
			bs->set_output_size(szScaled.cx, szScaled.cy);
			bs->set_horz_ratio(szScaled.cx, szDjVuPage.cx);
			bs->set_vert_ratio(szScaled.cy, szDjVuPage.cy);

			GP<GBitmap> bmStretched = GBitmap::create();
			bs->scale(rcSrc, *bm, rcDest, *bmStretched);
			bm = bmStretched;
		}

		pBmpPage = RenderBitmap(*bm, displaySettings);

		if (!bPreview)
		{
			CDIB* pBmpReduced = pBmpPage->ReduceColors();
			if (pBmpReduced != NULL)
			{
				delete pBmpPage;
				pBmpPage = pBmpReduced;
			}
		}
	}

	if (pBmpPage == NULL)
		return;

	int nDC = pDC->SaveDC();
	pDC->IntersectClipRect(rcFullPage);

	if (bPreview)
		pBmpPage->Draw(pDC, ptOffset + rcPage.TopLeft());
	else
		pBmpPage->Draw(pDC, ptOffset + rcPage.TopLeft(), szScaled);

	pDC->RestoreDC(nDC);

	delete pBmpPage;
}

unsigned int __stdcall PrintThreadProc(void* pvData)
{
	::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	IProgressInfo* pProgress = reinterpret_cast<IProgressInfo*>(pvData);
	CPrintDlg& dlg = *reinterpret_cast<CPrintDlg*>(pProgress->GetUserData());

	DjVuSource* pSource = dlg.GetSource();

	CPrintSettings& printSettings = dlg.m_settings;

	int nSheetCount = static_cast<int>(dlg.m_arrPages.size());
	if (nSheetCount == 0)
	{
		pProgress->StopProgress(0);
		::CoUninitialize();
		return 0;
	}

	int nPages = nSheetCount;
	if (printSettings.nCopies > 1 && printSettings.bCollate && !dlg.m_pPrinter->bCanCollate)
	{
		// We will do collation ourselves
		nPages = nSheetCount * printSettings.nCopies;
		dlg.m_pDevMode->dmCopies = 1;
	}

	pProgress->SetRange(0, nPages);
	pProgress->SetStatus(LoadString(IDS_PRINTING));

	// Printing
	CDC dcPrint;
	dcPrint.CreateDC(dlg.m_pPrinter->strDriverName, dlg.m_pPrinter->strPrinterName, NULL, dlg.m_pDevMode);

	if (dcPrint.m_hDC == NULL)
	{
		pProgress->StopProgress(1);
		::CoUninitialize();
		return 1;
	}

	dcPrint.SetMapMode(MM_TEXT);

	double fPrinterMMx = dcPrint.GetDeviceCaps(LOGPIXELSX) / 25.4;
	double fPrinterMMy = dcPrint.GetDeviceCaps(LOGPIXELSY) / 25.4;

	int nPageWidth, nPageHeight;
	if (printSettings.bIgnorePrinterMargins)
	{
		nPageWidth = dcPrint.GetDeviceCaps(PHYSICALWIDTH);
		nPageHeight = dcPrint.GetDeviceCaps(PHYSICALHEIGHT);

		int nOffsetX = dcPrint.GetDeviceCaps(PHYSICALOFFSETX);
		int nOffsetY = dcPrint.GetDeviceCaps(PHYSICALOFFSETY);
		dcPrint.SetViewportOrg(-nOffsetX, -nOffsetY);
	}
	else
	{
		nPageWidth = dcPrint.GetDeviceCaps(HORZRES);
		nPageHeight = dcPrint.GetDeviceCaps(VERTRES);
	}

	CSize szPaper(nPageWidth, nPageHeight);

	CRect rcOddPage = CRect(CPoint(0, 0), szPaper), rcEvenPage = rcOddPage;
	if (printSettings.bTwoPages && !dlg.IsPrintSelection())
	{
		if (printSettings.bLandscape)
		{
			szPaper.cx = szPaper.cx / 2;
			rcOddPage.right = szPaper.cx;
			rcEvenPage.left = rcOddPage.right;
		}
		else
		{
			szPaper.cy = szPaper.cy / 2;
			rcOddPage.bottom = szPaper.cy;
			rcEvenPage.top = rcOddPage.bottom;
		}
	}

	int nRotate = dlg.GetRotate();
	int nMode = dlg.GetMode();

	DOCINFO di;
	di.cbSize = sizeof(DOCINFO);
	di.fwType = 0;
	di.lpszDocName = PathFindFileName(dlg.GetSource()->GetFileName());
	di.lpszDatatype = NULL;
	di.lpszOutput = (dlg.m_bPrintToFile ? _T("C:\\Output.prn") : NULL);

	if (dcPrint.StartDoc(&di) <= 0)
	{
		pProgress->StopProgress(2);
		::CoUninitialize();
		return 2;
	}

	for (int i = 0; i < nPages; ++i)
	{
		pProgress->SetStatus(FormatString(IDS_PRINTING_PAGE, i + 1, nPages));
		pProgress->SetPos(i);

		if (pProgress->IsCancelled())
		{
			dcPrint.AbortDoc();

			pProgress->StopProgress(0);
			::CoUninitialize();
			return 0;
		}

		int nSheetIndex = i % nSheetCount;
		int nPage = dlg.m_arrPages[nSheetIndex].first - 1;
		int nSecondPage = dlg.m_arrPages[nSheetIndex].second - 1;

		if ((nPage < 0 || nPage >= pSource->GetPageCount()) &&
				(!printSettings.bTwoPages || nSecondPage < 0 || nSecondPage >= pSource->GetPageCount()))
			continue;

		if (dcPrint.StartPage() <= 0)
		{
			pProgress->StopProgress(3);
			::CoUninitialize();
			return 3;
		}

		GP<DjVuImage> pImage;
		if (nPage >= 0 && nPage < pSource->GetPageCount())
			pImage = pSource->GetPage(nPage, NULL);
		if (pImage != NULL)
		{
			GRect* pSelRect = NULL;
			if (dlg.IsPrintSelection())
				pSelRect = &dlg.m_rcSelection;

			PrintPage(&dcPrint, pImage, nRotate, nMode, rcOddPage, fPrinterMMx, fPrinterMMy, printSettings, pSelRect);
		}

		pImage = NULL;
		if (printSettings.bTwoPages && nSecondPage >= 0 && nSecondPage < pSource->GetPageCount())
			pImage = pSource->GetPage(nSecondPage, NULL);
		if (pImage != NULL)
		{
			PrintPage(&dcPrint, pImage, nRotate, nMode, rcEvenPage, fPrinterMMx, fPrinterMMy, printSettings);
		}

		if (dcPrint.EndPage() <= 0)
		{
			pProgress->StopProgress(4);
			::CoUninitialize();
			return 4;
		}
	}

	dcPrint.EndDoc();

	pProgress->SetPos(nPages);
	pProgress->StopProgress(0);

	::CoUninitialize();
	return 0;
}
