#if !defined(AFX_NEWFOLDER_H__FC899FE4_9DCD_11D2_A2B1_0000E84E7CA1__INCLUDED_)
#define AFX_NEWFOLDER_H__FC899FE4_9DCD_11D2_A2B1_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// NewFolder.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CNewFolder dialog

class CNewFolder : public CDialog
{
// Construction
public:
	CNewFolder(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CNewFolder)
	enum { IDD = IDD_NewFolder };
	CString	m_folder;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNewFolder)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CNewFolder)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NEWFOLDER_H__FC899FE4_9DCD_11D2_A2B1_0000E84E7CA1__INCLUDED_)
