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
#include "CropPagesDlg.h"
#include "DjVuSource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CCropPagesDlg dialog

IMPLEMENT_DYNAMIC(CCropPagesDlg, CMyDialog)

CCropPagesDlg::CCropPagesDlg(CWnd* pParent)
	: CMyDialog(CCropPagesDlg::IDD, pParent), m_bCropWhiteMargins(false), m_nMinWhiteMargins(0),
	m_nWhitePoint(0), m_nRangeType(CurrentPage), m_nPageFrom(1), m_nPageTo(1), m_nPageCount(1),
	m_nApplyRange(0)
{
	m_rcCropPage = CRect();
	m_szPageSize = CSize();
}

CCropPagesDlg::~CCropPagesDlg()
{
}

void CCropPagesDlg::DoDataExchange(CDataExchange* pDX)
{
	CMyDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_CROP_TOP, m_rcCropPage.top);
	DDX_Text(pDX, IDC_CROP_BOTTOM, m_rcCropPage.bottom);
	DDX_Text(pDX, IDC_CROP_LEFT, m_rcCropPage.left);
	DDX_Text(pDX, IDC_CROP_RIGHT, m_rcCropPage.right);
	DDX_Check(pDX, IDC_WHITE_MARGINS, m_bCropWhiteMargins);
	DDX_Text(pDX, IDC_MIN_MARGINS, m_nMinWhiteMargins);
	DDX_Text(pDX, IDC_WHITE_POINT, m_nWhitePoint);
	DDX_Radio(pDX, IDC_PAGE_RANGE_CURRENT, m_nRangeType);
	DDX_Text(pDX, IDC_PAGE_RANGE_FROM, m_nPageFrom);
	DDX_Text(pDX, IDC_PAGE_RANGE_TO, m_nPageTo);
	CString strPageCount = FormatString(IDS_OF_PAGE_COUNT, m_nPageCount);
	DDX_Text(pDX, IDC_PAGE_COUNT, strPageCount);
	DDX_Control(pDX, IDC_COMBO_PAGESINRANGE, m_cboPagesInRange);
}


BEGIN_MESSAGE_MAP(CCropPagesDlg, CMyDialog)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_CROP_TOP, &CCropPagesDlg::OnDeltaposSpinCropTop)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_CROP_BOTTOM, &CCropPagesDlg::OnDeltaposSpinCropBottom)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_CROP_LEFT, &CCropPagesDlg::OnDeltaposSpinCropLeft)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_CROP_RIGHT, &CCropPagesDlg::OnDeltaposSpinCropRight)
	ON_EN_KILLFOCUS(IDC_CROP_TOP, &CCropPagesDlg::OnEnKillfocusCropTop)
	ON_EN_KILLFOCUS(IDC_CROP_BOTTOM, &CCropPagesDlg::OnEnKillfocusCropBottom)
	ON_EN_KILLFOCUS(IDC_CROP_LEFT, &CCropPagesDlg::OnEnKillfocusCropLeft)
	ON_EN_KILLFOCUS(IDC_CROP_RIGHT, &CCropPagesDlg::OnEnKillfocusCropRight)
	ON_BN_CLICKED(IDC_WHITE_MARGINS, &CCropPagesDlg::OnBnClickedWhiteMargins)
	ON_EN_KILLFOCUS(IDC_WHITE_POINT, &CCropPagesDlg::OnEnKillfocusWhitePoint)
	ON_BN_CLICKED(IDC_ZERO_MARGINS, &CCropPagesDlg::OnBnClickedZeroMargins)
	ON_CONTROL_RANGE(BN_CLICKED, IDC_PAGE_RANGE_CURRENT, IDC_PAGE_RANGE_CUSTOM, OnCropRange)
	ON_EN_KILLFOCUS(IDC_PAGE_RANGE_FROM, &CCropPagesDlg::OnEnKillfocusPageRangeFrom)
	ON_CBN_SELCHANGE(IDC_COMBO_PAGESINRANGE, &CCropPagesDlg::OnCbnSelchangeComboPagesinrange)
	ON_EN_KILLFOCUS(IDC_PAGE_RANGE_TO, &CCropPagesDlg::OnEnKillfocusPageRangeTo)
END_MESSAGE_MAP()


// CCropPagesDlg message handlers

BOOL CCropPagesDlg::OnInitDialog()
{
	CMyDialog::OnInitDialog();

	UpdateData();
	if (m_rcCropPage.top + m_rcCropPage.bottom > m_szPageSize.cy - 10)
		m_rcCropPage.bottom = m_szPageSize.cy - m_rcCropPage.top - 10;
	m_rcCropPage.bottom = max(m_rcCropPage.bottom, 0);

	if (m_rcCropPage.top + m_rcCropPage.bottom > m_szPageSize.cy - 10)
		m_rcCropPage.top = m_szPageSize.cy - m_rcCropPage.bottom - 10;
	m_rcCropPage.top = max(m_rcCropPage.top, 0);

	if (m_rcCropPage.left + m_rcCropPage.right > m_szPageSize.cx - 10)
		m_rcCropPage.right = m_szPageSize.cx - m_rcCropPage.left - 10;
	m_rcCropPage.right = max(m_rcCropPage.right, 0);

	if (m_rcCropPage.left + m_rcCropPage.right > m_szPageSize.cx - 10)
		m_rcCropPage.left = m_szPageSize.cx - m_rcCropPage.right - 10;
	m_rcCropPage.left = max(m_rcCropPage.left, 0);

	m_cboPagesInRange.AddString(LoadString(IDS_PRINT_ALL_PAGES));
	m_cboPagesInRange.AddString(LoadString(IDS_PRINT_EVEN_PAGES));
	m_cboPagesInRange.AddString(LoadString(IDS_PRINT_ODD_PAGES));
	m_cboPagesInRange.SetCurSel(0);

	GetDlgItem(IDC_MIN_MARGINS)->EnableWindow(m_bCropWhiteMargins);
	GetDlgItem(IDC_WHITE_POINT)->EnableWindow(m_bCropWhiteMargins);

	GetDlgItem(IDC_PAGE_RANGE_FROM)->EnableWindow(m_nRangeType == CustomRange);
	GetDlgItem(IDC_PAGE_RANGE_TO)->EnableWindow(m_nRangeType == CustomRange);

	UpdateData(false);
	return true;
}

void CCropPagesDlg::OnDeltaposSpinCropTop(NMHDR *pNMHDR, LRESULT *pResult)
{
	UpdateData();
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);

	if (pNMUpDown->iDelta < 0)
		++m_rcCropPage.top;
	else
		--m_rcCropPage.top;

	if (m_rcCropPage.top < 0)
		m_rcCropPage.top = 0;
	else if (m_rcCropPage.top + m_rcCropPage.bottom > m_szPageSize.cy - 10)
		--m_rcCropPage.top;

	UpdateData(false);

	*pResult = 0;
}

void CCropPagesDlg::OnDeltaposSpinCropBottom(NMHDR *pNMHDR, LRESULT *pResult)
{
	UpdateData();
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);

	if (pNMUpDown->iDelta < 0)
		++m_rcCropPage.bottom;
	else
		--m_rcCropPage.bottom;

	if (m_rcCropPage.bottom < 0)
		m_rcCropPage.bottom = 0;
	else if (m_rcCropPage.top + m_rcCropPage.bottom > m_szPageSize.cy - 10)
		--m_rcCropPage.bottom;

	UpdateData(false);

	*pResult = 0;
}

void CCropPagesDlg::OnDeltaposSpinCropLeft(NMHDR *pNMHDR, LRESULT *pResult)
{
	UpdateData();
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);

	if (pNMUpDown->iDelta < 0)
		++m_rcCropPage.left;
	else
		--m_rcCropPage.left;

	if (m_rcCropPage.left < 0)
		m_rcCropPage.left = 0;
	else if (m_rcCropPage.left + m_rcCropPage.right > m_szPageSize.cx - 10)
		--m_rcCropPage.left;

	UpdateData(false);

	*pResult = 0;
}

void CCropPagesDlg::OnDeltaposSpinCropRight(NMHDR *pNMHDR, LRESULT *pResult)
{
	UpdateData();
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);

	if (pNMUpDown->iDelta < 0)
		++m_rcCropPage.right;
	else
		--m_rcCropPage.right;

	if (m_rcCropPage.right < 0)
		m_rcCropPage.right = 0;
	else if (m_rcCropPage.left + m_rcCropPage.right > m_szPageSize.cx - 10)
		--m_rcCropPage.right;

	UpdateData(false);

	*pResult = 0;
}

void CCropPagesDlg::OnEnKillfocusCropTop()
{
	UpdateData();
	if (m_rcCropPage.top + m_rcCropPage.bottom > m_szPageSize.cy - 10)
		m_rcCropPage.top = m_szPageSize.cy - m_rcCropPage.bottom - 10;
	m_rcCropPage.top = max(m_rcCropPage.top, 0);
	UpdateData(false);
}

void CCropPagesDlg::OnEnKillfocusCropBottom()
{
	UpdateData();
	if (m_rcCropPage.top + m_rcCropPage.bottom > m_szPageSize.cy - 10)
		m_rcCropPage.bottom = m_szPageSize.cy - m_rcCropPage.top - 10;
	m_rcCropPage.bottom = max(m_rcCropPage.bottom, 0);
	UpdateData(false);
}

void CCropPagesDlg::OnEnKillfocusCropLeft()
{
	UpdateData();
	if (m_rcCropPage.left + m_rcCropPage.right > m_szPageSize.cx - 10)
		m_rcCropPage.left = m_szPageSize.cx - m_rcCropPage.right - 10;
	m_rcCropPage.left = max(m_rcCropPage.left, 0);
	UpdateData(false);
}

void CCropPagesDlg::OnEnKillfocusCropRight()
{
	UpdateData();
	if (m_rcCropPage.left + m_rcCropPage.right > m_szPageSize.cx - 10)
		m_rcCropPage.right = m_szPageSize.cx - m_rcCropPage.left - 10;
	m_rcCropPage.right = max(m_rcCropPage.right, 0);
	UpdateData(false);
}

void CCropPagesDlg::OnBnClickedWhiteMargins()
{
	UpdateData();
	GetDlgItem(IDC_CROP_TOP)->EnableWindow(!m_bCropWhiteMargins);
	GetDlgItem(IDC_CROP_BOTTOM)->EnableWindow(!m_bCropWhiteMargins);
	GetDlgItem(IDC_CROP_LEFT)->EnableWindow(!m_bCropWhiteMargins);
	GetDlgItem(IDC_CROP_RIGHT)->EnableWindow(!m_bCropWhiteMargins);
	GetDlgItem(IDC_MIN_MARGINS)->EnableWindow(m_bCropWhiteMargins);
	GetDlgItem(IDC_WHITE_POINT)->EnableWindow(m_bCropWhiteMargins);
}

void CCropPagesDlg::OnEnKillfocusWhitePoint()
{
	UpdateData();
	m_nWhitePoint = max(0, min(120, m_nWhitePoint));
	UpdateData(false);
}

void CCropPagesDlg::OnBnClickedZeroMargins()
{
	m_rcCropPage.top = 0;
	m_rcCropPage.bottom = 0;
	m_rcCropPage.left = 0;
	m_rcCropPage.right = 0;
	UpdateData(false);
}

void CCropPagesDlg::OnCropRange(UINT nID)
{
	UpdateData();
	GetDlgItem(IDC_PAGE_RANGE_FROM)->EnableWindow(m_nRangeType == CustomRange);
	GetDlgItem(IDC_PAGE_RANGE_TO)->EnableWindow(m_nRangeType == CustomRange);
}

void CCropPagesDlg::OnEnKillfocusPageRangeFrom()
{
	UpdateData();
	if (m_nPageFrom < 1)
		m_nPageFrom = 1;
	UpdateData(false);
}

void CCropPagesDlg::OnEnKillfocusPageRangeTo()
{
	UpdateData();
}

void CCropPagesDlg::OnCbnSelchangeComboPagesinrange()
{
	UpdateData();
	m_nApplyRange = m_cboPagesInRange.GetCurSel();
}
