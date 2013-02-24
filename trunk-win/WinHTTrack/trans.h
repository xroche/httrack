#if !defined(AFX_TRANS_H__437D7274_F613_11D1_B222_006097BCBD81__INCLUDED_)
#define AFX_TRANS_H__437D7274_F613_11D1_B222_006097BCBD81__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// trans.h : header file
//

#include "ras.h"

/////////////////////////////////////////////////////////////////////////////
// Ctrans dialog

class Ctrans : public CPropertyPage
{
	DECLARE_DYNCREATE(Ctrans)
public:
  char RasString[256];
  RASDIALPARAMS dial;
  int hms;
// Construction
	Ctrans();   // standard constructor
	~Ctrans();   // standard constructor

// Dialog Data
	//{{AFX_DATA(Ctrans)
	enum { IDD = IDD_Debut };
	CButton	m_ctl_rasshut;
	CButton	m_ctl_rasdisc;
	CStatic	m_ctlcnx;
	CComboBox	m_ctlrasid;
	CStatic	m_ctl_wait;
	CEdit	m_ctl_ss;
	CEdit	m_ctl_mm;
	CEdit	m_ctl_hh;
	CString	m_hh;
	CString	m_mm;
	CString	m_ss;
	BOOL	m_wait;
	int		m_rasid;
	BOOL	m_rasdisc;
	BOOL	m_rasshut;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(Ctrans)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  char* GetTip(int id);
  void OnHelpInfo2();
  void FillProviderList(int fill);
  BOOL isfilled;

	// Generated message map functions
	//{{AFX_MSG(Ctrans)
	afx_msg void OnChangehh();
	virtual BOOL OnInitDialog();
	afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
	afx_msg void OnSelchangerasid();
	afx_msg void OnDropdownrasid();
	afx_msg void Onrasdisc();
	//}}AFX_MSG
  virtual BOOL OnKillActive( );
  virtual BOOL OnQueryCancel( );
  virtual BOOL OnWizardFinish( );
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
  virtual BOOL OnSetActive( );
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TRANS_H__437D7274_F613_11D1_B222_006097BCBD81__INCLUDED_)
