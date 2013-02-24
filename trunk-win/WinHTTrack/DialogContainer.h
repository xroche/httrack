#if !defined(AFX_DIALOGCONTAINER_H__4F25D0C2_5DF4_4149_BD78_FCB2CD06E2DB__INCLUDED_)
#define AFX_DIALOGCONTAINER_H__4F25D0C2_5DF4_4149_BD78_FCB2CD06E2DB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DialogContainer.h : header file
//

// Includes pour objet encapsulé
#include "WizTab.h"

/////////////////////////////////////////////////////////////////////////////
// CDialogContainer form view

#ifndef __AFXEXT_H__
#include <afxext.h>
#endif

class CDialogContainer : public CFormView
{
protected:
	CDialogContainer();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CDialogContainer)

  CWizTab* tab;
  CWizTab* tab2;
  BOOL scrollsize_declared;
  int view_w,view_h;

// Form Data
public:
	//{{AFX_DATA(CDialogContainer)
	enum { IDD = IDD_DIALOGCONTAINER_FORM };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDialogContainer)
	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	virtual void OnInitialUpdate();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CDialogContainer();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
	//{{AFX_MSG(CDialogContainer)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DIALOGCONTAINER_H__4F25D0C2_5DF4_4149_BD78_FCB2CD06E2DB__INCLUDED_)
