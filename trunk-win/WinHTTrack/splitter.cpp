// splitter.cpp : implementation file
//

#include "resource.h"
#include "stdafx.h"
#include "splitter.h"
#include "DialogContainer.h"
#include "cpp_lang.h"

#include "afxpriv.h"

// CDirTreeView
#include "DirTreeView.h"

// Pointeur sur nous
#include "WinHTTrack.h"
extern CWinHTTrackApp* this_app;

extern "C" {
  #include "HTTrackInterface.h"
};

//#include "htsbase.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

extern "C" {
  #include "HTTrackInterface.h"
};

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};

// splitter frame
extern CSplitterFrame* this_CSplitterFrame;
// termine flag
extern int termine;

/* DirTreeView */
#include "DirTreeView.h"
extern CDirTreeView* this_DirTreeView;



/////////////////////////////////////////////////////////////////////////////
// CSplitterFrame

// Create a splitter window which splits an output text view and an input view
//                           |
//    TEXT VIEW (CTextView)  | INPUT VIEW (CInputView)
//                           |

IMPLEMENT_DYNCREATE(CSplitterFrame, CMDIChildWnd)

CSplitterFrame::CSplitterFrame()
{
  this_CSplitterFrame=this;
}

CSplitterFrame::~CSplitterFrame()
{
  this_CSplitterFrame=NULL;
}

#define wm_IcnRest (WM_USER + 16)
BEGIN_MESSAGE_MAP(CSplitterFrame, CMDIChildWnd)
	//{{AFX_MSG_MAP(CSplitterFrame)
	ON_WM_MDIACTIVATE()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
  ON_MESSAGE( wm_IcnRest, IconRestore )
END_MESSAGE_MAP()
//

// Iconify
void CSplitterFrame::Onhide() 
{
  if (!iconifie) {
    //icnd.hWnd=this->GetMainWnd()->m_hWnd;
    //icnd.hWnd=AfxGetMainWnd()->m_hWnd;
    icnd.hWnd=this_CSplitterFrame->m_hWnd;
    strcpybuff(icnd.szTip,"WinHTTrack Website Copier");
    /* */
    AfxGetMainWnd()->ShowWindow(SW_HIDE);
    //ShowWindow(SW_SHOWMINNOACTIVE);
    Shell_NotifyIcon(NIM_ADD,&icnd);
    iconifie=true;
  } else {
    CheckRestore();
  }
}

LRESULT CSplitterFrame::IconRestore(WPARAM wParam,LPARAM lParam) {
  if (iconifie) {
    POINT point;
    GetCursorPos(&point);
    //ScreenToClient(&point);
    int msg=(UINT) lParam;
    if (msg==WM_RBUTTONDOWN) {  // menu      
      CMenu menu;
      VERIFY(menu.LoadMenu(IDR_POPUP));
      CMenu* pPopup = menu.GetSubMenu(0);
      ASSERT(pPopup != NULL); 
      pPopup->TrackPopupMenu(TPM_RIGHTBUTTON,
        point.x, point.y, AfxGetMainWnd());
    } else if (msg==WM_LBUTTONDBLCLK) {
      CheckRestore();
    }
  }
  return 0;
}

void CSplitterFrame::IconChange(CString st) {
  if (iconifie) {
    strcpybuff(icnd.szTip,"");
    strncatbuff(icnd.szTip,st,60);
    Shell_NotifyIcon(NIM_MODIFY,&icnd);
  }
}

void CSplitterFrame::CheckRestore() {
  if (iconifie) {
    Shell_NotifyIcon(NIM_DELETE,&icnd);
    //if (!termine) {
    if (AfxGetMainWnd()) {
      if (AfxGetMainWnd()->m_hWnd) {
        AfxGetMainWnd()->ShowWindow(SW_RESTORE);
        AfxGetMainWnd()->SetForegroundWindow();
      }
    }
    iconifie=false;
  }
}
// FIN Iconify


BOOL CSplitterFrame::Create( LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle , const RECT& rect , CMDIFrameWnd* pParentWnd , CCreateContext* pContext ) {
  /* recréer control tabs */
  this_app->NewTabs();

  // Enlever bordure fenêtre, et mettre en plein écran (pas de fenêtre dans la fenêtre!!)
  dwStyle&=(~(WS_MINIMIZEBOX|WS_BORDER|WS_CAPTION|WS_OVERLAPPED|WS_OVERLAPPEDWINDOW));
  dwStyle|=(WS_MAXIMIZE);
  int r=CMDIChildWnd::Create(lpszClassName,lpszWindowName,dwStyle ,rect ,pParentWnd ,pContext );
  if (r) {
    // Mettre en maximisé
    WINDOWPLACEMENT pl;
    RECT rc;
    rc.top=0;
    rc.left=0;
    rc.bottom=200;
    rc.right=400;
    pl.length=sizeof(pl);
    pl.flags=WPF_RESTORETOMAXIMIZED;
    pl.showCmd=SW_SHOWMAXIMIZED;
    pl.rcNormalPosition=rc;
    SetWindowPlacement(&pl);
  }

  // iconification
  iconifie=false;
  icnd.cbSize=sizeof(NOTIFYICONDATA);
  icnd.uID=0;              // euhh id=0
  icnd.uFlags=NIF_ICON|NIF_TIP|NIF_MESSAGE;
  icnd.uCallbackMessage=wm_IcnRest;  // notre callback
  icnd.hIcon=AfxGetApp()->LoadIcon(IDR_MAINFRAME);

  // Retourner résultat
  return r;
}

static TCHAR BASED_CODE szSection[] = _T("Settings");
static TCHAR BASED_CODE szWindowPos[] = _T("SplitterPos");
BOOL CSplitterFrame::OnCreateClient(LPCREATESTRUCT, CCreateContext* pContext) 
{
  /*
	if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) ||
		!m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}

	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}
	// TODO: Delete these three lines if you don't want the toolbar to
	//  be dockable
	m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
	EnableDocking(CBRS_ALIGN_ANY);
	DockControlBar(&m_wndToolBar);
  */
  //

	// create a splitter with 1 row, 2 columns
	if (!m_wndSplitter.CreateStatic(this, 1, 2))
	{
		TRACE0("Failed to CreateStaticSplitter\n");
		return FALSE;
	}

	// add the first splitter pane - the default view in column 0
	/*if (!m_wndSplitter.CreateView(0, 0,
		pContext->m_pNewViewClass, CSize(130, 50), pContext))
	{
		TRACE0("Failed to create first pane\n");
		return FALSE;
	}*/

  int PaneW=200,PaneH=50;
  {
    CString strBuffer = AfxGetApp()->GetProfileString(szSection, szWindowPos);
    if (!strBuffer.IsEmpty()) {
      int rx=0,ry=0;
      if (sscanf(strBuffer,"%d,%d",&rx,&ry)==2) {
        PaneW=rx;
        PaneH=ry;
      }
    }
  }

	// add the second splitter pane - an input view in column 1

	if (!m_wndSplitter.CreateView(0, 0,
		RUNTIME_CLASS(CDirTreeView), CSize(PaneW, PaneH), pContext))
	{
		TRACE0("Failed to create second pane\n");
		return FALSE;
	}
  if (this_DirTreeView)
    this_DirTreeView->docType=".whtt";

	// add the second splitter pane - an input view in column 1
	if (!m_wndSplitter.CreateView(0, 1,
		RUNTIME_CLASS(CDialogContainer), CSize(0,0), pContext))
	{
		TRACE0("Failed to create second pane\n");
		return FALSE;
	}

  //m_wndSplitter.SetColumnInfo(1,800,400);

  // menu entries - done with MDIactivate
  //SetMenuPrefs();

  // activate the input view
	SetActiveView((CView*)m_wndSplitter.GetPane(0,1));

	return TRUE;
}

CSplitterWnd* CSplitterFrame::GetSplitter() {
  return &m_wndSplitter;
}

BOOL CSplitterFrame::SetNewView(int row, int col, CRuntimeClass* pViewClass) {
  ASSERT(pViewClass->IsDerivedFrom(RUNTIME_CLASS(CView)));
  
  CView* pView = STATIC_DOWNCAST(CView, m_wndSplitter.GetPane(row, col));
  CFrameWnd* pFrame = pView->GetParentFrame();
  ASSERT(pFrame);
  
  // set up create context to preserve doc/frame etc.
  CCreateContext cc;
  memset(&cc, 0, sizeof(cc));
  cc.m_pNewViewClass = pViewClass;
  cc.m_pCurrentDoc = pView->GetDocument();
  cc.m_pNewDocTemplate = cc.m_pCurrentDoc ?
    cc.m_pCurrentDoc->GetDocTemplate() : NULL;
  cc.m_pCurrentFrame = pFrame;
  
  m_wndSplitter.DeleteView(row, col);                // delete old view

  /* */
  /* recréer control tabs */
  this_app->NewTabs();
  /* */

  VERIFY(m_wndSplitter.CreateView(row, col,          // create new one
    pViewClass,
    CSize(0,0),                      // will fix in RecalcLayout
    &cc));
  
  m_wndSplitter.RecalcLayout();                      // recompute layout 
  
  // initialize the view
  CWnd* pWnd = m_wndSplitter.GetPane(row, col);
  if (pWnd)
    pWnd->SendMessage(WM_INITIALUPDATE);

  GetActiveDocument()->SetModifiedFlag(FALSE);      // Document vide
  GetActiveDocument()->SetTitle("New Project");
  //SetDefaultTitle(GetActiveDocument());
  return 1;
}

BOOL CSplitterFrame::SetSaved() {
  GetActiveDocument()->SetModifiedFlag(FALSE);      // Document sauvé
  return 1;
}

// Nommer le document
BOOL CSplitterFrame::SetNewName(CString name) {
  GetActiveDocument()->SetPathName(name);
  int pos=max(name.ReverseFind('\\'),name.ReverseFind('/'))+1;
  GetActiveDocument()->SetTitle(name.Mid(pos));
  GetActiveDocument()->SetModifiedFlag();
  return 1;
}

BOOL CSplitterFrame::SetCurrentCategory(CString name) {
  m_projcateg = name;
  return TRUE;
}

CString CSplitterFrame::GetCurrentCategory() {
  return m_projcateg;
}


void CSplitterFrame::EnableExtEntries(BOOL state) {
  CMenu* menu = GetParent()->GetParent()->GetMenu();
  if (menu) {
    int status;
    if (!state)
      status=MF_GRAYED; 
    else
      status=MF_ENABLED; 
    menu->EnableMenuItem(2,status|MF_BYPOSITION);
    menu->EnableMenuItem(3,status|MF_BYPOSITION);
    
    GetParent()->GetParent()->DrawMenuBar();
  }
}

void CSplitterFrame::EnableSaveEntries(BOOL state) {
  CMenu* menu = GetParent()->GetParent()->GetMenu();
  if (menu) {
    int status;
    if (!state)
      status=MF_GRAYED; 
    else
      status=MF_ENABLED; 
    menu->EnableMenuItem(1,status|MF_BYPOSITION);
    menu->EnableMenuItem(1,status|MF_BYPOSITION);
    GetParent()->GetParent()->DrawMenuBar();
  }
}

void CSplitterFrame::SetMenuPrefs() {
  if (LANG_T(-1)) {    // Patcher en français
    CMenu* menu = GetParent()->GetParent()->GetMenu();
    if (menu) {
      ModifyMenuCP(menu, 0,MF_BYPOSITION,0,LANG(LANG_P1));
      ModifyMenuCP(menu, 1,MF_BYPOSITION,1,LANG(LANG_P2));
      ModifyMenuCP(menu, 2,MF_BYPOSITION,2,LANG(LANG_P3));
      ModifyMenuCP(menu, 3,MF_BYPOSITION,0,LANG(LANG_P4));
      ModifyMenuCP(menu, 4,MF_BYPOSITION,1,LANG(LANG_P5));
      ModifyMenuCP(menu, 5,MF_BYPOSITION,2,LANG(LANG_P6));
      //
      ModifyMenuCP(menu, ID_FILE_NEW ,MF_BYCOMMAND,ID_FILE_NEW,LANG_P18);
      ModifyMenuCP(menu, ID_FILE_OPEN ,MF_BYCOMMAND,ID_FILE_OPEN,LANG_P19);
      ModifyMenuCP(menu, ID_FILE_SAVE ,MF_BYCOMMAND,ID_FILE_SAVE,LANG_P20);
      ModifyMenuCP(menu, ID_FILE_SAVE_AS ,MF_BYCOMMAND,ID_FILE_SAVE_AS,LANG_P21);
      ModifyMenuCP(menu, ID_FILE_DELETE_PROJ ,MF_BYCOMMAND,ID_FILE_DELETE_PROJ,LANG_P22);
      ModifyMenuCP(menu, ID_FILE_BROWSE_SIT ,MF_BYCOMMAND,ID_FILE_BROWSE_SIT,LANG_P23);
      ModifyMenuCP(menu, ID_APP_EXIT,MF_BYCOMMAND,ID_APP_EXIT,LANG(LANG_P10));
      ModifyMenuCP(menu, ID_LoadDefaultOptions,MF_BYCOMMAND,ID_LoadDefaultOptions,LANG(LANG_P11));
      ModifyMenuCP(menu, ID_SaveProject,MF_BYCOMMAND,ID_SaveProject,LANG_SAVEPROJECT);
      ModifyMenuCP(menu, ID_NewProjectImport,MF_BYCOMMAND,ID_NewProjectImport,LANG(LANG_P18));
      ModifyMenuCP(menu, ID_SaveDefaultOptions,MF_BYCOMMAND,ID_SaveDefaultOptions,LANG(LANG_P12));
      ModifyMenuCP(menu, ID_ClearDefaultOptions,MF_BYCOMMAND,ID_ClearDefaultOptions,LANG(LANG_P12b));
      ModifyMenuCP(menu, ID_LOAD_OPTIONS,MF_BYCOMMAND,ID_FILE_OPEN,LANG(LANG_P13));
      ModifyMenuCP(menu, ID_FILE_SAVE_OPTIONS_AS,MF_BYCOMMAND,ID_FILE_SAVE_AS,LANG(LANG_P14));
      ModifyMenuCP(menu, IDC_langprefs,MF_BYCOMMAND,IDC_langprefs,LANG(LANG_P15));
      ModifyMenuCP(menu, ID_HELP,MF_BYCOMMAND,ID_HELP,LANG(LANG_P16));
      ModifyMenuCP(menu, IDC_ipabout,MF_BYCOMMAND,IDC_ipabout,LANG(LANG_P17));
      //
      /*
      ModifyMenuCP(menu, 0,MF_BYPOSITION,0,LANG(LANG_O1));
      ModifyMenuCP(menu, 1,MF_BYPOSITION,1,LANG(LANG_O2));
      ModifyMenuCP(menu, 2,MF_BYPOSITION,2,LANG(LANG_O3));
      ModifyMenuCP(menu, 3,MF_BYPOSITION,3,LANG(LANG_O4));
      ModifyMenuCP(menu, 4,MF_BYPOSITION,4,LANG(LANG_O5));
      */
      //
      ModifyMenuCP(menu, ID_FILE_PAUSE,MF_BYCOMMAND,ID_FILE_PAUSE,LANG(LANG_O10));
      ModifyMenuCP(menu, ID_FILE_EXIT,MF_BYCOMMAND,ID_FILE_EXIT,LANG(LANG_O11));
      ModifyMenuCP(menu, ID_OPTIONS_MODIFY,MF_BYCOMMAND,ID_OPTIONS_MODIFY,LANG(LANG_O12));
      ModifyMenuCP(menu, ID_LOG_VIEWLOG,MF_BYCOMMAND,ID_LOG_VIEWLOG,LANG(LANG_O13));
      ModifyMenuCP(menu, ID_LOG_VIEWERRORLOG,MF_BYCOMMAND,ID_LOG_VIEWERRORLOG,LANG(LANG_O14));
      ModifyMenuCP(menu, ID_LOG_VIEWTRANSFERS,MF_BYCOMMAND,ID_LOG_VIEWTRANSFERS,LANG(LANG_O14b));
      ModifyMenuCP(menu, ID_WINDOW_HIDE,MF_BYCOMMAND,ID_WINDOW_HIDE,LANG(LANG_O15));
      //ModifyMenuCP(menu, ID_VIEW_TOOLBAR,MF_BYCOMMAND,ID_VIEW_TOOLBAR,LANG(LANG_O18));
      ModifyMenuCP(menu, ID_VIEW_STATUS_BAR,MF_BYCOMMAND,ID_VIEW_STATUS_BAR,LANG(LANG_O19));
      ModifyMenuCP(menu, ID_WINDOW_SPLIT,MF_BYCOMMAND,ID_WINDOW_SPLIT,LANG(LANG_O20));      
      ModifyMenuCP(menu, ID_ABOUT,MF_BYCOMMAND,ID_ABOUT,LANG(LANG_O16));
      ModifyMenuCP(menu, ID_UPDATE,MF_BYCOMMAND,ID_UPDATE,LANG(LANG_O17));
      //
      GetParent()->GetParent()->DrawMenuBar();
    }
  }
}


/////////////////////////////////////////////////////////////////////////////
//

IMPLEMENT_DYNAMIC(CViewExSplitWnd, CSplitterWnd)

CViewExSplitWnd::CViewExSplitWnd()
{
}

CViewExSplitWnd::~CViewExSplitWnd()
{
}

CWnd* CViewExSplitWnd::GetActivePane(int* pRow, int* pCol)
{
	ASSERT_VALID(this);

	// attempt to use active view of frame window
	CWnd* pView = NULL;
	CFrameWnd* pFrameWnd = GetParentFrame();
	ASSERT_VALID(pFrameWnd);
	pView = pFrameWnd->GetActiveView();

	// failing that, use the current focus
	if (pView == NULL)
		pView = GetFocus();

	return pView;
}

//

/* Activation du MDI */
void CSplitterFrame::OnMDIActivate(BOOL bActivate, CWnd* pActivateWnd, CWnd* pDeactivateWnd) 
{
	CMDIChildWnd::OnMDIActivate(bActivate, pActivateWnd, pDeactivateWnd);
  SetMenuPrefs();
  /* disable ext. entries by default */
  this_CSplitterFrame->EnableExtEntries(FALSE);
  this_CSplitterFrame->EnableSaveEntries(TRUE);

  if (!bActivate) {
    int cxCur,cxMin;
    TCHAR szBuffer[sizeof("-32767")*8];
    m_wndSplitter.GetColumnInfo(0,cxCur,cxMin);
    sprintf(szBuffer,"%d,%d",cxCur,cxMin);
	  AfxGetApp()->WriteProfileString(szSection, szWindowPos, szBuffer);
  }
}


void CSplitterFrame::OnClose() 
{
  int cxCur,cxMin;
  TCHAR szBuffer[sizeof("-32767")*8];
  m_wndSplitter.GetColumnInfo(0,cxCur,cxMin);
  sprintf(szBuffer,"%d,%d",cxCur,cxMin);
	AfxGetApp()->WriteProfileString(szSection, szWindowPos, szBuffer);

	CMDIChildWnd::OnClose();
}
