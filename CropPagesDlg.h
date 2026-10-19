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

#include "MyDialog.h"

// CCropPagesDlg dialog

class CCropPagesDlg : public CMyDialog
{
	DECLARE_DYNAMIC(CCropPagesDlg)

public:
	CCropPagesDlg(CWnd* pParent = NULL);
	virtual ~CCropPagesDlg();

// Dialog Data
	enum { IDD = IDD_CROP_PAGES };
	
	CRect m_rcCropPage;
	CSize m_szPageSize;
	BOOL m_bCropWhiteMargins;
	int m_nMinWhiteMargins;
	int m_nWhitePoint;
	int m_nRangeType;
	int m_nPageFrom;
	int m_nPageTo;
	int m_nPageCount;
	int m_nApplyRange;
	enum RangeType
	{
		CurrentPage = 0,
		AllPages = 1,
		CustomRange = 2
	};
	enum ApplyRange
	{
		EvenAndOdd = 0,
		OnlyEven = 1,
		OnlyOdd = 2
	};

protected:
	void UpdateControls();
	CComboBox m_cboPagesInRange;

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnDeltaposSpinCropTop(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDeltaposSpinCropBottom(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDeltaposSpinCropLeft(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDeltaposSpinCropRight(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnEnKillfocusCropTop();
	afx_msg void OnEnKillfocusCropBottom();
	afx_msg void OnEnKillfocusCropLeft();
	afx_msg void OnEnKillfocusCropRight();
	afx_msg void OnBnClickedWhiteMargins();
	afx_msg void OnEnKillfocusWhitePoint();
	afx_msg void OnBnClickedZeroMargins();
	afx_msg void OnCropRange(UINT nID);
	afx_msg void OnEnKillfocusPageRangeFrom();
	afx_msg void OnCbnSelchangeComboPagesinrange();
	afx_msg void OnEnKillfocusPageRangeTo();
};
