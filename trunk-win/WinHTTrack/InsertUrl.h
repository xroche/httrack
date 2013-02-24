#if !defined(AFX_INSERTURL_H__2A8B8FE2_952E_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_INSERTURL_H__2A8B8FE2_952E_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// InsertUrl.h : header file
//

// Attention, définition existante également dans htslib.h
// (à modifier avec celle-ci)
#define POSTTOK "?>post"

/* Externe C */
extern "C" {
  #include "htscatchurl.h"
}
extern "C" {
  #include "HTTrackInterface.h"
	#include "httrack-library.h"
}

#include "CatchUrl.h"

/////////////////////////////////////////////////////////////////////////////
// CInsertUrl dialog

class CInsertUrl : public CDialog
{
// Construction
public:
	CInsertUrl(CWnd* pParent = NULL);   // standard constructor
  char* GetTip(int id);
  //
  CString dest_path;
  //
  CCatchUrl dial;
  T_SOC soc;
  char adr_prox[1024];
  int port_prox;

// Dialog Data
	//{{AFX_DATA(CInsertUrl)
	enum { IDD = IDD_InsertUrl };
	CString	m_urllogin;
	CString	m_urlpass;
	CString	m_urladr;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CInsertUrl)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  void OnHelpInfo2();

	// Generated message map functions
	//{{AFX_MSG(CInsertUrl)
	virtual BOOL OnInitDialog();
	afx_msg void Oncapt();
	//}}AFX_MSG
  afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_INSERTURL_H__2A8B8FE2_952E_11D3_A2B3_0000E84E7CA1__INCLUDED_)
