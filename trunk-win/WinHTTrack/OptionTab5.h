#if !defined(AFX_OPTIONTAB5_H__E6FA3FE6_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_OPTIONTAB5_H__E6FA3FE6_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// OptionTab5.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COptionTab5 dialog

class COptionTab5 : public CPropertyPage
{
	DECLARE_DYNCREATE(COptionTab5)

// Construction
public:
	COptionTab5();
	~COptionTab5();
  char* GetTip(int id);
  int depth_status;


// Dialog Data
	//{{AFX_DATA(COptionTab5)
	enum { IDD = IDD_OPTION5 };
	CEdit	m_ctl_pausebytes;
	CComboBox	m_ctl_depth2;
	CComboBox	m_ctl_depth;
	CString	m_maxhtml;
	CString	m_maxrate;
	CString	m_maxtime;
	CString	m_othermax;
	CString	m_sizemax;
	CString	m_depth;
	CString	m_maxconn;
	CString	m_depth2;
	CString	m_pausebytes;
	CString	m_maxlinks;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COptionTab5)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COptionTab5)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONTAB5_H__E6FA3FE6_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_)
