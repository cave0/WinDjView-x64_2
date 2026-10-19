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
#include "MyTreeView.h"
class DjVuSource;
struct Bookmark;


// CBookmarksView window

class CBookmarksView : public CMyTreeView, public Observer, public Observable
{
	DECLARE_DYNAMIC(CBookmarksView)

public:
	CBookmarksView(DjVuSource* pSource);
	virtual ~CBookmarksView();
	void LoadContents();
	int SearchInContents(GUTF8String& strFind, BOOL& MatchCase, BOOL& WholeWordsOnly, bool Prev);
	void SearchAllInContents(CBookmarksView* contentsTree, GUTF8String& strFind, BOOL& MatchCase, BOOL& WholeWordsOnly, int& nResult);

	void LoadUserBookmarks();
	void AddBookmark(Bookmark& bookmark);
	void EnableEditing(bool bEnable = true) { m_bEnableEditing = bEnable; }
	void DeleteAllBookmarks();
	int GetBookmarksCount();

	virtual void OnUpdate(const Observable* source, const Message* message);

// Implementation
protected:
	DjVuSource* m_pSource;
	CImageList m_imageList;
	bool m_bEnableEditing;

	void GoToBookmark(HTREEITEM hItem);
	void AddBookmarks(const GPList<DjVmNav::DjVuBookMark>& bookmarks,
		HTREEITEM hParent, GPosition& pos, int nCount);

	HTREEITEM AddBookmark(Bookmark& bookmark, HTREEITEM hParent);
	void DeleteBookmark(TreeNode* pNode);
	void RenameBookmark(TreeNode* pNode);
	void SetBookmarkDestination(TreeNode* pNode);
	void MoveBookmark(TreeNode* pNode, bool bUp);
	void AddShowpositionToContents(TreeNode* pParentNode, int nCount);
	bool IsRectCoord(CString strString, list<GRect>& rects);


	struct BookmarkInfo
	{
		GUTF8String strURL;
		Bookmark* pBookmark;
		HTREEITEM hIt;
	};
	enum TypePosition
	{
		none = 0,
		text = 1,
		view = 2,
		url = 3,
		selection = 4
	};
	struct ShowPosition
	{		
		ShowPosition()
			: nX(0), nY(0), textStart(-1), textLen(1), type(none), strURL(_T("")) {}
		int nX;
		int nY;
		int textStart;
		int textLen;
		TypePosition type;
		CString strURL;
		list<GRect> rects;
	};
	list<BookmarkInfo> m_links;
	list<Bookmark> m_contents;

	// Message map functions
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemClicked(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeyDown(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu);
	afx_msg void OnEnterIdle(UINT nWhy, CWnd* pWho);
	afx_msg LRESULT OnShowSettings(WPARAM wParam, LPARAM lParam);
	afx_msg int OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message);
	DECLARE_MESSAGE_MAP()
};
