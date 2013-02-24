#if !defined(AFX_WIZARD_H__55E76A32_F652_11D1_B223_006097BCBD81__INCLUDED_)
#define AFX_WIZARD_H__55E76A32_F652_11D1_B223_006097BCBD81__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// wizard.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// wizard dialog

class wizard : public CDialog
{
// Construction
public:
	wizard(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(wizard)
	enum { IDD = IDD_wizard };
	CString	m_reponse;
	CString	m_question;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(wizard)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(wizard)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WIZARD_H__55E76A32_F652_11D1_B223_006097BCBD81__INCLUDED_)
