#if !defined(AFX_OPTIONTAB6_H__E6FA3FE7_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_OPTIONTAB6_H__E6FA3FE7_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// OptionTab6.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COptionTab6 dialog

class COptionTab6 : public CPropertyPage
{
	DECLARE_DYNCREATE(COptionTab6)

// Construction
public:
	COptionTab6();
	~COptionTab6();
  char* GetTip(int id);

// Dialog Data
	//{{AFX_DATA(COptionTab6)
	enum { IDD = IDD_OPTION6 };
	CString	m_user;
	CString	m_footer;
	CString	m_accept_language;
	CString	m_other_headers;
	CString	m_default_referer;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COptionTab6)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COptionTab6)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONTAB6_H__E6FA3FE7_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_)
