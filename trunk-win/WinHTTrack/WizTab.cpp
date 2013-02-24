// Tab Control Principal

#include "stdafx.h"
#include "Shell.h"
#include "WizTab.h"
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

CWizTab* this_CWizTab;
CWizTab* this_intCWizTab;
CWizTab* this_intCWizTab2;

// Icone HTTrack
extern HICON httrack_icon;

// Instance WinHTTrack
#include "WinHTTrack.h"
extern CWinHTTrackApp* this_app;

// Helper
extern LaunchHelp* HtsHelper;
extern CMainTab* maintab;

/* Main splitter frame */
#include "DialogContainer.h"
#include "splitter.h"
extern CSplitterFrame* this_CSplitterFrame;

/////////////////////////////////////////////////////////////////////////////
// CWizTab

IMPLEMENT_DYNAMIC(CWizTab, CPropertySheet)

CWizTab::CWizTab(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
  AddControlPages();
}

CWizTab::CWizTab(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
  AddControlPages();
}

CWizTab::CWizTab(LPCTSTR pszCaption, int num, CWnd* pParentWnd, UINT iSelectPage)
:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
  is_inProgress=num;
  AddControlPages();
}

CWizTab::~CWizTab()
{
  ClearInits();
}

void CWizTab::DoInits() {
  m_tab0=NULL;
  m_tab1=NULL;
  m_tab2=NULL;
  m_tab3=NULL;
  m_tabend=NULL;
  m_tabprogress=NULL;
  if (!is_inProgress) {
    m_tab0=this_app->m_tab0;
    m_tab1=this_app->m_tab1;
    m_tab2=this_app->m_tab2;
    m_tab3=this_app->m_tab3;
    m_tabend=this_app->m_tabend;
  } else {
    m_tabprogress=this_app->m_tabprogress;
  }
  // Lecture du profile par défaut
  maintab = new CMainTab("WinHTTrack");
  //if (!is_inProgress)   // sinon buggue
  Read_profile("",0);
}

void CWizTab::ClearInits() {
  if (maintab)
    delete maintab;
  maintab=NULL;
  if (!is_inProgress)
    this_intCWizTab=NULL;
  else
    this_intCWizTab2=NULL;
}

void CWizTab::AddControlPages()
{
  m_hIcon = httrack_icon;
  m_psh.dwFlags |= PSP_USEHICON;  // utiliser icône
  m_psh.dwFlags &= ~PSH_HASHELP;  // pas de bouton help
  m_psh.hIcon = m_hIcon;
  //m_psh.pszIcon = "test";

  // objet lui même
  if (!is_inProgress)
    this_intCWizTab=this;
  else
    this_intCWizTab2=this;

  if (!is_inProgress)
    this_CWizTab=this;

  // pas de "apply"
  this->m_psh.dwFlags|=PSH_NOAPPLYNOW;

  DoInits();

  // Ajout des Control TAB dans la feuille principale (MainTab)
  if (!is_inProgress) {
    AddPage(m_tab0);
    AddPage(m_tab1);
    AddPage(m_tab2);
    AddPage(m_tab3);
  }

  /* uniquement pour calibrer la page, retiré lors de initdialog */
  if (is_inProgress)
    AddPage(m_tabprogress);

  if (!is_inProgress)
    AddPage(m_tabend);

  //SetActivePage(0);
  SetWizardMode(); 
}

void CWizTab::FinalInProgress()
{
  if (is_inProgress) {
    /* tout effacer */
    while(GetPageCount()>0)
      RemovePage(0);
    /* page finale */
    SetWizardButtons(0);
    AddPage(m_tabprogress);
    SetActivePage(0);
    
    /* enable ext. entries */
    this_CSplitterFrame->EnableExtEntries(TRUE);
    this_CSplitterFrame->EnableSaveEntries(FALSE);
  }
}

void CWizTab::EndInProgress()
{
  if (!is_inProgress) {
    /* unhide */
    this_CSplitterFrame->CheckRestore();
    /* tout effacer */
    while(GetPageCount()>0)
      RemovePage(0);
    
    /* page fin */
    SetWizardButtons(0);
    AddPage(m_tabend);
    SetActivePage(0);
    
    /* disable ext. entries */
    this_CSplitterFrame->EnableExtEntries(FALSE);
    this_CSplitterFrame->EnableSaveEntries(FALSE);
  }
}

BEGIN_MESSAGE_MAP(CWizTab, CPropertySheet)
//{{AFX_MSG_MAP(CWizTab)
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
// CWizTab message handlers

BOOL CWizTab::OnInitDialog()
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
  if (!is_inProgress)
    LoadPrefs();
  
  // Appliquer préférences
  Apply();
  
  int r = CPropertySheet::OnInitDialog();
  //xx RemovePage(m_tabprogress);
  //xx RemovePage(m_tabend);
  SetWizardButtons(PSWIZB_BACK|PSWIZB_NEXT);
  SetWindowPos(&wndTop,0,0,0,0,SWP_NOZORDER|SWP_NOSIZE|SWP_NOOWNERZORDER);
  SetActivePage(0);
  WHTT_LOCATION("FirstInfo");

  return r;
}

// L'utilisateur a appuyé sur "Apply"
void CWizTab::OnApplyNow()
{
  EnableWindow(false);
  Default();
  ApplyAndSave();
  EnableWindow(true);
}

// Sauver et appliquer les préférences
void CWizTab::ApplyAndSave() {
  CWaitCursor wait;      // Afficher curseur sablier
  bool err=false;  // Erreur lors de l'écriture des paramètres
  
  // Appliquer les préférences
  Apply();
  
  // Sauver préférences
  CWinApp* pApp = AfxGetApp();
 
  if (err)
    AfxMessageBox(LANG(LANG_DIAL2));
}

// Appliquer préférences
void CWizTab::Apply() {
  // Appliquer préférences
}

// Chargement des préférences
void CWizTab::LoadPrefs() {
  CWinApp* pApp = AfxGetApp();
  //n = pApp->GetProfileInt("Driver", "DriverId",0);   // No du driver
}

// Appel aide
void CWizTab::OnHelpInfo2() {
  (void)OnHelpInfo(NULL);
}

BOOL CWizTab::OnHelpInfo(HELPINFO* dummy) 
{
  //return CDialog::OnHelpInfo(pHelpInfo);
  //AfxGetApp()->WinHelp(0,HELP_FINDER);    // Index du fichier Hlp
  //LaunchHelp(pHelpInfo);

  /*if (this->GetActivePage() == m_tab1)
    HtsHelper->Help("xxc");
  else*/
    HtsHelper->Help();
  return true;
}


/*
// Capturer OK et Cancel
void CWizTab::OnOK( ) {
  // Sauver et appliquer préférences
  ApplyAndSave();
}
void CWizTab::OnCancel( ) {
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
char* CWizTab::GetTip(int ID)
{
  /*switch(ID) {
  }*/
  return "";
}
BOOL CWizTab::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
// TOOL TIPS
// ------------------------------------------------------------

