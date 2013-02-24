#if !defined(AFX_WIZLINKS_H__1D129B83_1981_11D2_A290_60D144C12802__INCLUDED_)
#define AFX_WIZLINKS_H__1D129B83_1981_11D2_A290_60D144C12802__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// WizLinks.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// WizLinks dialog

class WizLinks : public CDialog
{
  // gestion timer flash
  bool wflag; 
  UINT_PTR tm;

// Construction
public:
	WizLinks(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(WizLinks)
	enum { IDD = IDD_wizard_lnk };
	int		m_lnk;
	CString	m_url;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(WizLinks)
	public:
	virtual INT_PTR DoModal();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(WizLinks)
	afx_msg void Onskipall();
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WIZLINKS_H__1D129B83_1981_11D2_A290_60D144C12802__INCLUDED_)
