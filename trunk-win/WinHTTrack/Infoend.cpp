// infoend.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "infoend.h"
#include "iplog.h"
#include "NewProj.h"

extern CNewProj* dialog0;
extern "C" {
  #include "HTTrackInterface.h"
  //#include "htsbase.h"
  HTS_INLINE int fspc(FILE* fp,const char* type);
}

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
extern HICON httrack_icon;

#include "winhttrack.h"
extern CWinHTTrackApp* this_app;

#include "inprogress.h"
extern Cinprogress* inprogress;

/* Externe C */
extern "C" {
  #include "HTTrackInterface.h"
};

// Helper
extern LaunchHelp* HtsHelper;

/* Main splitter frame */
#include "DialogContainer.h"
#include "splitter.h"
extern CSplitterFrame* this_CSplitterFrame;

/* Main WizTab frame */
#include "WizTab.h"
extern CWizTab* this_CWizTab;
extern CWizTab* this_intCWizTab2;

/* Objet lui même */
Cinfoend* this_Cinfoend=NULL;

/////////////////////////////////////////////////////////////////////////////
// Cinfoend dialog

IMPLEMENT_DYNCREATE(Cinfoend, CPropertyPage)

Cinfoend::Cinfoend()
	: CPropertyPage(Cinfoend::IDD)
{
  this_Cinfoend=this;
	//{{AFX_DATA_INIT(Cinfoend)
	//}}AFX_DATA_INIT
}

Cinfoend::~Cinfoend() {
  this_Cinfoend=NULL;
}

void Cinfoend::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(Cinfoend)
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(Cinfoend, CPropertyPage)
	//{{AFX_MSG_MAP(Cinfoend)
	ON_BN_CLICKED(IDlog, Onlog)
	ON_BN_CLICKED(IDbrowse, Onbrowse)
	ON_WM_HELPINFO()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
  ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
  ON_COMMAND(ID_HELP,OnHelpInfo2)
	ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Cinfoend message handlers

BOOL Cinfoend::OnInitDialog() 
{
  UpdateData(false);      // force to call DoDataExchange

	//CPropertyPage::OnInitDialog();
	
  SetIcon(httrack_icon,false);
  SetIcon(httrack_icon,true);  
  EnableToolTips(true);     // TOOL TIPS
  //SetForegroundWindow();   // yop en premier plan!

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    //SetDlgItemTextCP(this, ,"");
    //SetWindowTextCP(this, LANG(LANG_D6) /*"Fin du miroir"*/);
    SetDlgItemTextCP(this, IDlog,LANG(LANG_D7) /*"Voir fichier d'audit"*/);
    SetDlgItemTextCP(this, IDbrowse,LANG(LANG_D8) /*"Lancer Web"*/);
    SetDlgItemTextCP(this_CWizTab, IDCANCEL,LANG_QUIT);
    //SetDlgItemTextCP(this, IDC_NewProject,LANG_D9);
    //SetDlgItemTextCP(this, IDOK,LANG_OK);
  }

  int error = HTS_STAT.stat_errors;
  if (error) {
    SetDlgItemTextCP(this, IDlog,LANG_O14);
    tm=SetTimer(WM_TIMER,250,NULL);
  }

	return TRUE;
}

void Cinfoend::Onlog() 
{
	char catbuff[CATBUFF_SIZE];
  char pathlog[HTS_URLMAXSIZE*2];
  strcpybuff(pathlog,dialog0->GetPath());
  Ciplog form;
  if (strlen(pathlog)>0)
  if ((pathlog[strlen(pathlog)-1]!='/') && (pathlog[strlen(pathlog)-1]!='\\'))
    strcatbuff(pathlog,"/");
  // fichier log existe ou on est télécommandé par un !
  if ( (fexist(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-err.txt")))
    || (fexist(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-log.txt"))) ) {
    strcpybuff(form.pathlog,pathlog);
    form.DoModal();
  } else {
    char s[HTS_URLMAXSIZE*2];
    sprintf(s,LANG(LANG_D1 /*"No log files in %s!"*/ ),pathlog);
    AfxMessageBox(s,MB_OK+MB_ICONEXCLAMATION);
  }
}

void Cinfoend::Onbrowse() 
{
	char catbuff[CATBUFF_SIZE];
  char pathlog[HTS_URLMAXSIZE*2];
  strcpybuff(pathlog,dialog0->GetPath());
  if (strlen(pathlog)==0)
    strcpybuff(pathlog,dialog0->GetPath());
  Ciplog form;
  if (strlen(pathlog)>0)
  if ((pathlog[strlen(pathlog)-1]!='/') && (pathlog[strlen(pathlog)-1]!='\\'))
    strcatbuff(pathlog,"\\");
  if ( fexist(fconcat(catbuff,sizeof(catbuff),pathlog,"index.html")) ) {
    ShellExecute(this->m_hWnd,"open",fconcat(catbuff,sizeof(catbuff),pathlog,"index.html"),"","",SW_RESTORE);	
  } else {
    char s[HTS_URLMAXSIZE*2];
    sprintf(s,LANG(LANG_D2 /*"No index.html file in %s!"*/ ),pathlog);
    AfxMessageBox(s,MB_OK+MB_ICONEXCLAMATION);
  }
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
BOOL Cinfoend::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
char* Cinfoend::GetTip(int ID)
{
  switch(ID) {
    case IDOK:     return LANG(LANG_D3 /*"Click to quit WinHTTrack"*/ ); break;
    case IDlog:    return LANG(LANG_D4 /*"View log files"*/ ); break;
    case IDbrowse: return LANG(LANG_D5 /*"Browse html start page"*/ ); break;
    //case : return ""; break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------



// Appel aide
void Cinfoend::OnHelpInfo2() {
  (void) OnHelpInfo(NULL);
}

BOOL Cinfoend::OnHelpInfo(HELPINFO* dummy) 
{
  //return CPropertyPage::OnHelpInfo(pHelpInfo);
  HtsHelper->Help("step5.html");
  return true;
  //AfxGetApp()->WinHelp(0,HELP_FINDER);    // Index du fichier Hlp
  //return true;
}

void Cinfoend::OnBye() 
{
  //this_CSplitterFrame->SetNewView(0,1,RUNTIME_CLASS(CDialogContainer));	
  // // // this_CWizTab2->DestroyWindow(); // crash!!

  //delete this_intCWizTab2->m_tabprogress;  // agh..
  //this_intCWizTab2->m_tabprogress=NULL; inprogress=NULL; this_app->m_tabprogress=NULL;    // re agh..

  /* tout effacer */
  //##while(this_intCWizTab2->GetPageCount()>0)
  //##  this_intCWizTab2->RemovePage(0);
  
  //##this_intCWizTab2=NULL;
  GetMainWindow()->SendMessage(WM_COMMAND,wm_ViewRestart,0);
}

BOOL Cinfoend::OnQueryCancel( ) {
  //if (AfxMessageBox(LANG(LANG_J1),MB_OKCANCEL)==IDOK) {
  /* Envoyer un WM_CLOSE à notre fenêtre principale */
  GetMainWindow()->SendMessage(WM_CLOSE,0,0);
  //}
  return FALSE;
}

BOOL Cinfoend::OnSetActive( ) {
  // détruire ICI sinon crash!!!!
  WHTT_LOCATION("Infoend");
  if (this_intCWizTab2) {
    this_intCWizTab2->DestroyWindow();
    delete this_intCWizTab2;
    this_intCWizTab2=NULL;
  }

  this_CWizTab->SetWizardButtons(PSWIZB_FINISH);
  SetWindowTextCP(this_app->GetMainWnd(), LANG_F18b);
  SetDlgItemTextCP(this_CWizTab, IDCANCEL,LANG_QUIT);
  return 1;
}

BOOL Cinfoend::OnWizardFinish( ) {
  OnBye();
	return 0;
}


void Cinfoend::OnTimer(UINT_PTR nIDEvent) 
{
  static BOOL wflag=FALSE;

  wflag=!wflag;

  CWnd* wnd=GetDlgItem(IDlog);
  if (wnd) {
    CString st="";
    if (wflag)
      st=LANG_O14;
    SetDlgItemTextCP(this, IDlog,st);
  }
  
	CPropertyPage::OnTimer(nIDEvent);
}

void Cinfoend::OnDestroy() 
{
  if (tm!=0) {
    KillTimer(tm); 
    tm=-1; 
  }
	CPropertyPage::OnDestroy();
}
