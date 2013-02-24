#if !defined(AFX_OPTIONTAB2_H__E6FA3FE3_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_OPTIONTAB2_H__E6FA3FE3_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "BuildOptions.h"

// OptionTab2.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COptionTab2 dialog

class COptionTab2 : public CPropertyPage
{
	DECLARE_DYNCREATE(COptionTab2)

// Construction
public:
	COptionTab2();
	~COptionTab2();
	CBuildOptions      Bopt;
  int modify;
  char* GetTip(int id);


// Dialog Data
	//{{AFX_DATA(COptionTab2)
	enum { IDD = IDD_OPTION2 };
	CComboBox	m_ctl_build;
	CButton	m_buildopt;
	int		m_build;
	BOOL	m_dos;
	BOOL	m_errpage;
	BOOL	m_external;
	BOOL	m_nopurge;
	BOOL	m_hidepwd;
	BOOL	m_hidequery;
	BOOL	m_iso9660;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COptionTab2)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COptionTab2)
	afx_msg void Onbuildopt();
	virtual BOOL OnInitDialog();
	afx_msg void OnSelchangebuild();
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONTAB2_H__E6FA3FE3_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_)
