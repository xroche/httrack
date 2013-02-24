#if !defined(AFX_OPTIONTAB4_H__E6FA3FE5_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_OPTIONTAB4_H__E6FA3FE5_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// OptionTab4.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COptionTab4 dialog

class COptionTab4 : public CPropertyPage
{
	DECLARE_DYNCREATE(COptionTab4)

// Construction
public:
	COptionTab4();
	~COptionTab4();
  char* GetTip(int id);


// Dialog Data
	//{{AFX_DATA(COptionTab4)
	enum { IDD = IDD_OPTION4 };
	CString	m_connexion;
	BOOL	m_remt;
	CString	m_retry;
	CString	m_timeout;
	BOOL	m_rems;
	CString	m_rate;
	BOOL	m_ka;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COptionTab4)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COptionTab4)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONTAB4_H__E6FA3FE5_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_)
