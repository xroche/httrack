// mainfrm.h : interface of the CHtmlFrame class
//
// This is a part of the Microsoft Foundation Classes C++ library.
// Copyright (C) 1992-1998 Microsoft Corporation
// All rights reserved.
//
// This source code is only intended as a supplement to the
// Microsoft Foundation Classes Reference and related
// electronic documentation provided with the library.
// See these sources for detailed information regarding the
// Microsoft Foundation Classes product.



#ifndef __AFXEXT_H__
#include <afxext.h>         // for access to CToolBar and CStatusBar
#endif

class CHtmlFrame : public CMDIFrameWnd
{
	DECLARE_DYNCREATE(CHtmlFrame)

protected:
	CStatusBar  m_wndStatusBar;
//	CToolBar    m_wndToolBar;

protected:
	//{{AFX_MSG(CHtmlFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
