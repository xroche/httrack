#if !defined(AFX_INFOURL_H__FF725966_B6BB_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_INFOURL_H__FF725966_B6BB_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// InfoUrl.h : header file
//

extern int termine;
extern void* StatsBufferback;
//extern lien_back* back;
extern int StatsBufferback_max;

/////////////////////////////////////////////////////////////////////////////
// CInfoUrl dialog

class CInfoUrl : public CDialog
{
// Construction
public:
	CInfoUrl(CWnd* pParent = NULL);   // standard constructor
  int id;
// Dialog Data
	//{{AFX_DATA(CInfoUrl)
	enum { IDD = IDD_InfoUrl };
	CComboBox	m_ctl_backlist;
	CProgressCtrl	m_slider;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CInfoUrl)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  UINT_PTR timer;
  void StartTimer();
  void StopTimer();

	// Generated message map functions
	//{{AFX_MSG(CInfoUrl)
	virtual BOOL OnInitDialog();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnClose();
	afx_msg void OnFreeze();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSelchangebacklist();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_INFOURL_H__FF725966_B6BB_11D3_A2B3_0000E84E7CA1__INCLUDED_)
