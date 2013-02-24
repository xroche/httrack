// splitter.h : custom splitter control and frame that contains it
//
// This is a part of the Microsoft Foundation Classes C++ library.
// Copyright (C) 1992-1998 Microsoft Corporation
// All rights reserved.
//
// This source code is only intended as a supplement to the
// Microsoft Foundation Classes Reference and related
// electronic documentation provided with the library.
// See these sources for detailed information regarding the
// Microsoft Foundation Classes product.

/////////////////////////////////////////////////////////////////////////////
// CSplitterFrame frame with splitter/wiper

class CSplitterFrame : public CMDIChildWnd
{
	DECLARE_DYNCREATE(CSplitterFrame)
protected:
	CSplitterFrame();   // protected constructor used by dynamic creation

public:
  void SetMenuPrefs();
	afx_msg void Onhide();
  BOOL SetNewName(CString name);
  BOOL SetSaved();
  BOOL SetCurrentCategory(CString name);
  CString GetCurrentCategory(void);

 // Attributes
protected:
	CSplitterWnd m_wndSplitter;
  CString      m_projcateg;
	//CStatusBar   m_wndStatusBar;
	//CToolBar     m_wndToolBar;
  NOTIFYICONDATA icnd;
  //
  afx_msg LRESULT IconRestore(WPARAM wParam,LPARAM lParam);

// Implementation
public:
  bool iconifie;
  //
	virtual ~CSplitterFrame();
  virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
  virtual BOOL Create( LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle , const RECT& rect , CMDIFrameWnd* pParentWnd , CCreateContext* pContext );
  CSplitterWnd* GetSplitter();
  BOOL SetNewView(int row, int col, CRuntimeClass* pViewClass);
  void EnableExtEntries(BOOL state);
  void EnableSaveEntries(BOOL state);
  
  void IconChange(CString st);
  void CheckRestore();
  
  // Generated message map functions
  //{{AFX_MSG(CSplitterFrame)
	afx_msg void OnMDIActivate(BOOL bActivate, CWnd* pActivateWnd, CWnd* pDeactivateWnd);
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
//

class CViewExSplitWnd : public CSplitterWnd
{
	DECLARE_DYNAMIC(CViewExSplitWnd)

// Implementation
public:
	CViewExSplitWnd();
	~CViewExSplitWnd();
	CWnd* GetActivePane(int* pRow = NULL, int* pCol = NULL);
};


