#if !defined(AFX_ADDFILTER_H__B88718A1_6113_11D2_A2B1_0000E84E7CA1__INCLUDED_)
#define AFX_ADDFILTER_H__B88718A1_6113_11D2_A2B1_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// AddFilter.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CAddFilter dialog

class CAddFilter : public CDialog
{
// Construction
public:
  int type;
	CAddFilter(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CAddFilter)
	enum { IDD = IDD_AddFilter };
	CStatic	m_ctl_affkeyword;
	CEdit	m_ctl_afext;
	CButton	m_ctl_ok;
	CComboBox	m_ctl_aftype;
	CString	m_addtype;
	CString	m_afquery;
	int		m_aftype;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAddFilter)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  char* GetTip(int id);
  void OnHelpInfo2();

	// Generated message map functions
	//{{AFX_MSG(CAddFilter)
	afx_msg void OnSelchangeAFtype();
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
  DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ADDFILTER_H__B88718A1_6113_11D2_A2B1_0000E84E7CA1__INCLUDED_)
