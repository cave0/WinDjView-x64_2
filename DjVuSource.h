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

#pragma once

#include "Global.h"
struct XMLNode;


typedef GList<DjVuTXT::Zone*> DjVuSelection;

struct Annotation
{
	Annotation()
		: bHideInactiveBorder(false), nBorderType(BorderNone),
		  crBorder(RGB(0, 0, 0)), nBorderWidth(1), bHideInactiveFill(false),
		  nFillType(FillSolid), crFill(RGB(255, 255, 0)), fTransparency(0.75),
		  crForeground(RGB(0, 0, 0)), bAlwaysShowComment(false),
		  bOvalShape(false), bIsLine(false), bHasArrow(false), nLineWidth(1) {}

	void UpdateBounds();
	GUTF8String GetXML() const;
	void Load(const XMLNode& node);
	void Fix();

	enum BorderType
	{
		BorderNone = 0,
		BorderSolid = 1,
		BorderXOR = 2,
		BorderShadowIn = 3,
		BorderShadowOut = 4,
		BorderEtchedIn = 5,
		BorderEtchedOut = 6
	};

	enum FillType
	{
		FillNone = 0,
		FillSolid = 1,
		FillXOR = 2
	};

	bool bHideInactiveBorder;
	int nBorderType;
	COLORREF crBorder;
	int nBorderWidth;
	bool bHideInactiveFill;
	int nFillType;
	COLORREF crFill;
	double fTransparency;
	COLORREF crForeground;
	bool bAlwaysShowComment;
	bool bOvalShape, bIsLine, bHasArrow;
	int nLineWidth;
	GUTF8String strComment;
	GUTF8String strURL;

	vector<GRect> rects;
	vector<pair<int, int> > points;
	GRect rectBounds;

	void Init(GP<GMapArea> pArea, const CSize& szPage, int nRotate);
	GP<GMapArea> sourceArea;
};

struct PageSettings
{
	list<Annotation> anno;
	bool bCropped;
	GUTF8String GetXML() const;
	void Load(const XMLNode& node);
};

struct Bookmark
{
private:
	// Trick to pacify the VC6 compiler
	list<Bookmark>* pchildren;

public:
	Bookmark()
		: pchildren(new list<Bookmark>()), children(*pchildren), pParent(NULL),
		  nLinkType(URL), nPage(0), ptOffset(0, 0), bMargin(false), bZoom(false), 
		  textStart(-1), textLen(1) {}
	Bookmark(const Bookmark& bm)
		: pchildren(new list<Bookmark>()), children(*pchildren) { *this = bm; }
	~Bookmark()
		{ delete pchildren; }
	Bookmark& operator=(const Bookmark& bm);
	void swap(Bookmark& bm);

	GUTF8String strTitle;
	Bookmark* pParent;

	enum LinkType
	{
		URL = 0,
		Page = 1,
		View = 2,
		Text = 3
	};

	bool HasLink() const
		{ return (nLinkType == URL ? strURL.length() > 0 : true); }

	int nLinkType;
	GUTF8String strURL;
	int nPage;
	CPoint ptOffset;
	bool bMargin;
	bool bZoom;
	int nZoomType, nPrevZoomType;
	double fZoom, fPrevZoom;
	int textStart;
	int textLen;

	list<Bookmark>& children;

	GUTF8String GetXML() const;
	void Load(const XMLNode& node);
};

struct DocSettings : public Observable
{
	DocSettings();

	enum SidebarTab
	{
		Thumbnails = 0,
		Contents = 1,
		Bookmarks = 2,
		PageIndex = 3
	};

	int nPage;
	CPoint ptOffset;
	int nZoomType;
	double fZoom;
	int nLayout;
	bool bFirstPageAlone;
	bool bRightToLeft;
	int nDisplayMode;
	int nRotate;
	int nOpenSidebarTab;
	wstring strAttrLastKnownLocation;

	CString strLastKnownLocation;

	map<int, PageSettings> pageSettings;
	list<pair <int, CRect> > cropPages;
	list<Bookmark> bookmarks;

	GUTF8String GetXML(bool skip_view_settings = false) const;
	void Load(const XMLNode& node);

	Annotation* AddAnnotation(const Annotation& anno, int nPage);
	bool DeleteBookmark(const Bookmark* pBookmark);
	bool DeleteAnnotation(const Annotation* pAnno, int nPage);
	PageSettings GetUncroppedAnno(int nPage, const PageSettings pSettings,
		list<pair <int, CRect> >::const_iterator& cit) const;
};

struct PageInfo
{
	PageInfo()
		: bDecoded(false), szPage(0, 0), nDPI(0), nInitialRotate(0),
		  bHasText(false), bAnnoDecoded(false), bTextDecoded(false), 
	      bAnnoCropped(false), bTextCropped(false) { rcCropPage.SetRectEmpty(); }

	void Update(GP<DjVuImage> pImage);
	void Update(const PageInfo& info); 
	void CropPage(int nRotate);
	void MoveZone(DjVuTXT::Zone* zone, int left, int bottom, bool bLeftBottom);
	void MoveAnno(Annotation* ant, int left, int bottom, bool bLeftBottom);
	void CropSource(int nRotate, bool bCrop = true);
	CRect GetCropRect(int nRotate);

	void DecodeAnno(GP<ByteStream> pAnnoStream);
	void DecodeText(GP<ByteStream> pTextStream);

	CString strPageTitle;
	bool bDecoded;
	CSize szPage;
	int nInitialRotate;
	int nDPI;
	bool bHasText;
	bool bAnnoDecoded;
	bool bAnnoCropped;
	list<Annotation> anno;
	GP<DjVuANT> pAnt;
	bool bTextDecoded;
	bool bTextCropped;
	GP<DjVuTXT> pText;
	CRect rcCropPage;
};

struct DictionaryInfo
{
	DictionaryInfo() : bEnabled(true), bInstalled(false) {}

	// Application-filled fields
	CString strFileName;
	CString strPathName;
	FILETIME ftModified;
	bool bEnabled;
	bool bInstalled;

	// Runtime-filled fields depending on current application language
	CString strTitle;
	CString strLangFrom;
	CString strLangTo;

	// Dictionary-filled fields
	void ReadPageIndex(const GUTF8String& str, bool bEncoded = true);
	void ReadCharMap(const GUTF8String& str, bool bEncoded = true);
	void ReadTitle(const GUTF8String& str, bool bEncoded = true);
	void ReadLangFrom(const GUTF8String& str, bool bEncoded = true);
	void ReadLangTo(const GUTF8String& str, bool bEncoded = true);

	GUTF8String strPageIndex;
	GUTF8String strCharMap;
	GUTF8String strLangFromCode, strLangToCode;
	GUTF8String strLangFromRaw, strLangToRaw, strTitleRaw;

	typedef pair<DWORD, GUTF8String> LocalizedString;
	vector<LocalizedString> titleLoc;
	vector<LocalizedString> langFromLoc;
	vector<LocalizedString> langToLoc;
	static void ReadLocalizedStrings(vector<LocalizedString>& loc, const XMLNode& node);
};

struct IApplication
{
	virtual bool LoadDocSettings(const CString& strKey, DocSettings* pSettings) = 0;
	virtual bool GetCropPages() = 0;
	virtual DictionaryInfo* GetDictionaryInfo(const CString& strPathName, bool bCheckPath = true) = 0;
	virtual void ReportFatalError() = 0;
};

class DjVuSource : public RefCount, public Observable
{
public:
	~DjVuSource();
	virtual void Release();

	// the caller must call Release() for the object returned
	static DjVuSource* FromFile(const CString& strFileName);
	static void SetApplication(IApplication* pApp) { pApplication = pApp; }

	GP<DjVuImage> GetPage(int nPage, Observer* observer = NULL);
	void RemoveFromCache(int nPage, Observer* observer);
	void ChangeObservedPages(Observer* observer,
			const vector<int>& add, const vector<int>& remove);

	PageInfo GetPageInfo(int nPage, bool bNeedText = false, bool bNeedAnno = false);
	bool GetCropPages() { return pApplication->GetCropPages(); }
	void SetCropPagesIn();
	void SetCropPagesOut();
	void SetPageUndecoded(int nPage) { m_pages[nPage].info.bDecoded = false; }
	void UpdatePageTitle(PageInfo& pageInfo, int nPage);
	int GetUrlToPagenum(GUTF8String url);
	bool IsPageCached(int nPage, Observer* observer);
	int GetPageCount() const { return m_nPageCount; }

	bool HasText() const { return m_bHasText; }
	int GetPageFromId(const GUTF8String& strPageId) const;

	GP<DjVmNav> GetContents() { return m_pDjVuDoc->get_djvm_nav(); }
	GP<DjVuDocument> GetDjVuDoc() { return m_pDjVuDoc; }

	CString GetFileName() const { return m_strFileName; }
	CString GetFilePath() const { return m_strFileWithPath; }
	bool SaveAs(const CString& strFileName);

	DocSettings* GetSettings() { return m_pSettings; }
	DictionaryInfo* GetDictionaryInfo() { return &m_dictInfo; }
	bool IsDictionary() const { return m_dictInfo.strPageIndex.length() != 0; }
	static void UpdateDictionaries();

	static map<MD5, DocSettings>& GetAllSettings() { return settings; }

	GUTF8String GetDocMetaData();
	GUTF8String GetPageMetaData(int nPage);
	bool IsDocMetaData(GMap<GUTF8String, GUTF8String>* data);
	void SetCropRect(int nPage, int nRotate, CRect& rcCrop);
	void MoveSource(int nPage, CPoint& ptMove);
	void MoveUserSource(int nPage, bool bCrop = true);
	void MoveUserSource(int nPage, CPoint& ptMove);

protected:
	struct PageRequest
	{
		HANDLE hEvent;
		GP<DjVuImage> pImage;
	};
	stack<HANDLE> m_eventCache;
	CCriticalSection m_eventLock;

	struct PageData : public Observable
	{
		PageData() : hDecodingThread(NULL) {}

		GP<DjVuImage> pImage;
		PageInfo info;

		HANDLE hDecodingThread;
		int nOrigThreadPriority;
		vector<PageRequest*> requests;
	};

	DjVuSource(const CString& strFileName, GP<DjVuDocument> pDoc, DocSettings* pSettings);
	PageInfo ReadPageInfo(int nPage, bool bNeedText = false, bool bNeedAnno = false);
	void ReadAnnotations(GP<ByteStream> pInclStream, set<GUTF8String>& processed, GP<ByteStream> pAnnoStream);
	void FindDocMetaData();
	bool CheckMatchTitle(CString strNewTitle, int& nPage, int nLastPage);

	GP<DjVuDocument> m_pDjVuDoc;
	CString m_strFileName;
	CString m_strFileWithPath;
	int m_nPageCount;
	CCriticalSection m_lock;
	bool m_bHasText;

	vector<PageData> m_pages;
	DocSettings* m_pSettings;
	DictionaryInfo m_dictInfo;

	static map<CString, DjVuSource*> openDocuments;
	static CCriticalSection openDocumentsLock;
	static map<MD5, DocSettings> settings;
	static IApplication* pApplication;

	vector<GMap<GUTF8String,GUTF8String> > m_pDocMetadata;
    GUTF8String m_strDocXmpMetadata;

private:
	DjVuSource() {}
};
