#if !defined(AFX_OPTIONTAB3_H__E6FA3FE4_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_OPTIONTAB3_H__E6FA3FE4_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// OptionTab3.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COptionTab3 dialog

class COptionTab3 : public CPropertyPage
{
	DECLARE_DYNCREATE(COptionTab3)

// Construction
public:
	COptionTab3();
	~COptionTab3();
  int modify;
  char* GetTip(int id);


// Dialog Data
	//{{AFX_DATA(COptionTab3)
	enum { IDD = IDD_OPTION3 };
	CComboBox	m_ctl_travel3;
	CComboBox	m_ctl_travel2;
	CComboBox	m_ctl_travel;
	CComboBox	m_ctl_filter;
	int		m_filter;
	int		m_travel;
	int		m_travel2;
	BOOL	m_windebug;
	BOOL	m_cache;
	int		m_travel3;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COptionTab3)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COptionTab3)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONTAB3_H__E6FA3FE4_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_)
