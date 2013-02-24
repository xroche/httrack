#if !defined(AFX_FirstInfo_H__DC893229_C7D6_448C_860C_54F4E35FFA84__INCLUDED_)
#define AFX_FirstInfo_H__DC893229_C7D6_448C_860C_54F4E35FFA84__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// FirstInfo.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CFirstInfo dialog

class CFirstInfo : public CPropertyPage
{
	DECLARE_DYNCREATE(CFirstInfo)

// Construction
public:
	CFirstInfo();
	~CFirstInfo();

// Dialog Data
	//{{AFX_DATA(CFirstInfo)
	enum { IDD = IDD_FirstInfo };
	CStatic	m_splash;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CFirstInfo)
	public:
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CFirstInfo)
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	//}}AFX_MSG
	BOOL OnInitDialog();
  BOOL OnSetActive();
  BOOL OnQueryCancel();
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FirstInfo_H__DC893229_C7D6_448C_860C_54F4E35FFA84__INCLUDED_)
