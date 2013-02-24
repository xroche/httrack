// WinHTTrackView.h : interface of the CWinHTTrackView class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_WINHTTRACKVIEW_H__812065CF_6988_4DE1_857D_61123451A159__INCLUDED_)
#define AFX_WINHTTRACKVIEW_H__812065CF_6988_4DE1_857D_61123451A159__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


class CWinHTTrackView : public CView
{
protected: // create from serialization only
	CWinHTTrackView();
	DECLARE_DYNCREATE(CWinHTTrackView)

// Attributes
public:
	CWinHTTrackDoc* GetDocument();

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWinHTTrackView)
	public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	protected:
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CWinHTTrackView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CWinHTTrackView)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in WinHTTrackView.cpp
inline CWinHTTrackDoc* CWinHTTrackView::GetDocument()
   { return (CWinHTTrackDoc*)m_pDocument; }
#endif

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WINHTTRACKVIEW_H__812065CF_6988_4DE1_857D_61123451A159__INCLUDED_)
