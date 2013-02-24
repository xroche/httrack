// FirstInfo.cpp : implementation file
//

#include "stdafx.h"
#include "winhttrack.h"
#include "FirstInfo.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/* Main WizTab frame */
#include "WizTab.h"
extern CWizTab* this_CWizTab;

/* Main splitter frame */
#include "DialogContainer.h"
#include "splitter.h"
extern CSplitterFrame* this_CSplitterFrame;

/* DirTreeView */
#include "DirTreeView.h"
extern CDirTreeView* this_DirTreeView;


/////////////////////////////////////////////////////////////////////////////
// CFirstInfo property page

IMPLEMENT_DYNCREATE(CFirstInfo, CPropertyPage)

CFirstInfo::CFirstInfo() : CPropertyPage(CFirstInfo::IDD)
{
	//{{AFX_DATA_INIT(CFirstInfo)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CFirstInfo::~CFirstInfo()
{
}

void CFirstInfo::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CFirstInfo)
	DDX_Control(pDX, IDC_SPLASH, m_splash);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CFirstInfo, CPropertyPage)
	//{{AFX_MSG_MAP(CFirstInfo)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFirstInfo message handlers

BOOL CFirstInfo::OnSetActive( ) {
  WHTT_LOCATION("FirstInfo");
  this_CWizTab->SetWizardButtons(PSWIZB_NEXT);
  SetDlgItemTextCP(this_CWizTab,IDCANCEL,LANG_QUIT);
  this_DirTreeView->ResetTree();
  this_CSplitterFrame->EnableSaveEntries(TRUE);
  //GetParent()->GetDlgItem(IDCANCEL)->ModifyStyle(0,WS_DISABLED);
  return 1;
}

BOOL CFirstInfo::OnQueryCancel( ) {
  //  if (AfxMessageBox(LANG(LANG_J1),MB_OKCANCEL)==IDOK) {
  /* Envoyer un WM_CLOSE à notre fenêtre principale */
  GetMainWindow()->SendMessage(WM_CLOSE,0,0);
  //  }
  return FALSE;
}

BOOL CFirstInfo::OnInitDialog() 
{

	CPropertyPage::OnInitDialog();
	EnableToolTips(true);     // TOOL TIPS

  WINDOWPLACEMENT wp;
  m_splash.GetWindowPlacement(&wp);
  wp.rcNormalPosition.right=wp.rcNormalPosition.left+300+1;
  wp.rcNormalPosition.bottom=wp.rcNormalPosition.top+69+1; 
  m_splash.SetWindowPlacement(&wp);

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    //SetDlgItemText(,"");
    
    SetDlgItemTextCP(this,IDC_STATIC_welcome, LANG_Y1);
  }

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


// !! COPIE DE ABOUT.CPP !!
void CFirstInfo::OnMouseMove(UINT nFlags, CPoint point) 
{
  int id=0;
  CWnd* w=ChildWindowFromPoint(point);
  if (w)
    id=w->GetDlgCtrlID();

  // Select
  switch(id) {
  case 0: break;
  case IDC_SPLASH:
    this->ClientToScreen(&point);
    w->ScreenToClient(&point);
    HCURSOR curs=NULL;
    if (
         (point.y>=120) && (point.y<=140)
      || (point.y<=80)
      || (point.y>=100) && (point.y<=110)
      ) {
      curs=AfxGetApp()->LoadCursor(IDC_CURSWWW);
    } else {
      curs=AfxGetApp()->LoadStandardCursor(IDC_ARROW);
    }
    if (curs) {
      //if (curs != currentCurs) {
        SetCursor(curs);
        // currentCurs=curs;
      //}
    }
  }  

	CDialog::OnMouseMove(nFlags, point);
}

void CFirstInfo::OnLButtonDown(UINT nFlags, CPoint point) 
{
  int id=0;
  CWnd* w=ChildWindowFromPoint(point);
  if (w)
    id=w->GetDlgCtrlID();

  // Select
  switch(id) {
    case 0: break;
    case IDC_SPLASH:
      this->ClientToScreen(&point);
      w->ScreenToClient(&point);
    if ((point.y>=100) && (point.y<=110) || (point.y<=80)) {
      if (!ShellExecute(NULL,"open","http://www.httrack.com","","",SW_RESTORE)) {
      }
    }
    break;
  }

  CDialog::OnLButtonDown(nFlags, point);
}
// !! FIN COPIE DE ABOUT.CPP !!

