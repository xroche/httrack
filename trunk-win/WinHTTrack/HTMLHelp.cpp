// HTMLHelp.cpp : implementation file
//

#include "stdafx.h"
#include "winhttrack.h"
#include "HTMLHelp.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CHTMLHelp

IMPLEMENT_DYNCREATE(CHTMLHelp, CHtmlView)

CHTMLHelp::CHTMLHelp()
{
	//{{AFX_DATA_INIT(CHTMLHelp)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CHTMLHelp::~CHTMLHelp()
{
}

void CHTMLHelp::DoDataExchange(CDataExchange* pDX)
{
	CHtmlView::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CHTMLHelp)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CHTMLHelp, CHtmlView)
	//{{AFX_MSG_MAP(CHTMLHelp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CHTMLHelp diagnostics

#ifdef _DEBUG
void CHTMLHelp::AssertValid() const
{
	CHtmlView::AssertValid();
}

void CHTMLHelp::Dump(CDumpContext& dc) const
{
	CHtmlView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CHTMLHelp message handlers
