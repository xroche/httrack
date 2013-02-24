#if !defined(AFX_TREEVIEWTOOLTIP_H__C5F45954_56A7_49B9_84B2_9C8BEBC46D08__INCLUDED_)
#define AFX_TREEVIEWTOOLTIP_H__C5F45954_56A7_49B9_84B2_9C8BEBC46D08__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TreeViewToolTip.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CTreeViewToolTip window

class CTreeViewToolTip : public CToolTipCtrl
{
// Construction
public:
	CTreeViewToolTip();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTreeViewToolTip)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CTreeViewToolTip();

	// Generated message map functions
protected:
  char* GetTip(int id);
	//{{AFX_MSG(CTreeViewToolTip)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TREEVIEWTOOLTIP_H__C5F45954_56A7_49B9_84B2_9C8BEBC46D08__INCLUDED_)
