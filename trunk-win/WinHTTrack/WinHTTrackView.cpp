// WinHTTrackView.cpp : implementation of the CWinHTTrackView class
//

#include "stdafx.h"
#include "WinHTTrack.h"

#include "WinHTTrackDoc.h"
#include "WinHTTrackView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackView

IMPLEMENT_DYNCREATE(CWinHTTrackView, CView)

BEGIN_MESSAGE_MAP(CWinHTTrackView, CView)
	//{{AFX_MSG_MAP(CWinHTTrackView)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackView construction/destruction

CWinHTTrackView::CWinHTTrackView()
{
	// TODO: add construction code here

}

CWinHTTrackView::~CWinHTTrackView()
{
}

BOOL CWinHTTrackView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CView::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackView drawing

void CWinHTTrackView::OnDraw(CDC* pDC)
{
	CWinHTTrackDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	// TODO: add draw code for native data here
}

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackView diagnostics

#ifdef _DEBUG
void CWinHTTrackView::AssertValid() const
{
	CView::AssertValid();
}

void CWinHTTrackView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CWinHTTrackDoc* CWinHTTrackView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CWinHTTrackDoc)));
	return (CWinHTTrackDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackView message handlers
