#if !defined(AFX_BUILDOPTIONS_H__9ADE0222_D39E_11D2_A2B1_0000E84E7CA1__INCLUDED_)
#define AFX_BUILDOPTIONS_H__9ADE0222_D39E_11D2_A2B1_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// BuildOptions.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CBuildOptions dialog

class CBuildOptions : public CDialog
{
// Construction
public:
	CBuildOptions(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CBuildOptions)
	enum { IDD = IDD_BuildOptions };
	CString	m_BuildString;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CBuildOptions)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  void OnHelpInfo2();

	// Generated message map functions
	//{{AFX_MSG(CBuildOptions)
	afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_BUILDOPTIONS_H__9ADE0222_D39E_11D2_A2B1_0000E84E7CA1__INCLUDED_)
