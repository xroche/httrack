#if !defined(AFX_INFOEND_H__B057B1C2_A192_11D2_A2B1_0000E84E7CA1__INCLUDED_)
#define AFX_INFOEND_H__B057B1C2_A192_11D2_A2B1_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// infoend.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// Cinfoend dialog

class Cinfoend : public CPropertyPage
{
public:
	Cinfoend();           // protected constructor used by dynamic creation
	~Cinfoend();           // protected constructor used by dynamic creation
  DECLARE_DYNCREATE(Cinfoend)
    // Construction
public:
	//Cinfoend(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(Cinfoend)
	enum { IDD = IDD_fin };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(Cinfoend)
	public:
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  char* GetTip(int id);
  void OnHelpInfo2();
  void OnBye();
  UINT_PTR tm;

	// Generated message map functions
	//{{AFX_MSG(Cinfoend)
	virtual BOOL OnInitDialog();
	afx_msg void Onlog();
	afx_msg void Onbrowse();
	afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
  virtual BOOL OnSetActive( );
  virtual BOOL OnWizardFinish( );
	DECLARE_MESSAGE_MAP()
  BOOL OnQueryCancel( );
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_INFOEND_H__B057B1C2_A192_11D2_A2B1_0000E84E7CA1__INCLUDED_)
