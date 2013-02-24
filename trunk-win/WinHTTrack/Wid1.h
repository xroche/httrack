#if !defined(AFX_WID1_H__8FF0CA01_F5EE_11D1_B222_006097BCBD81__INCLUDED_)
#define AFX_WID1_H__8FF0CA01_F5EE_11D1_B222_006097BCBD81__INCLUDED_

#include <afxole.h>
#include "EasyDropTarget.h"

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// Wid1.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// Wid1 dialog

class Wid1 : public CPropertyPage
{
	DECLARE_DYNCREATE(Wid1)
// Construction
public:
	Wid1();   // standard constructor
	~Wid1();   // standard constructor
  void OnChangepathlog();
  static CString TextToUrl(CString st,CLIPFORMAT cfFormat);
  //
	int filtreok;
	int cancel;
  int direction;
  int url_status;
  int filelist_status;
  int depth_status;
  int log_flip;
  int mir_status;
  int proj_status;
  int continue_status;
  int LAST_ACTION;
  int interact;     // entre les 2 champs
	// Dialog Data
	//{{AFX_DATA(Wid1)
	enum { IDD = IDD_WIZ1 };
	CStatic	m_mirtitle;
	CComboBox	m_ctl_todo;
	int		m_C1;
	CString	m_urls;
	int		m_todo;
	CString	m_infomain;
	CString	m_filelist;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(Wid1)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
public:
  void Refresh();
protected:
  void AfterInitDialog( );
  void AfterChangepathlog();
  int load_after_changes;
  char* GetTip(int id);
  void AddText(CString add_st);
	// Generated message map functions
	//{{AFX_MSG(Wid1)
	afx_msg void OnChangeUrl();
	afx_msg BOOL OnInitDialog();
	afx_msg void OnSelchangetodo();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
	afx_msg void Onsetopt();
	afx_msg void Onlogin2();
	afx_msg void Onbr();
	afx_msg void OnChangefilelist();
	//}}AFX_MSG
  virtual BOOL OnKillActive( );
  virtual BOOL OnQueryCancel( );
  virtual BOOL OnSetActive( );
  virtual LRESULT OnWizardNext();
	afx_msg void OnHelpInfo2();
  //afx_msg void OnDropFiles(HDROP hDropInfo);
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
  afx_msg LRESULT DragDropText(WPARAM wParam,LPARAM lParam);
public:
	afx_msg void OnLoadprofile();
	afx_msg void OnSaveprofile();
  afx_msg void OnLoaddefault();
  afx_msg void OnResetdefault();
  afx_msg void OnSavedefault();
  //afx_msg void OnNewProject();
  afx_msg void OnSaveProject();
	DECLARE_MESSAGE_MAP()

  void CleanUrls();
  // drag&drop
  //afx_msg void OnDropFiles( HDROP hDropInfo );
private: 
  CEasyDropTarget* drag;
};



//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WID1_H__8FF0CA01_F5EE_11D1_B222_006097BCBD81__INCLUDED_)
