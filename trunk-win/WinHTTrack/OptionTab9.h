#if !defined(AFX_OPTIONTAB9_H__8D023EA5_A8C3_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_OPTIONTAB9_H__8D023EA5_A8C3_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// OptionTab9.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COptionTab9 dialog

class COptionTab9 : public CPropertyPage
{
	DECLARE_DYNCREATE(COptionTab9)

// Construction
public:
	COptionTab9();
	~COptionTab9();
  char* GetTip(int id);
  int modify;

// Dialog Data
	//{{AFX_DATA(COptionTab9)
	enum { IDD = IDD_OPTION9 };
	BOOL	m_index;
	BOOL	m_logf;
	BOOL	m_Cache2;
	int		m_logtype;
	BOOL	m_norecatch;
	BOOL	m_index2;
	BOOL	m_index_mail;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COptionTab9)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COptionTab9)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
  DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONTAB9_H__8D023EA5_A8C3_11D3_A2B3_0000E84E7CA1__INCLUDED_)
