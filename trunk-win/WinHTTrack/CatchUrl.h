#if !defined(AFX_CATCHURL_H__589FEA02_D671_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_CATCHURL_H__589FEA02_D671_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CatchUrl.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CCatchUrl dialog

class CCatchUrl : public CDialog
{
// Construction
public:
	CCatchUrl(CWnd* pParent = NULL);   // standard constructor
  char* GetTip(int id);

// Dialog Data
	//{{AFX_DATA(CCatchUrl)
	enum { IDD = IDD_CatchUrl };
	CString	m_info;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCatchUrl)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CCatchUrl)
	afx_msg void OnClose();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CATCHURL_H__589FEA02_D671_11D3_A2B3_0000E84E7CA1__INCLUDED_)
