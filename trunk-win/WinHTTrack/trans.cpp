// trans.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "trans.h"

extern "C" {
  #include "HTTrackInterface.h"
}

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern char OPTIONhh[32];
extern char OPTIONmm[32];
extern char OPTIONss[32];
extern HICON httrack_icon;

// Chargement des librairies RAS
#if USE_RAS
extern CDynamicRAS* LibRas;
extern int LibRasUse;
#endif

// Helper
extern LaunchHelp* HtsHelper;

// reference sur objet
extern Ctrans* dialog2;

/* Main WizTab frame */
#include "WizTab.h"
extern CWizTab* this_CWizTab;
extern CWizTab* this_intCWizTab2;

/* Main splitter frame */
#include "DialogContainer.h"
#include "splitter.h"
extern CSplitterFrame* this_CSplitterFrame;

// prise en compte prefs
extern void compute_options() ;

/* Back to FirstInfo */
//#include "FirstInfo.h"

/* Dialog final */
#include "inprogress.h"

/* maintab */
#include "maintab.h"
extern CMainTab* maintab;

/* newproj */
#include "newproj.h"
extern CNewProj* dialog0;

extern CShellApp* CShellApp_app;

/* debug */
extern FILE* fp_debug;


/////////////////////////////////////////////////////////////////////////////
// Ctrans dialog

IMPLEMENT_DYNCREATE(Ctrans, CPropertyPage)

Ctrans::Ctrans()
	: CPropertyPage(Ctrans::IDD)
{
  dialog2=this;    /* NOTER REFERENCE */
  hms=0;
	//{{AFX_DATA_INIT(Ctrans)
	m_hh = _T("");
	m_mm = _T("");
	m_ss = _T("");
	m_rasid = -1;
	m_rasdisc = FALSE;
	m_rasshut = FALSE;
	//}}AFX_DATA_INIT
}

Ctrans::~Ctrans() {
  dialog2=NULL;
}

void Ctrans::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(Ctrans)
	DDX_Control(pDX, IDC_rasshut, m_ctl_rasshut);
	DDX_Control(pDX, IDC_rasdisc, m_ctl_rasdisc);
	DDX_Control(pDX, IDC_cnx, m_ctlcnx);
	DDX_Control(pDX, IDC_rasid, m_ctlrasid);
	DDX_Control(pDX, IDC_wait, m_ctl_wait);
	DDX_Control(pDX, IDC_ss, m_ctl_ss);
	DDX_Control(pDX, IDC_mm, m_ctl_mm);
	DDX_Control(pDX, IDC_hh, m_ctl_hh);
	DDX_Text(pDX, IDC_hh, m_hh);
	DDV_MaxChars(pDX, m_hh, 2);
	DDX_Text(pDX, IDC_mm, m_mm);
	DDV_MaxChars(pDX, m_mm, 2);
	DDX_Text(pDX, IDC_ss, m_ss);
	DDV_MaxChars(pDX, m_ss, 2);
	DDX_CBIndex(pDX, IDC_rasid, m_rasid);
	DDX_Check(pDX, IDC_rasdisc, m_rasdisc);
	DDX_Check(pDX, IDC_rasshut, m_rasshut);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(Ctrans, CPropertyPage)
	//{{AFX_MSG_MAP(Ctrans)
	ON_EN_CHANGE(IDC_hh, OnChangehh)
	ON_WM_HELPINFO()
	ON_CBN_SELCHANGE(IDC_rasid, OnSelchangerasid)
	ON_CBN_DROPDOWN(IDC_rasid, OnDropdownrasid)
	ON_WM_SHOWWINDOW()
	ON_BN_CLICKED(IDC_rasdisc, Onrasdisc)
	//}}AFX_MSG_MAP
	ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
  ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
  ON_COMMAND(ID_HELP,OnHelpInfo2)
	ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Ctrans message handlers

void Ctrans::OnChangehh() 
{
  CString st="";
  char tempo[256];

  // TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CPropertyPage::OnInitDialog()
	// function to send the EM_SETEVENTMASK message to the control
	// with the ENM_CHANGE flag ORed into the lParam mask.

  // TODO: Add your control notification handler code here

  GetDlgItemText(IDC_hh,st);
  strcpybuff(tempo,st);

  if ((strlen(tempo)>0)!=hms) {
    hms=!hms;
    if (strlen(tempo)>0) {
      m_ctl_wait.ModifyStyle(WS_DISABLED,0);
    }
    else {
      m_ctl_wait.ModifyStyle(0,WS_DISABLED);
    }    
    m_ctl_wait.RedrawWindow();
 }  

// m_wait=TRUE; 

}

BOOL Ctrans::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();

  ((CButton*)GetDlgItem(IDC_select_start))->SetCheck(1);
  strcpybuff(RasString,"");
  
  SetIcon(httrack_icon,false);
  SetIcon(httrack_icon,true);  
	EnableToolTips(true);     // TOOL TIPS

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    //SetDlgItemTextCP(this, ,"");
    SetWindowTextCP(this, LANG(LANG_J9) /*"Démarrer.."*/);
    SetDlgItemTextCP(this, IDC_select_start,LANG(LANG_J10) /*"Vous pouvez démarrer le miroir en pressant la touche DEMARRER,\nou définir avant les options de connexion"*/);
    SetDlgItemTextCP(this, IDC_select_save,LANG(LANG_J10b));
    SetDlgItemTextCP(this, IDC_STATIC_delay,LANG(LANG_J11) /*"Retarder"*/);
    SetDlgItemTextCP(this, IDC_wait,LANG(LANG_J12) /*"Attendre avant de commencer jusqu'à: (hh/mm/ss)"*/);
    //SetDlgItemTextCP(this, IDC_avant,LANG(LANG_BACK) /*"<- AVANT"*/);
    SetDlgItemTextCP(this, IDCANCEL,LANG(LANG_QUIT) /*"Quitter"*/);
    SetDlgItemTextCP(this, IDOK,LANG(LANG_J13) /*"DEMARRER!"*/);
    SetDlgItemTextCP(this, IDC_STATIC_ras,LANG(LANG_J14) /*"Connexion provider"*/);
    SetDlgItemTextCP(this, IDC_cnx,LANG(LANG_J15) /*"Connecter à ce provider"*/);
    SetDlgItemTextCP(this, IDC_rasdisc,LANG_J16);
  }

  // liste vide pour commencer
  isfilled=FALSE;
  FillProviderList(0);
  m_ctlrasid.SetCurSel(0);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


void Ctrans::FillProviderList(int fill) 
{
// remplssage su champ des noms de connexion RAS
#if USE_RAS
  if (LibRasUse && fill) {
    DWORD size;
    RASENTRYNAME* adr;
    int count=256;
    size = sizeof(RASENTRYNAME)*(count+2);
    adr = (RASENTRYNAME*) (char*) calloc(size,1);
    if (adr) {
      DWORD ent;
      int i;
      for(i=0;i<count;i++) {
        adr[i].dwSize=sizeof(RASENTRYNAME);
        strcpybuff(adr[i].szEntryName,"");
      }
      if (LibRas->RasEnumEntries(NULL,NULL,(RASENTRYNAME*) adr,&size,&ent) == 0) {
        int i;
        GetDlgItem(IDC_STATIC_ras)->ModifyStyle(WS_DISABLED,0);
        GetDlgItem(IDC_STATIC_ras)->RedrawWindow();
        m_ctlrasid.ResetContent();
        m_ctlrasid.InsertString(-1,LANG(LANG_J2 /*"do not connect to a provider (already connected)","pas de connexion à un provider (déja connecté)"*/));
        for(i=0;i<(int) ent;i++) {
          m_ctlrasid.InsertString(-1,adr[i].szEntryName);
        }
      }
      free(adr);
    }
  } else
#endif
  {
    if (LibRasUse) {
      GetDlgItem(IDC_STATIC_ras)->ModifyStyle(WS_DISABLED,0);
    } else {
      GetDlgItem(IDC_STATIC_ras)->ModifyStyle(0,WS_DISABLED);
    }
    GetDlgItem(IDC_STATIC_ras)->RedrawWindow();
    m_ctlrasid.ResetContent();
    m_ctlrasid.InsertString(-1,LANG_J2b);
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
BOOL Ctrans::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
char* Ctrans::GetTip(int ID)
{
  switch(ID) {
  case IDC_rasid: return LANG_J8b; break;
    case IDC_hh: case IDC_mm: case IDC_ss: 
      return LANG(LANG_J3); break; // "Schedule the mirror","Programmer un miroir"); break;
    case IDCANCEL:  return LANG(LANG_J4); break; // "Quit WinHTTrack","Quitter WinHTTrack"); break;
    case IDC_avant: return LANG(LANG_J5); break; // "Back to the start page","Retour à la première page"); break;
    case IDOK:      return LANG(LANG_J6); break; // "Click to start!","Clic pour démarrer!"); break;
    case IDC_rasdisc: return LANG_J17; break;
    //case : return ""; break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------






// Appel aide
void Ctrans::OnHelpInfo2() {
  (void) OnHelpInfo(NULL);
}

BOOL Ctrans::OnHelpInfo(HELPINFO* dummy) 
{
  //return CPropertyPage::OnHelpInfo(pHelpInfo);
  HtsHelper->Help();
  return true;
  //AfxGetApp()->WinHelp(0,HELP_FINDER);    // Index du fichier Hlp
  //return true;
}

void Ctrans::OnSelchangerasid() 
{
  m_ctl_rasdisc.SetCheck(1);
  int r = m_ctlrasid.GetCurSel();
  if (r!=CB_ERR) {
    if (r==0) {
      m_ctlcnx.ModifyStyle(0,WS_DISABLED); 
    } else {
      m_ctlcnx.ModifyStyle(WS_DISABLED,0); 
    }
    m_ctlcnx.RedrawWindow();
  }
}

BOOL Ctrans::OnWizardFinish( )
{
  BOOL DoLaunchMirror = ((CButton*)GetDlgItem(IDC_select_start))->GetCheck();

	// TODO: Add extra validation here
  int r = m_ctlrasid.GetCurSel();
  strcpybuff(RasString,"");
  if ((r!=CB_ERR) && (r != 0)) {    // sélection provider
    if (m_ctlrasid.GetLBText(r,RasString) != CB_ERR) {
      if (strlen(RasString)>0) {
        //
#if USE_RAS
        if (LibRasUse) {
          if (strlen(RasString)>0) {    // sélection provider
            BOOL ifpass;
            dial.dwSize = sizeof(dial);
            strcpybuff(dial.szEntryName,RasString);
            strcpybuff(dial.szPhoneNumber,"");
            strcpybuff(dial.szCallbackNumber,"");
            strcpybuff(dial.szUserName,"");
            strcpybuff(dial.szPassword,"");
            strcpybuff(dial.szDomain,"");
            if (!LibRas->RasGetEntryDialParams(NULL,&dial,&ifpass)) {
              if (!ifpass) {    // pas de pass
                AfxMessageBox(LANG(LANG_J7 /*"No saved password for this connection!","Pas de mot de passe sauvé pour cette connexion!"*/));
                return 0;    // cancel
              }
            } else {
              AfxMessageBox(LANG(LANG_J8 /*"Can not get RAS setup","Impossible d'obtenir les paramètres de connexion"*/));
              return 0;    // cancel
            }
          }
        }
#endif
        //
      }
    }
  }

  /* prise en compte des options */
  UpdateData(TRUE);         // DoDataExchange
  compute_options() ;

  // Index "top" si nécessaire!
  if (maintab->m_option3.m_windebug) {
    fp_debug=fopen("winhttrack.log","wb");
  } else {
    fp_debug = NULL;
  }
  if (fp_debug) {
    fprintf(fp_debug,"Writing winprofile.ini\r\n");
    fflush(fp_debug);
  }
  /*
  {
    char path[1024];
    strcpybuff(path,dialog0->GetBasePath());
    make_empty_index(path);
  }
  */
  
  // sauver profile dans hts-cache si besoin est
  //if(maintab->m_option3.m_cache) {
  Save_current_profile(0);
  //}
  // FIN sauver profile dans hts-cache si besoin est
  if (fp_debug) {
    if (DoLaunchMirror)
      fprintf(fp_debug,"Profile written, launching mirror\r\n");
    else
      fprintf(fp_debug,"Profile written, saving settings\r\n");
    fflush(fp_debug);
  }
  CShellApp_app->end_path=dialog0->GetBasePath();
  CShellApp_app->end_path_complete=dialog0->GetPath();

  if (DoLaunchMirror) {
    /* Switcher sur Cinprogress! */
    /*this_CSplitterFrame->SetNewView(0,1,RUNTIME_CLASS(Cinprogress));*/
    this_CWizTab->ModifyStyle(WS_VISIBLE,0,0);
    this_CWizTab->RedrawWindow();
    //
    //{
    //  CWizTab* tmp;
    //  tmp=this_CWizTab;
    this_CWizTab=this_intCWizTab2;
    //  this_CWizTab2=tmp;
    //}
    //
    this_CWizTab->ModifyStyle(0,WS_VISIBLE,0);
    this_CWizTab->ModifyStyle(WS_DISABLED,0,0);
    this_CSplitterFrame->RedrawWindow();
    this_CWizTab->FinalInProgress();
    // Lancement du miroir
    LaunchMirror();
  } else {      /* Ok, saved, exit (new project) */
    Build_TopIndex();
    this_CSplitterFrame->SetNewView(0,1,RUNTIME_CLASS(CDialogContainer));
  }

	return 0;
}

void Ctrans::OnDropdownrasid() 
{
#if USE_RAS
  if (!isfilled) {
    isfilled=TRUE;
    InitRAS();
    FillProviderList(1);
  }
#endif
}

BOOL Ctrans::OnSetActive( ) {
  this_CWizTab->SetWizardButtons(PSWIZB_BACK|PSWIZB_FINISH);
  WHTT_LOCATION("trans");
  return 1;
}

BOOL Ctrans::OnQueryCancel( ) {
  this_CSplitterFrame->SetNewView(0,1,RUNTIME_CLASS(CDialogContainer));
  return 0;
}

BOOL Ctrans::OnKillActive( ) {
  UpdateData(TRUE);         // DoDataExchange
  return 1;
}

void Ctrans::Onrasdisc() 
{
  InitRAS();
}
