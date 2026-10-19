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

//////////////////////////////////////////////////////////////////////
// Copyright (C) Microsoft Corporation. All rights reserved.
// Windows XP theme API

#pragma once
#pragma pack(push,4)
#ifndef THEMEAPI

typedef HANDLE HTHEME;

#define DTT_GRAYED 0x1 // draw a grayed-out string

enum THEMESIZE
{
    TS_MIN,             // minimum size
    TS_TRUE,            // size without stretching
    TS_DRAW,            // size that theme mgr will use to draw part
};

#define HTTB_BACKGROUNDSEG          0x0000
#define HTTB_FIXEDBORDER            0x0002  // Return code may be either HTCLIENT or HTBORDER.
#define HTTB_CAPTION                0x0004
#define HTTB_RESIZINGBORDER_LEFT    0x0010  // Hit test left resizing border
#define HTTB_RESIZINGBORDER_TOP     0x0020  // Hit test top resizing border
#define HTTB_RESIZINGBORDER_RIGHT   0x0040  // Hit test right resizing border
#define HTTB_RESIZINGBORDER_BOTTOM  0x0080  // Hit test bottom resizing border
#define HTTB_RESIZINGBORDER         (HTTB_RESIZINGBORDER_LEFT|HTTB_RESIZINGBORDER_TOP|\
                                     HTTB_RESIZINGBORDER_RIGHT|HTTB_RESIZINGBORDER_BOTTOM)
#define HTTB_SIZINGTEMPLATE         0x0100
#define HTTB_SYSTEMSIZINGMARGINS    0x0200

struct MARGINS
{
    int cxLeftWidth;      // width of left border that retains its size
    int cxRightWidth;     // width of right border that retains its size
    int cyTopHeight;      // height of top border that retains its size
    int cyBottomHeight;   // height of bottom border that retains its size
};
typedef MARGINS* PMARGINS;

#define MAX_INTLIST_COUNT 10

struct INTLIST
{
    int iValueCount;      // number of values in iValues
    int iValues[MAX_INTLIST_COUNT];
};
typedef INTLIST* PINTLIST;

enum PROPERTYORIGIN
{
    PO_STATE,           // property was found in the state section
    PO_PART,            // property was found in the part section
    PO_CLASS,           // property was found in the class section
    PO_GLOBAL,          // property was found in [globals] section
    PO_NOTFOUND         // property was not found
};

#define ETDT_DISABLE        0x00000001
#define ETDT_ENABLE         0x00000002
#define ETDT_USETABTEXTURE  0x00000004
#define ETDT_ENABLETAB      (ETDT_ENABLE | ETDT_USETABTEXTURE)

#define STAP_ALLOW_NONCLIENT    (1 << 0)
#define STAP_ALLOW_CONTROLS     (1 << 1)
#define STAP_ALLOW_WEBCONTENT   (1 << 2)

#define SZ_THDOCPROP_DISPLAYNAME       L"DisplayName"
#define SZ_THDOCPROP_CANONICALNAME     L"ThemeName"
#define SZ_THDOCPROP_TOOLTIP           L"ToolTip"
#define SZ_THDOCPROP_AUTHOR            L"author"

#define DTBG_CLIPRECT        0x00000001   // rcClip has been specified
#define DTBG_DRAWSOLID       0x00000002   // draw transparent/alpha images as solid
#define DTBG_OMITBORDER      0x00000004   // don't draw border of part
#define DTBG_OMITCONTENT     0x00000008   // don't draw content area of part
#define DTBG_COMPUTINGREGION 0x00000010   // TRUE if calling to compute region
#define DTBG_MIRRORDC        0x00000020   // assume the hdc is mirrorred and

struct DTBGOPTS
{
    DWORD dwSize;           // size of the struct
    DWORD dwFlags;          // which options have been specified
    RECT rcClip;            // clipping rectangle
};
typedef DTBGOPTS* PDTBGOPTS;

struct TMPROPINFO
{
    LPCWSTR pszName;
    SHORT sEnumVal;
    BYTE bPrimVal;
};

struct TMSCHEMAINFO
{
    DWORD dwSize;
    int iSchemaDefVersion;
    int iThemeMgrVersion;
    int iPropCount;
    const TMPROPINFO* pPropTable;
};

#endif
#pragma pack(pop)

#ifndef WM_THEMECHANGED
#define WM_THEMECHANGED     0x031A
#endif

bool IsThemed();

HTHEME XPOpenThemeData(HWND hwnd, LPCWSTR pszClassList);
HRESULT XPCloseThemeData(HTHEME hTheme);
HRESULT XPDrawThemeBackground(HTHEME hTheme, HDC hdc,
    int iPartId, int iStateId, const RECT* pRect, const RECT* pClipRect);
HRESULT XPDrawThemeText(HTHEME hTheme, HDC hdc, int iPartId,
    int iStateId, LPCWSTR pszText, int iCharCount, DWORD dwTextFlags,
    DWORD dwTextFlags2, const RECT* pRect);
HRESULT XPGetThemeBackgroundContentRect(HTHEME hTheme, HDC hdc,
    int iPartId, int iStateId,  const RECT* pBoundingRect,
    OUT RECT* pContentRect);
HRESULT XPGetThemeBackgroundExtent(HTHEME hTheme, HDC hdc,
    int iPartId, int iStateId, const RECT* pContentRect,
    OUT RECT* pExtentRect);
HRESULT XPGetThemePartSize(HTHEME hTheme, HDC hdc, int iPartId,
    int iStateId, RECT* prc, THEMESIZE eSize, OUT SIZE* psz);
HRESULT XPGetThemeTextExtent(HTHEME hTheme, HDC hdc,
    int iPartId, int iStateId, LPCWSTR pszText, int iCharCount,
    DWORD dwTextFlags, const RECT* pBoundingRect,
    OUT RECT* pExtentRect);
HRESULT XPGetThemeTextMetrics(HTHEME hTheme, HDC hdc,
    int iPartId, int iStateId, OUT TEXTMETRIC* ptm);
HRESULT XPGetThemeBackgroundRegion(HTHEME hTheme, HDC hdc,
    int iPartId, int iStateId, const RECT* pRect, OUT HRGN* pRegion);
HRESULT XPHitTestThemeBackground(HTHEME hTheme, HDC hdc, int iPartId,
    int iStateId, DWORD dwOptions, const RECT* pRect, HRGN hrgn,
    POINT ptTest, OUT WORD* pwHitTestCode);
HRESULT XPDrawThemeEdge(HTHEME hTheme, HDC hdc, int iPartId,
	int iStateId, const RECT* pDestRect, UINT uEdge, UINT uFlags, OUT RECT* pContentRect);
HRESULT XPDrawThemeIcon(HTHEME hTheme, HDC hdc, int iPartId,
    int iStateId, const RECT* pRect, HIMAGELIST himl, int iImageIndex);
BOOL XPIsThemePartDefined(HTHEME hTheme, int iPartId,
    int iStateId);
BOOL XPIsThemeBackgroundPartiallyTransparent(HTHEME hTheme,
    int iPartId, int iStateId);
HRESULT XPGetThemeColor(HTHEME hTheme, int iPartId,
    int iStateId, int iPropId, OUT COLORREF* pColor);
HRESULT XPGetThemeMetric(HTHEME hTheme, HDC hdc, int iPartId,
    int iStateId, int iPropId, OUT int* piVal);
HRESULT XPGetThemeString(HTHEME hTheme, int iPartId,
    int iStateId, int iPropId, OUT LPWSTR pszBuff, int cchMaxBuffChars);
HRESULT XPGetThemeBool(HTHEME hTheme, int iPartId,
    int iStateId, int iPropId, OUT BOOL* pfVal);
HRESULT XPGetThemeInt(HTHEME hTheme, int iPartId,
    int iStateId, int iPropId, OUT int* piVal);
HRESULT XPGetThemeEnumValue(HTHEME hTheme, int iPartId,
    int iStateId, int iPropId, OUT int* piVal);
HRESULT XPGetThemePosition(HTHEME hTheme, int iPartId,
    int iStateId, int iPropId, OUT POINT* pPoint);
HRESULT XPGetThemeFont(HTHEME hTheme, HDC hdc, int iPartId,
    int iStateId, int iPropId, OUT LOGFONTW* pFont);
HRESULT XPGetThemeRect(HTHEME hTheme, int iPartId,
    int iStateId, int iPropId, OUT RECT* pRect);
HRESULT XPGetThemeMargins(HTHEME hTheme, HDC hdc, int iPartId,
    int iStateId, int iPropId, RECT* prc, OUT MARGINS* pMargins);
HRESULT XPGetThemeIntList(HTHEME hTheme, int iPartId,
    int iStateId, int iPropId, OUT INTLIST* pIntList);
HRESULT XPGetThemePropertyOrigin(HTHEME hTheme, int iPartId,
    int iStateId, int iPropId, OUT PROPERTYORIGIN* pOrigin);
HRESULT XPSetWindowTheme(HWND hwnd, LPCWSTR pszSubAppName,
    LPCWSTR pszSubIdList);
HRESULT XPGetThemeFilename(HTHEME hTheme, int iPartId,
    int iStateId, int iPropId, OUT LPWSTR pszThemeFileName, int cchMaxBuffChars);
COLORREF XPGetThemeSysColor(HTHEME hTheme, int iColorId);
HBRUSH XPGetThemeSysColorBrush(HTHEME hTheme, int iColorId);
BOOL XPGetThemeSysBool(HTHEME hTheme, int iBoolId);
int XPGetThemeSysSize(HTHEME hTheme, int iSizeId);
HRESULT XPGetThemeSysFont(HTHEME hTheme, int iFontId, OUT LOGFONTW* plf);
HRESULT XPGetThemeSysString(HTHEME hTheme, int iStringId,
    OUT LPWSTR pszStringBuff, int cchMaxStringChars);
HRESULT XPGetThemeSysInt(HTHEME hTheme, int iIntId, int* piValue);
BOOL XPIsThemeActive();
BOOL XPIsAppThemed();
HTHEME XPGetWindowTheme(HWND hwnd);
HRESULT XPEnableThemeDialogTexture(HWND hwnd, DWORD dwFlags);
BOOL XPIsThemeDialogTextureEnabled(HWND hwnd);
DWORD XPGetThemeAppProperties();
void XPSetThemeAppProperties(DWORD dwFlags);
HRESULT XPGetCurrentThemeName(
    OUT LPWSTR pszThemeFileName, int cchMaxNameChars,
    OUT LPWSTR pszColorBuff, int cchMaxColorChars,
    OUT LPWSTR pszSizeBuff, int cchMaxSizeChars);
HRESULT XPGetThemeDocumentationProperty(LPCWSTR pszThemeName,
    LPCWSTR pszPropertyName, OUT LPWSTR pszValueBuff, int cchMaxValChars);
HRESULT XPDrawThemeParentBackground(HWND hwnd, HDC hdc, RECT* prc);
HRESULT XPEnableTheming(BOOL fEnable);
HRESULT XPDrawThemeBackgroundEx(HTHEME hTheme, HDC hdc,
    int iPartId, int iStateId, const RECT* pRect, const DTBGOPTS* pOptions);

/////////////////////////////////////////////////////////////////////
// Theme Manager properties, parts, states, etc

