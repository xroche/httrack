// Tab Control Principal

#include "stdafx.h"
#include "Shell.h"
#include "Maintab.h"
#include "direct.h"

#include "winsvc.h "

#include "windows.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Icone HTTrack
extern HICON httrack_icon;

// Helper
extern LaunchHelp* HtsHelper;


/////////////////////////////////////////////////////////////////////////////
// CMainTab

//IMPLEMENT_DYNAMIC(CMainTab, CPropertySheet)

//HINSTANCE hInst = NULL;
//SC_HANDLE hSCMan = NULL;


CMainTab::CMainTab(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
  AddControlPages();
}

CMainTab::CMainTab(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
  AddControlPages();
}

CMainTab::~CMainTab()
{
}

void CMainTab::AddControlPages()
{
  m_hIcon = httrack_icon;
  m_psh.dwFlags |= PSP_USEHICON;  // utiliser icône
  m_psh.dwFlags &= ~PSH_HASHELP;  // pas de bouton help
  m_psh.hIcon = m_hIcon;
  //m_psh.pszIcon = "test";

  // pas de "apply"
  this->m_psh.dwFlags|=PSH_NOAPPLYNOW;

  // Ajout des Control TAB dans la feuille principale (MainTab)
  AddPage(&m_option10);       /* Proxy */
  AddPage(&m_option7);        /* Filters */
  AddPage(&m_option5);        /* Limits */
  AddPage(&m_option4);        /* Flow Control */
  AddPage(&m_option1);        /* Links */
  AddPage(&m_option2);        /* Build */
  AddPage(&m_option8);        /* Spider */
  AddPage(&m_option11);       /* MIME types */
  AddPage(&m_option6);        /* Browser ID */
  AddPage(&m_option9);        /* Log, Index, cache */
  AddPage(&m_option3);        /* Expert */
}

void CMainTab::DefineDefaultProxy()
{
  while(GetPageCount()>0)
    RemovePage(0);
  AddPage(&m_option10);       /* Only proxy */
}

void CMainTab::UnDefineDefaultProxy() {
  AddControlPages();
}

BEGIN_MESSAGE_MAP(CMainTab, CPropertySheet)
//{{AFX_MSG_MAP(CMainTab)
ON_WM_QUERYDRAGICON()
ON_WM_SYSCOMMAND()
ON_WM_TIMER()
ON_WM_HELPINFO()
//}}AFX_MSG_MAP
ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
ON_COMMAND(ID_HELP,OnHelpInfo2)
ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
//ON_BN_CLICKED(IDOK, OnOK)
//ON_BN_CLICKED(IDCANCEL, OnCancel)
ON_COMMAND(ID_APPLY_NOW,OnApplyNow)
ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMainTab message handlers

BOOL CMainTab::OnInitDialog()
{
  // IDM_ABOUTBOX must be in the system command range.
  //ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
  //ASSERT(IDM_ABOUTBOX < 0xF000);
  
  SetIcon(m_hIcon, TRUE);
  SetIcon(m_hIcon, FALSE);
  EnableToolTips(true);     // TOOL TIPS

/*
  CMenu* pSysMenu = GetSystemMenu(FALSE);
  if (pSysMenu != NULL)
  {
    CString strAboutMenu;
    strAboutMenu.LoadString(IDS_ABOUTBOX);
    if (!strAboutMenu.IsEmpty())
    {
      pSysMenu->AppendMenu(MF_SEPARATOR);
      pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
    }
  }
*/  
  // Chargement des préférences
  LoadPrefs();
  
  // Appliquer préférences
  Apply();
  
  int r = CPropertySheet::OnInitDialog();
  //SetActivePage(GetPageCount()-1);
  SetActivePage(0);

  // mode modif à la volée
  return r;
}
HCURSOR CMainTab::OnQueryDragIcon()
{
  return (HCURSOR) m_hIcon;
}

BOOL CMainTab::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext)
{
  //removing the default DS_CONTEXT_HELP style
  //dwStyle= WS_SYSMENU | WS_POPUP | WS_CAPTION | DS_MODALFRAME | WS_VISIBLE;
  return CWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext);
}

void CMainTab::OnSysCommand(UINT nID, LPARAM lParam)
{
  /*if ((nID & 0xFFF0) == IDM_ABOUTBOX)
  {
    SetActivePage(GetPageCount()-1);    // Afficher informations sur le programme et affichant la dernière page des control TAB
 	}
  else
  {
  */
    CPropertySheet::OnSysCommand(nID, lParam);
  /*}
  */
}

// L'utilisateur a appuyé sur "Apply"
void CMainTab::OnApplyNow()
{
  EnableWindow(false);
  Default();
  ApplyAndSave();
  EnableWindow(true);
}

// Sauver et appliquer les préférences
void CMainTab::ApplyAndSave() {
  CWaitCursor wait;      // Afficher curseur sablier
  bool err=false;  // Erreur lors de l'écriture des paramètres
  
  // Appliquer les préférences
  Apply();
  
  // Sauver préférences
  CWinApp* pApp = AfxGetApp();
  //if (!pApp->WriteProfileInt("Driver", "DriverId",numero_driver))          // No du driver
  //  err=true;
 
  if (err)
    AfxMessageBox(LANG(LANG_DIAL2));
}

// Appliquer préférences
void CMainTab::Apply() {
  // Appliquer préférences
}

// Chargement des préférences
void CMainTab::LoadPrefs() {
  CWinApp* pApp = AfxGetApp();
  //n = pApp->GetProfileInt("Driver", "DriverId",0);   // No du driver
}

// Appel aide
void CMainTab::OnHelpInfo2() {
  (void)OnHelpInfo(NULL);
}

BOOL CMainTab::OnHelpInfo(HELPINFO* dummy) 
{
  //return CDialog::OnHelpInfo(pHelpInfo);
  //AfxGetApp()->WinHelp(0,HELP_FINDER);    // Index du fichier Hlp
  //LaunchHelp(pHelpInfo);

  if (this->GetActivePage() == &m_option1)
    HtsHelper->Help("step9_opt1.html");
  else if (this->GetActivePage() == &m_option5)
    HtsHelper->Help("step9_opt2.html");
  else if (this->GetActivePage() == &m_option4)
    HtsHelper->Help("step9_opt3.html");
  else if (this->GetActivePage() == &m_option7)
    HtsHelper->Help("step9_opt4.html");
  else if (this->GetActivePage() == &m_option2)
    HtsHelper->Help("step9_opt5.html");
  else if (this->GetActivePage() == &m_option8)
    HtsHelper->Help("step9_opt6.html");
  else if (this->GetActivePage() == &m_option10)
    HtsHelper->Help("step9_opt7.html");
  else if (this->GetActivePage() == &m_option6)
    HtsHelper->Help("step9_opt8.html");
  else if (this->GetActivePage() == &m_option9)
    HtsHelper->Help("step9_opt9.html");
  else if (this->GetActivePage() == &m_option3)
    HtsHelper->Help("step9_opt10.html");
  else if (this->GetActivePage() == &m_option11)
    HtsHelper->Help("step9_opt11.html");
  else
    HtsHelper->Help();
  return true;
}


/*
// Capturer OK et Cancel
void CMainTab::OnOK( ) {
  // Sauver et appliquer préférences
  ApplyAndSave();
}
void CMainTab::OnCancel( ) {
  // Recharger préférences
  LoadPrefs();
}
*/



// ------------------------------------------------------------
// TOOL TIPS
//
// ajouter dans le .cpp:
// remplacer les deux <nom classe>:: par le nom de la classe::
// dans la message map, ajouter
// ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
// dans initdialog ajouter
// EnableToolTips(true);     // TOOL TIPS
//
// ajouter dans le .h:
// char* GetTip(int id);
// et en generated message map
// afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
char* CMainTab::GetTip(int ID)
{
  switch(ID) {
    case IDOK:           return LANG(LANG_TIPOK); break;      
    case IDCANCEL:       return LANG(LANG_TIPCANCEL); break;
    case IDHELP:         return LANG_TIPHELP; break;
  }
  return "";
}
BOOL CMainTab::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
{
  TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pNMHDR;
  UINT_PTR nID = pNMHDR->idFrom;
  if (pTTT->uFlags & TTF_IDISHWND)
  {
    // idFrom is actually the HWND of the tool
    nID = ::GetDlgCtrlID((HWND)nID);
    if(nID)
    {
      char* st = GetTip((int) nID);
      if (st != "") {
        pTTT->lpszText = st;
        pTTT->hinst = AfxGetResourceHandle();
        return(TRUE);
      }
    }
  }
  return(FALSE);
}
// TOOL TIPS
// ------------------------------------------------------------

