#if !defined(AFX_OPTIONTAB1_H__E6FA3FE2_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_OPTIONTAB1_H__E6FA3FE2_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// OptionTab1.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COptionTab1 dialog

class COptionTab1 : public CPropertyPage
{
	DECLARE_DYNCREATE(COptionTab1)

// Construction
public:
	COptionTab1();
	~COptionTab1();
  char* GetTip(int id);
  int modify;

// Dialog Data
	//{{AFX_DATA(COptionTab1)
	enum { IDD = IDD_OPTION1 };
	BOOL	m_link;
	BOOL	m_parseall;
	BOOL	m_testall;
	BOOL	m_htmlfirst;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COptionTab1)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COptionTab1)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONTAB1_H__E6FA3FE2_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_)
