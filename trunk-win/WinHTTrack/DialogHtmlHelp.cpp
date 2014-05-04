// DialogHtmlHelp.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "DialogHtmlHelp.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern "C" {
  #include "HTTrackInterface.h"
  //#include "htsbase.h"
};

extern HICON httrack_icon;

/////////////////////////////////////////////////////////////////////////////
// CDialogHtmlHelp dialog


CDialogHtmlHelp::CDialogHtmlHelp(CWnd* pParent /*=NULL*/)
	: CDialog(CDialogHtmlHelp::IDD, pParent)
{
  page="";
	//{{AFX_DATA_INIT(CDialogHtmlHelp)
	//}}AFX_DATA_INIT
}


void CDialogHtmlHelp::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDialogHtmlHelp)
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDialogHtmlHelp, CDialog)
	//{{AFX_MSG_MAP(CDialogHtmlHelp)
	ON_WM_SIZE()
	ON_WM_CREATE()
	ON_BN_CLICKED(IDC_BACK, OnBack)
	ON_BN_CLICKED(IDC_FORWARD, OnForward)
	ON_BN_CLICKED(IDC_STOP, OnStop)
	ON_BN_CLICKED(IDC_RELOAD, OnReload)
	ON_BN_CLICKED(IDC_HOME, OnHome)
	ON_BN_CLICKED(ID_FILE_PRINT, OnFilePrint)
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
  ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
  ON_COMMAND(ID_HELP,OnHelpInfo2)
	ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
	ON_COMMAND(ID_BUTTON_HOME,OnHome)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDialogHtmlHelp message handlers

BOOL CDialogHtmlHelp::OnInitDialog() 
{
	CDialog::OnInitDialog();
  SetIcon(httrack_icon,false);
  SetIcon(httrack_icon,true);
  EnableToolTips(true);     // TOOL TIPS
  SetForegroundWindow();   // yop en premier plan!

  strcpybuff(home,"file://");
  {
    char* a=home+strlen(home);
    ::GetModuleFileName(NULL, a, (DWORD) ( sizeof(home)/sizeof(TCHAR) - 1 - strlen(home) ));
    // strcatbuff(home,AfxGetApp()->m_pszHelpFilePath);
    a = home + strlen(home) -1;
    while( (*a!='\\') && ( a > home ) ) a--;
    if (*a=='\\') {
      *(a+1)='\0';
    }
    strcatbuff(home,"html\\");
    strcpybuff(home_dir,home);
    strcatbuff(home,"index.html");
  }

  // créer
  if (m_page.CreateFromStatic(IDC_HTMLVIEW, this)) {
    m_page.SetToolBar(false);
    m_page.SetMenuBar(false);
    m_page.SetStatusBar(false);
    m_page.SetRegisterAsBrowser(false);
    m_page.SetFullScreen(false);
    if (page.GetLength()==0)
      OnHome();
    else
      Go(page);
    UpdateWindow();
  } else {
    EndDialog(IDCANCEL);
    if (!ShellExecute(NULL,"open",home+strlen("file://"),"","",SW_RESTORE))
      AfxMessageBox(LANG(LANG_DIAL1));
  }
  return TRUE;  
}

// Resize
void CDialogHtmlHelp::OnSize(UINT nType, int cx, int cy) 
{
  CDialog::OnSize(nType, cx, cy);

  HWND ip = ::GetDlgItem(this->m_hWnd,IDC_HTMLVIEW);
  if (ip) {
    int w,h;
    RECT rect;
    ::GetWindowRect(ip,&rect);
    // screen coord -> client coord
    POINT a,b;
    a.x=rect.left; a.y=rect.top; b.x=rect.right; b.y=rect.bottom;
    ::ScreenToClient(this->m_hWnd,&a); ::ScreenToClient(this->m_hWnd,&b);
    rect.left=a.x; rect.top=a.y; rect.right=b.x; rect.bottom=b.y;
    w = max(cx - rect.left,320);
    h = max(cy - rect.top,200);
    ::SetWindowPos(ip,NULL,0,0,w,h,SWP_NOZORDER|SWP_NOMOVE|SWP_NOOWNERZORDER);
    //base.OnSizeCallback(this,nType,cx,cy);
  }
}

int CDialogHtmlHelp::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;

  /*
  if (!m_wndToolBar.Create(this))
		return -1;      // fail to create
  if (!m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
		return -1;      // fail to create

  // TODO: Remove this if you don't want tool tips or a resizeable toolbar
	m_wndToolBar.SetBarStyle(m_wndToolBar.GetBarStyle() |
		CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
  */
	
	return 0;
}

// help de l'help..
// Appel aide
void CDialogHtmlHelp::OnHelpInfo2() {
  (void)OnHelpInfo(NULL);
}

BOOL CDialogHtmlHelp::OnHelpInfo(HELPINFO* dummy) 
{
  return true;
}


// Home
void CDialogHtmlHelp::OnHome() {
  m_page.Navigate2(home);
  m_page.ShowWindow(SW_SHOW);
  this->GetDlgItem(IDC_HTMLVIEW)->SetFocus();
}

void CDialogHtmlHelp::Go(CString st) {
  if (st.Left(7)!="http://") {
    char tempo[1024];
    strcpybuff(tempo,home_dir);
    strcatbuff(tempo,st);
    m_page.Navigate2(tempo);
  } else
    m_page.Navigate2(st);
  m_page.ShowWindow(SW_SHOW);
  this->GetDlgItem(IDC_HTMLVIEW)->SetFocus();
}

void CDialogHtmlHelp::OnBack() 
{
  m_page.GoBack();
  this->GetDlgItem(IDC_HTMLVIEW)->SetFocus();
}

void CDialogHtmlHelp::OnForward() 
{
  m_page.GoForward();
  this->GetDlgItem(IDC_HTMLVIEW)->SetFocus();
}

void CDialogHtmlHelp::OnStop() 
{
  m_page.Stop();
  this->GetDlgItem(IDC_HTMLVIEW)->SetFocus();
}

void CDialogHtmlHelp::OnReload() 
{
  m_page.Refresh();
  this->GetDlgItem(IDC_HTMLVIEW)->SetFocus();
}

void CDialogHtmlHelp::OnFilePrint() 
{
  this->GetDlgItem(IDC_HTMLVIEW)->SendMessage(WM_COMMAND,ID_FILE_PRINT,NULL);
  this->GetDlgItem(IDC_HTMLVIEW)->SetFocus();
}


// ------------------------------------------------------------
// TOOL TIPS
//
// ajouter dans le .cpp:
// remplacer les deux Wid1:: par le nom de la classe::
// dans la message map, ajouter
// ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
// dans initdialog ajouter
// EnableToolTips(true);     // TOOL TIPS
//
// ajouter dans le .h:
// char* GetTip(int id);
// et en generated message map
// afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
BOOL CDialogHtmlHelp::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
{
  TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pNMHDR;
  UINT_PTR nID =pNMHDR->idFrom;
  if (pTTT->uFlags & TTF_IDISHWND)
  {
    // idFrom is actually the HWND of the tool
    nID = ::GetDlgCtrlID((HWND)nID);
    if(nID)
    {
      char* st=GetTip((int)nID);
      if (st != "") {
        pTTT->lpszText = st;
        pTTT->hinst = AfxGetResourceHandle();
        return(TRUE);
      }
    }
  }
  return(FALSE);
}
char* CDialogHtmlHelp::GetTip(int ID)
{
  switch(ID) {
    case IDCANCEL:   return LANG(LANG_B3); /*"Cancel","Annuler");*/ break;
    case IDOK:       return LANG(LANG_B4); /*"Add this filter","Ajouter cette règle");*/ break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------


BOOL CDialogHtmlHelp::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) 
{
	// TODO: Add your specialized code here and/or call the base class

  dwStyle|=(WS_OVERLAPPEDWINDOW);
  dwStyle&=~(WS_CHILD);
  pParentWnd=NULL;
	return CDialog::Create(IDD, pParentWnd);
  //ModifyStyleEx(WS_EX_TOPMOST,0);
  //SetWindowPos(&wndBottom,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
}

/* Auto-destruction! */
void CDialogHtmlHelp::OnClose() 
{
	///CDialog::OnClose();
	int r=CDialog::DestroyWindow();
}
