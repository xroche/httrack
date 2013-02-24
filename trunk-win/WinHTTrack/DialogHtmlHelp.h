#if !defined(AFX_DIALOGHTMLHELP_H__FED0CE81_AB10_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_DIALOGHTMLHELP_H__FED0CE81_AB10_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DialogHtmlHelp.h : header file
//

#include "resource.h"
//#include "LaunchHelpBase.h"
#include "HtmlCtrl.h"

// type callback
typedef void (* Helper_CallBack ) (CWnd*);
typedef void (* OnSize_CallBack ) (CWnd*,UINT nType, int cx, int cy); 

/////////////////////////////////////////////////////////////////////////////
// CDialogHtmlHelp dialog

class CDialogHtmlHelp : public CDialog
{
// Construction
public:
  void Go(CString st);
	CDialogHtmlHelp(CWnd* pParent = NULL);   // standard constructor
  Helper_CallBack callback;
  OnSize_CallBack callbackOnSize;
  char* GetTip(int id);
  //
  CString page;
protected:
	CHtmlCtrl m_page;
	CToolBar     m_wndToolBar;
  char home[1024];
  char home_dir[1024];
//private:
//  LaunchHelpBase base;

// Dialog Data
	//{{AFX_DATA(CDialogHtmlHelp)
	enum { IDD = IDD_HtmlHelp };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDialogHtmlHelp)
	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL
	afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );

// Implementation
protected:
  void OnHelpInfo2();

	// Generated message map functions
	//{{AFX_MSG(CDialogHtmlHelp)
	virtual BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnBack();
	afx_msg void OnForward();
	afx_msg void OnStop();
	afx_msg void OnReload();
	afx_msg void OnHome();
	afx_msg void OnFilePrint();
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DIALOGHTMLHELP_H__FED0CE81_AB10_11D3_A2B3_0000E84E7CA1__INCLUDED_)
