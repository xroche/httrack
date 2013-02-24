// DialogContainer.cpp : implementation file
//

// Les dialogues doivent avoir comme flags:
// CHILD, NONE, VISIBLE
// Et surcharger Oncancel et OnOK


#include "stdafx.h"
#include "winhttrack.h"
#include "DialogContainer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDialogContainer

IMPLEMENT_DYNCREATE(CDialogContainer, CFormView)

CDialogContainer::CDialogContainer()
	: CFormView(CDialogContainer::IDD)
{
  scrollsize_declared=FALSE;
  tab=new CWizTab("WinHTTrack Website Copier",0);
  tab2=new CWizTab("WinHTTrack Website Copier",1);
	//{{AFX_DATA_INIT(CDialogContainer)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CDialogContainer::~CDialogContainer()
{
  /*
  voir WizTab.cpp
  delete tab;
  delete tab2;
  tab=tab2=NULL;
  */
}

void CDialogContainer::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDialogContainer)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDialogContainer, CFormView)
	//{{AFX_MSG_MAP(CDialogContainer)
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDialogContainer diagnostics

#ifdef _DEBUG
void CDialogContainer::AssertValid() const
{
	CFormView::AssertValid();
}

void CDialogContainer::Dump(CDumpContext& dc) const
{
	CFormView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CDialogContainer message handlers

BOOL CDialogContainer::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) 
{
	int r=CFormView::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext);
  tab->CPropertySheet::Create(this,WS_CHILD|WS_VISIBLE,0);
  tab2->CPropertySheet::Create(this,WS_CHILD|WS_VISIBLE,0);
  return r;
}

void CDialogContainer::OnInitialUpdate() 
{
	CFormView::OnInitialUpdate();

  tab2->ModifyStyle(WS_VISIBLE,0,0);
  tab2->ModifyStyle(0,WS_DISABLED,0);
  tab->RedrawWindow();
  tab->SetFocus();

  {
    RECT rect;
    ::GetWindowRect(tab->m_hWnd,&rect);
    // screen coord -> client coord
    POINT a,b;
    a.x=rect.left; a.y=rect.top; b.x=rect.right; b.y=rect.bottom;
    ::ScreenToClient(tab->m_hWnd,&a); ::ScreenToClient(tab->m_hWnd,&b);
    rect.left=a.x; rect.top=a.y; rect.right=b.x; rect.bottom=b.y;
    view_w = rect.right-rect.left;
    view_h = rect.bottom-rect.top;
  }
  {
    RECT rect;
    ::GetWindowRect(tab2->m_hWnd,&rect);
    // screen coord -> client coord
    POINT a,b;
    a.x=rect.left; a.y=rect.top; b.x=rect.right; b.y=rect.bottom;
    ::ScreenToClient(tab2->m_hWnd,&a); ::ScreenToClient(tab2->m_hWnd,&b);
    rect.left=a.x; rect.top=a.y; rect.right=b.x; rect.bottom=b.y;
    view_w = max(view_w , rect.right-rect.left);
    view_h = max(view_h , rect.bottom-rect.top);
  }

	CSize sizeTotal;
  sizeTotal.cx = view_w;
  sizeTotal.cy = view_h;
  SetScrollSizes(MM_TEXT, sizeTotal);
  scrollsize_declared=TRUE;
  //
  CRect curRect;
  GetClientRect(curRect);
  //tab->MoveWindow(curRect);	
}

void CDialogContainer::OnSize(UINT nType, int cx, int cy) 
{
	CFormView::OnSize(nType, cx, cy);
	
  if (scrollsize_declared) {
    if (tab->m_hWnd) {
      // now capture current size and resize child
      CRect curRect;
      GetClientRect(curRect);
      //curRect.left=curRect.top=0;
      //curRect.right=max(curRect.right,view_w);
      //curRect.bottom=max(curRect.bottom,view_h);
      //tab->MoveWindow(curRect);	
    }
  }
	
}

