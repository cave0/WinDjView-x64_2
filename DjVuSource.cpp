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
#include "DjVuSource.h"
#include "XMLParser.h"
#include "libdjvu/DjVmDir.h"
//#include "DocPropertiesDlg.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

bool DjVuSource::CheckMatchTitle(CString strNewTitle, int& nPage, int nLastPage)
{
	for (; nPage <= nLastPage; ++nPage)
	{
		if (strNewTitle == m_pages[nPage].info.strPageTitle)
			return true;
	}
	return false;
}

void DjVuSource::UpdatePageTitle(PageInfo& pageInfo, int nPage)
{
	int nType = GetDjVuDoc()->get_doc_type();
	GP<DjVmDir> pDir = NULL;

	if (nType == DjVuDocument::BUNDLED || nType == DjVuDocument::INDIRECT)
		pDir = GetDjVuDoc()->get_djvm_dir();

	if (pDir != NULL)
	{
		GP<DjVmDir::File> pFile = pDir->page_to_file(nPage);				
		if (pFile)
		{
			GUTF8String id = pFile->get_load_name();
			GUTF8String title = pFile->get_title();
			if (id != title)
				pageInfo.strPageTitle = MakeCString(title);
		}
	}
}

int DjVuSource::GetUrlToPagenum(GUTF8String url)
{
	if (url[0] == '#')
		url = url.substr(1, -1);
	else 
		return -2;

	CString strTitle = MakeCString(url);
	strTitle.Trim();
	int nPage = 0;
	if (CheckMatchTitle(strTitle, nPage, m_nPageCount - 1))
	{
		return nPage;
	}
	else if (strTitle[0] == '+' || strTitle[0] == '-')
	{
		return -1;
	}
	else if (url.is_int() && _stscanf(strTitle, _T("%d"), &nPage) == 1)
	{
		return nPage - 1;
	}
	else
	{
		return GetPageFromId(url);
	}
}

// PageInfo

void PageInfo::Update(GP<DjVuImage> pImage)
{
	if (pImage == NULL)
		return;

	if (!bDecoded)
	{
		nInitialRotate = GetTotalRotate(pImage, 0);
		szPage = CSize(pImage->get_width(), pImage->get_height());
		if (nInitialRotate % 2 != 0)
			swap(szPage.cx, szPage.cy);

		nDPI = pImage->get_dpi();
		if (szPage.cx <= 0 || szPage.cy <= 0)
		{
			szPage.cx = 100;
			szPage.cy = 100;
			nDPI = 100;
		}

		bHasText = !!(pImage->get_djvu_file()->text != NULL);
		bDecoded = true;
	}

	try
	{
		if (bHasText && !bTextDecoded)
			DecodeText(pImage->get_text());
	}
	catch (GException&)
	{
	}

	try
	{
		if (!bAnnoDecoded)
			DecodeAnno(pImage->get_anno());
	}
	catch (GException&)
	{
	}
}

void PageInfo::Update(const PageInfo& info)
{
	if (/*!bDecoded && */info.bDecoded)
	{
		nInitialRotate = info.nInitialRotate;
		szPage = info.szPage;
		nDPI = info.nDPI;
		bHasText = info.bHasText;
		bDecoded = true;
		strPageTitle = info.strPageTitle;
	}

	if (!bTextDecoded && info.bTextDecoded)
	{
		pText = info.pText;
		bTextDecoded = true;
		bTextCropped = info.bTextCropped;
	}

	if (!bAnnoDecoded && info.bAnnoDecoded)
	{
		pAnt = info.pAnt;
		anno = info.anno;
		bAnnoDecoded = true;
		bAnnoCropped = info.bAnnoCropped;
	}
}

static const TCHAR pszTagCropPage[] = _T("crop-page");

void PageInfo::CropPage(int nRotate)
{
	CRect rect = GetCropRect(nInitialRotate);
	int nHoriz = rect.left + rect.right;
	int nVert = rect.top + rect.bottom;
	if (szPage.cx - nHoriz > 4 && szPage.cy - nVert > 4)
	{
		szPage.cx -= nHoriz;
		szPage.cy -= nVert;
	}
	CropSource(nRotate);
}

void PageInfo::DecodeAnno(GP<ByteStream> pAnnoStream)
{
	if (pAnnoStream == NULL)
		return;

	pAnnoStream->seek(0);
	GP<DjVuAnno> pDjVuAnno = DjVuAnno::create();
	pDjVuAnno->decode(pAnnoStream);
	pAnt = pDjVuAnno->ant;

	if (pAnt != NULL)
	{
		for (GPosition pos = pAnt->map_areas; pos; ++pos)
		{
			GP<GMapArea> pArea = pAnt->map_areas[pos];
			anno.push_back(Annotation());
			anno.back().Init(pArea, szPage, nInitialRotate);
		}
	}

	bAnnoDecoded = true;
}

void PageInfo::DecodeText(GP<ByteStream> pTextStream)
{
	if (pTextStream == NULL)
		return;

	pTextStream->seek(0);
	GP<DjVuText> pDjVuText = DjVuText::create();
	pDjVuText->decode(pTextStream);
	pText = pDjVuText->txt;
	bTextDecoded = true;
}


// Annotation

static const TCHAR pszTagAnnotation[] = _T("annotation");
static const TCHAR pszAttrBorder[] = _T("border");
static const TCHAR pszAttrBorderColor[] = _T("border-color");
static const TCHAR pszAttrBorderHideInactive[] = _T("border-hide-inactive");
static const TCHAR pszAttrFill[] = _T("fill");
static const TCHAR pszAttrFillColor[] = _T("fill-color");
static const TCHAR pszAttrFillTransparency[] = _T("fill-transparency");
static const TCHAR pszAttrFillHideInactive[] = _T("fill-hide-inactive");
static const TCHAR pszAttrComment[] = _T("comment");
static const TCHAR pszAttrCommentAlwaysShow[] = _T("always-show-comment");
static const TCHAR pszAttrURL[] = _T("url");

static const TCHAR pszTagRect[] = _T("rect");
static const TCHAR pszAttrLeft[] = _T("left");
static const TCHAR pszAttrTop[] = _T("top");
static const TCHAR pszAttrRight[] = _T("right");
static const TCHAR pszAttrBottom[] = _T("bottom");

void Annotation::UpdateBounds()
{
	if (!rects.empty())
	{
		rectBounds = rects[0];
		for (size_t i = 1; i < rects.size(); ++i)
			rectBounds.recthull(GRect(rectBounds), rects[i]);
	}
	else if (!points.empty())
	{
		rectBounds = GRect(points[0].first, points[0].second, 1, 1);
		for (size_t i = 1; i < points.size(); ++i)
			rectBounds.recthull(GRect(rectBounds), GRect(points[i].first, points[i].second, 1, 1));
	}
}

GUTF8String Annotation::GetXML() const
{
	CString strRects;
	for (size_t i = 0; i < rects.size(); ++i)
	{
		strRects += FormatString(_T("<%s %s=\"%d\" %s=\"%d\" %s=\"%d\" %s=\"%d\" />\n"),
				pszTagRect, pszAttrLeft, rects[i].xmin, pszAttrTop, rects[i].ymin,
				pszAttrRight, rects[i].xmax, pszAttrBottom, rects[i].ymax);
	}

	CString strBorder;
	strBorder.Format(_T("%s=\"%d\" %s=\"#%06x\" %s=\"%d\""),
			pszAttrBorder, nBorderType, pszAttrBorderColor, crBorder,
			pszAttrBorderHideInactive, static_cast<int>(bHideInactiveBorder));

	CString strFill;
	strFill.Format(_T("%s=\"%d\" %s=\"#%06x\" %s=\"%d%%\" %s=\"%d\""),
			pszAttrFill, nFillType, pszAttrFillColor, crFill,
			pszAttrFillTransparency, static_cast<int>(fTransparency*100 + 0.5),
			pszAttrFillHideInactive, static_cast<int>(bHideInactiveFill));

	CString strBegin;
	strBegin.Format(_T("<%s %s %s %s=\"%d\" %s=\""),
			pszTagAnnotation, strBorder, strFill, pszAttrCommentAlwaysShow,
			static_cast<int>(bAlwaysShowComment), pszAttrComment);

	CString strEnd;
	strEnd.Format(_T(">\n%s</%s>\n"), strRects, pszTagAnnotation);

	GUTF8String strURLAttr;
	if (strURL.length() > 0)
	{
		strURLAttr = MakeUTF8String(CString(pszAttrURL)) + "=\"" + strURL.toEscaped() + "\" ";
	}

	return MakeUTF8String(strBegin) + strComment.toEscaped() + "\" "
		+ strURLAttr + MakeUTF8String(strEnd);
}

void Annotation::Load(const XMLNode& node)
{
	if (MakeCString(node.tagName) != pszTagAnnotation)
		return;

	int borderType;
	if (node.GetIntAttribute(pszAttrBorder, borderType))
	{
		if (borderType >= BorderNone && borderType <= BorderXOR)
			nBorderType = borderType;
	}

	COLORREF color;
	if (node.GetColorAttribute(pszAttrBorderColor, color))
		crBorder = color;

	int nHideBorder;
	if (node.GetIntAttribute(pszAttrBorderHideInactive, nHideBorder))
		bHideInactiveBorder = !!nHideBorder;

	int fillType;
	if (node.GetIntAttribute(pszAttrFill, fillType))
	{
		if (fillType >= FillNone && fillType <= FillXOR)
			nFillType = fillType;
	}

	if (node.GetColorAttribute(pszAttrFillColor, color))
		crFill = color;

	int nPercent;
	if (node.GetIntAttribute(pszAttrFillTransparency, nPercent))
	{
		if (nPercent >= 0 && nPercent <= 100)
			fTransparency = nPercent / 100.0;
	}

	int nHideFill;
	if (node.GetIntAttribute(pszAttrFillHideInactive, nHideFill))
		bHideInactiveFill = !!nHideFill;

	wstring str;
	if (node.GetAttribute(pszAttrComment, str))
		strComment = MakeUTF8String(str);

	int nShowComment;
	if (node.GetIntAttribute(pszAttrCommentAlwaysShow, nShowComment))
		bAlwaysShowComment = !!nShowComment;

	if (node.GetAttribute(pszAttrURL, str))
		strURL = MakeUTF8String(str);

	rects.clear();
	list<XMLNode>::const_iterator it;
	for (it = node.childElements.begin(); it != node.childElements.end(); ++it)
	{
		const XMLNode& child = *it;
		if (MakeCString(child.tagName) == pszTagRect)
		{
			GRect rect;
			if (!child.GetIntAttribute(pszAttrLeft, rect.xmin)
					|| !child.GetIntAttribute(pszAttrTop, rect.ymin)
					|| !child.GetIntAttribute(pszAttrRight, rect.xmax)
					|| !child.GetIntAttribute(pszAttrBottom, rect.ymax))
				continue;

			rects.push_back(rect);
		}
	}

	UpdateBounds();
}

static void Rotate(const CSize& szPage, int nRotate, GRect& rect)
{
	GRect input(0, 0, szPage.cx, szPage.cy);
	GRect output(0, 0, szPage.cx, szPage.cy);

	if ((nRotate % 2) != 0)
		swap(input.xmax, input.ymax);

	GRectMapper mapper;
	mapper.clear();
	mapper.set_input(input);
	mapper.set_output(output);
	mapper.rotate(nRotate);
	mapper.unmap(rect);
}

void Annotation::Init(GP<GMapArea> pArea, const CSize& szPage, int nRotate)
{
	sourceArea = pArea;

	if (pArea->border_type == GMapArea::NO_BORDER)
		nBorderType = BorderNone;
	else if (pArea->border_type == GMapArea::XOR_BORDER)
		nBorderType = BorderXOR;
	else if (pArea->border_type == GMapArea::SHADOW_IN_BORDER)
		nBorderType = BorderShadowIn;
	else if (pArea->border_type == GMapArea::SHADOW_OUT_BORDER)
		nBorderType = BorderShadowOut;
	else if (pArea->border_type == GMapArea::SHADOW_EIN_BORDER)
		nBorderType = BorderEtchedIn;
	else if (pArea->border_type == GMapArea::SHADOW_EOUT_BORDER)
		nBorderType = BorderEtchedOut;
	else
		nBorderType = BorderSolid;

	if (nBorderType == BorderSolid)
	{
		DWORD dwColor = pArea->border_color;
		crBorder = RGB(GetBValue(dwColor), GetGValue(dwColor), GetRValue(dwColor));
	}

	if (pArea->get_shape_type() != GMapArea::RECT && nBorderType > BorderXOR)
		nBorderType = BorderXOR;

	nBorderWidth = max(2, min(32, pArea->border_width));

	bHideInactiveBorder = !pArea->border_always_visible;

	if (pArea->hilite_color == 0xffffffff)
		nFillType = FillNone;
	else if (pArea->hilite_color == 0xff000000)
		nFillType = FillXOR;
	else
		nFillType = FillSolid;

	if (nFillType == FillSolid)
	{
		DWORD dwColor = pArea->hilite_color;
		crFill = RGB(GetBValue(dwColor), GetGValue(dwColor), GetRValue(dwColor));
	}

	fTransparency = 1.0 - max(0, min(100, pArea->opacity)) / 100.0;

	if (pArea->is_text)
	{
		bAlwaysShowComment = true;
		DWORD dwColor = pArea->foreground_color;
		crForeground = RGB(GetBValue(dwColor), GetGValue(dwColor), GetRValue(dwColor));
	}

	strComment = pArea->comment;
	strURL = pArea->url;

	if (pArea->get_shape_type() == GMapArea::LINE || pArea->get_shape_type() == GMapArea::POLY)
	{
		GMapPoly* pPoly = (GMapPoly*)(GMapArea*) pArea;
		for (int i = 0; i < pPoly->get_points_num(); ++i)
		{
			GRect rect(pPoly->get_x(i), pPoly->get_y(i), 1, 1);
			if (nRotate != 0)
				Rotate(szPage, nRotate, rect);
			points.push_back(make_pair(rect.xmin, rect.ymin));
		}

		if (pArea->is_line)
		{
			bIsLine = true;
			bHasArrow = pArea->has_arrow;
			nLineWidth = max(1, min(32, pArea->line_width));

			DWORD dwColor = pArea->foreground_color;
			crForeground = RGB(GetBValue(dwColor), GetGValue(dwColor), GetRValue(dwColor));
		}
	}
	else
	{
		if (pArea->get_shape_type() == GMapArea::OVAL)
			bOvalShape = true;

		rects.push_back(pArea->get_bound_rect());
		if (nRotate != 0)
			Rotate(szPage, nRotate, rects.back());
	}

	UpdateBounds();
}

void Annotation::Fix()
{
	if (nBorderType < BorderNone || nBorderType > BorderXOR)
		nBorderType = BorderNone;
	if (nFillType < FillNone || nFillType > FillXOR)
		nFillType = FillSolid;
}


// PageSettings

static const TCHAR pszTagPageSettings[] = _T("page-settings");
static const TCHAR pszAttrNumber[] = _T("number");

GUTF8String PageSettings::GetXML() const
{
	GUTF8String result;

	list<Annotation>::const_iterator it;
	for (it = anno.begin(); it != anno.end(); ++it)
	{
		const Annotation& annotation = *it;
		result += annotation.GetXML();
	}

	return result;
}

void PageSettings::Load(const XMLNode& node)
{
	if (MakeCString(node.tagName) != pszTagPageSettings)
		return;
	list<XMLNode>::const_iterator it;
	for (it = node.childElements.begin(); it != node.childElements.end(); ++it)
	{
		const XMLNode& child = *it;
		if (MakeCString(child.tagName) == pszTagAnnotation)
		{
			anno.push_back(Annotation());
			Annotation& annotation = anno.back();
			annotation.Load(child);

			if (annotation.rects.empty())
				anno.pop_back();
		}
	}
}


// Bookmark

static const TCHAR pszTagBookmark[] = _T("bookmark");
static const TCHAR pszAttrTitle[] = _T("title");
static const TCHAR pszAttrType[] = _T("type");
static const TCHAR pszAttrPage[] = _T("page");
static const TCHAR pszAttrOffsetX[] = _T("offset-x");
static const TCHAR pszAttrOffsetY[] = _T("offset-y");
static const TCHAR pszAttrTextStart[] = _T("text-start");
static const TCHAR pszAttrTextLength[] = _T("text-length");
static const TCHAR pszAttrMargin[] = _T("margin");

Bookmark& Bookmark::operator=(const Bookmark& bm)
{
	if (this != &bm)
	{
		children = bm.children;
		strURL = bm.strURL;
		strTitle = bm.strTitle;
		pParent = bm.pParent;
		nLinkType = bm.nLinkType;
		nPage = bm.nPage;
		ptOffset = bm.ptOffset;
		textStart = bm.textStart;
		textLen = bm.textLen;
		bMargin = bm.bMargin;
		bZoom = bm.bZoom;
		nZoomType = bm.nZoomType;
		fZoom = bm.fZoom;
		nPrevZoomType = bm.nPrevZoomType;
		fPrevZoom = bm.fPrevZoom;
	}

	return *this;
}

void Bookmark::swap(Bookmark& bm)
{
	if (this != &bm)
	{
		children.swap(bm.children);
		std::swap(strURL, bm.strURL);
		std::swap(strTitle, bm.strTitle);
		std::swap(pParent, bm.pParent);
		std::swap(nLinkType, bm.nLinkType);
		std::swap(nPage, bm.nPage);
		std::swap(ptOffset, bm.ptOffset);
		std::swap(textStart, bm.textStart);
		std::swap(textLen, bm.textLen);
		std::swap(bMargin, bm.bMargin);
		std::swap(bZoom, bm.bZoom);
		std::swap(nZoomType, bm.nZoomType);
		std::swap(fZoom, bm.fZoom);
		std::swap(nPrevZoomType, bm.nPrevZoomType);
		std::swap(fPrevZoom, bm.fPrevZoom);
	}
}

GUTF8String Bookmark::GetXML() const
{
	GUTF8String result;
	result += MakeUTF8String(FormatString(_T("<%s %s=\""), pszTagBookmark, pszAttrTitle));
	result += strTitle.toEscaped();
	result += MakeUTF8String(FormatString(_T("\" %s=\"%d\" "), pszAttrType, nLinkType));

	if (nLinkType == Page || nLinkType == View || nLinkType == Text)
	{
		result += MakeUTF8String(FormatString(_T("%s=\"%d\" "), pszAttrPage, nPage));
		if (nLinkType == View)
		{
			result += MakeUTF8String(FormatString(_T("%s=\"%d\" "), pszAttrOffsetX, ptOffset.x));
			result += MakeUTF8String(FormatString(_T("%s=\"%d\" "), pszAttrOffsetY, ptOffset.y));
			result += MakeUTF8String(FormatString(_T("%s=\"%d\" "), pszAttrMargin, (int) bMargin));
		}
		else if (nLinkType == Text)
		{
			result += MakeUTF8String(FormatString(_T("%s=\"%d\" "), pszAttrTextStart, textStart));
			result += MakeUTF8String(FormatString(_T("%s=\"%d\" "), pszAttrTextLength, textLen));
		}
	}
	else if (nLinkType == URL)
	{
		result += MakeUTF8String(FormatString(_T("%s=\""), pszAttrURL));
		result += strURL.toEscaped();
		result += "\" ";
	}

	if (!children.empty())
	{
		result += ">\n";
		for (list<Bookmark>::iterator it = children.begin(); it != children.end(); ++it)
			result += (*it).GetXML();
		result += MakeUTF8String(FormatString(_T("</%s>\n"), pszTagBookmark));
	}
	else
		result += "/>\n";

	return result;
}

void Bookmark::Load(const XMLNode& node)
{
	if (MakeCString(node.tagName) != pszTagBookmark)
		return;

	wstring str;
	if (node.GetAttribute(pszAttrTitle, str))
		strTitle = MakeUTF8String(str);

	node.GetIntAttribute(pszAttrType, nLinkType);

	if (nLinkType == URL)
	{
		if (node.GetAttribute(pszAttrURL, str))
			strURL = MakeUTF8String(str);
	}
	else if (nLinkType == Page || nLinkType == View || nLinkType == Text)
	{
		node.GetIntAttribute(pszAttrPage, nPage);
		if (nLinkType == View)
		{
			node.GetLongAttribute(pszAttrOffsetX, ptOffset.x);
			node.GetLongAttribute(pszAttrOffsetY, ptOffset.y);

			int nMargin;
			if (node.GetIntAttribute(pszAttrMargin, nMargin))
				bMargin = (nMargin != 0);
		}
		else if (nLinkType == Text)
		{
			node.GetIntAttribute(pszAttrTextStart, textStart);
			node.GetIntAttribute(pszAttrTextLength, textLen);
		}

	}

	list<XMLNode>::const_iterator it;
	for (it = node.childElements.begin(); it != node.childElements.end(); ++it)
	{
		const XMLNode& child = *it;
		if (MakeCString(child.tagName) == pszTagBookmark)
		{
			children.push_back(Bookmark());
			Bookmark& bookmark = children.back();
			bookmark.Load(child);
		}
	}
}


// DocSettings

static const TCHAR pszTagSettings[] = _T("settings");
static const TCHAR pszAttrStartPage[] = _T("start-page");
static const TCHAR pszAttrZoomType[] = _T("zoom-type");
static const TCHAR pszAttrZoom[] = _T("zoom");
static const TCHAR pszAttrLayout[] = _T("layout");
static const TCHAR pszAttrFirstPage[] = _T("first-page");
static const TCHAR pszAttrRightToLeft[] = _T("right-to-left");
static const TCHAR pszAttrDisplayMode[] = _T("display-mode");
static const TCHAR pszAttrRotate[] = _T("rotate");
static const TCHAR pszAttrOpenSidebarTab[] = _T("sidebar-tab");
static const TCHAR pszAttrLastKnownLocation[] = _T("last-known-location");
static const TCHAR pszTagBookmarks[] = _T("bookmarks");
static const TCHAR pszTagContent[] = _T("content");

DocSettings::DocSettings()
	: nPage(-1), ptOffset(0, 0), nZoomType(-10), fZoom(100.0), nLayout(-1),
	  bFirstPageAlone(false), bRightToLeft(false), nDisplayMode(-1), nRotate(0),
	  nOpenSidebarTab(-1)
{
}

PageSettings DocSettings::GetUncroppedAnno(int nPage, const PageSettings pSettings, list<pair <int, CRect> >::const_iterator& cit) const
{
	PageSettings pReturn = pSettings;
	if (pReturn.anno.size() > 0)
	{
		int nRectPage = (*cit).first;
		CRect rect = (*cit).second;
		while (nRectPage < nPage && cit != cropPages.end())
		{
			nRectPage = (*cit).first;
			rect = (*cit).second;
			if (nRectPage <= nPage)
				++cit;
		}
		if (nRectPage == nPage)
		{
			for (list<Annotation>::iterator lit = pReturn.anno.begin(); lit != pReturn.anno.end(); ++lit)
			{
				Annotation* ant = &(*lit);
				for (size_t i = 0; i < ant->rects.size(); ++i)
				{
					ant->rects[i].translate(rect.left, rect.bottom);
				}
			}
		}
	}
	return pReturn;
}

GUTF8String DocSettings::GetXML(bool skip_view_settings) const
{
	GUTF8String result;

	CString strHead;
	if (skip_view_settings)
	{
		strHead.Format(_T("<%s>\n"), pszTagContent);
	}
	else
	{
		wstring strAttrLastKnownLocation;
		MakeWString(strLastKnownLocation, strAttrLastKnownLocation);
		strHead.Format(_T("<%s %s=\"%d\" %s=\"%d\" %s=\"%d\" %s=\"%d\" %s=\"%.2lf%%\"")
				_T(" %s=\"%d\" %s=\"%d\" %s=\"%d\" %s=\"%d\" %s=\"%d\" %s=\"%d\" %s=\"%s\">\n"),
				pszTagSettings, pszAttrStartPage, nPage, pszAttrOffsetX, ptOffset.x,
				pszAttrOffsetY, ptOffset.y, pszAttrZoomType, nZoomType, pszAttrZoom, fZoom,
				pszAttrLayout, nLayout, pszAttrFirstPage, static_cast<int>(bFirstPageAlone),
				pszAttrRightToLeft, static_cast<int>(bRightToLeft),
				pszAttrDisplayMode, nDisplayMode, pszAttrRotate, nRotate,
				pszAttrOpenSidebarTab, nOpenSidebarTab, pszAttrLastKnownLocation, strAttrLastKnownLocation.c_str());
	}

	result += MakeUTF8String(strHead);

	if (!bookmarks.empty())
	{
		result += MakeUTF8String(FormatString(_T("<%s>\n"), pszTagBookmarks));
		for (list<Bookmark>::const_iterator it = bookmarks.begin(); it != bookmarks.end(); ++it)
			result += (*it).GetXML();
		result += MakeUTF8String(FormatString(_T("</%s>\n"), pszTagBookmarks));
	}

	map<int, PageSettings>::const_iterator it;
	list<pair <int, CRect> >::const_iterator cit = cropPages.begin();
	for (it = pageSettings.begin(); it != pageSettings.end(); ++it)
	{
		int nPage = (*it).first + 1;
		const PageSettings& settings = (*it).second;

		GUTF8String strPageXML;
		if (settings.bCropped && cit != cropPages.end())
			strPageXML = GetUncroppedAnno(nPage - 1, settings, cit).GetXML();
		else
			strPageXML = settings.GetXML();
		if (strPageXML.length() != 0)
		{
			result += MakeUTF8String(FormatString(_T("<%s %s=\"%d\" >\n"),
					pszTagPageSettings, pszAttrNumber, nPage));
			result += strPageXML;
			result += MakeUTF8String(FormatString(_T("</%s>\n"), pszTagPageSettings));
		}
	}
	if (!cropPages.empty())
	{
		for (cit = cropPages.begin(); cit != cropPages.end(); ++cit)
		{
			int nPage = (*cit).first + 1;
			const CRect rect = (*cit).second;
			result += MakeUTF8String(FormatString(_T("<%s %s=\"%d\" >\n"),
				pszTagCropPage, pszAttrNumber, nPage));
			result += MakeUTF8String(FormatString(_T("<%s %s=\"%d\" %s=\"%d\" %s=\"%d\" %s=\"%d\" />\n"),
				pszTagRect, pszAttrLeft, rect.left, pszAttrTop, rect.top,
				pszAttrRight, rect.right, pszAttrBottom, rect.bottom));
			result += MakeUTF8String(FormatString(_T("</%s>\n"), pszTagCropPage));
		}
	}

	if (skip_view_settings)
		result += MakeUTF8String(FormatString(_T("</%s>\n"), pszTagContent));
	else
		result += MakeUTF8String(FormatString(_T("</%s>\n"), pszTagSettings));

	return result;
}

void DocSettings::Load(const XMLNode& node)
{
	CString strTagName = MakeCString(node.tagName);
	if (strTagName != pszTagSettings && strTagName != pszTagContent)
		return;

	if (strTagName == pszTagSettings)
	{
		node.GetIntAttribute(pszAttrStartPage, nPage);
		node.GetLongAttribute(pszAttrOffsetX, ptOffset.x);
		node.GetLongAttribute(pszAttrOffsetY, ptOffset.y);
		node.GetIntAttribute(pszAttrZoomType, nZoomType);
		node.GetDoubleAttribute(pszAttrZoom, fZoom);
		node.GetIntAttribute(pszAttrLayout, nLayout);
		node.GetIntAttribute(pszAttrDisplayMode, nDisplayMode);
		node.GetIntAttribute(pszAttrRotate, nRotate);
		node.GetIntAttribute(pszAttrOpenSidebarTab, nOpenSidebarTab);
		node.GetAttribute(pszAttrLastKnownLocation, strAttrLastKnownLocation);

		int nFirstPage;
		if (node.GetIntAttribute(pszAttrFirstPage, nFirstPage))
			bFirstPageAlone = (nFirstPage != 0);

		int nRightToLeft;
		if (node.GetIntAttribute(pszAttrRightToLeft, nRightToLeft))
			bRightToLeft = (nRightToLeft != 0);
	}

	pageSettings.clear();
	bookmarks.clear();

	list<XMLNode>::const_iterator it;
	for (it = node.childElements.begin(); it != node.childElements.end(); ++it)
	{
		const XMLNode& child = *it;
		if (MakeCString(child.tagName) == pszTagPageSettings)
		{
			int pageNo;
			if (!child.GetIntAttribute(pszAttrNumber, pageNo))
				continue;

			PageSettings& data = pageSettings[pageNo - 1];
			data.Load(child);
			data.bCropped = false;
		}
		else if (MakeCString(child.tagName) == pszTagBookmarks)
		{
			list<XMLNode>::const_iterator it;
			for (it = child.childElements.begin(); it != child.childElements.end(); ++it)
			{
				const XMLNode& bmNode = *it;
				if (MakeCString(bmNode.tagName) == pszTagBookmark)
				{
					bookmarks.push_back(Bookmark());
					Bookmark& bookmark = bookmarks.back();
					bookmark.Load(bmNode);
				}
			}
		}
		else if (MakeCString(child.tagName) == pszTagCropPage)
		{
			int pageNo;
			if (!child.GetIntAttribute(pszAttrNumber, pageNo))
				continue;
			for (list<XMLNode>::const_iterator cit = child.childElements.begin(); cit != child.childElements.end(); ++cit)
			{
				const XMLNode& child_rect = *cit;
				if (MakeCString(child_rect.tagName) == pszTagRect)
				{
					GRect rect;
					if (!child_rect.GetIntAttribute(pszAttrLeft, rect.xmin)
						|| !child_rect.GetIntAttribute(pszAttrTop, rect.ymin)
						|| !child_rect.GetIntAttribute(pszAttrRight, rect.xmax)
						|| !child_rect.GetIntAttribute(pszAttrBottom, rect.ymax))
						continue;
					cropPages.push_back(make_pair(pageNo - 1, CRect(rect.xmin, rect.ymin, rect.xmax, rect.ymax)));
				}
			}
		}
	}
}

Annotation* DocSettings::AddAnnotation(const Annotation& anno, int nPage)
{
	PageSettings& settings = pageSettings[nPage];
	settings.anno.push_back(anno);

	Annotation* pNewAnno = &(settings.anno.back());
	UpdateObservers(AnnotationMsg(ANNOTATION_ADDED, pNewAnno, nPage));

	return pNewAnno;
}

bool DocSettings::DeleteBookmark(const Bookmark* pBookmark)
{
	list<Bookmark>& bmList = (pBookmark->pParent != NULL ?
		pBookmark->pParent->children : bookmarks);

	for (list<Bookmark>::iterator it = bmList.begin(); it != bmList.end(); ++it)
	{
		Bookmark* pCurBookmark = &(*it);
		if (pCurBookmark == pBookmark)
		{
			list<Bookmark> temp;
			temp.splice(temp.end(), bmList, it);
			ASSERT(&(temp.front()) == pCurBookmark);

			UpdateObservers(BookmarkMsg(BOOKMARK_DELETED, pCurBookmark));
			return true;
		}
	}

	return false;
}

bool DocSettings::DeleteAnnotation(const Annotation* pAnno, int nPage)
{
	PageSettings& settings = pageSettings[nPage];
	list<Annotation>::iterator it;
	for (it = settings.anno.begin(); it != settings.anno.end(); ++it)
	{
		Annotation* pCurAnno = &(*it);
		if (pCurAnno == pAnno)
		{
			list<Annotation> temp;
			temp.splice(temp.end(), settings.anno, it);
			ASSERT(&(temp.front()) == pCurAnno);

			UpdateObservers(AnnotationMsg(ANNOTATION_DELETED, pCurAnno, nPage));

			if (settings.anno.empty())
				pageSettings.erase(nPage);
			return true;
		}
	}

	if (settings.anno.empty())
		pageSettings.erase(nPage);
	return false;
}


// DictionaryInfo

static const TCHAR pszTagString[] = _T("string");
static const TCHAR pszAttrCode[] = _T("code");
static const TCHAR pszAttrLocalization[] = _T("localization");
static const TCHAR pszAttrValue[] = _T("value");

void DictionaryInfo::ReadPageIndex(const GUTF8String& str, bool bEncoded)
{
	string strTemp = str;
	if (bEncoded)
		Base64Decode(strTemp);

	strPageIndex = strTemp.c_str();
}

void DictionaryInfo::ReadCharMap(const GUTF8String& str, bool bEncoded)
{
	string strTemp = str;
	if (bEncoded)
		Base64Decode(strTemp);

	strCharMap = strTemp.c_str();
}

void DictionaryInfo::ReadTitle(const GUTF8String& str, bool bEncoded)
{
	titleLoc.clear();

	if (str.length() == 0)
		return;

	string strTemp = str;
	if (bEncoded)
		Base64Decode(strTemp);

	strTitleRaw = strTemp.c_str();

	stringstream sin(strTemp.c_str());
	XMLParser parser;
	if (parser.Parse(sin))
		ReadLocalizedStrings(titleLoc, *parser.GetRoot());
}

void DictionaryInfo::ReadLangFrom(const GUTF8String& str, bool bEncoded)
{
	strLangFromCode = "";
	langFromLoc.clear();

	if (str.length() == 0)
		return;

	string strTemp = str;
	if (bEncoded)
		Base64Decode(strTemp);

	strLangFromRaw = strTemp.c_str();

	stringstream sin(strTemp.c_str());
	XMLParser parser;
	if (parser.Parse(sin))
	{
		wstring strCode;
		if (parser.GetRoot()->GetAttribute(pszAttrCode, strCode))
			strLangFromCode = MakeUTF8String(strCode);

		ReadLocalizedStrings(langFromLoc, *parser.GetRoot());
	}
}

void DictionaryInfo::ReadLangTo(const GUTF8String& str, bool bEncoded)
{
	strLangToCode = "";
	langToLoc.clear();

	if (str.length() == 0)
		return;

	string strTemp = str;
	if (bEncoded)
		Base64Decode(strTemp);

	strLangToRaw = strTemp.c_str();

	stringstream sin(strTemp.c_str());
	XMLParser parser;
	if (parser.Parse(sin))
	{
		wstring strCode;
		if (parser.GetRoot()->GetAttribute(pszAttrCode, strCode))
			strLangToCode = MakeUTF8String(strCode);

		ReadLocalizedStrings(langToLoc, *parser.GetRoot());
	}
}

void DictionaryInfo::ReadLocalizedStrings(vector<LocalizedString>& loc, const XMLNode& node)
{
	list<XMLNode>::const_iterator it;
	for (it = node.childElements.begin(); it != node.childElements.end(); ++it)
	{
		const XMLNode& child = *it;
		if (MakeCString(child.tagName) == pszTagString)
		{
			DWORD code;
			if (!child.GetHexAttribute(pszAttrLocalization, code))
				continue;

			wstring value;
			if (!child.GetAttribute(pszAttrValue, value))
				continue;

			loc.push_back(make_pair(code, MakeUTF8String(value)));
		}
	}
}


// DjVuSource

CCriticalSection DjVuSource::openDocumentsLock;
map<CString, DjVuSource*> DjVuSource::openDocuments;
map<MD5, DocSettings> DjVuSource::settings;
IApplication* DjVuSource::pApplication = NULL;

static const char pszPageIndexKey[] = "page-index";
static const char pszCharMapKey[] = "char-map";
static const char pszTitleKey[] = "title-localized";
static const char pszLangFromKey[] = "language-from";
static const char pszLangToKey[] = "language-to";

DjVuSource::DjVuSource(const CString& strFileName, GP<DjVuDocument> pDoc, DocSettings* pSettings)
	: m_strFileName(strFileName), m_pDjVuDoc(pDoc), m_nPageCount(0), m_bHasText(false),
	  m_pSettings(pSettings)
{
	m_nPageCount = m_pDjVuDoc->get_pages_num();
	ASSERT(m_nPageCount > 0);

	FindDocMetaData();
	m_pages.clear();
	m_pages.resize(m_nPageCount);
	SetCropPagesIn();

	m_strFileWithPath = strFileName;

	if (pApplication != NULL)
	{
		if (pApplication->GetDictionaryInfo(strFileName) != NULL)
			m_dictInfo.bInstalled = true;
		if (GetCropPages() && m_pSettings->pageSettings.size() > 0)
		{
			map<int, PageSettings>::iterator it;
			for (it = m_pSettings->pageSettings.begin(); it != m_pSettings->pageSettings.end(); ++it)
			{
				MoveUserSource(it->first);
			}
		}
	}

	PageInfo info = GetPageInfo(0, false, true);

	if (info.pAnt != NULL) // подумать надо ли это?
	{
		//m_dictInfo.ReadPageIndex(info.pAnt->metadata[pszPageIndexKey]);
		//m_dictInfo.ReadCharMap(info.pAnt->metadata[pszCharMapKey]);
		//m_dictInfo.ReadTitle(info.pAnt->metadata[pszTitleKey]);
		//m_dictInfo.ReadLangFrom(info.pAnt->metadata[pszLangFromKey]);
		//m_dictInfo.ReadLangTo(info.pAnt->metadata[pszLangToKey]);
	}
}

DjVuSource::~DjVuSource()
{
	while (!m_eventCache.empty())
	{
		HANDLE hEvent = m_eventCache.top();
		m_eventCache.pop();

		::CloseHandle(hEvent);
	}

	m_pDjVuDoc = NULL;

	// Close open files
	GURL url = GURL::Filename::UTF8(MakeUTF8String(m_strFileName));
	DataPool::load_file(url);

	UpdateObservers(SOURCE_RELEASED);
}

void DjVuSource::Release()
{
	openDocumentsLock.Lock();
	if (InterlockedDecrement(&m_nRefCount) <= 0)
	{
		openDocuments.erase(m_strFileName);
		delete this;
	}
	openDocumentsLock.Unlock();
}

DjVuSource* DjVuSource::FromFile(const CString& strFileName)
{
	TCHAR pszName[MAX_PATH] = { 0 };
	LPTSTR pszFileName;
	GetFullPathName(strFileName, MAX_PATH, pszName, &pszFileName);

	openDocumentsLock.Lock();
	map<CString, DjVuSource*>::iterator itSource = openDocuments.find(pszName);
	if (itSource != openDocuments.end())
	{
		DjVuSource* pSource = (*itSource).second;
		pSource->AddRef();
		openDocumentsLock.Unlock();
		return pSource;
	}

	CFile file;
	if (!file.Open(pszName, CFile::modeRead | CFile::shareDenyWrite))
		return NULL;

	int nLength = static_cast<int>(min(file.GetLength(), 0x40000));
	LPBYTE pBuf = new BYTE[nLength];
	file.Read(pBuf, nLength);
	file.Close();

	MD5 digest(pBuf, nLength);
	delete[] pBuf;

	GP<DjVuDocument> pDoc = NULL;
	try
	{
		GURL url = GURL::Filename::UTF8(MakeUTF8String(CString(pszName)));
		pDoc = DjVuDocument::create(url);
		pDoc->wait_get_pages_num();
	}
	catch (GException&)
	{
		return NULL;
	}

	if (pDoc->get_pages_num() == 0)
	{
		return NULL;
	}

	map<MD5, DocSettings>::iterator it = settings.find(digest);
	bool bExisting = (it != settings.end());
	DocSettings* pSettings = &settings[digest];
	pSettings->strLastKnownLocation = pszName;

	if (!bExisting && pApplication != NULL)
	{
		pApplication->LoadDocSettings(digest.ToString(), pSettings);

		// Remove annotations for non-existing pages.
		map<int, PageSettings>::iterator it;
		for (it = pSettings->pageSettings.begin(); it != pSettings->pageSettings.end();)
		{
			int nPage = it->first;
			if (nPage < 0 || nPage >= pDoc->get_pages_num())
				pSettings->pageSettings.erase(it++);
			else
				++it;
		}
	}

	DjVuSource* pSource = new DjVuSource(pszName, pDoc, pSettings);
	openDocuments.insert(make_pair(CString(pszName), pSource));
	openDocumentsLock.Unlock();

	return pSource;
}

bool DjVuSource::IsPageCached(int nPage, Observer* observer)
{
	ASSERT(nPage >= 0 && nPage < m_pDjVuDoc->get_pages_num());
	const PageData& data = m_pages[nPage];

	m_lock.Lock();
	bool bCached = (data.pImage != NULL
			&& (data.IsObservedBy(observer) || !data.HasObservers()));
	m_lock.Unlock();

	return bCached;
}

GP<DjVuImage> DjVuSource::GetPage(int nPage, Observer* observer)
{
	ASSERT(nPage >= 0 && nPage < m_nPageCount);
	PageData& data = m_pages[nPage];

	m_lock.Lock();
	GP<DjVuImage> pImage = data.pImage;
	if (pImage != NULL)
	{
		m_lock.Unlock();
		return pImage;
	}

	if (data.hDecodingThread != NULL)
	{
		// Another thread is already decoding this page. Put
		// ourselves in a list of waiting threads
		PageRequest request;

		m_eventLock.Lock();
		if (m_eventCache.empty())
		{
			request.hEvent = ::CreateEvent(NULL, false, false, NULL);
		}
		else
		{
			request.hEvent = m_eventCache.top();
			::ResetEvent(request.hEvent);
			m_eventCache.pop();
		}
		m_eventLock.Unlock();

		// Temporarily increase priority of the decoding thread if needed
		HANDLE hOurThread = ::GetCurrentThread();
		int nOurPriority = ::GetThreadPriority(hOurThread);
		if (::GetThreadPriority(data.hDecodingThread) < nOurPriority)
			::SetThreadPriority(data.hDecodingThread, nOurPriority);

		data.requests.push_back(&request);

		if (observer != NULL)
			data.AddObserver(observer);

		m_lock.Unlock();

		::WaitForSingleObject(request.hEvent, INFINITE);
		pImage = request.pImage;

		m_eventLock.Lock();
		m_eventCache.push(request.hEvent);
		m_eventLock.Unlock();

		// Page will be put in cache by the thread which decoded it
		return pImage;
	}

	data.hDecodingThread = ::GetCurrentThread();
	data.nOrigThreadPriority = ::GetThreadPriority(data.hDecodingThread);

	m_lock.Unlock();

	try
	{
		GP<DjVuFile> file = m_pDjVuDoc->get_djvu_file(nPage);
		if (file)
		{
			pImage = DjVuImage::create(file);
			file->resume_decode();
			if (pImage && THREADMODEL != NOTHREADS)
				pImage->wait_for_complete_decode();
		}
	}
	catch (GException&)
	{
	}
	catch (...)
	{
		if (pApplication != NULL)
			pApplication->ReportFatalError();
	}

	m_lock.Lock();
	if (pImage != NULL)
	{
		pImage->set_rotate(0);

		if (observer != NULL || data.HasObservers())
		{
			data.pImage = pImage;

			if (observer != NULL)
				data.AddObserver(observer);
		}
		bool bIsFirstDecode = data.info.szPage.cx == 0 && data.info.szPage.cy == 0;
		data.info.Update(pImage);
		if (pApplication->GetCropPages())
		{
			if (bIsFirstDecode)
				data.info.CropPage(m_pSettings->nRotate);
			else
				data.info.CropSource(m_pSettings->nRotate);
			MoveUserSource(nPage);
		}
		UpdatePageTitle(data.info, nPage);
		if (data.info.bHasText)
			m_bHasText = true;
	}

	// Notify all waiting threads that image is ready
	for (size_t i = 0; i < data.requests.size(); ++i)
	{
		data.requests[i]->pImage = pImage;
		::SetEvent(data.requests[i]->hEvent);
	}

	ASSERT(data.hDecodingThread == ::GetCurrentThread());
	::SetThreadPriority(data.hDecodingThread, data.nOrigThreadPriority);
	data.hDecodingThread = NULL;
	data.requests.clear();
	m_lock.Unlock();

	return pImage;
}

void DjVuSource::MoveSource(int nPage, CPoint& ptMove)
{
	ASSERT(nPage >= 0 && nPage < m_nPageCount);
	PageInfo& pInfo = m_pages[nPage].info;
	if (pInfo.pText != NULL)
		pInfo.MoveZone(&pInfo.pText->page_zone, ptMove.x, ptMove.y, true);
	if (pInfo.anno.size() > 0)
	{
		list<Annotation>::iterator it;
		for (it = pInfo.anno.begin(); it != pInfo.anno.end(); ++it)
		{
			pInfo.MoveAnno(&(*it), ptMove.x, ptMove.y, true);
		}
	}
	MoveUserSource(nPage, ptMove);
}

void DjVuSource::MoveUserSource(int nPage, bool bCrop)
{
	if (bCrop == m_pSettings->pageSettings[nPage].bCropped || m_pSettings->pageSettings[nPage].anno.size() == 0)
		return;
	m_pSettings->pageSettings[nPage].bCropped = bCrop;
	PageInfo& pInfo = m_pages[nPage].info;
	CRect rect = pInfo.GetCropRect(m_pSettings->nRotate);
	int k = bCrop ? 1 : -1;
	CPoint ptMove(k * pInfo.rcCropPage.left, k * pInfo.rcCropPage.bottom);
	MoveUserSource(nPage, ptMove);
}

void DjVuSource::MoveUserSource(int nPage, CPoint& ptMove)
{
	PageInfo& pInfo = m_pages[nPage].info;
	PageSettings& settings = GetSettings()->pageSettings[nPage];
	list<Annotation>::iterator it;
	for (it = settings.anno.begin(); it != settings.anno.end(); ++it)
	{
		pInfo.MoveAnno(&(*it), ptMove.x, ptMove.y, true);
	}
}

void DjVuSource::SetCropRect(int nPage, int nRotate, CRect& rcCrop)
{
	ASSERT(nPage >= 0 && nPage < m_nPageCount);
	PageData& data = m_pages[nPage];

	if (nRotate != 0)
	{
		CRect rect = CRect();
		switch (nRotate)
		{
		case 1 /*ID_ROTATE_LEFT*/:
			rect = CRect(rcCrop.bottom, rcCrop.left, rcCrop.top, rcCrop.right);
			break;

		case 3 /*ID_ROTATE_RIGHT*/:	
			rect = CRect(rcCrop.top, rcCrop.right, rcCrop.bottom, rcCrop.left);
			break;

		case 2 /*ID_ROTATE_180*/:
			rect = CRect(rcCrop.right, rcCrop.bottom, rcCrop.left, rcCrop.top);
			break;
		}
		rcCrop = rect;
	}
	data.info.rcCropPage = rcCrop;
}

CRect PageInfo::GetCropRect(int nRotate)
{
	bool bIsRectZero = rcCropPage.top == 0 && rcCropPage.bottom == 0 
		&& rcCropPage.left == 0 && rcCropPage.right == 0;
	if (nRotate == 0 || bIsRectZero)
		return rcCropPage;

	CRect rect;
	switch (nRotate)
	{
	case 1 /*ID_ROTATE_LEFT*/:
		rect = CRect(rcCropPage.top, rcCropPage.right, rcCropPage.bottom, rcCropPage.left);
		break;

	case 3 /*ID_ROTATE_RIGHT*/:	
		rect = CRect(rcCropPage.bottom, rcCropPage.left, rcCropPage.top, rcCropPage.right);
		break;

	case 2 /*ID_ROTATE_180*/:
		rect = CRect(rcCropPage.right, rcCropPage.bottom, rcCropPage.left, rcCropPage.top);
		break;

	default:
		rect.CopyRect(rcCropPage);
		break;
	}
	return rect;
}

void PageInfo::CropSource(int nRotate, bool bCrop)
{
	CRect rect = GetCropRect(nRotate);
	if (bTextDecoded && ((bCrop && !bTextCropped) || (!bCrop && bTextCropped)))
	{
		DjVuTXT::Zone* zone = &(pText->page_zone);
		MoveZone(zone, rcCropPage.left, rcCropPage.bottom, bCrop);
		bTextCropped = bCrop;
	}
	if (bAnnoDecoded && anno.size() > 0 && ((bCrop && !bAnnoCropped) || (!bCrop && bAnnoCropped)))
	{
		list<Annotation>::iterator it;
		for (it = anno.begin(); it != anno.end(); ++it)
		{
			MoveAnno(&(*it), rcCropPage.left, rcCropPage.bottom, bCrop);
		}
		bAnnoCropped = bCrop;
	}
}

void PageInfo::MoveZone(DjVuTXT::Zone* zone, int left, int bottom, bool bLeftBottom)
{
	int k = bLeftBottom ? -1 : 1;
	zone->rect.translate(k * left, k * bottom);
	for(GPosition pos=zone->children; pos; ++pos)
	{
		MoveZone(&zone->children[pos], left, bottom, bLeftBottom);
	}
}

void PageInfo::MoveAnno(Annotation* ant, int left, int bottom, bool bLeftBottom)
{
	int k = bLeftBottom ? -1 : 1;
	ant->rectBounds.translate(k * left, k * bottom);
	for (size_t i = 0; i < ant->points.size(); ++i)
	{
		ant->points[i].first += k * left;
		ant->points[i].second += k * bottom;
	}
	for (size_t i = 0; i < ant->rects.size(); ++i)
	{
		ant->rects[i].translate(k * left, k * bottom);
	}
}

PageInfo DjVuSource::GetPageInfo(int nPage, bool bNeedText, bool bNeedAnno)
{
	ASSERT(nPage >= 0 && nPage < m_nPageCount);
	PageData& data = m_pages[nPage];

	m_lock.Lock();

	if (data.info.bDecoded && (data.info.bTextDecoded || !bNeedText)
			&& (data.info.bAnnoDecoded || !bNeedAnno))
	{
		PageInfo info = data.info;
		if (pApplication->GetCropPages() && (!info.bTextCropped || !info.bAnnoCropped))
		{
			info.CropSource(m_pSettings->nRotate);
			MoveUserSource(nPage);
		}
		m_lock.Unlock();
		return info;
	}
	//m_lock.Unlock();
	PageInfo info = ReadPageInfo(nPage, bNeedText, bNeedAnno);
	//m_lock.Lock();

	data.info.Update(info);
	if (pApplication->GetCropPages())
	{
		data.info.CropPage(m_pSettings->nRotate);
		MoveUserSource(nPage);
		info.szPage = data.info.szPage;
	}
	else
	{
		data.info.CropSource(m_pSettings->nRotate, false);
		MoveUserSource(nPage, false);
		info.szPage = data.info.szPage;
	}

	if (data.info.bHasText)
		m_bHasText = true;
	m_lock.Unlock();

	return info;
}

void DjVuSource::SetCropPagesOut()
{
	m_pSettings->cropPages.clear();
	for (int nPage = 0; nPage < m_nPageCount; ++nPage)
	{
		CRect& rect = m_pages[nPage].info.rcCropPage;
		bool bIsRectZero = rect.top == 0 && rect.bottom == 0 
			&& rect.left == 0 && rect.right == 0;
		if (!bIsRectZero)
			m_pSettings->cropPages.push_back(make_pair(nPage, rect));
	}
}

void DjVuSource::SetCropPagesIn()
{
	list<pair <int, CRect> >::const_iterator it;
	for (it = m_pSettings->cropPages.begin(); it != m_pSettings->cropPages.end(); ++it)
	{
		const pair <int, CRect> & crop = *it;
		int nPage = crop.first;
		if (nPage >= 0 && nPage < m_nPageCount)
		{
			m_pages[nPage].info.rcCropPage = crop.second;
		}
	}
}

void DjVuSource::RemoveFromCache(int nPage, Observer* observer)
{
	ASSERT(nPage >= 0 && nPage < m_nPageCount);
	PageData& data = m_pages[nPage];
	GP<DjVuImage> pImage = NULL;

	m_lock.Lock();
	data.RemoveObserver(observer);
	if (!data.HasObservers())
	{
		pImage = m_pages[nPage].pImage;
		m_pages[nPage].pImage = NULL;
	}
	m_lock.Unlock();

	// This will cause the destructor to be called, if page is removed from cache
	pImage = NULL;
}

void DjVuSource::ChangeObservedPages(Observer* observer,
		const vector<int>& add, const vector<int>& remove)
{
	m_lock.Lock();
	for (size_t i = 0; i < add.size(); ++i)
	{
		ASSERT(add[i] >= 0 && add[i] < m_nPageCount);
		m_pages[add[i]].AddObserver(observer);
	}
	for (size_t j = 0; j < remove.size(); ++j)
	{
		ASSERT(remove[j] >= 0 && remove[j] < m_nPageCount);
		m_pages[remove[j]].RemoveObserver(observer);
	}
	m_lock.Unlock();
}

int DjVuSource::GetPageFromId(const GUTF8String& strPageId) const
{
	int n = -1;
	if (m_pDjVuDoc != NULL)
	{
		try
		{	
			return m_pDjVuDoc->id_to_page(strPageId);
		}
		catch (GException&)
		{
		}
		catch (...)
		{
			if (pApplication != NULL)
				pApplication->ReportFatalError();
		}
	}
	return n;
}

void DjVuSource::ReadAnnotations(GP<ByteStream> pInclStream,
		set<GUTF8String>& processed, GP<ByteStream> pAnnoStream)
{
	// Look for shared annotations
	GUTF8String strInclude;
	char buf[1024];
	size_t nLength;
	while ((nLength = pInclStream->read(buf, 1024)))
		strInclude += GUTF8String(buf, (UINT)nLength);

	// Eat '\n' in the beginning and at the end
	while (strInclude.length() > 0 && strInclude[0] == '\n')
		strInclude = strInclude.substr(1, static_cast<unsigned int>(-1));

	while (strInclude.length() > 0 && strInclude[static_cast<int>(strInclude.length()) - 1] == '\n')
		strInclude.setat(strInclude.length() - 1, 0);

	if (strInclude.length() > 0 && processed.find(strInclude) == processed.end())
	{
		processed.insert(strInclude);

		GURL urlInclude = m_pDjVuDoc->id_to_url(strInclude);
		GP<DataPool> pool = m_pDjVuDoc->request_data(NULL, urlInclude);
		GP<ByteStream> stream = pool->get_stream();
		GP<IFFByteStream> iff(IFFByteStream::create(stream));

		// Check file format
		GUTF8String chkid;
		if (!iff->get_chunk(chkid) ||
			(chkid != "FORM:DJVI" && chkid != "FORM:DJVU" &&
			chkid != "FORM:PM44" && chkid != "FORM:BM44"))
		{
			return;
		}

		// Find chunk with page info
		while (iff->get_chunk(chkid) != 0)
		{
			GP<ByteStream> chunk_stream = iff->get_bytestream();

			if (chkid == "INCL")
			{
				ReadAnnotations(pInclStream, processed, pAnnoStream);
			}
			else if (chkid == "FORM:ANNO")
			{
				pAnnoStream->copy(*chunk_stream);
			}
			else if (chkid == "ANTa" || chkid == "ANTz")
			{
				const GP<IFFByteStream> iffout = IFFByteStream::create(pAnnoStream);
				iffout->put_chunk(chkid);
				iffout->copy(*chunk_stream);
				iffout->close_chunk();
			}

			iff->seek_close_chunk();
		}
	}
}

PageInfo DjVuSource::ReadPageInfo(int nPage, bool bNeedText, bool bNeedAnno)
{
	ASSERT(nPage >= 0 && nPage < m_nPageCount);
	PageInfo pageInfo;
	pageInfo.szPage.cx = 100;
	pageInfo.szPage.cy = 100;
	pageInfo.nDPI = 100;
	pageInfo.bDecoded = true;

	GP<ByteStream> pAnnoStream;
	if (bNeedAnno)
		pAnnoStream = ByteStream::create();

	GP<ByteStream> pTextStream;
	if (bNeedText)
		pTextStream = ByteStream::create();

	try
	{
		// Get raw data from the document and decode only requested chunks
		// DjVuFile is not used to ensure that we do not wait for a lock
		// to be released and thus do not block the UI thread
		GURL url = m_pDjVuDoc->page_to_url(nPage);
		GP<DataPool> pool = m_pDjVuDoc->request_data(NULL, url);
		GP<ByteStream> stream = pool->get_stream();
		GP<IFFByteStream> iff(IFFByteStream::create(stream));

		// Check file format
		GUTF8String chkid;
		if (!iff->get_chunk(chkid) ||
			(chkid != "FORM:DJVI" && chkid != "FORM:DJVU" &&
			 chkid != "FORM:PM44" && chkid != "FORM:BM44"))
		{
			return pageInfo;
		}

		bool bHasIW44 = false;

		// Find chunk with page info
		while (iff->get_chunk(chkid) != 0)
		{
			GP<ByteStream> chunk_stream = iff->get_bytestream();

			if (chkid == "INFO")
			{
				// Get page dimensions and resolution from info chunk
				GP<DjVuInfo> pInfo = DjVuInfo::create();
				pInfo->decode(*chunk_stream);

				// Check data for consistency
				pageInfo.szPage.cx = max(pInfo->width, 0);
				pageInfo.szPage.cy = max(pInfo->height, 0);
				pageInfo.nInitialRotate = pInfo->orientation;
				pageInfo.nDPI = max(pInfo->dpi, 0);

				if ((pInfo->orientation & 1) != 0)
					swap(pageInfo.szPage.cx, pageInfo.szPage.cy);
			}
			else if (!bHasIW44 && (chkid == "PM44" || chkid == "BM44"))
			{
				bHasIW44 = true;

				// Get image dimensions and resolution from bitmap chunk
				UINT serial = chunk_stream->read8();
				UINT slices = chunk_stream->read8();
				UINT major = chunk_stream->read8();
				UINT minor = chunk_stream->read8();

				UINT xhi = chunk_stream->read8();
				UINT xlo = chunk_stream->read8();
				UINT yhi = chunk_stream->read8();
				UINT ylo = chunk_stream->read8();

				pageInfo.szPage.cx = (xhi << 8) | xlo;
				pageInfo.szPage.cy = (yhi << 8) | ylo;
				pageInfo.nDPI = 100;
			}
			else if (chkid == "TXTa" || chkid == "TXTz")
			{
				pageInfo.bHasText = true;

				if (bNeedText)
				{
					const GP<IFFByteStream> iffout = IFFByteStream::create(pTextStream);
					iffout->put_chunk(chkid);
					iffout->copy(*chunk_stream);
					iffout->close_chunk();
				}
			}
			else if (bNeedAnno && chkid == "FORM:ANNO")
			{
				pAnnoStream->copy(*chunk_stream);
			}
			else if (bNeedAnno && (chkid == "ANTa" || chkid == "ANTz"))
			{
				const GP<IFFByteStream> iffout = IFFByteStream::create(pAnnoStream);
				iffout->put_chunk(chkid);
				iffout->copy(*chunk_stream);
				iffout->close_chunk();
			}
			else if (bNeedAnno && chkid == "INCL")
			{
				set<GUTF8String> processed;
				ReadAnnotations(chunk_stream, processed, pAnnoStream);
			}

			iff->seek_close_chunk();
		}

		if (bNeedText && pTextStream->tell())
			pageInfo.DecodeText(pTextStream);

		if (bNeedAnno && pAnnoStream->tell())
			pageInfo.DecodeAnno(pAnnoStream);

		UpdatePageTitle(pageInfo, nPage);

	}
	catch (GException&)
	{
	}
	catch (...)
	{
		if (pApplication != NULL)
			pApplication->ReportFatalError();
	}

	return pageInfo;
}

bool DjVuSource::SaveAs(const CString& strFileName)
{
	if (AfxComparePath(strFileName, m_strFileName))
		return false;

	try
	{
		m_pDjVuDoc->wait_for_complete_init();
		GURL url = GURL::Filename::UTF8(MakeUTF8String(strFileName));

		// Close open files for this url
		DataPool::load_file(url);

		if (!CopyFile(m_strFileName, strFileName, false))
			return false;
	}
	catch (GException&)
	{
		return false;
	}

	return true;
}

void DjVuSource::UpdateDictionaries()
{
	if (pApplication == NULL)
		return;

	openDocumentsLock.Lock();
	map<CString, DjVuSource*>::iterator it;
	for (it = openDocuments.begin(); it != openDocuments.end(); ++it)
	{
		DjVuSource* pSource = (*it).second;
		pSource->m_dictInfo.bInstalled = (pApplication->GetDictionaryInfo(pSource->m_strFileName) != NULL);
	}
	openDocumentsLock.Unlock();
}

void DjVuSource::FindDocMetaData()
{
	try
	{
		const GP<DjVmDir::File> sh_anno = m_pDjVuDoc->get_djvm_dir()->get_shared_anno_file();
		if (sh_anno == NULL)
			return;
		GURL url = m_pDjVuDoc->id_to_url(sh_anno->get_load_name());
		GP<DataPool> pool = m_pDjVuDoc->request_data(NULL, url);
		GP<ByteStream> stream = pool->get_stream();
		GP<IFFByteStream> iff(IFFByteStream::create(stream));

		GP<ByteStream> pAnnoStream = ByteStream::create();

		GUTF8String chkid;
		if (!iff->get_chunk(chkid) || chkid != "FORM:DJVI")
			return;
		while (iff->get_chunk(chkid) != 0)
		{
			GP<ByteStream> chunk_stream = iff->get_bytestream();

			if (chkid == "INCL")
			{
				ASSERT(FALSE);
			}
			else if (chkid == "FORM:ANNO")
			{
				pAnnoStream->copy(*chunk_stream);
			}
			else if (chkid == "ANTa" || chkid == "ANTz")
			{
				const GP<IFFByteStream> iffout = IFFByteStream::create(pAnnoStream);
				iffout->put_chunk(chkid);
				iffout->copy(*chunk_stream);
				iffout->close_chunk();
			}

			iff->seek_close_chunk();
		}
		if (pAnnoStream != NULL)
		{

			pAnnoStream->seek(0);
			GP<DjVuAnno> pDjVuAnno = DjVuAnno::create();
			pDjVuAnno->decode(pAnnoStream);

			if (pDjVuAnno->ant != NULL)
			{
				if (pDjVuAnno->ant->metadata.size() > 0)
				{
					m_pDocMetadata = pDjVuAnno->ant->metadata;
				}
				else if (pDjVuAnno->ant->xmpmetadata.length() > 0)
				{
					m_strDocXmpMetadata = pDjVuAnno->ant->xmpmetadata;
				}
			}
		}
	}
	catch (GException&)
	{
	}

}

GUTF8String DjVuSource::GetPageMetaData(int nPage)
{
	GUTF8String strPageMetadata;

	try
	{
		GURL url = m_pDjVuDoc->page_to_url(nPage);
		GP<DataPool> pool = m_pDjVuDoc->request_data(NULL, url);
		GP<ByteStream> stream = pool->get_stream();
		GP<IFFByteStream> iff(IFFByteStream::create(stream));

		GP<ByteStream> pAnnoStream = ByteStream::create();

		GUTF8String chkid;
		if (!iff->get_chunk(chkid) ||
			(chkid != "FORM:DJVI" && chkid != "FORM:DJVU" &&
			chkid != "FORM:PM44" && chkid != "FORM:BM44"))
		{
			return strPageMetadata;
		}
		while (iff->get_chunk(chkid) != 0)
		{
			GP<ByteStream> chunk_stream = iff->get_bytestream();

			/*if (chkid == "INCL")
			{
			ASSERT(FALSE);
			}
			else */if (chkid == "FORM:ANNO")
			{
				pAnnoStream->copy(*chunk_stream);
			}
			else if (chkid == "ANTa" || chkid == "ANTz")
			{
				const GP<IFFByteStream> iffout = IFFByteStream::create(pAnnoStream);
				iffout->put_chunk(chkid);
				iffout->copy(*chunk_stream);
				iffout->close_chunk();
			}

			iff->seek_close_chunk();
		}
		if (pAnnoStream != NULL)
		{

			pAnnoStream->seek(0);
			GP<DjVuAnno> pDjVuAnno = DjVuAnno::create();
			pDjVuAnno->decode(pAnnoStream);

			if (pDjVuAnno->ant != NULL)
			{
				if (pDjVuAnno->ant->metadata.size() > 0)
				{
					vector<GMap<GUTF8String, GUTF8String> > metadata = pDjVuAnno->ant->metadata;
					for(UINT i = 0; i < metadata.size(); ++i)
					{
						if (!IsDocMetaData(&metadata[i]))
						{
							GPosition pos = metadata[i];
							GUTF8String str_data = metadata[i][pos];
							if (str_data.length() > 0)
								strPageMetadata += metadata[i].key(pos) + "\t" + str_data + "\n";
						}
					}
				}
				else if (pDjVuAnno->ant->xmpmetadata.length() > 0)
				{
					strPageMetadata = pDjVuAnno->ant->xmpmetadata + "\n";
				}
			}
		}
	}
	catch (GException&)
	{
	}
	return strPageMetadata; 
}

GUTF8String DjVuSource::GetDocMetaData()
{
	GUTF8String strDocMetadata;

	if (m_pDocMetadata.size() > 0)
	{
		for(UINT i = 0; i < m_pDocMetadata.size(); ++i)
		{
			GPosition pos = m_pDocMetadata[i];
			GUTF8String str_data = m_pDocMetadata[i][pos];
			if (str_data.length() > 0)
				strDocMetadata += m_pDocMetadata[i].key(pos) + "\t" + str_data + "\n";
		}
	}
	else if (m_strDocXmpMetadata.length() > 0)
	{
		strDocMetadata = m_strDocXmpMetadata + "\n";
	}
	return strDocMetadata;
}

bool DjVuSource::IsDocMetaData(GMap<GUTF8String, GUTF8String>* data)
{
	if (m_pDocMetadata.size() > 0)
	{
		for(UINT i = 0; i < m_pDocMetadata.size(); ++i)
		{
			GPosition pos = m_pDocMetadata[i];
			GUTF8String strKey = m_pDocMetadata[i].key(pos);
			if (strKey != data->key(data->firstpos()))
				continue;
			if (m_pDocMetadata[i][pos] == (*data)[strKey])
				return true;
		}
	}
	return false;
}
