#if !defined(AFX_PROXYID_H__17B166A2_693D_11D3_A2B2_0000E84E7CA1__INCLUDED_)
#define AFX_PROXYID_H__17B166A2_693D_11D3_A2B2_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// ProxyId.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CProxyId dialog

class CProxyId : public CDialog
{
// Construction
public:
	CProxyId(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CProxyId)
	enum { IDD = IDD_ProxyId };
	CString	m_proxadr;
	CString	m_proxlogin;
	CString	m_proxpass;
	CString	m_proxport;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CProxyId)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  char* GetTip(int id);
	afx_msg void OnHelpInfo2();


	// Generated message map functions
	//{{AFX_MSG(CProxyId)
	virtual BOOL OnInitDialog();
	afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROXYID_H__17B166A2_693D_11D3_A2B2_0000E84E7CA1__INCLUDED_)
