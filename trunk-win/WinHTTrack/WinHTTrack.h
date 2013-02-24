// WinHTTrack.h : main header file for the WINHTTRACK application
//

#if !defined(AFX_WINHTTRACK_H__C54E332B_F6D3_4407_A9CC_77943F9B45CC__INCLUDED_)
#define AFX_WINHTTRACK_H__C54E332B_F6D3_4407_A9CC_77943F9B45CC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols
#include "shell.h"

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackApp:
// See WinHTTrack.cpp for the implementation of this class
//

// En-tête pour l'affichage des tabs
#include "NewProj.h"
#include "Wid1.h"
#include "trans.h"
#include "FirstInfo.h"
#include "inprogress.h"
#include "infoend.h"


class CWinHTTrackApp : public CWinApp
{
public:
	CWinHTTrackApp();
	~CWinHTTrackApp();
  void NewTabs();       /* recréer control tabs */
  //
  CFirstInfo*        m_tab0;
  CNewProj*          m_tab1;
  Wid1*              m_tab2;
  Ctrans*            m_tab3;
  Cinprogress*       m_tabprogress;
  Cinfoend*          m_tabend;
private:
  void DeleteTabs();
  //
  BOOL RmDir(CString srcpath);
  BOOL WSockInit();
  CShellApp app;
  void OnHelpInfo2();
  BOOL OnHelpInfo(HELPINFO* dummy);
  //

  // Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWinHTTrackApp)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

// Implementation
	//{{AFX_MSG(CWinHTTrackApp)
	afx_msg void OnFileSave();
	afx_msg void OnFileSaveAs();
	afx_msg void OnFileMruFile1();
	//}}AFX_MSG
 	//afx_msg void OnWizard();
  afx_msg void OnFileNew( );
  afx_msg void OnFileOpen( );
	afx_msg void OnAppAbout();
	afx_msg void OnViewRestart();
	afx_msg void OnWizRequest1();
	afx_msg void OnWizRequest2();
	afx_msg void OnWizRequest3();
  afx_msg void OnFileDelete();
  afx_msg void OnBrowseWebsites();
  //
	afx_msg void Oniplog(int);
	afx_msg void OniplogLog();
	afx_msg void OniplogErr();
  afx_msg void OnPause();
  afx_msg void OnViewTransfers();
  afx_msg void OnModifyOpt();
	afx_msg void Onipabout();
  afx_msg void OnUpdate();
  // Fwd
  afx_msg void FwOnLoadprofile();
  afx_msg void FwOnSaveprofile();
  afx_msg void FwOnLoaddefault();
  afx_msg void FwOnSavedefault();
  afx_msg void FwOnResetdefault();
  //
  afx_msg void FwOnModifyOpt();
  afx_msg void FwOnPause();
  afx_msg void FwOniplogLog();
  afx_msg void FwOniplogErr();
  afx_msg void FwOnViewTransfers();
  //
  void FwOnhide();
  //
  afx_msg void OnEndMirror();
  //
  virtual CDocument* OpenDocumentFile( LPCTSTR lpszFileName);
  DECLARE_MESSAGE_MAP()
};

extern CWinHTTrackApp* this_app;
static CWnd* GetMainWindow() {
  CWnd* wnd = AfxGetMainWnd();
  if (wnd == NULL && this_app != NULL)
    wnd = this_app->GetMainWnd();
  return wnd;
}

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WINHTTRACK_H__C54E332B_F6D3_4407_A9CC_77943F9B45CC__INCLUDED_)
