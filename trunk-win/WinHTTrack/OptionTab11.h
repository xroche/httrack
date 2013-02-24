#if !defined(AFX_OPTIONTAB11_H__0203BCB7_9CC5_4316_B227_9F41CCFF513D__INCLUDED_)
#define AFX_OPTIONTAB11_H__0203BCB7_9CC5_4316_B227_9F41CCFF513D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// OptionTab11.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COptionTab11 dialog

class COptionTab11 : public CPropertyPage
{
	DECLARE_DYNCREATE(COptionTab11)

// Construction
public:
	COptionTab11();
	~COptionTab11();
  char* GetTip(int id);
  int modify;

// Dialog Data
	//{{AFX_DATA(COptionTab11)
	enum { IDD = IDD_OPTION11 };
	CString	m_ext1;
	CString	m_ext2;
	CString	m_ext3;
	CString	m_ext4;
	CString	m_ext5;
	CString	m_ext6;
	CString	m_ext7;
	CString	m_ext8;
	CString	m_mime1;
	CString	m_mime2;
	CString	m_mime3;
	CString	m_mime4;
	CString	m_mime5;
	CString	m_mime6;
	CString	m_mime7;
	CString	m_mime8;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COptionTab11)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COptionTab11)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONTAB11_H__0203BCB7_9CC5_4316_B227_9F41CCFF513D__INCLUDED_)
