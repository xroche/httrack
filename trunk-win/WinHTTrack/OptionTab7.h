#if !defined(AFX_OPTIONTAB7_H__E6FA3FE8_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_OPTIONTAB7_H__E6FA3FE8_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// OptionTab7.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COptionTab7 dialog

class COptionTab7 : public CPropertyPage
{
	DECLARE_DYNCREATE(COptionTab7)

// Construction
public:
	COptionTab7();
	~COptionTab7();
  char* GetTip(int id);
  int modify;

// Dialog Data
	//{{AFX_DATA(COptionTab7)
	enum { IDD = IDD_OPTION7 };
	CString	m_url2;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COptionTab7)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COptionTab7)
	afx_msg void OnAdd1();
	afx_msg void OnAdd2();
	virtual BOOL OnInitDialog();
	afx_msg void OnCheck1();
	afx_msg void OnCheck2();
	afx_msg void OnCheck3();
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	DECLARE_MESSAGE_MAP()

  void EnsureIncluded(BOOL state, CString filter);

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONTAB7_H__E6FA3FE8_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_)
