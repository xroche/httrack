#if !defined(AFX_IPLOG_H__D416CFE1_8966_11D2_A2B1_0000E84E7CA1__INCLUDED_)
#define AFX_IPLOG_H__D416CFE1_8966_11D2_A2B1_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// iplog.h : header file
//

/* basic HTTrack defs */
extern "C" {
#include "htsglobal.h"
}

/////////////////////////////////////////////////////////////////////////////
// Ciplog dialog

class Ciplog : public CDialog
{
// Construction
public:
	Ciplog(CWnd* pParent = NULL);   // standard constructor
  char pathlog[HTS_URLMAXSIZE*2];
  int type_log;
  int type_filter;
private:
  FILE* fp;
  int wait_me;
  int type;
  int filter;
  CString txt;

public:
// Dialog Data
	//{{AFX_DATA(Ciplog)
	enum { IDD = IDD_iplog };
	CComboBox	m_ctl_hideinfo;
	CEdit	m_ctl_iplog;
	CString	m_iplog;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(Ciplog)
	public:
	virtual BOOL DestroyWindow();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  char* GetTip(int id);
  void OnHelpInfo2();
  //
  UINT_PTR timer;
  void StartTimer();
  void StopTimer();
  void AffLogRefresh();

	// Generated message map functions
	//{{AFX_MSG(Ciplog)
	afx_msg void OnScroll();
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
	afx_msg void Onchangelog();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnSelchangeHideinfo();
	afx_msg void OnFind();
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_IPLOG_H__D416CFE1_8966_11D2_A2B1_0000E84E7CA1__INCLUDED_)
