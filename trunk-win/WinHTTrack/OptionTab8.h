#if !defined(AFX_OPTIONTAB8_H__8D023EA4_A8C3_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_OPTIONTAB8_H__8D023EA4_A8C3_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// OptionTab8.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COptionTab8 dialog

class COptionTab8 : public CPropertyPage
{
	DECLARE_DYNCREATE(COptionTab8)

// Construction
public:
	COptionTab8();
	~COptionTab8();
  char* GetTip(int id);
  int modify;

// Dialog Data
	//{{AFX_DATA(COptionTab8)
	enum { IDD = IDD_OPTION8 };
	int		m_checktype;
	BOOL	m_cookies;
	BOOL	m_parsejava;
	int		m_robots;
	BOOL	m_http10;
	BOOL	m_toler;
	BOOL	m_updhack;
	BOOL	m_urlhack;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COptionTab8)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COptionTab8)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
  DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONTAB8_H__8D023EA4_A8C3_11D3_A2B3_0000E84E7CA1__INCLUDED_)
