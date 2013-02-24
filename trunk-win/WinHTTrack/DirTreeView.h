#if !defined(AFX_DIRTREEVIEW_H__DFB224E0_828F_426D_A9A3_471D7A2F5108__INCLUDED_)
#define AFX_DIRTREEVIEW_H__DFB224E0_828F_426D_A9A3_471D7A2F5108__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DirTreeView.h : header file
//

#include <afxcview.h>
#include "TreeViewToolTip.h"

/////////////////////////////////////////////////////////////////////////////
// CDirTreeView view

class CDirTreeView : public CTreeView
{
protected:
	CDirTreeView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CDirTreeView)
protected:
  UINT_PTR timer;
  HANDLE    whandle[1024];
  HTREEITEM pos_whandle[1024];
  int       count_whandle;
  void StartTimer();
  void StopTimer();
  //
  void BuildTrackHandles();
  void DestroyTrackHandles();
  void DoTrackHandles();

// Attributes
public:
	CTreeViewToolTip m_TreeViewToolTip;
  //
  CString refreshPath;      /* callback */
  CString docType;          /* document type (ex: .doc) */
  //
  int redraw_in_progress;

// Operations
public:
  void RefreshTree();
  BOOL EnsureVisible(CString path);
  BOOL EnsureVisibleBl(CString path);
  void ResetTree();
  void WaitThreads();

protected:
  BOOL RefreshDir(HTREEITEM position,BOOL nohide=FALSE);
  void RestoreVisibles(HTREEITEM position,CString& backup_visibles);
  void RefreshPos(HTREEITEM position);
  CString GetItemPath(HTREEITEM position);
  HTREEITEM GetPathItem(CString path,BOOL open=FALSE,BOOL nofail=FALSE,BOOL nohide=FALSE,HTREEITEM rootposition=NULL);
  CString GetItemName(HTREEITEM position);
  //
  CImageList imagelist;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDirTreeView)
	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CDirTreeView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CDirTreeView)
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnItemexpanding(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclk(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEndlabeledit(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydown(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnSelchanged(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CDirTreeViewShellEx
/*
class CDirTreeViewShellEx : public CWinThread
{
  DECLARE_DYNAMIC(CDirTreeViewShellEx)
public:
  CString File;
  virtual BOOL InitInstance( );
};
*/
UINT CDirTreeViewShellEx( LPVOID pP );
UINT CDirTreeViewRefresh( LPVOID pP );

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.





#endif // !defined(AFX_DIRTREEVIEW_H__DFB224E0_828F_426D_A9A3_471D7A2F5108__INCLUDED_)
