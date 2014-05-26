// inprogress.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "inprogress.h"
#include "about.h"
#include "iplog.h"

#include "NewProj.h"
//#include "option.h"

/* Externe C */
extern "C" {
  #include "HTTrackInterface.h"
}

#include "Wid1.h"
#include "EasyDropTarget.h"

#include "InfoUrl.h"

extern CNewProj* dialog0;
extern CMainTab* maintab;

extern int termine_requested;
extern int termine;
extern int soft_term_requested;
extern HICON httrack_icon;
extern int termine;

// Helper
extern LaunchHelp* HtsHelper;

// InfoUrl
extern CInfoUrl* _Cinprogress_inst;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Refresh
//extern int INFILLMEM_LOCKED;     // refresh mémoire en cours
extern InpInfo SInfo;
int inprogress_refresh();

// this app
#include "Winhttrack.h"
extern CWinHTTrackApp* this_app;


// objet
extern Cinprogress* inprogress;


/* pour la fin */
#include "DialogContainer.h"
#include "splitter.h"
extern CSplitterFrame* this_CSplitterFrame;
#include "infoend.h"
extern Cinfoend* this_Cinfoend;

/* Main WizTab frame */
#include "WizTab.h"
extern CWizTab* this_CWizTab;
extern CWizTab* this_intCWizTab;

// Pour la fin
char end_mirror_msg[8192]="";

extern t_StatsBuffer StatsBuffer[NStatsBuffer];


/////////////////////////////////////////////////////////////////////////////
// Cinprogress dialog
IMPLEMENT_DYNCREATE(Cinprogress, CPropertyPage)


Cinprogress::Cinprogress()
	: CPropertyPage(Cinprogress::IDD)
{
  timer=0;
  //{{AFX_DATA_INIT(Cinprogress)
	m_inphide = FALSE;
	//}}AFX_DATA_INIT
}

void Cinprogress::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(Cinprogress)
	DDX_Control(pDX, IDC_nn9, m_nn9);
	DDX_Control(pDX, IDC_nn8, m_nn8);
	DDX_Control(pDX, IDC_nn7, m_nn7);
	DDX_Control(pDX, IDC_nn6, m_nn6);
	DDX_Control(pDX, IDC_nn5, m_nn5);
	DDX_Control(pDX, IDC_nn4, m_nn4);
	DDX_Control(pDX, IDC_nn3, m_nn3);
	DDX_Control(pDX, IDC_nn2, m_nn2);
	DDX_Control(pDX, IDC_nn13, m_nn13);
	DDX_Control(pDX, IDC_nn12, m_nn12);
	DDX_Control(pDX, IDC_nn11, m_nn11);
	DDX_Control(pDX, IDC_nn10, m_nn10);
	DDX_Control(pDX, IDC_nn1, m_nn1);
	DDX_Control(pDX, IDC_nn0, m_nn0);
	DDX_Control(pDX, IDC_nm13, m_nm13);
	DDX_Control(pDX, IDC_nm12, m_nm12);
	DDX_Control(pDX, IDC_nm11, m_nm11);
	DDX_Control(pDX, IDC_nm10, m_nm10);
	DDX_Control(pDX, IDC_nm9, m_nm9);
	DDX_Control(pDX, IDC_nm8, m_nm8);
	DDX_Control(pDX, IDC_nm7, m_nm7);
	DDX_Control(pDX, IDC_nm6, m_nm6);
	DDX_Control(pDX, IDC_nm5, m_nm5);
	DDX_Control(pDX, IDC_nm4, m_nm4);
	DDX_Control(pDX, IDC_nm3, m_nm3);
	DDX_Control(pDX, IDC_nm2, m_nm2);
	DDX_Control(pDX, IDC_nm1, m_nm1);
	DDX_Control(pDX, IDC_nm0, m_nm0);
	DDX_Control(pDX, IDC_st13, m_st13);
	DDX_Control(pDX, IDC_st12, m_st12);
	DDX_Control(pDX, IDC_st11, m_st11);
	DDX_Control(pDX, IDC_st10, m_st10);
	DDX_Control(pDX, IDC_st9, m_st9);
	DDX_Control(pDX, IDC_st8, m_st8);
	DDX_Control(pDX, IDC_st7, m_st7);
	DDX_Control(pDX, IDC_st6, m_st6);
	DDX_Control(pDX, IDC_st5, m_st5);
	DDX_Control(pDX, IDC_st4, m_st4);
	DDX_Control(pDX, IDC_st3, m_st3);
	DDX_Control(pDX, IDC_st2, m_st2);
	DDX_Control(pDX, IDC_st1, m_st1);
	DDX_Control(pDX, IDC_st0, m_st0);
	DDX_Control(pDX, IDC_sk0, m_sk0);
	DDX_Control(pDX, IDC_sk1, m_sk1);
	DDX_Control(pDX, IDC_sk2, m_sk2);
	DDX_Control(pDX, IDC_sk3, m_sk3);
	DDX_Control(pDX, IDC_sk4, m_sk4);
	DDX_Control(pDX, IDC_sk5, m_sk5);
	DDX_Control(pDX, IDC_sk6, m_sk6);
	DDX_Control(pDX, IDC_sk7, m_sk7);
	DDX_Control(pDX, IDC_sk8, m_sk8);
	DDX_Control(pDX, IDC_sk9, m_sk9);
	DDX_Control(pDX, IDC_sk10, m_sk10);
	DDX_Control(pDX, IDC_sk11, m_sk11);
	DDX_Control(pDX, IDC_sk12, m_sk12);
	DDX_Control(pDX, IDC_sk13, m_sk13);
	DDX_Control(pDX, IDC_sl0, m_sl0);
	DDX_Control(pDX, IDC_sl1, m_sl1);
	DDX_Control(pDX, IDC_sl2, m_sl2);
	DDX_Control(pDX, IDC_sl3, m_sl3);
	DDX_Control(pDX, IDC_sl4, m_sl4);
	DDX_Control(pDX, IDC_sl5, m_sl5);
	DDX_Control(pDX, IDC_sl6, m_sl6);
	DDX_Control(pDX, IDC_sl7, m_sl7);
	DDX_Control(pDX, IDC_sl8, m_sl8);
	DDX_Control(pDX, IDC_sl9, m_sl9);
	DDX_Control(pDX, IDC_sl10, m_sl10);
	DDX_Control(pDX, IDC_sl11, m_sl11);
	DDX_Control(pDX, IDC_sl12, m_sl12);
	DDX_Control(pDX, IDC_sl13, m_sl13);
	DDX_Check(pDX, IDC_inphide, m_inphide);
	//}}AFX_DATA_MAP
}

//const UINT wm_IcnRest = RegisterWindowMessage( FINDMSGSTRING );
#define wm_CEasyDropTargetCallback (WM_USER + 2)
#define wm_Timer (WM_USER + 3)
#define wm_MirrorFinished (WM_USER + 4)
BEGIN_MESSAGE_MAP(Cinprogress, CPropertyPage)
	//{{AFX_MSG_MAP(Cinprogress)
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_sk0, Onsk0)
	ON_BN_CLICKED(IDC_sk1, Onsk1)
	ON_BN_CLICKED(IDC_sk2, Onsk2)
	ON_BN_CLICKED(IDC_sk3, Onsk3)
	ON_BN_CLICKED(IDC_sk4, Onsk4)
	ON_BN_CLICKED(IDC_sk5, Onsk5)
	ON_BN_CLICKED(IDC_sk6, Onsk6)
	ON_BN_CLICKED(IDC_sk7, Onsk7)
	ON_BN_CLICKED(IDC_sk8, Onsk8)
	ON_BN_CLICKED(IDC_sk9, Onsk9)
	ON_BN_CLICKED(IDCANCEL, OnEscape)
	ON_BN_CLICKED(IDC_ipabout, Onipabout)
	ON_WM_DESTROY()
	ON_WM_HELPINFO()
	ON_BN_CLICKED(IDC_sk10, Onsk10)
	ON_BN_CLICKED(IDC_sk11, Onsk11)
	ON_BN_CLICKED(IDC_sk12, Onsk12)
	ON_BN_CLICKED(IDC_sk13, Onsk13)
	ON_WM_CREATE()
	ON_BN_CLICKED(IDC_nm0, Onnm0)
	ON_BN_CLICKED(IDC_nm1, Onnm1)
	ON_BN_CLICKED(IDC_nm2, Onnm2)
	ON_BN_CLICKED(IDC_nm3, Onnm3)
	ON_BN_CLICKED(IDC_nm4, Onnm4)
	ON_BN_CLICKED(IDC_nm5, Onnm5)
	ON_BN_CLICKED(IDC_nm6, Onnm6)
	ON_BN_CLICKED(IDC_nm7, Onnm7)
	ON_BN_CLICKED(IDC_nm8, Onnm8)
	ON_BN_CLICKED(IDC_nm9, Onnm9)
	ON_BN_CLICKED(IDC_nm10, Onnm10)
	ON_BN_CLICKED(IDC_nm11, Onnm11)
	ON_BN_CLICKED(IDC_nm12, Onnm12)
	ON_BN_CLICKED(IDC_nm13, Onnm13)
	ON_BN_CLICKED(IDC_nn0, Onnm0)
	ON_BN_CLICKED(IDC_nn1, Onnm1)
	ON_BN_CLICKED(IDC_nn2, Onnm2)
	ON_BN_CLICKED(IDC_nn3, Onnm3)
	ON_BN_CLICKED(IDC_nn4, Onnm4)
	ON_BN_CLICKED(IDC_nn5, Onnm5)
	ON_BN_CLICKED(IDC_nn6, Onnm6)
	ON_BN_CLICKED(IDC_nn7, Onnm7)
	ON_BN_CLICKED(IDC_nn8, Onnm8)
	ON_BN_CLICKED(IDC_nn9, Onnm9)
	ON_BN_CLICKED(IDC_nn10, Onnm10)
	ON_BN_CLICKED(IDC_nn11, Onnm11)
	ON_BN_CLICKED(IDC_nn12, Onnm12)
	ON_BN_CLICKED(IDC_nn13, Onnm13)
  ON_BN_CLICKED(IDC_st0, Onst0)
	ON_BN_CLICKED(IDC_st1, Onst1)
	ON_BN_CLICKED(IDC_st2, Onst2)
	ON_BN_CLICKED(IDC_st3, Onst3)
	ON_BN_CLICKED(IDC_st4, Onst4)
	ON_BN_CLICKED(IDC_st5, Onst5)
	ON_BN_CLICKED(IDC_st6, Onst6)
	ON_BN_CLICKED(IDC_st7, Onst7)
	ON_BN_CLICKED(IDC_st8, Onst8)
	ON_BN_CLICKED(IDC_st9, Onst9)
	ON_BN_CLICKED(IDC_st10, Onst10)
	ON_BN_CLICKED(IDC_st11, Onst11)
	ON_BN_CLICKED(IDC_st12, Onst12)
	ON_BN_CLICKED(IDC_st13, Onst13)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_inphide, Oninphide)
	//}}AFX_MSG_MAP
  ON_MESSAGE( wm_CEasyDropTargetCallback, DragDropText)
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
  ON_BN_CLICKED(ID_ABOUT,Onipabout)
  ON_BN_CLICKED(ID_FILE_EXIT,OnStopall)
  ON_BN_CLICKED(ID_LOG_VIEWLOG,OniplogLog)
  ON_BN_CLICKED(ID_LOG_VIEWERRORLOG,OniplogErr)
  ON_BN_CLICKED(ID_LOG_VIEWTRANSFERS,OnViewTransfers)
  ON_BN_CLICKED(ID_FILE_PAUSE,OnPause)
  ON_BN_CLICKED(ID_OPTIONS_MODIFY,OnModifyOpt)
  ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
  ON_COMMAND(ID_HELP,OnHelpInfo2)
	ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
  // Fin du miroir
  ON_MESSAGE( wm_MirrorFinished, OnEndMirror)
  END_MESSAGE_MAP()
//ON_REGISTERED_MESSAGE( wm_IcnRest, IconRestore )
/////////////////////////////////////////////////////////////////////////////
// Cinprogress message handlers

void Cinprogress::OnClose() 
{
  OnStopall();
}

void Cinprogress::Onsk0()  {
  if (hts_is_parsing(global_opt, -1)) {  // parsing
    if (hts_is_testing(global_opt))
      hts_cancel_test(global_opt);     // cancel test
    /*else*/
    hts_cancel_parsing(global_opt);  // cancel parsing
  } else
    StatsBuffer_cancel(0);
}
void Cinprogress::Onsk1()  {
  StatsBuffer_cancel(1);
}
void Cinprogress::Onsk2()  {
  StatsBuffer_cancel(2);
}
void Cinprogress::Onsk3()  {
  StatsBuffer_cancel(3);
}
void Cinprogress::Onsk4()  {
  StatsBuffer_cancel(4);
}
void Cinprogress::Onsk5()  {
  StatsBuffer_cancel(5);
}
void Cinprogress::Onsk6()  {
  StatsBuffer_cancel(6);
}
void Cinprogress::Onsk7()  {
  StatsBuffer_cancel(7);
}
void Cinprogress::Onsk8()  {
  StatsBuffer_cancel(8);
}
void Cinprogress::Onsk9()  {
  StatsBuffer_cancel(9);
}
void Cinprogress::Onsk10()  {
  StatsBuffer_cancel(10);
}
void Cinprogress::Onsk11()  {
  StatsBuffer_cancel(11);
}
void Cinprogress::Onsk12()  {
  StatsBuffer_cancel(12);
}
void Cinprogress::Onsk13()  {
  StatsBuffer_cancel(13);
}


// Capture des static
void Cinprogress::Onnm0()  { StatsBuffer_info(0); }
void Cinprogress::Onnm1()  { StatsBuffer_info(1); }
void Cinprogress::Onnm2()  { StatsBuffer_info(2); }
void Cinprogress::Onnm3()  { StatsBuffer_info(3); }
void Cinprogress::Onnm4()  { StatsBuffer_info(4); }
void Cinprogress::Onnm5()  { StatsBuffer_info(5); }
void Cinprogress::Onnm6()  { StatsBuffer_info(6); }
void Cinprogress::Onnm7()  { StatsBuffer_info(7); }
void Cinprogress::Onnm8()  { StatsBuffer_info(8); }
void Cinprogress::Onnm9()  { StatsBuffer_info(9); }
void Cinprogress::Onnm10() { StatsBuffer_info(10); }
void Cinprogress::Onnm11() { StatsBuffer_info(11); }
void Cinprogress::Onnm12() { StatsBuffer_info(12); }
void Cinprogress::Onnm13() { StatsBuffer_info(13); }

void Cinprogress::Onst0()  { StatsBuffer_info(0); }
void Cinprogress::Onst1()  { StatsBuffer_info(1); }
void Cinprogress::Onst2()  { StatsBuffer_info(2); }
void Cinprogress::Onst3()  { StatsBuffer_info(3); }
void Cinprogress::Onst4()  { StatsBuffer_info(4); }
void Cinprogress::Onst5()  { StatsBuffer_info(5); }
void Cinprogress::Onst6()  { StatsBuffer_info(6); }
void Cinprogress::Onst7()  { StatsBuffer_info(7); }
void Cinprogress::Onst8()  { StatsBuffer_info(8); }
void Cinprogress::Onst9()  { StatsBuffer_info(9); }
void Cinprogress::Onst10() { StatsBuffer_info(10); }
void Cinprogress::Onst11() { StatsBuffer_info(11); }
void Cinprogress::Onst12() { StatsBuffer_info(12); }
void Cinprogress::Onst13() { StatsBuffer_info(13); }

// touche escape
void Cinprogress::OnEscape() { 
  OnStopall();
}

void Cinprogress::OnStopall() 
{
  this_CSplitterFrame->CheckRestore();
  if (AfxMessageBox(
    LANG(LANG_H1 /*"Stop WinHTTrack?",
           "Stopper WinHTTrack?"*/)
    ,MB_OKCANCEL+MB_ICONQUESTION)==IDOK) {
    hts_setpause(global_opt, 0);
    if (soft_term_requested)
      termine_requested=1;
    else {
      soft_term_requested=1;
      hts_request_stop(global_opt, 0);
    }
  }
}

void Cinprogress::Onipabout() 
{
  Cabout about;
  about.DoModal();
}

void Cinprogress::OnDestroy() 
{
  //((CWnd*)this)->m_pCtrlCont->OnUIActivate(NULL);
  WHTT_LOCK();
  termine_requested=1;    // quit!
  WHTT_UNLOCK();
  StopTimer();
  if (BackAffLog)
  if (form.m_hWnd)
    form.EndDialog(IDOK);       // terminer!
  this_CSplitterFrame->CheckRestore();
  //Sleep(150);             // évite les problèmes d'accès à m_hWnd juste avant le destroy
  CPropertyPage::OnDestroy();
}

BOOL Cinprogress::OnInitDialog() 
{
  m_inphide=TRUE;
  UpdateData(false);      // force to call DoDataExchange
  inprogress=this;

  memset(&SInfo, 0, sizeof(SInfo));

  BackAffLog=NULL;
  strcpybuff(pathlog,"");

	//CPropertyPage::OnInitDialog();
  EnableToolTips(true);     // TOOL TIPS

  // initialisation des champs pour les redraws en boucle
  element[0][0]=&m_st0;
  element[0][1]=&m_st1;
  element[0][2]=&m_st2;
  element[0][3]=&m_st3;
  element[0][4]=&m_st4;
  element[0][5]=&m_st5;
  element[0][6]=&m_st6;
  element[0][7]=&m_st7;
  element[0][8]=&m_st8;
  element[0][9]=&m_st9;
  element[0][10]=&m_st10;
  element[0][11]=&m_st11;
  element[0][12]=&m_st12;
  element[0][13]=&m_st13;

  element[1][0]=&m_nm0;
  element[1][1]=&m_nm1;
  element[1][2]=&m_nm2;
  element[1][3]=&m_nm3;
  element[1][4]=&m_nm4;
  element[1][5]=&m_nm5;
  element[1][6]=&m_nm6;
  element[1][7]=&m_nm7;
  element[1][8]=&m_nm8;
  element[1][9]=&m_nm9;
  element[1][10]=&m_nm10;
  element[1][11]=&m_nm11;
  element[1][12]=&m_nm12;
  element[1][13]=&m_nm13;

  // rajouté
  element[4][0]=&m_nn0;
  element[4][1]=&m_nn1;
  element[4][2]=&m_nn2;
  element[4][3]=&m_nn3;
  element[4][4]=&m_nn4;
  element[4][5]=&m_nn5;
  element[4][6]=&m_nn6;
  element[4][7]=&m_nn7;
  element[4][8]=&m_nn8;
  element[4][9]=&m_nn9;
  element[4][10]=&m_nn10;
  element[4][11]=&m_nn11;
  element[4][12]=&m_nn12;
  element[4][13]=&m_nn13;

  element[2][0]=&m_sl0;
  element[2][1]=&m_sl1;
  element[2][2]=&m_sl2;
  element[2][3]=&m_sl3;
  element[2][4]=&m_sl4;
  element[2][5]=&m_sl5;
  element[2][6]=&m_sl6;
  element[2][7]=&m_sl7;
  element[2][8]=&m_sl8;
  element[2][9]=&m_sl9;
  element[2][10]=&m_sl10;
  element[2][11]=&m_sl11;
  element[2][12]=&m_sl12;
  element[2][13]=&m_sl13;

  element[3][0]=&m_sk0;
  element[3][1]=&m_sk1;
  element[3][2]=&m_sk2;
  element[3][3]=&m_sk3;
  element[3][4]=&m_sk4;
  element[3][5]=&m_sk5;
  element[3][6]=&m_sk6;
  element[3][7]=&m_sk7;
  element[3][8]=&m_sk8;
  element[3][9]=&m_sk9;
  element[3][10]=&m_sk10;
  element[3][11]=&m_sk11;
  element[3][12]=&m_sk12;
  element[3][13]=&m_sk13;

  /* Init checkbox */
  CMenu* m;
  if (m = AfxGetApp()->GetMainWnd()->GetMenu()) {
    m->CheckMenuItem(ID_FILE_PAUSE,MF_UNCHECKED);
  }

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    //SetDlgItemTextCP(this, ,"");
    SetDlgItemTextCP(this, IDC_STATIC_bytes,LANG(LANG_H8) /*"Octets sauvés:"*/);
    SetDlgItemTextCP(this, IDC_STATIC_scanned,LANG(LANG_H9) /*"Liens parcourus:"*/);
    SetDlgItemTextCP(this, IDC_STATIC_time,LANG(LANG_H10) /*"Temps:"*/);
    SetDlgItemTextCP(this, IDC_STATIC_sockets,LANG(LANG_H11) /*"Connexions:"*/);
    SetDlgItemTextCP(this, IDC_inphide,LANG(LANG_H12) /*"En cours:"*/);
    SetDlgItemTextCP(this, IDC_STATIC_informations,LANG_H16);   
    SetDlgItemTextCP(this, IDC_STATIC_written,LANG_H17);   
    SetDlgItemTextCP(this, IDC_STATIC_updated,LANG_H18);   
    SetDlgItemTextCP(this, IDC_STATIC_errors,LANG_H19);    
    SetDlgItemTextCP(this, IDC_STATIC_inprog,LANG_H20);    
    //SetDlgItemTextCP(this, IDC_hide,LANG(LANG_H13) /*"Cacher"*/);
    SetDlgItemTextCP(this, IDC_STATIC_trate,LANG(LANG_H14) /*"Taux transfert"*/);
    for(int i=0;i<NStatsBuffer;i++)
      SetWindowTextCP(element[3][i], LANG(LANG_H15));
  }

  /*
  {
    LOGFONT lf;
    if (element[0][0]->GetFont()->GetLogFont(&lf)) {
      CFont* fnt = new CFont();
      if (fnt->CreateFont(lf.lfHeight,0,0,0,FW_DONTCARE,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Fixedsys")) {
        int i;
        for (i=0;i<NStatsBuffer; i++) {
          element[0][i]->SetFont(fnt,true);
          element[1][i]->SetFont(fnt,true);
        }
      }
    }
  }
  */

  SetIcon(httrack_icon,false);
  SetIcon(httrack_icon,true);  

  if (ShellOptions != NULL && ShellOptions->choixdeb[0] == '!')
    Oniplog(0);           // ouvrir log

  // Lancer timer!
  StartTimer();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void Cinprogress::StartTimer() {
  if (!timer) {
    timer=SetTimer(WM_TIMER,HTS_SLEEP_WIN,NULL);
  }
}
void Cinprogress::StopTimer() {
  if (timer) {
    KillTimer(timer);
    timer=0;
  }
}

UINT AffLog( LPVOID pP ) {
  Cinprogress* inp = (Cinprogress*) pP;
  if (inp != NULL) {
    inp->form.m_iplog=(CString) "No log report";
    inp->form.DoModal();
    inp->BackAffLog = NULL;
  }
  return 0;    // thread completed successfully
}

// log pour fichiers d'erreur et de log
void Cinprogress::OniplogLog() {
  Oniplog(0);
}
void Cinprogress::OniplogErr() {
  Oniplog(1);
}

void Cinprogress::Oniplog(int mode)  {
	char catbuff[CATBUFF_SIZE];
  if (!BackAffLog) {  // pas encore lancé
    strcpybuff(pathlog,dialog0->GetPath());
    if (strlen(pathlog)>0)
    if ((pathlog[strlen(pathlog)-1]!='/') && (pathlog[strlen(pathlog)-1]!='\\'))
      strcatbuff(pathlog,"/");
    // fichier log existe ou on est télécommandé par un !
    if ( (fexist(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-err.txt")))
      || (fexist(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-log.txt")))
      || (ShellOptions != NULL && ShellOptions->choixdeb[0]=='!') ) {
      if (mode)
        form.type_log=0;
      else
        form.type_log=1;
      strcpybuff(form.pathlog,pathlog);
      BackAffLog = AfxBeginThread(AffLog,this);
    } else {
      char s[1024];
      sprintf(s,LANG(LANG_H2 /*"No log files in %s!","Aucun fichier d'audit dans %s!"*/),pathlog);
      AfxMessageBox(s,MB_OK+MB_ICONEXCLAMATION);
    }
  }
}

void Cinprogress::OnViewTransfers() 
{
  StatsBuffer_info(0);
}

void Cinprogress::OnPause() 
{
  //int m;
  //if (!hts_setpause(-1))
  //  m=AfxMessageBox(LANG(LANG_H3 /*"Pause Transfer?","Placer le transfert sur pause?"*/),MB_OKCANCEL+MB_ICONQUESTION);
  //else
  //  m=IDOK;
  //if (m==IDOK) {
	hts_setpause(global_opt, !hts_setpause(global_opt, -1));
	CMenu* m;
	if (m = AfxGetApp()->GetMainWnd()->GetMenu()) {
		if (hts_setpause(global_opt, -1))
			m->CheckMenuItem(ID_FILE_PAUSE,MF_CHECKED);
		else
			m->CheckMenuItem(ID_FILE_PAUSE,MF_UNCHECKED);
	}
  //}
}

// modif options
void Cinprogress::OnModifyOpt() 
{
  //
  maintab->m_option1.modify=
    maintab->m_option2.modify=
    maintab->m_option3.modify=
    maintab->m_option7.modify=
    maintab->m_option8.modify=
    maintab->m_option9.modify=
    maintab->m_option11.modify=
    maintab->m_option10.modify=1;    // mode modification
  if (maintab->DoModal() == IDOK) {
    int n;
    LLint ln;
    float nf;

    httrackp *const opt = hts_create_opt();
    assert(opt->size_httrackp == sizeof(httrackp));

    opt->log = opt->errlog = NULL;

    // dévalider champs (non modifiés)
    opt->maxsite = -1;
    opt->maxfile_nonhtml = -1;
    opt->maxfile_html = -1;
    opt->maxsoc = -1;
    opt->nearlink = -1;
    opt->timeout = -1;
    opt->rateout = -1;
    opt->maxtime = -1;
		//opt->mms_maxtime = -1;
    opt->maxrate = -1;
    StringClear(opt->user_agent);
    opt->retry = -1;
    opt->hostcontrol = -1;
    opt->errpage = -1;
    opt->travel = -1;
    opt->external = -1;
    opt->delete_old=-1;
    opt->parseall=-1;
    opt->delete_old=-1;

    opt->travel=0;         // NOTE: NE SERA PRIS EN COMPTE QUE LE BIT 8
    if(maintab->m_option1.m_testall)
      opt->travel|=256;     

    if(maintab->m_option1.m_parseall)
      opt->parseall=1;     
    else
      opt->parseall=0;     

    // near link,err page
    if (maintab->m_option1.m_link)
      opt->nearlink=1;
    else
      opt->nearlink=0;

    if (maintab->m_option2.m_errpage)
      opt->errpage=1;
    else
      opt->errpage=0;

    if (maintab->m_option2.m_external)
      opt->external=1;
    else
      opt->external=0;

    if (maintab->m_option2.m_nopurge)
      opt->delete_old=1;
    else
      opt->delete_old=0;


    // host control
    {
      int a=0;
      if (maintab->m_option4.m_remt)
        a+=1;
      if (maintab->m_option4.m_rems)
        a+=2;
      opt->hostcontrol=a;
    }

    // sockets
    if (strcmp(maintab->m_option4.m_connexion,"")!=0) {
      if (sscanf(maintab->m_option4.m_connexion,"%d",&n) == 1)
        opt->maxsoc = n;
    } 

    // maxfile_nonhtml
    if (strcmp(maintab->m_option5.m_othermax,"")!=0) {
      if (sscanf(maintab->m_option5.m_othermax,LLintP,&ln) == 1)
        opt->maxfile_nonhtml = ln;
    } else
      opt->maxfile_nonhtml = -1;

    // maxfile_html
    if (strcmp(maintab->m_option5.m_maxhtml,"")!=0) {
      if (sscanf(maintab->m_option5.m_maxhtml,LLintP,&ln) == 1)
        opt->maxfile_html = ln;
    } else
      opt->maxfile_html = -1;

    // maxsite
    if (strcmp(maintab->m_option5.m_sizemax,"")!=0) {
      if (sscanf(maintab->m_option5.m_sizemax,LLintP,&ln) == 1)
        opt->maxsite = ln;
    } else
      opt->maxsite = -1;

    // fragment
    if (strcmp(maintab->m_option5.m_pausebytes,"")!=0) {
      if (sscanf(maintab->m_option5.m_pausebytes,LLintP,&ln) == 1)
        opt->fragment = ln;
    } else
      opt->fragment = -1;

    // timeout
    if (strcmp(maintab->m_option4.m_timeout,"")!=0) {
      if (sscanf(maintab->m_option4.m_timeout,"%d",&n) == 1)
        opt->timeout = n;
    } else
      opt->timeout = -1;

    // rateout
    if (strcmp(maintab->m_option4.m_rate,"")!=0) {
      if (sscanf(maintab->m_option4.m_rate,"%d",&n) == 1)
        opt->rateout = n;
    } else
      opt->rateout = -1;

    // maxtime
    if (strcmp(maintab->m_option5.m_maxtime,"")!=0) {
      if (sscanf(maintab->m_option5.m_maxtime,"%d",&n) == 1)
        opt->maxtime = n;
    } else
      opt->maxtime = -1;

    // maxrate
    if (strcmp(maintab->m_option5.m_maxrate,"")!=0) {
      if (sscanf(maintab->m_option5.m_maxrate,"%d",&n) == 1)
        opt->maxrate = n;
    } else
      opt->maxrate = -1;

    // max. connect
    if (strcmp(maintab->m_option5.m_maxconn,"")!=0) {
      if (sscanf(maintab->m_option5.m_maxconn,"%f",&nf) == 1)
        opt->maxconn = nf;
    } else
      opt->maxconn = -1;

    // retry
    if (strcmp(maintab->m_option4.m_retry,"")!=0) {
      if (sscanf(maintab->m_option4.m_retry,"%d",&n) == 1)
        opt->retry = n;
    } else
      opt->retry = -1;

    // user_agent
    if (strcmp(maintab->m_option6.m_user,"")!=0) {
      StringCopy(opt->user_agent, maintab->m_option6.m_user);
    }

		if (global_opt != NULL) {
			copy_htsopt(opt, global_opt);
		}

    hts_free_opt(opt);
  }
  maintab->m_option1.modify=
    maintab->m_option2.modify=
    maintab->m_option3.modify=
    maintab->m_option7.modify=
    maintab->m_option8.modify=
    maintab->m_option9.modify=
    maintab->m_option11.modify=
    maintab->m_option10.modify=0;
}



// canceller un lien manuellement
void Cinprogress::StatsBuffer_cancel(int id) {
  hts_cancel_file_push(global_opt, StatsBuffer[id].url_sav);
}
void Cinprogress::StatsBuffer_info(int id) {
  WHTT_LOCK();
  if ((!termine) && (!termine_requested) && (inprogress->m_hWnd)) {
    if (SInfo.stat_time>0) {
      if (StatsBuffer[id].etat[0]) {
        CInfoUrl box;
        box.id=StatsBuffer[id].back;
        _Cinprogress_inst=&box;
        WHTT_UNLOCK();
        {
          box.DoModal();
          _Cinprogress_inst=NULL;
        }
        return;
      }
    }
  }
  WHTT_UNLOCK();
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
BOOL Cinprogress::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
char* Cinprogress::GetTip(int ID)
{
  switch(ID) {
    //case IDC_STOPALL: return LANG(LANG_H4); break; // "Stop the mirror","Stopper le miroir"); break;
    //case IDC_hide:    return LANG(LANG_H5); break; // "Hide this window behind the system tray","Cacher la fenêtre dans la barre système"); break;
    case IDC_sk0:     return LANG(LANG_H6); break; // "Click to skip a link or interrupt parsing","Clic pour sauter un lien ou interrompre"); break;
    case IDC_sk1: case IDC_sk2: case IDC_sk3: case IDC_sk4:
    case IDC_sk5: case IDC_sk6: case IDC_sk7: case IDC_sk8: case IDC_sk9:
                      return LANG(LANG_H7); break; // "Click to skip a link","Clic pour sauter un lien"); break;
                      //
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------



// Appel aide
void Cinprogress::OnHelpInfo2() {
  (void)OnHelpInfo(NULL);
}

BOOL Cinprogress::OnHelpInfo(HELPINFO* dummy) 
{
  //return CPropertyPage::OnHelpInfo(pHelpInfo);
  HtsHelper->Help();
  return true;
  //AfxGetApp()->WinHelp(0,HELP_FINDER);    // Index du fichier Hlp
  //return true;
}


int Cinprogress::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CPropertyPage::OnCreate(lpCreateStruct) == -1)
		return -1;

  // Drag&Drop
  drag=new CEasyDropTarget(this);
  if (drag->IsRegistered()) {
    drag->SetTextCallback(wm_CEasyDropTargetCallback);
  }
	
	return 0;
}


// Message from CEasyDropTarget
LRESULT Cinprogress::DragDropText(WPARAM wParam,LPARAM lParam) {
  if (lParam) {
    CString st=*((CString*) lParam);
    CLIPFORMAT cfFormat = (CLIPFORMAT) wParam;
    st=Wid1::TextToUrl(st,cfFormat);
    if (st=="")
      AfxMessageBox(LANG(LANG_DIAL11),MB_SYSTEMMODAL);
    else {
      CString aff=LANG(LANG_DIAL12);
      if (AfxMessageBox(aff+st,MB_SYSTEMMODAL|MB_YESNO|MB_ICONQUESTION)==IDYES) {
        int pause=0;
        char** tab=CEasyDropTarget::StringToArray(st);
        hts_addurl(global_opt, tab);
        if (pause=hts_setpause(global_opt, -1))
          hts_setpause(global_opt, 0);      // enlever pause
        {
          int i=0;
          while((hts_addurl(global_opt, NULL)) && (i<100)) {
            Sleep(100);
            i++;
          }
          if (!(i<100)) {
            hts_resetaddurl(global_opt);
            AfxMessageBox(LANG(LANG_DIAL13));
          }
          hts_setpause(global_opt, pause);      // remettre pause éventuelle
        }
        CEasyDropTarget::ReleaseStringToArray(tab);
        tab=NULL;
      }
    }
  }
  return 0;
}

// Refresh
void Cinprogress::OnTimer(UINT_PTR nIDEvent) 
{
  WHTT_LOCK();
  if (!termine) {
    if (SInfo.refresh) {
      hts_is_parsing(global_opt, 0);        // refresh demandé si en mode parsing
      // while(INFILLMEM_LOCKED) Sleep(10);    // attendre au cas où
      if (!termine)
        inprogress_refresh();        // on refresh!
    }
    SInfo.ask_refresh=1;
  }
  WHTT_UNLOCK();
  CPropertyPage::OnTimer(nIDEvent);
}

BOOL Cinprogress::DestroyWindow() 
{
  StopTimer();
  inprogress=NULL;
  delete drag;
	return CPropertyPage::DestroyWindow();
}

// Fin

LRESULT Cinprogress::OnEndMirror(WPARAM /* wP*/, LPARAM /*lP*/) {
	char catbuff[CATBUFF_SIZE];
	char catbuff2[CATBUFF_SIZE];
  //this_CSplitterFrame->SetNewView(0,1,RUNTIME_CLASS(Cinfoend));

  // Copie de trans.cpp
  this_CWizTab->ModifyStyle(WS_VISIBLE,0,0);
  this_CWizTab->RedrawWindow();
  //
  //{
  //  CWizTab* tmp;
  //  tmp=this_CWizTab;
  this_CWizTab=this_intCWizTab;
  //  this_CWizTab2=tmp;
  //}
  //
  this_CWizTab->ModifyStyle(0,WS_VISIBLE,0);
  this_CWizTab->ModifyStyle(WS_DISABLED,0,0);
  this_CSplitterFrame->RedrawWindow();
  // Fin de Copie de trans.cpp

  this_CWizTab->EndInProgress();
  if (IsWindow(this_Cinfoend->m_hWnd))
    SetDlgItemTextCP(this_Cinfoend, IDC_infoend,end_mirror_msg);

  if (hts_is_exiting(global_opt) == 1) {     /* Interrupted mirror! */
    char pathlog[HTS_URLMAXSIZE*2];
    strcpybuff(pathlog,dialog0->GetPath());
    if (strlen(pathlog)>0) {
      if ((pathlog[strlen(pathlog)-1]!='/') && (pathlog[strlen(pathlog)-1]!='\\'))
        strcatbuff(pathlog,"/");
    }
    // Aborted updated.. restore old cache?!
    if ( 
      fexist(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-cache/old.zip"))
      ||
      (fexist(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-cache/old.dat")))
      && (fexist(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-cache/old.ndx"))) 
      ) {
      if (AfxMessageBox(LANG_F22b,MB_YESNO|MB_DEFBUTTON2) == IDYES) {
        if (fexist(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-cache/old.dat"))
          && fexist(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-cache/old.ndx"))) {
          if (remove(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-cache/new.dat"))) {
            AfxMessageBox(LANG_F24 );
          }
          if (remove(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-cache/new.ndx"))) {
            AfxMessageBox(LANG_F24 );
          }
        }
        if (remove(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-cache/new.lst"))) {
          AfxMessageBox(LANG_F24 );
        }
        if (fexist(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-cache/old.zip"))) {
          if (remove(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-cache/new.zip"))) {
            AfxMessageBox(LANG_F24 );
          }
        }
        remove(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-cache/new.txt"));
        rename(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-cache/old.zip"),fconcat(catbuff2,sizeof(catbuff2),pathlog,"hts-cache/new.zip"));
        rename(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-cache/old.dat"),fconcat(catbuff2,sizeof(catbuff2),pathlog,"hts-cache/new.dat"));
        rename(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-cache/old.ndx"),fconcat(catbuff2,sizeof(catbuff2),pathlog,"hts-cache/new.ndx"));
        rename(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-cache/old.lst"),fconcat(catbuff2,sizeof(catbuff2),pathlog,"hts-cache/new.lst"));
        rename(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-cache/old.txt"),fconcat(catbuff2,sizeof(catbuff2),pathlog,"hts-cache/new.txt"));
      }
    }
  } else if (hts_is_exiting(global_opt) == 2) {     /* No connection! */
    AfxMessageBox(LANG_F22c );
  }
  return S_OK;
}

BOOL Cinprogress::OnQueryCancel( ) {
  OnStopall();
  //this_CSplitterFrame->SetNewView(0,1,RUNTIME_CLASS(CDialogContainer));
  return FALSE;
}


void Cinprogress::Oninphide() 
{
  int status=IsDlgButtonChecked(IDC_inphide);
  if (status) {
    GetDlgItem(IDC_STATIC_actions)->ModifyStyle(WS_DISABLED,0);  // disabled
    GetDlgItem(IDC_STATIC_actions)->ModifyStyle(0,WS_VISIBLE);   // not visible
  } else {
    GetDlgItem(IDC_STATIC_actions)->ModifyStyle(0,WS_DISABLED);  // not disabled
    GetDlgItem(IDC_STATIC_actions)->ModifyStyle(WS_VISIBLE,0);   // visible
  }
  int i;
  for(i=0;i<NStatsBuffer;i++) {
    int j;
    for(j=0;j<5;j++) {
      if (status) {
        inprogress->element[j][i]->ModifyStyle(WS_DISABLED,0);  // disabled
        inprogress->element[j][i]->ModifyStyle(0,WS_VISIBLE);   // not visible
      } else {
        inprogress->element[j][i]->ModifyStyle(0,WS_DISABLED);  // not disabled
        inprogress->element[j][i]->ModifyStyle(WS_VISIBLE,0);   // visible
      }
    }
  }
  RedrawWindow();
}

BOOL Cinprogress::OnSetActive() 
{
  WHTT_LOCATION("inprogress");
	return CPropertyPage::OnSetActive();
}
