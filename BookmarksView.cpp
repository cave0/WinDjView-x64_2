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
#include "BookmarksView.h"

#include "DjVuSource.h"
#include "BookmarkDlg.h"
#include "DjVuView.h"
#include "NavPane.h"
#include "MDIChild.h"


// CBookmarksView

IMPLEMENT_DYNAMIC(CBookmarksView, CMyTreeView)

BEGIN_MESSAGE_MAP(CBookmarksView, CMyTreeView)
	ON_NOTIFY_REFLECT(TVN_SELCHANGED, OnSelChanged)
	ON_NOTIFY_REFLECT(TVN_ITEMCLICKED, OnItemClicked)
	ON_NOTIFY_REFLECT(TVN_KEYDOWN, OnKeyDown)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_CONTEXTMENU()
	ON_WM_MENUSELECT()
	ON_WM_ENTERIDLE()
	ON_MESSAGE(WM_SHOW_SETTINGS, OnShowSettings)
	ON_WM_MOUSEACTIVATE()
END_MESSAGE_MAP()

CBookmarksView::CBookmarksView(DjVuSource* pSource)
	: m_bEnableEditing(false), m_pSource(pSource)
{
}

CBookmarksView::~CBookmarksView()
{
}


// CBookmarksView message handlers

int CBookmarksView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CMyTreeView::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_imageList.Create(16, 16, ILC_COLOR24 | ILC_MASK, 0, 1);
	CBitmap bitmap;
	bitmap.LoadBitmap(IDB_BOOKMARKS);
	m_imageList.Add(&bitmap, RGB(192, 64, 32));
	SetImageList(&m_imageList, TVSIL_NORMAL);

	SetItemHeight(20);
	SetWrapLabels(theApp.GetAppSettings()->bWrapLongBookmarks);

	theApp.AddObserver(this);

	return 0;
}

void CBookmarksView::OnDestroy()
{
	theApp.RemoveObserver(this);

	CMyTreeView::OnDestroy();
}

//void CBookmarksView::LoadContents()
//{
//	BeginBatchUpdate();
//	DeleteAllItems();
//	m_links.clear();
//
//	const GPList<DjVmNav::DjVuBookMark>& bookmarks = m_pSource->GetContents()->getBookMarkList();
//	GPosition pos = bookmarks;
//	AddBookmarks(bookmarks, TVI_ROOT, pos, bookmarks.size());
//
//	EndBatchUpdate();
//}

void CBookmarksView::LoadContents()
{
	BeginBatchUpdate();
	DeleteAllItems();
	m_links.clear();
	m_contents.clear();

	const GPList<DjVmNav::DjVuBookMark>& bookmarks = m_pSource->GetContents()->getBookMarkList();
	GPosition pos = bookmarks;
	AddBookmarks(bookmarks, TVI_ROOT, pos, bookmarks.size());
	
	TreeNode* pNode = reinterpret_cast<TreeNode*>(m_links.back().hIt);
	TreeNode* pParentNode = pNode->pParent;
	if (pParentNode->strLabel == _T("bookmarks_showposition") && pParentNode->pParent->dwUserData == NULL)
	{
		AddShowpositionToContents(pParentNode, m_contents.size());
		DeleteItem((HTREEITEM) pParentNode);
	}
	EndBatchUpdate();
}

void CBookmarksView::AddShowpositionToContents(TreeNode* pParentNode, int nCount)
{
	int i = 0;
	int nPageCount = m_pSource->GetPageCount();

	TreeNode* pPositionNode = pParentNode->pChild;
	TreeNode* pLastChildNode = pParentNode->pLastChild;

	list<CString> strList;
	for (; ; pPositionNode = pPositionNode->pNext)
	{
		strList.push_back(pPositionNode->strLabel);
		if (pPositionNode == pParentNode->pLastChild)
			break;
	}
	vector <ShowPosition> vShowPosition;
	vShowPosition.resize(nCount - strList.size() - 1);
	
	for (list<CString>::iterator itList = strList.begin(); itList != strList.end(); ++itList, ++i)
	{
		int nItemPos = -1;
		CString strText = (*itList).Trim();
		strText.Replace(_T(","),_T(";"));
		if (strText[0] == 't')
		{
			vShowPosition[i].type = text;
			strText.Replace(_T("t"),_T(""));
		}
		CString strFirst, strSecond, strThird, strFourth;
		int nFirst, nSecond, nThird, nFourth;

		int nPos = strText.Find('*');
		if (nPos != -1)
		{
			--i;
			strFirst = strText.Mid(0, nPos);
			strText = strText.Mid(nPos + 1, strText.GetLength() - nPos - 1);
			if (_stscanf(strFirst, _T("%d"), &nFirst) == 1)
				nItemPos = nFirst - 1;
			else
				continue;
		}

		nPos = strText.Find(';');
		if (nPos != -1)
		{
			if (nItemPos >= 0 && IsRectCoord(strText, vShowPosition[nItemPos].rects))
			{
				vShowPosition[nItemPos].type = selection;
			}
			else
			{
				strFirst = strText.Mid(0, nPos);
				strSecond = strText.Mid(nPos + 1, strText.GetLength() - nPos - 1);
				nPos = strSecond.Find(';');
				if (nPos != -1)
				{
					strThird = strSecond.Mid(nPos + 1, strSecond.GetLength() - nPos - 1);
					strSecond = strSecond.Mid(0, nPos);
					nPos = strThird.Find(';');
					if (nPos != -1)
					{
						strFourth  = strThird.Mid(nPos + 1, strThird.GetLength() - nPos - 1);
						strThird = strThird.Mid(0, nPos);
					}
				}
			}
		}
		else
		{
			strFirst = _T("0");
			strSecond = strText;
		}
		if (_stscanf(strFirst, _T("%d"), &nFirst) == 1 && _stscanf(strSecond, _T("%d"), &nSecond) == 1)
		{
			if (nItemPos >= 0)
			{
				if (_stscanf(strThird, _T("%d"), &nThird) == 1 && _stscanf(strFourth, _T("%d"), &nFourth) == 1)
				{
					{
						GRect rect = GRect(nFirst, nSecond, nThird, nFourth);
						vShowPosition[nItemPos].type = selection;
						vShowPosition[nItemPos].rects.push_back(rect);
					}
				}
			}
			else if (nFirst + nSecond > 0 && strText.Find('-') == -1)
			{
				if (vShowPosition[i].type == text)
				{
					vShowPosition[i].textStart = max(nFirst, 0);
					vShowPosition[i].textLen = max(nSecond, 1);
				}
				else
				{
					vShowPosition[i].type = view;
					vShowPosition[i].nX = max(nFirst, 0);
					vShowPosition[i].nY = nSecond;
				}
			}
			else if (strText.Find('.') != -1 || strText.Find('-') != -1)
			{
				vShowPosition[i].type = url;
				vShowPosition[i].strURL = _T("?&showposition=") + strText;
			}
			else
				continue;
		}
		else
			continue;
	}
	list<BookmarkInfo>::iterator it = m_links.begin();
	for (i = 0; reinterpret_cast<TreeNode*>(it->hIt) != pParentNode; ++it)
	{
		GUTF8String strURL = it->strURL;
		int Num = m_pSource->GetUrlToPagenum(strURL);
		if (Num >= 0 && Num < nPageCount)
		{
			Bookmark* bm = it->pBookmark;
			bm->nPage = Num;
			if (vShowPosition[i].type == view)
			{
				bm->nLinkType = Bookmark::View;
				bm->ptOffset.x = vShowPosition[i].nX;
				bm->ptOffset.y = vShowPosition[i].nY;
				if (bm->ptOffset.y <= 0)
				{
					PageInfo pInfo = m_pSource->GetPageInfo(Num);
					bm->ptOffset.y += pInfo.szPage.cy;
				}
			}
			else if (vShowPosition[i].type == text)
			{
				bm->nLinkType = Bookmark::Text;
				bm->textStart = vShowPosition[i].textStart;
				bm->textLen = vShowPosition[i].textLen;
			}
			else if (vShowPosition[i].type == url)
			{
				bm->nLinkType = Bookmark::URL;
				bm->strURL = MakeUTF8String(vShowPosition[i].strURL + _T("&page=")) + GUTF8String(Num + 1);
			}
			else if (vShowPosition[i].type == selection)
			{
				bm->nLinkType = Bookmark::URL;
				CString strText = _T("?&showposition=");
				for (list<GRect>::iterator rect_it = vShowPosition[i].rects.begin(); rect_it != vShowPosition[i].rects.end(); ++rect_it)
				{
					strText = strText + MakeCString(rect_it->xmin) + _T(';') + MakeCString(rect_it->ymin) + _T(';') + 
						MakeCString(rect_it->width()) + _T(';') + MakeCString(rect_it->height()) + _T(';');
				}
				strText.TrimRight(';');
				bm->strURL = MakeUTF8String(strText + _T("&page=")) + GUTF8String(Num + 1);
			}
		}

		if ( ++i == vShowPosition.size())
			break;
	}
}

bool CBookmarksView::IsRectCoord(CString strString, list<GRect>& rects)
{
	strString += _T(';');
	bool bResult = false;
	int nCount = 0;
	int nPos = 0;
	int nOldPos = 0;
	while ((nPos = strString.Find(';', nPos)) > 0)
	{
		if (++nCount % 4 != 0)
		{
			++nPos;
			continue;
		}
		CString str = strString.Mid(nOldPos, nPos - nOldPos);
		nOldPos = ++nPos;
		int n1, n2, n3, n4;
		str.Replace(_T(';'),_T(' '));
		n1 = n2 = n3 = n4 = -1;
		if (_stscanf(str, _T("%d%d%d%d"), &n1, &n2, &n3, &n4) != 4)
			return bResult;
		rects.push_back(GRect(n1, n2, n3, n4));
		bResult = true;
	}
	return bResult;
}

int CBookmarksView::SearchInContents(GUTF8String& strFind, BOOL& MatchCase, BOOL& WholeWordsOnly, bool Prev)
{
	bool hitBeforeSel = false;
	bool hitAfterSel = false;
	bool firstEntry = true;

	HTREEITEM selItem = GetSelectedItem();

	if (!selItem) 
		hitAfterSel = true;

	list<BookmarkInfo>::iterator itStart;
	list<BookmarkInfo>::iterator itEnd;

	if (Prev)
	{
		itStart = --m_links.end();
		itEnd = m_links.begin();
	}
	else
	{
		itStart = m_links.begin();
		itEnd = --m_links.end();
	}

	list<BookmarkInfo>::iterator it = itStart;

	do
	{
		if (!firstEntry)
			Prev ? --it : ++it;

		firstEntry = false;

		HTREEITEM hItem = it->hIt;
		TreeNode* pNode = reinterpret_cast<TreeNode*>(hItem);

		if (hItem == selItem) 
		{
			hitAfterSel = true;
			//continue;
		}

		GUTF8String strTitleUTF8 = MatchCase ? MakeUTF8String(pNode->strLabel) : MakeUTF8String(pNode->strLabel).upcase();

		int nPos = 0;		
		for(;;)
		{
			nPos = strTitleUTF8.search(strFind, nPos);
			if (nPos == -1)
				break;
			int nEnd = nPos + strFind.length();
			if (!WholeWordsOnly || ((nPos == 0 || strTitleUTF8[nPos - 1] == 0x20) && 
				(nEnd == strTitleUTF8.length() || strTitleUTF8[nEnd] == 0x20)))
				break;
			nPos = nEnd;
		}

		if (nPos == -1)
			continue;
		hitBeforeSel = true;


		if (!hitAfterSel || hItem == selItem) 
			continue;

		ExpandParent(hItem);
		SelectItem(hItem);
		return 1;	// совпадение есть	
	} while (it != itEnd);

	if (hitBeforeSel && selItem) 
	{		
		SelectNode(NULL);
		return 2; // совпадения есть выше по дереву
	}
	return 0; // совпадений нет
}





void CBookmarksView::SearchAllInContents(CBookmarksView* contentsTree, GUTF8String& strFind, BOOL& MatchCase, BOOL& WholeWordsOnly, int& nResult)
{
	BeginBatchUpdate();
	DeleteAllItems();
	m_links.clear();
	list<BookmarkInfo>::iterator it;
	for (it = contentsTree->m_links.begin(); it != contentsTree->m_links.end(); ++it)
	{
		TreeNode* pNode = reinterpret_cast<TreeNode*>(it->hIt);
		GUTF8String strTitleUTF8 = MatchCase ? MakeUTF8String(pNode->strLabel) : MakeUTF8String(pNode->strLabel).upcase();
			
		int nPos = 0;		
		for(;;)
		{
			nPos = strTitleUTF8.search(strFind, nPos);
			if (nPos == -1)
				break;
			int nEnd = nPos + strFind.length();
			if (!WholeWordsOnly || ((nPos == 0 || strTitleUTF8[nPos - 1] == 0x20) && 
				(nEnd == strTitleUTF8.length() || strTitleUTF8[nEnd] == 0x20)))
				break;
			nPos = nEnd;
		}
		if (nPos == -1)
			continue;
		HTREEITEM hItem = InsertItem(pNode->strLabel, 0, 1, TVI_ROOT);
		m_links.push_back(BookmarkInfo());
		BookmarkInfo& info = m_links.back();
		info.strURL = it->strURL;
		info.pBookmark = it->pBookmark;
		info.hIt = it->hIt;

		SetItemData(hItem, (DWORD_PTR)&info, pNode->nLink);
		++nResult;
	}
	EndBatchUpdate();
}

void CBookmarksView::LoadUserBookmarks()
{
	BeginBatchUpdate();
	DeleteAllItems();
	m_links.clear();

	DocSettings* pSettings = m_pSource->GetSettings();

	list<Bookmark>::iterator it;
	for (it = pSettings->bookmarks.begin(); it != pSettings->bookmarks.end(); ++it)
		AddBookmark(*it, TVI_ROOT);

	EndBatchUpdate();
}

//void CBookmarksView::AddBookmarks(const GPList<DjVmNav::DjVuBookMark>& bookmarks,
//	HTREEITEM hParent, GPosition& pos, int nCount)
//{
//	for (int i = 0; i < nCount && !!pos; ++i)
//	{
//		const GP<DjVmNav::DjVuBookMark> bm = bookmarks[pos];
//
//		bool hasLink = true;
//
//		CString strTitle = MakeCString(bm->displayname);
//
//		HTREEITEM hItem = InsertItem(strTitle, 0, 1, hParent);
//		m_links.push_back(BookmarkInfo());
//		BookmarkInfo& info = m_links.back();
//
//		info.strURL = bm->url;
//		info.pBookmark = NULL;
//		if (info.strURL.length() == 0) 
//			hasLink = false;
//
//		info.hIt = hItem;
//		SetItemData(hItem, (DWORD_PTR)&info, MakeCString(info.strURL));
//		AddBookmarks(bookmarks, hItem, ++pos, bm->count);
//	}
//}

void CBookmarksView::AddBookmarks(const GPList<DjVmNav::DjVuBookMark>& bookmarks,
	HTREEITEM hParent, GPosition& pos, int nCount)
{
	for (int i = 0; i < nCount && !!pos; ++i)
	{
		const GP<DjVmNav::DjVuBookMark> bm = bookmarks[pos];

		//bool hasLink = true;
		CString strTitle = MakeCString(bm->displayname);

		HTREEITEM hItem = InsertItem(strTitle, 0, 1, hParent);
		m_links.push_back(BookmarkInfo());
		BookmarkInfo& info = m_links.back();
		m_contents.push_back(Bookmark());

		Bookmark& bookmark = m_contents.back();
		bookmark.nLinkType = Bookmark::URL;
		bookmark.strURL = bm->url;
		bookmark.strTitle = bm->displayname;

		info.strURL = bm->url;
		//info.pBookmark = NULL;
		info.pBookmark = &bookmark;
		//if (info.strURL.length() == 0) 
		//	hasLink = false;

		info.hIt = hItem;
		SetItemData(hItem, (DWORD_PTR)&info, MakeCString(info.strURL));
		AddBookmarks(bookmarks, hItem, ++pos, bm->count);
	}
}

void CBookmarksView::AddBookmark(Bookmark& bookmark)
{
	HTREEITEM hItem = AddBookmark(bookmark, TVI_ROOT);
	SelectItem(hItem);
}

HTREEITEM CBookmarksView::AddBookmark(Bookmark& bookmark, HTREEITEM hParent)
{
	CString strTitle = MakeCString(bookmark.strTitle);
	HTREEITEM hItem = InsertItem(strTitle, 0, 1, hParent);

	m_links.push_back(BookmarkInfo());
	BookmarkInfo& info = m_links.back();
	info.strURL = bookmark.strURL;
	info.pBookmark = &bookmark;
	info.hIt = hItem;
	CString nLink = info.strURL.length() > 0 ? MakeCString(info.strURL) :  _T("#") + MakeCString( info.pBookmark->nPage + 1);
	SetItemData(hItem, (DWORD_PTR)&info, nLink);

	list<Bookmark>::iterator it;
	for (it = bookmark.children.begin(); it != bookmark.children.end(); ++it)
		AddBookmark(*it, hItem);

	return hItem;
}

void CBookmarksView::GoToBookmark(HTREEITEM hItem)
{
	BookmarkInfo* pInfo = (BookmarkInfo*) GetItemData(hItem);
	if (pInfo->pBookmark != NULL)
	{
		if (pInfo->pBookmark->HasLink())
			UpdateObservers(BookmarkMsg(BOOKMARK_CLICKED, pInfo->pBookmark));
	}
	else
	{
		if (pInfo->strURL.length() > 0)
			UpdateObservers(LinkClicked(pInfo->strURL));
	}
}

void CBookmarksView::OnSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

	if (pNMTreeView->action == TVC_BYMOUSE)
	{
		HTREEITEM hItem = pNMTreeView->itemNew.hItem;
		if (hItem != NULL)
			GoToBookmark(hItem);
	}

	*pResult = 0;
}

void CBookmarksView::OnItemClicked(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

	if (pNMTreeView->action == TVC_BYMOUSE)
	{
		HTREEITEM hItem = pNMTreeView->itemNew.hItem;
		if (hItem != NULL)
			GoToBookmark(hItem);
	}

	*pResult = 0;
}

void CBookmarksView::OnKeyDown(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMTVKEYDOWN pTVKeyDown = reinterpret_cast<LPNMTVKEYDOWN>(pNMHDR);

	if (pTVKeyDown->wVKey == VK_RETURN || pTVKeyDown->wVKey == VK_SPACE)
	{
		HTREEITEM hItem = GetSelectedItem();
		if (hItem != NULL)
			GoToBookmark(hItem);

		*pResult = 0;
	}
	else
		*pResult = 1;
}

void CBookmarksView::OnUpdate(const Observable* source, const Message* message)
{
	if (message->code == APP_SETTINGS_CHANGED)
	{
		SetWrapLabels(theApp.GetAppSettings()->bWrapLongBookmarks);
	}
}

void CBookmarksView::OnContextMenu(CWnd* pWnd, CPoint point)
{
	TreeNode* pNode = m_pSelection;
	if (!m_bEnableEditing || pNode == NULL || pNode == m_pRoot)
		return;

	CRect rcClient = ::GetClientRect(this);
	ClientToScreen(rcClient);

	if (!rcClient.PtInRect(point))
	{
		point = CPoint(pNode->rcLabel.left + 2, pNode->rcLabel.bottom)
				+ rcClient.TopLeft() - GetScrollPosition();
	}

	CMenu menu;
	menu.LoadMenu(IDR_POPUP);

	CMenu* pPopup = menu.GetSubMenu(1);
	ASSERT(pPopup != NULL);

	if (pNode->pNext == NULL)
		pPopup->EnableMenuItem(ID_BOOKMARK_MOVEDOWN, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
	if (pNode->pParent->pChild == pNode)
		pPopup->EnableMenuItem(ID_BOOKMARK_MOVEUP, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

	int nCommand = pPopup->TrackPopupMenu(TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD,
			point.x, point.y, this);

	switch (nCommand)
	{
	case ID_BOOKMARK_DELETE_ALL:
		DeleteAllBookmarks();
		break;

	case ID_BOOKMARK_DELETE:
		DeleteBookmark(pNode);
		break;
	
	case ID_BOOKMARK_RENAME:
		RenameBookmark(pNode);
		break;

	case ID_BOOKMARK_SETDESTINATION:
		SetBookmarkDestination(pNode);
		break;

	case ID_BOOKMARK_MOVEUP:
	case ID_BOOKMARK_MOVEDOWN:
		MoveBookmark(pNode, nCommand == ID_BOOKMARK_MOVEUP);
		break;
	}
}

LRESULT CBookmarksView::OnShowSettings(WPARAM wParam, LPARAM lParam)
{
	CMenu menu;
	menu.LoadMenu(IDR_POPUP);

	CMenu* pPopup = menu.GetSubMenu(3);
	ASSERT(pPopup != NULL);

	CRect rcButton = (LPRECT) lParam;
	TPMPARAMS tpm;
	tpm.cbSize = sizeof(tpm);
	tpm.rcExclude = rcButton;

	bool bCanToggleTopLevel = false;
	bool bExpandTopLevel = true;
	for (TreeNode* pNode = m_pRoot->pChild; pNode != NULL; pNode = pNode->pNext)
	{
		if (pNode->pChild != NULL)
		{
			bCanToggleTopLevel = true;
			if (!pNode->bCollapsed)
				bExpandTopLevel = false;
		}
	}

	CString strToggle;
	AfxExtractSubString(strToggle, LoadString(IDS_BOOKMARK_TOP_LEVEL), bExpandTopLevel ? 0 : 1);
	pPopup->ModifyMenu(ID_BOOKMARK_TOP_LEVEL, MF_BYCOMMAND | MF_STRING,
			ID_BOOKMARK_TOP_LEVEL, strToggle);
	if (!bCanToggleTopLevel)
	{
		pPopup->EnableMenuItem(ID_BOOKMARK_TOP_LEVEL, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
		pPopup->EnableMenuItem(ID_BOOKMARK_ALL_LEVEL, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
	}
	else if (!HasCollapsedChildren(m_pRoot->pChild))
		pPopup->EnableMenuItem(ID_BOOKMARK_ALL_LEVEL, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

	if (theApp.GetAppSettings()->bFindBookmarkTitle)
		pPopup->CheckMenuItem(ID_SEARCH_BOOKMARK_TITLE, MF_BYCOMMAND | MF_CHECKED);

	if (theApp.GetAppSettings()->bWrapLongBookmarks)
		pPopup->CheckMenuItem(ID_BOOKMARK_WRAP, MF_BYCOMMAND | MF_CHECKED);

	int nID = ::TrackPopupMenuEx(pPopup->m_hMenu, TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD,
			rcButton.left, rcButton.bottom, m_hWnd, &tpm);

	if (nID == ID_BOOKMARK_WRAP)
	{
		theApp.GetAppSettings()->bWrapLongBookmarks = !theApp.GetAppSettings()->bWrapLongBookmarks;
		theApp.UpdateObservers(APP_SETTINGS_CHANGED);
	}
	else if (nID == ID_SEARCH_BOOKMARK_TITLE)
	{
		theApp.GetAppSettings()->bFindBookmarkTitle = !theApp.GetAppSettings()->bFindBookmarkTitle;
	}
	else if (nID == ID_BOOKMARK_TOP_LEVEL && bCanToggleTopLevel)
	{
		BeginBatchUpdate();
		for (TreeNode* pNode = m_pRoot->pChild; pNode != NULL; pNode = pNode->pNext)
		{
			ExpandNode(pNode, bExpandTopLevel);
			if (bExpandTopLevel && pNode->pChild != NULL)
			{
				for (TreeNode* pChildNode = pNode->pChild; pChildNode != NULL; pChildNode = pChildNode->pNext)
					ExpandNode(pChildNode, false);
			}
		}
		EndBatchUpdate();
	}
	else if (nID == ID_BOOKMARK_ALL_LEVEL && bCanToggleTopLevel)
	{
		BeginBatchUpdate();
		for (TreeNode* pNode = m_pRoot->pChild; pNode != NULL; pNode = pNode->pNext)
			ExpandNode(pNode, true, true);
		EndBatchUpdate();
	}
	return 0;
}

void CBookmarksView::DeleteBookmark(TreeNode* pNode)
{
	if (pNode == NULL)
		return;

	BookmarkInfo* pInfo = (BookmarkInfo*) pNode->dwUserData;
	if (pInfo->pBookmark == NULL)
		return;

	if (AfxMessageBox(IDS_PROMPT_BOOKMARK_DELETE, MB_ICONEXCLAMATION | MB_YESNO) == IDYES)
	{
		m_pSource->GetSettings()->DeleteBookmark(pInfo->pBookmark);
		DeleteItem((HTREEITEM) pNode);

		pInfo->pBookmark = NULL;
		pInfo->strURL.empty();
	}

	SetFocus();
}

int CBookmarksView::GetBookmarksCount()
{
	int nCount = 0;
	list<BookmarkInfo>::iterator it;
	for (it = m_links.begin(); it != m_links.end(); ++it)
	{
		TreeNode* pNode = reinterpret_cast<TreeNode*>(it->hIt);

		if (pNode == NULL)
			continue;

		BookmarkInfo* pInfo = (BookmarkInfo*) pNode->dwUserData;
		if (pInfo->pBookmark != NULL)
			nCount++;
	}
	return nCount;
}

void CBookmarksView::DeleteAllBookmarks()
{
	if (AfxMessageBox(IDS_PROMPT_BOOKMARK_DELETE_ALL, MB_ICONEXCLAMATION | MB_YESNO) == IDYES)
	{
		list<BookmarkInfo>::iterator it;
		for (it = m_links.begin(); it != m_links.end(); ++it)
		{
			TreeNode* pNode = reinterpret_cast<TreeNode*>(it->hIt);

			if (pNode == NULL)
				continue;

			BookmarkInfo* pInfo = (BookmarkInfo*) pNode->dwUserData;
			if (pInfo->pBookmark == NULL)
				continue;
			m_pSource->GetSettings()->DeleteBookmark(pInfo->pBookmark);
			DeleteItem((HTREEITEM) pNode);

			pInfo->pBookmark = NULL;
			pInfo->strURL.empty();
		}
	}

	SetFocus();
}

void CBookmarksView::RenameBookmark(TreeNode* pNode)
{
	if (pNode == NULL)
		return;

	BookmarkInfo* pInfo = (BookmarkInfo*) pNode->dwUserData;
	if (pInfo->pBookmark == NULL)
		return;

	CBookmarkDlg dlg(IDS_RENAME_BOOKMARK);
	dlg.m_strTitle = pNode->strLabel;

	if (dlg.DoModal())
	{
		pNode->strLabel = dlg.m_strTitle;
		pInfo->pBookmark->strTitle = MakeUTF8String(dlg.m_strTitle);
		RecalcLayout();
	}

	SetFocus();
}

void CBookmarksView::SetBookmarkDestination(TreeNode* pNode)
{
	if (pNode == NULL)
		return;

	BookmarkInfo* pInfo = (BookmarkInfo*) pNode->dwUserData;
	if (pInfo->pBookmark == NULL)
		return;

	if (AfxMessageBox(IDS_PROMPT_BOOKMARK_DESTINATION, MB_ICONEXCLAMATION | MB_YESNO) == IDYES)
	{
		CDjVuView* pView = (CDjVuView*) GetTopLevelFrame()->GetActiveView();
		ASSERT(pView != NULL);
		pView->CreateBookmarkFromView(*pInfo->pBookmark);
	}

	SetFocus();
}

void CBookmarksView::OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu)
{
	CMyTreeView::OnMenuSelect(nItemID, nFlags, hSysMenu);

	GetTopLevelFrame()->SendMessage(WM_MENUSELECT, MAKEWPARAM(nItemID, nFlags),
			(LPARAM) hSysMenu);
}

void CBookmarksView::OnEnterIdle(UINT nWhy, CWnd* pWho)
{
	CMyTreeView::OnEnterIdle(nWhy, pWho);

	GetTopLevelFrame()->SendMessage(WM_ENTERIDLE, nWhy, (LPARAM) pWho->GetSafeHwnd());
}

void CBookmarksView::MoveBookmark(TreeNode* pNode, bool bUp)
{
	if (pNode == m_pRoot)
		return;

	TreeNode* pSwapNode = pNode->pNext;
	if (bUp)
	{
		pSwapNode = pNode->pParent->pChild;
		while (pSwapNode != NULL && pSwapNode != pNode && pSwapNode->pNext != NULL && pSwapNode->pNext != pNode)
			pSwapNode = pSwapNode->pNext;
	}
	if (pSwapNode == NULL || pSwapNode == pNode)
		return;

	BookmarkInfo* pInfo = (BookmarkInfo*) pNode->dwUserData;
	BookmarkInfo* pSwapInfo = (BookmarkInfo*) pSwapNode->dwUserData;

	pInfo->pBookmark->swap(*pSwapInfo->pBookmark);
	swap(pNode->strLabel, pSwapNode->strLabel);

	RecalcLayout();
	SelectNode(pSwapNode);
}

int CBookmarksView::OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message)
{
	// From MFC: CView::OnMouseActivate
	// Don't call CFrameWnd::SetActiveView

	int nResult = CWnd::OnMouseActivate(pDesktopWnd, nHitTest, message);
	if (nResult == MA_NOACTIVATE || nResult == MA_NOACTIVATEANDEAT)
		return nResult;

	if (message == WM_LBUTTONDOWN)
	{
		// set focus to this view, but don't notify the parent frame
		OnActivateView(true, this, this);
	}

	return nResult;
}