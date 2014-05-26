// Shell.cpp : Defines the class behaviors for the application.
//

// thread windows
#include <process.h>

#include "stdafx.h"
#include "Shell.h"
#include "NewProj.h"

#include <WS2tcpip.h>  // Note: weird C2894 error if not included here
extern "C" {
  #include "HTTrackInterface.h"
};
//#include "htsbase.h"

// Ras
#if USE_RAS 
#include "RasLoad.h"
#endif

#include <afxdisp.h>
#include "ras.h"
#include "mmsystem.h"

//#include "ShellDoc.h"
//#include "ShellView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// PATCH-->
// PATCH-->
#include "about.h"
#include "infoend.h"

// flag de termination
int termine=0;
int termine_requested=0;
int shell_terminated=0;
int soft_term_requested=0;
FILE* fp_debug=NULL;

#include "stdafx.h"
#include "Shell.h"
#include "process.h"
//#include "ShellDoc.h"
//#include "ShellView.h"
//#include "essai.h"
//#include "get.h"
//#include "parameter.h"
extern "C" {
  #include "HTTrackInterface.h"
}
#include "Wid1.h"
#include "trans.h"
#include "InfoUrl.h"
//#include "option.h"
//#include "filter.h"
//#include "wizard.h"
//
#include "maintab.h"
//
#include "MemRegister.h"

// LANG
#include "newlang.h"


// PATCH-->
//#include "wizard2.h"
//#include "WizLinks.h"

#include "inprogress.h"

#include "SYS\TIMEB.H"

// htswrap_add
extern "C" {
  #include "htswrap.h"
};

// --- --- --- --- Options --- --- --- ---

#define MAX_LEN_INPROGRESS 32

// lancement en multithread du shell ET de gethostbyname
#define SHELL_MULTITHREAD 1
//#define HTS_XGMETHOD 2    // 1: AfxBeginThread 2: _beginthread
// --- --- --- --- Options --- --- --- ---
//int INREDRAW_LOCKED=0;      // refresh graphique en cours
//int INFILLMEM_LOCKED=0;     // refresh mémoire en cours
int HTTRACK_result=0;
//
CInfoUrl* _Cinprogress_inst=NULL;

extern HICON httrack_icon;

/* Main splitter frame */
#include "DialogContainer.h"
#include "splitter.h"
extern CSplitterFrame* this_CSplitterFrame;

/* Main WizTab frame */
#include "WizTab.h"
extern CWizTab* this_CWizTab;
/* Argh - pas de domodal dans des autres threads ?!?! */
char WIZ_question[1000];
char WIZ_reponse[1000];

httrackp *global_opt = NULL;

// Fonctionnement des THREADS:
//
// principal ---> robot & refresh data (thread 1)
//           ---> refresh graphique    (thread 2)
//           GO!> boucle gestion domodal() et boutons
// arrêt: principal demande l'arrêt (termine_requested)
//        thread1 active termine et que thread2 ait fini de refresher
//        thread2 se termine
//        thread1 retourne 0 à hts_loop
//        le robot termine
//        le thread1 active termine, termine le formulaire et se termine
//        principal ayant quitté le formulaire affiche le message de fin


// htslib.c
extern "C" {
  HTSEXT_API void qsec2str(char *st,TStamp t);
}

// construction index général
// void Build_TopIndex();

void compute_options() ;
static void StripControls(char * chaine);
void lance(void);
int check_continue(char* pathlog);
int inprogress_refresh();
//int inprogress_refresh_scan();
void Write_profile(CString path,int load_path);
void Read_profile(CString path,int load_path);

extern "C" {
  #include "htscore.h"
}

CString _HTTRACK_VERSION = HTTRACK_VERSION;

CShellOptions* ShellOptions;


// Fichier tempo
FILE* tmpf=NULL;
MemRegister tmpm;

CNewProj* dialog0=NULL;
Wid1* dialog1=NULL;
Ctrans* dialog2=NULL;
//Coption dialog3;
//Cfilter diafiltre;
/*
wizard diawiz;
wizard2 diawiz2;
WizLinks diawiz3;
*/
//
CMainTab* maintab=NULL;
CShellApp* CShellApp_app=NULL;

#include "infoend.h"
extern Cinfoend* this_Cinfoend;


// PATCH-->
Cinprogress* inprogress=NULL;

// nbre de slides
t_StatsBuffer StatsBuffer[NStatsBuffer];
void* StatsBufferback=NULL;
int StatsBufferback_max=0;
InpInfo SInfo;

#if USE_RAS
// Chargement des librairies RAS
CDynamicRAS* LibRas=NULL;
int LibRasUse=0;
//
int connected=0;
int disconnect=0;
int shutdown_pc=0;
HRASCONN conn = NULL;
int has_started=0;
char connected_err[1000]="";
#endif

// pour message final
extern char end_mirror_msg[8192];
#include "winhttrack.h"
extern CWinHTTrackApp* this_app;

// PATCH-->
// PATCH-->
// FIN PATCH


/////////////////////////////////////////////////////////////////////////////
// CShellApp

/*
BEGIN_MESSAGE_MAP(CShellApp, CWinApp)
//{{AFX_MSG_MAP(CShellApp)
ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
// NOTE - the ClassWizard will add and remove mapping macros here.
//    DO NOT EDIT what you see in these blocks of generated code!
//}}AFX_MSG_MAP
// Standard file based document commands
ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
// Standard print setup command
ON_COMMAND(ID_FILE_PRINT_SETUP, CWinApp::OnFilePrintSetup)
ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()
*/

/////////////////////////////////////////////////////////////////////////////
// CShellApp construction

CShellApp::CShellApp()
{
  // TODO: add construction code here,
  // Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CShellApp object

//CShellApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CShellApp initialization

static void __cdecl RunBackMirror( LPVOID pP ) {
  CShellApp_app->init_lance();
}

BOOL LaunchMirror() {
  WHTT_LOCK();
  //hts_resetvar();
  WHTT_UNLOCK();
  hts_newthread(RunBackMirror, NULL);
  return 0;
}

#if 0
BOOL CShellApp::InitInstance()
{
  CShellApp_app=this;
  termine=termine_requested=shell_terminated=soft_term_requested=0;
  
  // Initialize OLE libraries
  /*
  if (!AfxOleInit())
  {
  AfxMessageBox(LANG(LANG_F1));
  return FALSE;
  }
  AfxEnableControlContainer();
  */
  
  // Fixer localisation dans la base de registre
  //SetRegistryKey("WinHTTrack");
  
  // Standard initialization
  // If you are not using these features and wish to reduce the size
  //  of your final executable, you should remove from the following
  //  the specific initialization routines you do not need.
  /*
  #ifdef _AFXDLL
  Enable3dControls();			// Call this when using MFC in a shared DLL
  #else
  Enable3dControlsStatic();	// Call this when linking to MFC statically
  #endif
  */
  
  // Register the application's document templates.  Document templates
  //  serve as the connection between documents, frame windows and views.
  
  // Change the registry key under which our settings are stored.
  // You should modify this string to be something appropriate
  // such as the name of your company or organization.
  //SetRegistryKey(_T("Local AppWizard-Generated Applications"));
  
  //LoadStdProfileSettings();  // Load standard INI file options (including MRU)
  
  // Register the application's document templates.  Document templates
  //  serve as the connection between documents, frame windows and views.
  
  // Parse command line for standard shell commands, DDE, file open
  /*CCommandLineInfo cmdInfo;
  ParseCommandLine(cmdInfo);
  */
  
  _Cinprogress_inst=NULL;
  
  LibRasUse=0;
  
  /*
  #if USE_RAS
  LibRas=new CDynamicRAS();
  if (LibRas->IsRASLoaded()) 
  LibRasUse=1;
  else
  LibRasUse=0;
  #endif
  */
  
  maintab = new CMainTab("WinHTTrack Website Copier");
  
  // PATCH-->
  // éxécution..
  init_lance();
  
  delete maintab;
  maintab=NULL;
  
  // PATCH-->
  /*
  // Dispatch commands specified on the command line
  if (!ProcessShellCommand(cmdInfo))
		return FALSE;
    
      // The one and only window has been initialized, so show and update it.
      m_pMainWnd->ShowWindow(SW_SHOW);
      m_pMainWnd->UpdateWindow();
  */  
  
  
  return TRUE;
}
#endif

// PATCH-->
// routines diverses

char* _SN(LLint n) {
  static char str[256];
  str[0]='\0';
  sprintf(str,LLintP,(LLint)n);        /* 64-bit */
  return str;
}

// t existe-t-il comme répertoire?
int dir_check(char* t) {
  int dir;
  FILE* fp=fopen(t,"rb");
  dir=(errno==13);  // is directory
  if (fp) fclose(fp);
  return dir;
}

void check_temp(char* t,char* s) {
  if (strlen(s)==0)
    if (dir_check(t))
      strcpybuff(s,t);
}



// PATCH-->
// Routines gestion dials
// Patché pour 100% dials


void CShellApp::init_lance() {
  hts_init();
  termine=termine_requested=shell_terminated=soft_term_requested=0;
  lance();             // Lancer miroir!
  if (fp_debug) {
    fprintf(fp_debug,"Building top index\r\n");
    fflush(fp_debug);
  }
  Build_TopIndex();
}

/* sauver profile */
/* ask: demande confirmation, si cela vaut le coup */
int Save_current_profile(int ask) {
  CString winprofile;
  if (ask) {
    if ((dialog0->GetName().IsEmpty()) && (dialog1->m_urls.IsEmpty()))
      return IDNO;
    int r;
    char msg[256];
    sprintf(msg,"%s?\r\n%s",LANG_SAVEPROJECT,dialog0->GetName());
    if ((r=AfxMessageBox(msg,MB_YESNOCANCEL)) != IDYES)
      return r;
  }
  //
  CWaitCursor wait;
  
  // sauver whtt
  {
    CString st=dialog0->GetBasePath()+dialog0->GetName()+".whtt";
    FILE* fp=fopen(st,"wb");
    if (fp) fclose(fp);
  }
  
  // sauver ini
  winprofile=dialog0->GetPath();
  if (winprofile.GetLength()>0)
    if ((winprofile.Right(1)!="/") && (winprofile.Right(1)!="\\"))
      winprofile+="\\";
    winprofile+="hts-cache\\";
    {
      char tempo[HTS_URLMAXSIZE*2];
      int i;
      strcpybuff(tempo,winprofile);
      for(i=0;i<(int)strlen(tempo);i++)
        if (tempo[i]=='\\')
          tempo[i]='/';              
        structcheck(tempo);
    }
    
    winprofile+="winprofile.ini";
    Write_profile(winprofile,0);
    
    // marquer document comme "sauvé"
    if (this_CSplitterFrame)
      this_CSplitterFrame->SetSaved();
    
    return IDYES;
}

// reprise possible?
int check_continue(char* pathlog) {
	char catbuff[CATBUFF_SIZE];
	char catbuff2[CATBUFF_SIZE];
	char catbuff3[CATBUFF_SIZE];
  char path_log[256];
  strcpybuff(path_log,pathlog);
  if (strlen(path_log)>0)
    if ((path_log[strlen(path_log)-1]!='/') && (path_log[strlen(path_log)-1]!='\\'))
      strcatbuff(path_log,"/");
    if (
      fexist(fconcat(catbuff,sizeof(catbuff),path_log,"hts-cache/new.zip"))
      ||
      (fexist(fconcat(catbuff2,sizeof(catbuff2),path_log,"hts-cache/new.dat")))
      && (fexist(fconcat(catbuff3,sizeof(catbuff3),path_log,"hts-cache/new.ndx")))
      ) {  // il existe déja un cache précédent.. renommer
      //if (fexist(fconcat(path_log,"hts-cache/doit.log"))) {    // un cache est présent
      return 1;
      //}
    }
    if (
      fexist(fconcat(catbuff,sizeof(catbuff),path_log,"hts-cache/old.zip"))
      ||
      (fexist(fconcat(catbuff2,sizeof(catbuff2),path_log,"hts-cache/old.dat")))
      && (fexist(fconcat(catbuff3,sizeof(catbuff3),path_log,"hts-cache/old.ndx")))
      ) {  // il existe déja un ancien cache précédent.. renommer
      return 1;
    }
    AfxMessageBox(LANG(LANG_F2 /*"There is no cache in the directory indicated\nWinHTTrack can not find any interrupted mirror!"*/),MB_OK+MB_ICONSTOP);
    return 0;
}


void CShellApp::OptPannel() {
  Write_profile("<mem>",0);       // enregistrer profile dans buffer local
  if (maintab->DoModal()==IDCANCEL) {
    Read_profile("<mem>",0);      // restaurer
  }
}


#define ADD_MIME_IN_COPT(A) do { \
  if(strlen(maintab->m_option11.m_ext##A)+strlen(maintab->m_option11.m_mime##A)) { \
  ShellOptions->buff_MIME += "--assume\n"; \
  ShellOptions->buff_MIME += maintab->m_option11.m_ext##A; \
  ShellOptions->buff_MIME += "="; \
  ShellOptions->buff_MIME += maintab->m_option11.m_mime##A; \
  ShellOptions->buff_MIME += "\n"; \
  } \
} while(0)
  
// parser maintab et calculer options
void compute_options() { 
  CWaitCursor wait;
  
  /* Effacer options */
  if (ShellOptions != NULL) {
    delete ShellOptions;
    ShellOptions = NULL;
  }
  ShellOptions = new CShellOptions();
  /* Effacer options */
  
  // Mode de miroir
  switch(dialog1->m_todo) {
  case CB_ERR: case 0:
    ShellOptions->choixdeb = "w";
    break;
  case 1:
    ShellOptions->choixdeb = "W";
    break;
  case 2:
    ShellOptions->choixdeb = "g";
    break;
  case 3:
    ShellOptions->choixdeb = "Y";
    break;
  case 4:
    ShellOptions->choixdeb = "!";
    break;
  default:
    if (dialog1->m_todo==dialog1->LAST_ACTION)
      ShellOptions->choixdeb = "i";    // reprise
    else
      ShellOptions->choixdeb = "/";    // reprise cache prioritaire
    break;
  }
  
  // URLS
  ShellOptions->url = dialog1->m_urls;
  StripControls(ShellOptions->url.GetBuffer(0));
  // --- formation du path
  ShellOptions->path = "\"";
  ShellOptions->path += dialog0->GetPath0();
  ShellOptions->path += "\"";
  //ShellOptions->path += ",";
  //ShellOptions->path += "\"";
  //ShellOptions->path += dialog0->GetPath0();
  //ShellOptions->path += "\"";
  
  // filelist
  {
    CString st=dialog1->m_filelist;
    st.TrimLeft();
    st.TrimRight();
    ShellOptions->filelist = st;
  }
  
  // stocker état et hh/mm/ss
  ShellOptions->hh = dialog2->m_hh;
  ShellOptions->mm = dialog2->m_mm;
  ShellOptions->ss = dialog2->m_ss;
  if (ShellOptions->hh.GetLength()) {    // heure
    int x,y,z;
    sscanf(ShellOptions->hh.GetBuffer(0),"%d",&x);
    x=min(max(x,0),23);
    ShellOptions->hh.Format("%d",x);
    //
    sscanf(ShellOptions->mm.GetBuffer(0),"%d",&y);
    y=min(max(y,0),59);
    ShellOptions->mm.Format("%d",y);
    //
    sscanf(ShellOptions->ss.GetBuffer(0),"%d",&z);
    z=min(max(z,0),59);
    ShellOptions->ss.Format("%d",z);
    //
    ShellOptions->waittime = "";
    {
      char str[32];
      ShellOptions->waittime = "#u";
      sprintf(str,"%d",x*3600+y*60+z);
      ShellOptions->waittime += str;
    }
  }
  ShellOptions->_RasString = dialog2->RasString;
  ShellOptions->_dial=dialog2->dial;
  
  if (ShellOptions->choixdeb[0]=='/') {
    ShellOptions->cache = "C1";      // cache prio
  } else {
    if(!maintab->m_option3.m_cache) 
      ShellOptions->cache = "C0"; 
    else 
      ShellOptions->cache = "C2";     // cache non prio 
    //ShellOptions->cache[0]='\0'; 
  }
  
  // ne pas recharger fichiers déja pris mais effacés
  if(maintab->m_option9.m_norecatch) ShellOptions->norecatch = "%n"; else ShellOptions->norecatch = "";
  
  // proxy
  ShellOptions->proxy = maintab->m_option10.m_proxy;
  ShellOptions->port = maintab->m_option10.m_port;
  if (maintab->m_option10.m_ftpprox) 
    ShellOptions->proxyftp = "%f";
  else
    ShellOptions->proxyftp = "%f0";   
  
  //depth
  ShellOptions->depth = maintab->m_option5.m_depth;
  ShellOptions->extdepth = maintab->m_option5.m_depth2;
  
  if(!maintab->m_option9.m_index) ShellOptions->index = "I0"; else ShellOptions->index = ""; 
  if(!maintab->m_option9.m_index2) ShellOptions->index2 = "%I0"; else ShellOptions->index2 = "%I"; 
  if(!maintab->m_option9.m_index_mail) ShellOptions->index_mail = ""; else ShellOptions->index_mail = "%M"; 
  if(maintab->m_option2.m_dos) 
    ShellOptions->dos = "L0"; 
  else if(maintab->m_option2.m_iso9660) 
    ShellOptions->dos = "L2"; 
  else 
    ShellOptions->dos = ""; 
  if(maintab->m_option1.m_testall) ShellOptions->testall = "t"; else ShellOptions->testall = ""; 
  if(maintab->m_option1.m_parseall) ShellOptions->parseall = "%P"; else ShellOptions->parseall = "%P0"; 
  if(maintab->m_option1.m_link) ShellOptions->link = "n"; else ShellOptions->link = ""; 
  if(maintab->m_option1.m_htmlfirst) ShellOptions->htmlfirst = "p7"; else ShellOptions->htmlfirst = ""; 
  if(maintab->m_option2.m_errpage) ShellOptions->errpage = "o0"; else ShellOptions->errpage = ""; 
  if(maintab->m_option2.m_external) ShellOptions->external = "x"; else ShellOptions->external = ""; 
  if(maintab->m_option2.m_nopurge) ShellOptions->nopurge = "X0"; else ShellOptions->nopurge = ""; 
  if(maintab->m_option2.m_hidepwd) ShellOptions->hidepwd = "%x"; else ShellOptions->hidepwd = ""; 
  if(maintab->m_option2.m_hidequery) ShellOptions->hidequery = "%q0"; else ShellOptions->hidequery = ""; 
  
  ShellOptions->robots = "";
  if(maintab->m_option8.m_robots==0) ShellOptions->robots = "s0"; 
  else if(maintab->m_option8.m_robots==1) ShellOptions->robots = "s1"; 
  else if(maintab->m_option8.m_robots==2) ShellOptions->robots = "s2"; 
  
  // cookies,checktype,parsejava
  if(maintab->m_option8.m_cookies==0) ShellOptions->cookies = "b0"; // else ShellOptions->cookies = "b1";
  if (maintab->m_option8.m_checktype>=0)
    ShellOptions->checktype.Format("u%d",maintab->m_option8.m_checktype);
  if(maintab->m_option8.m_parsejava==0) ShellOptions->parsejava = "j0"; // else ShellOptions->cookies = "j1";
  if (maintab->m_option8.m_http10) ShellOptions->http10 = "%h";   // HTTP/1.0 notamment
  if (maintab->m_option8.m_toler)  ShellOptions->toler = "%B";    // tolerent
  if (maintab->m_option8.m_updhack)  ShellOptions->updhack = "%s";    // update hack
  if (maintab->m_option8.m_urlhack)  ShellOptions->urlhack = "%u";    // URL hack
  else                               ShellOptions->urlhack = "%u0";
  
  // store all in cache,logtype
  if(maintab->m_option9.m_Cache2!=0) ShellOptions->Cache2 = "k";
  if(maintab->m_option9.m_logtype==1) ShellOptions->logtype = "z";
  else if(maintab->m_option9.m_logtype==2) ShellOptions->logtype = "Z";
  if (maintab->m_option3.m_windebug) ShellOptions->logtype += "%H";      // debug headers
  
  ShellOptions->build = "";
  if      (maintab->m_option2.m_build==0) ShellOptions->build = "N0";
  else if (maintab->m_option2.m_build==1) ShellOptions->build = "N1";
  else if (maintab->m_option2.m_build==2) ShellOptions->build = "N2";
  else if (maintab->m_option2.m_build==3) ShellOptions->build = "N3";
  else if (maintab->m_option2.m_build==4) ShellOptions->build = "N4";
  else if (maintab->m_option2.m_build==5) ShellOptions->build = "N5";
  else if (maintab->m_option2.m_build==6) ShellOptions->build = "N100";
  else if (maintab->m_option2.m_build==7) ShellOptions->build = "N101";
  else if (maintab->m_option2.m_build==8) ShellOptions->build = "N102";
  else if (maintab->m_option2.m_build==9) ShellOptions->build = "N103";
  else if (maintab->m_option2.m_build==10) ShellOptions->build = "N104";
  else if (maintab->m_option2.m_build==11) ShellOptions->build = "N105";
  else if (maintab->m_option2.m_build==12) ShellOptions->build = "N99";
  else if (maintab->m_option2.m_build==13) ShellOptions->build = "N199";
  else if (maintab->m_option2.m_build==14) {
    ShellOptions->build = "-N \"";
    ShellOptions->build += maintab->m_option2.Bopt.m_BuildString;
    ShellOptions->build += "\"";
  }
  
  ShellOptions->filtre = "";
  if      (maintab->m_option3.m_filter==0) ShellOptions->filtre = "p0";
  else if (maintab->m_option3.m_filter==1) ShellOptions->filtre = "p1";
  else if (maintab->m_option3.m_filter==2) ShellOptions->filtre = "p2";
  else if (maintab->m_option3.m_filter==3) {    /* default */
    if(!maintab->m_option1.m_htmlfirst) ShellOptions->filtre = "p3";
  }
  else if (maintab->m_option3.m_filter==4) ShellOptions->filtre = "p7";
  //
  if      (maintab->m_option3.m_travel==0) ShellOptions->filtre += "S";
  else if (maintab->m_option3.m_travel==1) ShellOptions->filtre += "D";
  else if (maintab->m_option3.m_travel==2) ShellOptions->filtre += "U";
  else if (maintab->m_option3.m_travel==3) ShellOptions->filtre += "B";
  //
  if      (maintab->m_option3.m_travel2==0) ShellOptions->filtre += "a";
  else if (maintab->m_option3.m_travel2==1) ShellOptions->filtre += "d";
  else if (maintab->m_option3.m_travel2==2) ShellOptions->filtre += "l";
  else if (maintab->m_option3.m_travel2==3) ShellOptions->filtre += "e";
  //
  if      (maintab->m_option3.m_travel3==0) ShellOptions->filtre += "K0";
  else if (maintab->m_option3.m_travel3==1) ShellOptions->filtre += "K";
  else if (maintab->m_option3.m_travel3==2) ShellOptions->filtre += "K3";
  else if (maintab->m_option3.m_travel3==3) ShellOptions->filtre += "K4";

  if (maintab->m_option9.m_logf) ShellOptions->log = "f2"; else ShellOptions->log = "Q"; 
  
  if(maintab->m_option5.m_sizemax!=""){
    ShellOptions->max = "M";
    ShellOptions->max += maintab->m_option5.m_sizemax;
  } else ShellOptions->max = "";
  
  if(maintab->m_option5.m_pausebytes!=""){
    ShellOptions->frag = "G";
    ShellOptions->frag += maintab->m_option5.m_pausebytes;
  } else ShellOptions->frag = "";
  
  
  if(maintab->m_option5.m_maxhtml!="" || maintab->m_option5.m_othermax!="" ){
    ShellOptions->maxfile = "m";
    if(maintab->m_option5.m_othermax!="") ShellOptions->maxfile += maintab->m_option5.m_othermax;
    else ShellOptions->maxfile += "0";
    if(maintab->m_option5.m_maxhtml!="") {ShellOptions->maxfile += ",";ShellOptions->maxfile += maintab->m_option5.m_maxhtml;}
    else {ShellOptions->maxfile += ",";ShellOptions->maxfile += "0";}
  } else ShellOptions->maxfile = "";
  
  if(strcmp(maintab->m_option4.m_connexion,"")!=0){
    ShellOptions->conn = "c";
    ShellOptions->conn += maintab->m_option4.m_connexion;
  } else ShellOptions->conn = "";
  
  if(strcmp(maintab->m_option4.m_timeout,"")!=0){
    ShellOptions->time = "T";
    ShellOptions->time += maintab->m_option4.m_timeout;
  } else ShellOptions->time = "";
  
  // quitter host si timeout ou rate out
  ShellOptions->hostquit = "";
  {
    int a=0;
    if (maintab->m_option4.m_remt)
      a+=1;
    if (maintab->m_option4.m_rems)
      a+=2;
    ShellOptions->hostquit.Format("H%d",a);
  }

  // Keep-Alive
  if (maintab->m_option4.m_ka) {
    ShellOptions->ka = "%k";
  } else {
    ShellOptions->ka = "%k0";
  }

  
  //--> max time
  if(strcmp(maintab->m_option5.m_maxtime,"")!=0){
    ShellOptions->maxtime = "E";
    ShellOptions->maxtime += maintab->m_option5.m_maxtime;
  } else ShellOptions->maxtime = "";
  
  //--> max rate
  if(strcmp(maintab->m_option5.m_maxrate,"")!=0){
    ShellOptions->maxrate = "A";
    ShellOptions->maxrate += maintab->m_option5.m_maxrate;
  } else ShellOptions->maxrate = "";
  
  if(strcmp(maintab->m_option5.m_maxconn,"")!=0){
    ShellOptions->maxconn = "%c";
    ShellOptions->maxconn += maintab->m_option5.m_maxconn;
  } else ShellOptions->maxconn = "";
  
  if(strcmp(maintab->m_option5.m_maxlinks,"")!=0){
    ShellOptions->maxlinks = "#L";
    ShellOptions->maxlinks += maintab->m_option5.m_maxlinks;
  } else ShellOptions->maxlinks = "";
  
  if(strcmp(maintab->m_option4.m_rate,"")!=0){
    ShellOptions->rate = "J";
    ShellOptions->rate += maintab->m_option4.m_rate;
  } else ShellOptions->rate = "";
  
  if(strcmp(maintab->m_option6.m_user,"")!=0){
    ShellOptions->user = "\"";
    ShellOptions->user += maintab->m_option6.m_user;
    ShellOptions->user += "\"";
  } else ShellOptions->user = "";
  
  if(strcmp(maintab->m_option6.m_footer,"")!=0){
    ShellOptions->footer = "\"";
    ShellOptions->footer += maintab->m_option6.m_footer;
    ShellOptions->footer += "\"";
  } else ShellOptions->footer = "";
  
  if(strcmp(maintab->m_option6.m_accept_language,"")!=0){
    ShellOptions->accept_language = "\"";
    ShellOptions->accept_language += maintab->m_option6.m_accept_language;
    ShellOptions->accept_language += "\"";
  } else ShellOptions->accept_language = "";

  if(strcmp(maintab->m_option6.m_other_headers,"")!=0){
    ShellOptions->other_headers += maintab->m_option6.m_other_headers;
  } else ShellOptions->other_headers = "";

  if(strcmp(maintab->m_option6.m_default_referer,"")!=0){
    ShellOptions->default_referer = "\"";
    ShellOptions->default_referer += maintab->m_option6.m_default_referer;
    ShellOptions->default_referer += "\"";
  } else ShellOptions->default_referer = "";

  if(strcmp(maintab->m_option4.m_retry,"")!=0){
    ShellOptions->retry = "R";
    ShellOptions->retry += maintab->m_option4.m_retry;
  } else ShellOptions->retry = "";
  
  if(strcmp(maintab->m_option7.m_url2,"")!=0){
    ShellOptions->buff_filtres = maintab->m_option7.m_url2;
  } else ShellOptions->buff_filtres = "";
  
  
  // MIME
  ShellOptions->buff_MIME = "";
  ADD_MIME_IN_COPT(1);
  ADD_MIME_IN_COPT(2);
  ADD_MIME_IN_COPT(3);
  ADD_MIME_IN_COPT(4);
  ADD_MIME_IN_COPT(5);
  ADD_MIME_IN_COPT(6);
  ADD_MIME_IN_COPT(7);
  ADD_MIME_IN_COPT(8);

  /* autres options: RAS */
  if (dialog2->m_rasdisc)
    disconnect=1;     /* déconnexion à la fin */
  else
    disconnect=0;

  /* autres options: Shutdown */
  if (dialog2->m_rasshut)
    shutdown_pc=1;     /* étendre à la fin */
  else
    shutdown_pc=0;
}

/* From MSDN: */
void CALLBACK
MessageBoxTimer(HWND hwnd, UINT uiMsg, UINT idEvent, DWORD dwTime)
{
  PostQuitMessage(0);
}
UINT
TimedMessageBox(
                HWND hwndParent,
                LPCTSTR ptszMessage,
                LPCTSTR ptszTitle,
                UINT flags,
                DWORD dwTimeout)
{
  UINT_PTR idTimer;
  UINT uiResult;
  MSG msg;

  /*
  *  Set a timer to dismiss the message box.
  */ 
  idTimer = SetTimer(NULL, 0, dwTimeout, (TIMERPROC)MessageBoxTimer);

  uiResult = MessageBox(hwndParent, ptszMessage, ptszTitle, flags);

  /*
  *  Finished with the timer.
  */ 
  KillTimer(NULL, idTimer);

  /*
  *  See if there is a WM_QUIT message in the queue. If so,
  *  then you timed out. Eat the message so you don't quit the
  *  entire application.
  */ 
  if (PeekMessage(&msg, NULL, WM_QUIT, WM_QUIT, PM_REMOVE)) {

    /*
    *  If you timed out, then return zero.
    */ 
    uiResult = 0;
  }

  return uiResult;
}

/* From MSDN */
BOOL InitiateSystemShutdownExWithPriv(
  LPTSTR lpMessage,
  DWORD dwTimeout,
  BOOL bForceAppsClosed,
  BOOL bRebootAfterShutdown,
  DWORD dwReason)
{
   HANDLE hToken;              // handle to process token 
   TOKEN_PRIVILEGES tkp;       // pointer to token structure 
 
   BOOL fResult;               // system shutdown flag 
 
   // Get the current process token handle so we can get shutdown 
   // privilege. 
 
   if (!OpenProcessToken(GetCurrentProcess(), 
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) 
      return FALSE; 
 
   // Get the LUID for shutdown privilege. 
 
   LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, 
        &tkp.Privileges[0].Luid); 
 
   tkp.PrivilegeCount = 1;  // one privilege to set    
   tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
 
   // Get shutdown privilege for this process. 
 
   AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, 
      (PTOKEN_PRIVILEGES) NULL, 0); 
 
   // Cannot test the return value of AdjustTokenPrivileges. 
 
   if (GetLastError() != ERROR_SUCCESS) 
      return FALSE; 
 
   // Display the shutdown dialog box and start the countdown. 

#if 0
  fResult = InitiateSystemShutdownEx(
    NULL,
    lpMessage,
    dwTimeout,
    bForceAppsClosed,
    bRebootAfterShutdown,
    dwReason
    );
#else
#ifndef EWX_FORCEIFHUNG
#define EWX_FORCEIFHUNG     0x00000010
#endif
   UINT msgRes = TimedMessageBox(NULL, lpMessage, _T("Shutdown"), 
     MB_OKCANCEL  | MB_SYSTEMMODAL | MB_ICONEXCLAMATION, 
     dwTimeout*1000);
     if (msgRes == 0 || msgRes == IDOK) {
       fResult = ExitWindowsEx(EWX_LOGOFF | EWX_POWEROFF | EWX_FORCEIFHUNG, dwReason);
     } else {
       fResult = 0;
     }
#endif

   // Disable shutdown privilege.
   tkp.Privileges[0].Attributes = 0; 
   AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, 
        (PTOKEN_PRIVILEGES) NULL, 0); 
 
   if (!fResult) 
      return FALSE; 
   return TRUE; 
}


// Les routines à définir:
int __cdecl httrackengine_check(t_hts_callbackarg *carg, httrackp *opt, const char* adr,const char* fil,int status) {  // appelé par le wizard
  return -1;
}
int __cdecl httrackengine_check_mime(t_hts_callbackarg *carg, httrackp *opt, const char* adr,const char* fil,const char* mime,int status) {  // appelé par le wizard
  ATLTRACE(__FUNCTION__ " : check %s%s : <%s>\r\n", adr, fil, mime);
  return -1;
}
EXECUTION_STATE (WINAPI * SetThreadExecutionState_)(IN EXECUTION_STATE) = NULL;
void __cdecl httrackengine_init(t_hts_callbackarg *carg) {    // appelé lors de l'init de HTTRACK, avant le début d'un miroir
  ATLTRACE(__FUNCTION__ " : init\r\n");
  // Finished
  PlaySound("MirrorStarted", NULL, SND_ASYNC | SND_NOWAIT | SND_APPLICATION);

#if USE_RAS
  has_started=0;
#endif

  /* Dynamic SetThreadExecutionState_ */
  if (SetThreadExecutionState_ == NULL) {
    HANDLE handle = LoadLibraryA("Kernel32.dll");
    if (handle != INVALID_HANDLE_VALUE) {
      SetThreadExecutionState_ = (EXECUTION_STATE (__stdcall *)(EXECUTION_STATE)) GetProcAddress((HMODULE)handle, "SetThreadExecutionState");
    }
  }

  httrackengine_loop(NULL, NULL, NULL, 0, 0, 0, 0, NULL, 0);  // init
  //printf("DEMARRAGE DU MIROIR DETECTE\n");  
}
void __cdecl httrackengine_uninit(t_hts_callbackarg *carg) {  // appelé en fin de miroir (peut être utile!!!)
  ATLTRACE(__FUNCTION__ " : uninit\r\n");
  // Finished
  PlaySound("MirrorFinished", NULL, SND_ASYNC | SND_NOWAIT | SND_APPLICATION);

 // Disconnect RAS
#if USE_RAS
  if (LibRasUse) {        /* librairie RAS chargée */
    if (disconnect) {     /* on doit déconnecter */
      if (connected) {    /* on a initié une connexion */
        if (conn)
          LibRas->RasHangUp(conn);
      } else {            /* tout déconnecter */
        // On coupe tout (non, pas bourrin)
        DWORD size;
        RASCONN* adr;
        int count=256;
        size = sizeof(RASCONN)*(count+2);
        adr = (RASCONN*) (char*) calloc(size,1);
        if (adr) {
          DWORD ent;
          int i;
          for(i=0;i<count;i++) {
            adr[i].dwSize=sizeof(RASCONN);
            strcpybuff(adr[i].szEntryName,"");
          }
          if (LibRas->RasEnumConnections((RASCONN*) adr,&size,&ent) == 0) {
            for(i=0;i<(int)ent;i++) {
              LibRas->RasHangUp(adr[i].hrasconn);
            }
          }
          free(adr);
        }
        
      }
    }
  }
#endif

  // Shutdown PC
  if (shutdown_pc) {
    //ExitWindowsEx(EWX_SHUTDOWN | EWX_POWEROFF | EWX_FORCEIFHUNG, 
    //  SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_OTHER);
    InitiateSystemShutdownExWithPriv( 
      _T(
        "WinHTTrack has finished the mirror, the system will soon reboot.\r\n"
        "Click 'OK' to reboot.\r\n"
        "** CLICK NOW 'CANCEL' TO CANCEL THE REBOOT! **\r\n"
      ),
      6,
      TRUE /* bForceAppsClosed */,
      FALSE /* bRebootAfterShutdown */,
      SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_OTHER
      );
  }
}
int __cdecl httrackengine_start(t_hts_callbackarg *carg, httrackp *opt) {   // appelé lors du démarrage du miroir (premières requètes)
  ATLTRACE(__FUNCTION__ " : starting\r\n");
#if USE_RAS
  // connexion RAS
  has_started=1;    // démarrage
  connected=0;
  conn = NULL;
  memset(&SInfo, 0, sizeof(SInfo));
  if (LibRasUse) {
    if (ShellOptions->_RasString.GetLength()>0) {    // sélection provider
      if (!LibRas->RasDial(NULL,NULL,&ShellOptions->_dial,NULL,NULL,&conn)) {
        RASCONNSTATUS status;
        do {
          status.dwSize = sizeof(status);
          LibRas->RasGetConnectStatus(conn,&status);
          switch(status.rasconnstate) {
          case RASCS_Connected : 
            connected=1;
            break;
          case RASCS_Disconnected :
            strcpybuff(connected_err,LANG(LANG_F3 /*"Could not connect to provider"*/));
            connected=-1;
            break;
          }
        } while(connected==0);
      } else {
        strcpybuff(connected_err,LANG(LANG_F3 /*"Could not connect to provider","Impossible d'établir la connexion"*/));
        connected=-1;
        //termine=1;
      }
    }
    //
    if (connected != -1)  // si pas d'erreur RAS
      return 1;
    else
      return 0;
  } else
    return 1;
#else
  return 1;
#endif
}
int  httrackengine_end(t_hts_callbackarg *carg, httrackp *opt) {     // appelé lors de la fin du miroir (plus de liens à charger)
  ATLTRACE(__FUNCTION__ " : end\r\n");
  WHTT_LOCK();
  termine=1;
  if (_Cinprogress_inst) {
    _Cinprogress_inst->EndDialog(IDOK);
    _Cinprogress_inst=NULL;
  }
  WHTT_UNLOCK();
  return 1;
}
int __cdecl httrackengine_htmlpreprocess(t_hts_callbackarg *carg, httrackp *opt, char** html,int* len,const char* url_address,const char* url_file) {
  ATLTRACE(__FUNCTION__ " : preprocessing %s%s (%d bytes)\r\n", url_address, url_file, *len);
  return 1;
}
int __cdecl httrackengine_htmlpostprocess(t_hts_callbackarg *carg, httrackp *opt, char** html,int* len,const char* url_address,const char* url_file) {
  ATLTRACE(__FUNCTION__ " : postprocessing %s%s (%d bytes)\r\n", url_address, url_file, *len);
  //char *old = *html;
  //*html = hts_strdup(*html);
  //hts_free(old);
  return 1;
}
int __cdecl httrackengine_htmlcheck(t_hts_callbackarg *carg, httrackp *opt, char* html,int len,const char* url_address,const char* url_file) {    // appelé à chaque fois qu'un html doit être scanné (utile pour la prospection mais inutile ici)
  return 1;
}
int __cdecl httrackengine_chopt(t_hts_callbackarg *carg, httrackp *opt) {
  ATLTRACE(__FUNCTION__ " : changing options\r\n");
  return 1;
}
void __cdecl httrackengine_filesave(t_hts_callbackarg *carg, httrackp *opt, const char* file) {
}
void __cdecl httrackengine_filesave2(t_hts_callbackarg *carg, httrackp *opt, const char* adr, const char* fil, const char* save, int is_new, int is_modified, int not_updated) {
  ATLTRACE(__FUNCTION__ " : saving %s%s as %s (new=%d, modified=%d, notupdated=%d)\r\n", adr, fil, save, is_new, is_modified, not_updated);
}

// Le routine la plus utile sans doute: elle refresh les tableaux
// C'est la 2e routine en thread qui assure le refresh graphique
// (plus efficace)
// -->C'est elle qui décide de tout arrêter si elle détecte in termine_request<--
int __cdecl httrackengine_loop(t_hts_callbackarg *carg, httrackp *opt,
                               lien_back* back,int back_max,int back_index,
                               int lien_n,int lien_tot,
                               int stat_time,
                               hts_stat_struct* stats) {    // appelé à chaque boucle de HTTrack
  static char s[HTS_URLMAXSIZE*2]="";  // utilisé plus loin
  int stat_written=-1;
  int stat_updated=-1;
  int stat_errors=-1;
  int stat_warnings=-1;
  int stat_infos=-1;
  int nbk=-1;
  LLint nb=-1;
  int stat_nsocket=-1;
  LLint stat_bytes=-1;
  LLint stat_bytes_recv=-1;
  int irate=-1;

  /* Mutex */
  WHTT_LOCK();

  /* Forces the system to be in the working state by resetting the system idle timer. */
  if (SetThreadExecutionState_ != NULL)
    SetThreadExecutionState_(ES_SYSTEM_REQUIRED);

  if (stats) {
    stat_written=stats->stat_files;
    stat_updated=stats->stat_updated_files;
    stat_errors=stats->stat_errors;
    stat_warnings=stats->stat_warnings;
    stat_infos=stats->stat_infos;
    nbk=stats->nbk;
    stat_nsocket=stats->stat_nsocket;
    irate=(int)stats->rate;
    nb=stats->nb;
    stat_bytes=stats->nb;
    stat_bytes_recv=stats->HTS_TOTAL_RECV;
  }
  
#if !SHELL_MULTITHREAD
  static TStamp last_time;
#endif
  int rate;
  
  if (back_max == 0) {
#if !SHELL_MULTITHREAD
    last_time=0;
#endif
    // en cas de manque de time
    SInfo.refresh=1;
    SInfo.stat_timestart=time_local();
    WHTT_UNLOCK();
    return 1;
  }
  
  if ((termine) || (termine_requested)) {
    SInfo.refresh=0;      // pas de refresh
    SInfo.refresh=0;      // pas de refresh
    termine_requested=1;
    WHTT_UNLOCK();
    return 0;
  }
  
  if (stat_written>=0) SInfo.stat_written=stat_written;
  if (stat_updated>=0) SInfo.stat_updated=stat_updated;
  if (stat_errors>=0)  SInfo.stat_errors=stat_errors;
  if (stat_warnings>=0)  SInfo.stat_warnings=stat_warnings;
  if (stat_infos>=0)  SInfo.stat_infos=stat_infos;
  
#if SHELL_MULTITHREAD 
  //if (((tl-last_time)>=100) || ((tl-last_time)<0)) {   // chaque 100 ms
  if (SInfo.ask_refresh) {
#else
    TStamp tl=0;
    {
      time_t tt;
      struct tm* A;
      tt=time(NULL);
      A=localtime(&tt);
      tl+=A->tm_sec;
      tl+=A->tm_min*60;
      tl+=A->tm_hour*60*60;
      //tl+=A->tm_yday*60*60*24;
      //tl+=A->tm_year*60*60*24*365;
      
      tl*=1000;  // en ms
      
      struct _timeb timebuffer;
      char *timeline;
      _ftime( &timebuffer );
      timeline = ctime( & ( timebuffer.time ) );
      
      tl+=timebuffer.millitm;    // + ms
    }
    if (((tl-last_time)>=HTS_SLEEP_WIN) || ((tl-last_time)<0)) {   // chaque 250 ms
      last_time=tl;
#endif
      //INFILLMEM_LOCKED=1;    // locker interface
      // OPTI int rate;
      SInfo.ask_refresh=0;
      
      // pour éviter temps cpu consommé trop grand
      // Sleep(10);
      
      // initialiser ft
      if ((stat_nsocket==-1)) {
        if (SInfo.ft==-1) {
          SInfo.ft=stat_time;
        }
      }
      
#if !SHELL_MULTITHREAD
      //
      MSG msg;  
      if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE )) {     
        switch (msg.message) {
        case WM_COMMAND:        
          switch(msg.wParam) { 
          case ID_APP_ABOUT: {
            Cabout about;
            about.DoModal();                                    }
            break;
          case ID_APP_EXIT: case WM_CLOSE :
            termine_requested=1;
            break;
          case  WM_DESTROY: case WM_NCDESTROY: 
            termine_requested=1;
            break;
          }
          break;        
          default:
            DefWindowProc(msg.hwnd,msg.message,msg.wParam,msg.lParam);
            break;
        }
      }
      //
#endif
      
      // calculer heure si ce n'est déja fait
      if (stat_time<0)
        SInfo.stat_time=(int) (time_local()-SInfo.stat_timestart);
      
      // calculer transfer rate
      if ((stat_time>0) && (stat_bytes_recv>0))
        rate=(int)(stat_bytes_recv/stat_time);
      else
        rate=0;    // pas d'infos
      
      // stocker infos: octets transférés, temps, etc.
      if (stat_bytes>=0) SInfo.stat_bytes=stat_bytes;      // bytes
      if (stat_time>=0) SInfo.stat_time=stat_time;         // time
      if (lien_tot>=0) SInfo.lien_tot=lien_tot; // nb liens
      if (lien_n>=0) SInfo.lien_n=lien_n;       // scanned
      SInfo.stat_nsocket=stat_nsocket;          // socks
      if (rate>0)  SInfo.rate=rate;                // rate
      if (irate>=0) SInfo.irate=irate;             // irate
      if (SInfo.irate<0) SInfo.irate=SInfo.rate;
      if (nbk>=0) SInfo.stat_back=nbk;
      
      // back: tableau de back_max éléments de cache
      // back_max: nombre d'éléments ^^^^
      // lien_tot: nombre total de liens traités pour le moment
      // stat_bytes: octets sauvegardés
      // stat_bytes_recv: octets téléchargés
      // stat_time: temps en seconde depuis le début du miroir
      // stat_nsocket: nombre de sockets connectées actuellement
      // on peut en déduire rate=stat_bytes_recv/stat_time
      
      // printf("loop.. %d liens, %d octets, %d secondes, %d sockets, TAUX=%d\n",lien_tot,stat_bytes,stat_time,stat_nsocket,rate);
      
      // parcourir registre des liens
      if (back_index>=0) {  // seulement si index passé
        int j,k;
        int index=0;
        int ok=0;         // idem
        int l;            // idem
        int M=32;         // idem
        
        StatsBufferback=(void*) back;
        StatsBufferback_max=back_max;
        {
          int i;
          for(i=0;i<NStatsBuffer;i++) {
            strcpybuff(StatsBuffer[i].etat,"");
            strcpybuff(StatsBuffer[i].nom,"");
            strcpybuff(StatsBuffer[i].fichier,"");
            strcpybuff(StatsBuffer[i].url_sav,"");
            StatsBuffer[i].back=NULL;
            StatsBuffer[i].size=0;
            StatsBuffer[i].sizetot=0;
          }
        }
        for(k=0;k<2;k++) {    // 0: lien en cours 1: autres liens
          for(j=0;(j<3) && (index<NStatsBuffer);j++) {  // passe de priorité
            int _i;
            for(_i=0+k;(_i< max(back_max*k,1) ) && (index<NStatsBuffer);_i++) {  // no lien
              int i=(back_index+_i)%back_max;    // commencer par le "premier" (l'actuel)
              if (back[i].status>=0) {     // signifie "lien actif"
                // int ok=0;  // OPTI
                ok=0;
                switch(j) {
                case 0:     // prioritaire
                  if ((back[i].status>0) && (back[i].status<99)) {
                    strcpybuff(StatsBuffer[index].etat,LANG(LANG_F4 /*"receive","réception"*/)); ok=1;
                  }
                  break;
                case 1:
                  if (back[i].status==99) {
                    strcpybuff(StatsBuffer[index].etat,LANG(LANG_F5 /*"request","requète"*/)); ok=1;
                  }
                  else if (back[i].status==STATUS_CONNECTING) {
                    strcpybuff(StatsBuffer[index].etat,LANG(LANG_F6 /*"connect","connexion"*/)); ok=1;
                  }
                  else if (back[i].status==101) {
                    strcpybuff(StatsBuffer[index].etat,LANG(LANG_F7 /*"search","recherche"*/)); ok=1;
                  }
                  else if (back[i].status==1000) {    // ohh le beau ftp
										char proto[] = "ftp";
										if (back[i].url_adr[0]) {
											char* ep = strchr(back[i].url_adr, ':');
											char* eps = strchr(back[i].url_adr, '/');
											int count;
											if (ep != NULL && ep < eps && (count = (int) (ep - back[i].url_adr) ) < 4) {
												proto[0] = '\0';
												strncat(proto, back[i].url_adr, count);
											}
										}
                    sprintf(StatsBuffer[index].etat,"%s: %s",proto,back[i].info); ok=1;
                  }
                  else if (back[i].status==102) {         // SSL handshake
                    strcpybuff(StatsBuffer[index].etat,LANG(LANG_F6 /*"connect","connexion"*/)); ok=1;
                  }
                  else if (back[i].status==STATUS_ALIVE) {         // waiting (keep-alive)
                    strcpybuff(StatsBuffer[index].etat,LANG(LANG_F8)); ok=1;
                  }
                  break;
                default:
                  if (back[i].status==0) {  // prêt
                    if ((back[i].r.statuscode==200)) {
                      strcpybuff(StatsBuffer[index].etat,LANG(LANG_F8 /*"ready","prêt"*/)); ok=1;
                    }
                    else if ((back[i].r.statuscode>=100) && (back[i].r.statuscode<=599)) {
                      char tempo[256]; tempo[0]='\0';
                      infostatuscode(tempo,back[i].r.statuscode);
                      strcpybuff(StatsBuffer[index].etat,tempo); ok=1;
                    }
                    else {
                      strcpybuff(StatsBuffer[index].etat,LANG(LANG_F9 /*"error","erreur"*/)); ok=1;
                    }
                  }
                  break;
                }
                
                if (ok) {
                  // OPTI int l;
                  // OPTI int M=32;     // longueur
                  // OPTI char s[HTS_URLMAXSIZE*2]="";
                  //
                  StatsBuffer[index].back=i;        // index pour + d'infos
                  //
                  s[0]='\0';
                  strcpybuff(StatsBuffer[index].url_sav,back[i].url_sav);   // pour cancel
                  if (strcmp(back[i].url_adr,"file://"))
                    strcatbuff(s,back[i].url_adr);
                  else
                    strcatbuff(s,"localhost");
                  if (back[i].url_fil[0]!='/')
                    strcatbuff(s,"/");
                  strcatbuff(s,back[i].url_fil);
                  
                  StatsBuffer[index].fichier[0]='\0';
                  {
                    char* a=strrchr(s,'/');
                    if (a) {
                      strncatbuff(StatsBuffer[index].fichier,a,200);
                      *a='\0';
                    }
                  }
                  
                  if ((l = (int) strlen(s))<MAX_LEN_INPROGRESS)
                    strcpybuff(StatsBuffer[index].nom,s);
                  else {
                    // couper
                    StatsBuffer[index].nom[0]='\0';
                    strncatbuff(StatsBuffer[index].nom,s,MAX_LEN_INPROGRESS/2-2);
                    strcatbuff(StatsBuffer[index].nom,"...");
                    strcatbuff(StatsBuffer[index].nom,s+l-MAX_LEN_INPROGRESS/2+2);
                  }
                  
                  //if (back[i].url_fil[0]!='/') printf("/");
                  
                  if (back[i].r.totalsize>0) {  // taille prédéfinie
                    StatsBuffer[index].sizetot=back[i].r.totalsize;
                    StatsBuffer[index].size=back[i].r.size;
                  } else {  // pas de taille prédéfinie
                    if (back[i].status==0) {  // prêt
                      StatsBuffer[index].sizetot=back[i].r.size;
                      StatsBuffer[index].size=back[i].r.size;
                    } else {
                      StatsBuffer[index].sizetot=8192;
                      StatsBuffer[index].size=(back[i].r.size % 8192);
                    }
                  }
                  index++;
                }
              }
            }
          }
        }
    }
    
#if SHELL_MULTITHREAD
    SInfo.refresh=1;     // on signale qu'il faut faire un refresh!
#else
    inprogress_refresh();  // tout de suite (non multithread)
#endif
    // INFILLMEM_LOCKED=0;    // délocker interface
  }
  WHTT_UNLOCK();
  return (termine==0);
}

int inprogress_refresh() {
	strc_int2bytes2 strc, strc2;
  static int toggle=0;
  // WHTT_LOCK(); // deja fait
  if ((!termine) && (!termine_requested) && (inprogress->m_hWnd)) {
    if (SInfo.refresh) {
      // INREDRAW_LOCKED=1;
      // REFRESH (si nb de socket==-1 on manage les fenetres)
      int icn;
      icn=inprogress->IsIconic();
      CString lnk;
      if (SInfo.stat_back)
        lnk.Format("%d/%d (+%d)",SInfo.lien_n,SInfo.lien_tot-1,SInfo.stat_back);
      else
        lnk.Format("%d/%d",SInfo.lien_n,SInfo.lien_tot-1);
      if (!icn) {
        int parsing=0;
        if (!soft_term_requested) {
          if (!hts_setpause(global_opt, -1)) {
            if (!(parsing=hts_is_parsing(global_opt, -1)))
              SetDlgItemTextCP(inprogress, IDC_inforun,LANG(LANG_F10 /*"Receiving files.","Réception des fichiers"*/)); 
            else {
              switch(hts_is_testing(global_opt)) {
              case 0:
                SetDlgItemTextCP(inprogress, IDC_inforun,LANG(LANG_F11 /*"Parsing HTML file..","Parcours du fichier HTML"*/)); 
                break;
              case 1:
                SetDlgItemTextCP(inprogress, IDC_inforun,LANG(LANG_F12 /*"Parsing HTML file (testing links)..","Parcours du fichier HTML (test des liens)"*/)); 
                break;
              case 2:
                SetDlgItemTextCP(inprogress, IDC_inforun,LANG(LANG_F11b)); 
                break;
              case 3:
                SetDlgItemTextCP(inprogress, IDC_inforun,LANG(LANG_F11c)); 
                break;
              case 4:
                SetDlgItemTextCP(inprogress, IDC_inforun,LANG(LANG_F15)); 
                break;
              case 5:
                SetDlgItemTextCP(inprogress, IDC_inforun,LANG(LANG_F15b)); 
                break;
              }
            }
          } else {
            if (((toggle++)/5)%2)
              SetDlgItemTextCP(inprogress, IDC_inforun,LANG(LANG_F13 /*"Paused (select [File]/[Pause transfer] to continue)","Interrompu (choisir [Fichier]/[Interrompre transferts] pour continuer)"*/));
            else
              SetDlgItemTextCP(inprogress, IDC_inforun,"");
          }
        } else {
          if (((toggle++)/5)%2)
            SetDlgItemTextCP(inprogress, IDC_inforun,LANG(LANG_F13b));
          else
            SetDlgItemTextCP(inprogress, IDC_inforun,"");
        }
        
        if (SInfo.stat_time>0) {
          char s[256];
          qsec2str(s,(TStamp) SInfo.stat_time);
          SetDlgItemTextUTF8(inprogress, IDC_i1 ,s);  // time
        } else
          SetDlgItemTextUTF8(inprogress, IDC_i1 , _SN(SInfo.stat_time) );  // time
        SetDlgItemTextUTF8(inprogress, IDC_i0 , int2bytes(&strc, SInfo.stat_bytes) );  // bytes
        SetDlgItemTextUTF8(inprogress, IDC_i2 , lnk);  // scanned
        if (SInfo.stat_nsocket>0)
          SetDlgItemTextUTF8(inprogress, IDC_i3 , _SN(SInfo.stat_nsocket) );  // socks
        else
          SetDlgItemTextUTF8(inprogress, IDC_i3 , "none" );  // wait
        
        CString st;
        st.Format("%s (%s)", int2bytessec(&strc, SInfo.irate), int2bytessec(&strc2, SInfo.rate));
        SetDlgItemTextUTF8(inprogress, IDC_i4 , st );  // rate
        
        SetDlgItemTextUTF8(inprogress, IDC_i5 , _SN(SInfo.stat_errors) );
        SetDlgItemTextUTF8(inprogress, IDC_i6 , _SN(SInfo.stat_written) );
        {
          char tempo[256];
          int pc=0;
          if (SInfo.stat_written)
            pc=(int)((SInfo.stat_updated*100)/(SInfo.stat_written));
          if (pc)
            sprintf(tempo,"%d (%d%%)",SInfo.stat_updated,pc);
          else
            sprintf(tempo,"%d",SInfo.stat_updated);
          SetDlgItemTextUTF8(inprogress, IDC_i7 , tempo );
        }
        
        /*if (!parsing)*/
        {
          {
            int i;
            for(i=0;i<NStatsBuffer;i++) {
              if (StatsBuffer[i].sizetot>0) {
                TStamp d = ((TStamp) StatsBuffer[i].size * 1000);
                d = d / ((TStamp) StatsBuffer[i].sizetot);
                StatsBuffer[i].offset = (int) d;
              } else
                StatsBuffer[i].offset = 0;
            }
          }
          
          if (!parsing)
            inprogress->m_sl0.SetRange(0,1000);
          inprogress->m_sl1.SetRange(0,1000);
          inprogress->m_sl2.SetRange(0,1000);
          inprogress->m_sl3.SetRange(0,1000);
          inprogress->m_sl4.SetRange(0,1000);
          inprogress->m_sl5.SetRange(0,1000);
          inprogress->m_sl6.SetRange(0,1000);
          inprogress->m_sl7.SetRange(0,1000);
          inprogress->m_sl8.SetRange(0,1000);
          inprogress->m_sl9.SetRange(0,1000);
          inprogress->m_sl10.SetRange(0,1000);
          inprogress->m_sl11.SetRange(0,1000);
          inprogress->m_sl12.SetRange(0,1000);
          inprogress->m_sl13.SetRange(0,1000);
          
          if (!parsing)
            inprogress->m_sl0.SetPos(StatsBuffer[0].offset);
          inprogress->m_sl1.SetPos(StatsBuffer[1].offset);
          inprogress->m_sl2.SetPos(StatsBuffer[2].offset);
          inprogress->m_sl3.SetPos(StatsBuffer[3].offset);
          inprogress->m_sl4.SetPos(StatsBuffer[4].offset);
          inprogress->m_sl5.SetPos(StatsBuffer[5].offset);
          inprogress->m_sl6.SetPos(StatsBuffer[6].offset);
          inprogress->m_sl7.SetPos(StatsBuffer[7].offset);
          inprogress->m_sl8.SetPos(StatsBuffer[8].offset);
          inprogress->m_sl9.SetPos(StatsBuffer[9].offset);
          inprogress->m_sl10.SetPos(StatsBuffer[10].offset);
          inprogress->m_sl11.SetPos(StatsBuffer[11].offset);
          inprogress->m_sl12.SetPos(StatsBuffer[12].offset);
          inprogress->m_sl13.SetPos(StatsBuffer[13].offset);
          
          // redraw en boucle
          {
            int i=0;
            if (parsing)
              i++;
            for( ; i<NStatsBuffer;i++) {
              CString st;
              st = StatsBuffer[i].etat;
              st.Replace("&", "&&");
              SetWindowTextCP(inprogress->element[0][i], st);
              st = StatsBuffer[i].nom;
              st.Replace("&", "&&");
              SetWindowTextUTF8(inprogress->element[1][i], st);
              st = StatsBuffer[i].fichier;
              st.Replace("&", "&&");
              SetWindowTextUTF8(inprogress->element[4][i], st);
              
              if ((strlen(StatsBuffer[i].etat)==0) != StatsBuffer[i].actived) {
                StatsBuffer[i].actived=!StatsBuffer[i].actived;
                if (!StatsBuffer[i].actived)
                  inprogress->element[3][i]->ModifyStyle(WS_DISABLED,0);
                else
                  inprogress->element[3][i]->ModifyStyle(0,WS_DISABLED);
                inprogress->element[3][i]->RedrawWindow();
              }
            }
          }
          //
        }
        /* else*/
        if (parsing) {  // parsing
          //
          inprogress->m_sl0.SetRange(0,100);
          inprogress->m_sl0.SetPos(parsing);
          SetWindowTextCP(inprogress->element[0][0], LANG(LANG_F14 /*"scanning","parcours"*/));
          SetWindowTextUTF8(inprogress->element[1][0], StatsBuffer[0].nom);
          SetWindowTextCP(inprogress->element[4][0], "");
          //inprogress->element[0][1]->SetWindowTextCP(this, );
        }
        
      }
      {
        static char last_info[256]="";
        char info[256];
        if ((SInfo.stat_nsocket==-1)) {
#if USE_RAS
          if (!has_started)
#endif
            SetDlgItemTextCP(inprogress, IDC_nm0,LANG(LANG_F15 /*"Waiting for specific hour to start","Attente de l'heure programmée pour démarrer"*/));
#if USE_RAS
          else
            SetDlgItemTextCP(inprogress, IDC_nm0,LANG(LANG_F16 /*"Connecting to provider","Connexion au provider"*/));
#endif
          inprogress->m_sl0.SetRange(0,SInfo.ft);
          inprogress->m_sl0.SetPos(SInfo.ft-SInfo.stat_time);  // temps restant
          // SetDlgItemTextCP(inprogress, IDC_nm1,_SN(ft));
          if (icn && (!this_CSplitterFrame->iconifie)) {  // minimisée mais pas en icone
            sprintf(info,"[%d s]",SInfo.stat_time);
          } else {
            sprintf(info,LANG(LANG_F17 /*"Mirror waiting [%d seconds]","Miroir en attente [%d secondes]"*/),SInfo.stat_time);
          }
        } else {
          if (icn) {  // minimisée
            sprintf(info,"[%s]",lnk);
          } else {
            char byteb[256];
            sprintf(byteb, LLintP, SInfo.stat_bytes);
            sprintf(info,LANG(LANG_F18),lnk,byteb);
          }
        }
        if (strcmp(info,last_info)) {       /* a changé */
          strcpybuff(last_info,info);           /* recopier */
          if (this_CSplitterFrame->iconifie)  // minimisé icone
            this_CSplitterFrame->IconChange(last_info);
          else
            SetWindowTextCP(GetMainWindow(), last_info);
        }
      }  
      
      //inprogress->UpdateWindow();
    } else {
    }
  }
  // WHTT_UNLOCK();
  return 1;
}

/* Plantages si DoModal() dans un thread != du principal.. passons.. */
const char* __cdecl httrackengine_query(t_hts_callbackarg *carg, httrackp *opt, const char* question) {
  strcpybuff(WIZ_question,question);
  strcpybuff(WIZ_reponse, "");
  // AfxGetMainWnd()
  CWnd* wnd = GetMainWindow();
  if (wnd) {
    wnd->SendMessage(WM_COMMAND,wm_WizRequest1,0);
  }
  return WIZ_reponse;
}

const char* __cdecl httrackengine_query2(t_hts_callbackarg *carg, httrackp *opt, const char* question) {
  strcpybuff(WIZ_question,question);
  strcpybuff(WIZ_reponse, "");
  // AfxGetMainWnd()
  CWnd* wnd = GetMainWindow();
  if (wnd) {
    wnd->SendMessage(WM_COMMAND,wm_WizRequest2,0);
  }
  return WIZ_reponse;
}

const char* __cdecl httrackengine_query3(t_hts_callbackarg *carg, httrackp *opt, const char* question) {
  strcpybuff(WIZ_question,question);
  strcpybuff(WIZ_reponse, "");
  CWnd* wnd = GetMainWindow();
  if (wnd) {
    wnd->SendMessage(WM_COMMAND,wm_WizRequest3,0);
  }
  return WIZ_reponse;
}

void __cdecl httrackengine_pause(t_hts_callbackarg *carg, httrackp *opt, const char* lockfile) {
  ATLTRACE(__FUNCTION__ " : pause\r\n");
  AfxMessageBox("Engine paused.. click OK to continue!",MB_OK);
  remove(lockfile);
}

// modif RX 10/10/98 pour gestion des , et des tabs
static void StripControls(char* chaine)
{
  for(int i=0 ; chaine[i] != '\0' ; i++)
  {
    if(chaine[i]=='\n' || chaine[i]==13 || chaine[i]==9) {
      chaine[i]=' ';
    }
  }
}

#if SHELL_MULTITHREAD

static int __cdecl ExcFilter_(DWORD dwExceptCode, PEXCEPTION_POINTERS pExceptPtrs) {
  return EXCEPTION_CONTINUE_EXECUTION;
}                              

void __cdecl RunBackRobot(void* al_p) {
  int argc;
  char** argv;

  while((!inprogress) && (!termine)) Sleep(10);
  if (inprogress)
    while ((!inprogress->m_hWnd) || (termine)) Sleep(10);   // attendre formulaire
    //Sleep(100);
    
    Robot_params* al=(Robot_params*) al_p;
    argc = al->argc;
    argv = al->argv;
    /* launch the engine */
    hts_init();
#ifndef _DEBUG
    __try
#endif
		{
			if (global_opt != NULL)
			{
				hts_free_opt(global_opt);
				global_opt = NULL;
			}
      global_opt = hts_create_opt();
      assert(global_opt->size_httrackp == sizeof(httrackp));

      CHAIN_FUNCTION(global_opt, init, httrackengine_init, NULL);
      CHAIN_FUNCTION(global_opt, uninit, httrackengine_uninit, NULL);
      CHAIN_FUNCTION(global_opt, start, httrackengine_start, NULL);
      CHAIN_FUNCTION(global_opt, end, httrackengine_end, NULL);
      CHAIN_FUNCTION(global_opt, chopt, httrackengine_chopt, NULL);
      CHAIN_FUNCTION(global_opt, preprocess, httrackengine_htmlpreprocess, NULL);
      CHAIN_FUNCTION(global_opt, postprocess, httrackengine_htmlpostprocess, NULL);
      CHAIN_FUNCTION(global_opt, check_html, httrackengine_htmlcheck, NULL);
      CHAIN_FUNCTION(global_opt, query, httrackengine_query, NULL);
      CHAIN_FUNCTION(global_opt, query2, httrackengine_query2, NULL);
      CHAIN_FUNCTION(global_opt, query3, httrackengine_query3, NULL);
      CHAIN_FUNCTION(global_opt, loop, httrackengine_loop, NULL);
      CHAIN_FUNCTION(global_opt, check_link, httrackengine_check, NULL);
      CHAIN_FUNCTION(global_opt, check_mime, httrackengine_check_mime, NULL);
      CHAIN_FUNCTION(global_opt, pause, httrackengine_pause, NULL);
      CHAIN_FUNCTION(global_opt, filesave, httrackengine_filesave, NULL);
      CHAIN_FUNCTION(global_opt, filesave2, httrackengine_filesave2, NULL);
      
			HTTRACK_result = hts_main2(argc, argv, global_opt);
    }
#ifndef _DEBUG
    __except(ExcFilter_(GetExceptionCode(), GetExceptionInformation()))
    {
      HTTRACK_result = -100;
    }
#endif
    /* clear all vars */
    WHTT_LOCK();
    termine=1;
    WHTT_UNLOCK();
    htsthread_wait_n(1);
    hts_uninit();

    // Cleanup argv
    if (argv != NULL) {
      for(int i = 0 ; argv[i] != NULL ; i++) {
        freet(argv[i]);
        argv[i] = NULL;
      }
      freet(argv);
    }
}
#endif

// modifs RX 10/10/98: gestion des ,
CString change(char* chaine,char c) {
  int comma=1; int first=1;
  CString chaine1;
  for(int i=0;i < (int) strlen(chaine);i++) {
    switch(chaine[i]) {
    case 10: case 13: case 9: case ' ': case ',':
      comma=1; 
      break;
    default:
      if (comma) {
        if (!first) chaine1 +=' ';
        else first=0; 
        chaine1  +=c; 
        comma=0;
      }
      chaine1 += chaine[i]; 
      break;
    }
  }
  return chaine1;
}

class SeparatorComparator {
public:
  inline virtual bool isSeparator(const char c) const = 0;
};

class SpaceSeparatorComparator: public SeparatorComparator {
public:
  inline bool isSeparator(const char c) const {
    return c == ' ' || c == '\t' || c == '\n';
  }
};

class EOLSeparatorComparator: public SeparatorComparator {
public:
  inline bool isSeparator(const char c) const {
    return c == '\n';
  }
};

static const SpaceSeparatorComparator instSpaceSeparatorComparator;
static const EOLSeparatorComparator instEOLSeparatorComparator;

static void splitStringInArray(CSimpleArray<CString> &args, 
                               const CString &str, 
                               const SeparatorComparator &comp=instEOLSeparatorComparator,
                               const CString &separator=CString()) {
  const int size = str.GetLength();
  int i, last;
  for(i = 0, last = 0 ; i <= size; i++) {
    if (i == size || comp.isSeparator(str[i])) {
      if (last != i) {
        CString sub = str.Mid(last, i - last);
        sub.Trim(_T(" \t\r\n"));
        if (sub.GetLength() != 0) {
          if (separator.GetLength() !=0) {
            args.Add(separator);
          }
          args.Add(sub);
        }
      }
      last = i + 1;
    }
  }
}

// Lancement
void lance(void) {
  char **argv;
  int argvAlloc = 1024;
  int argc=1;
  int g=0;
  int i =0;
	char catbuff[CATBUFF_SIZE], catbuff2[CATBUFF_SIZE];

  CSimpleArray<CString> args;
  CString single;

  if (fp_debug) {
    fprintf(fp_debug,"Building command line\r\n");
    fflush(fp_debug);
  }


  // Single command (compacted)
  single = "-";
  args.Add("--dummy");   // room for single command flags
  if (ShellOptions->choixdeb[0]!='W')
    single += "q";         // quiet
  
  if (ShellOptions->choixdeb[0]=='/')
    single += "i";
  else if (ShellOptions->choixdeb[0]!='!')
    single += ShellOptions->choixdeb;

  // option de profondeur
  if(strcmp(ShellOptions->depth,"")!=0) { 
    single += "r";
    single += ShellOptions->depth;
  }
  if(strcmp(ShellOptions->extdepth,"")!=0) { 
    single += "%e";
    single += ShellOptions->extdepth;
  }
  single += ShellOptions->cache;
  single += ShellOptions->norecatch;
  single += ShellOptions->testall;
  single += ShellOptions->parseall;
  single += ShellOptions->link;
  single += ShellOptions->external;
  single += ShellOptions->nopurge;
  single += ShellOptions->hidepwd;
  single += ShellOptions->hidequery;
  single += ShellOptions->robots;
  single += ShellOptions->cookies;
  single += ShellOptions->checktype;
  single += ShellOptions->parsejava;
  single += ShellOptions->Cache2;
  single += ShellOptions->logtype;
  single += ShellOptions->http10;
  single += ShellOptions->toler;
  single += ShellOptions->updhack;
  single += ShellOptions->urlhack;
  
  // si get, ne pas faire
  if (strcmp(ShellOptions->choixdeb,"g")!=0) {
    if(ShellOptions->build[0]=='-') {
      args.Add(ShellOptions->build);
    } else {
      single += ShellOptions->build;
    }
  }
  single += ShellOptions->dos;
  single += ShellOptions->index;
  single += ShellOptions->index2;
  single += ShellOptions->index_mail;
  single += ShellOptions->htmlfirst;
  single += ShellOptions->filtre;
  single += ShellOptions->max;
  single += ShellOptions->frag;
  single += ShellOptions->maxfile;
  single += ShellOptions->conn;
  single += ShellOptions->time;
  single += ShellOptions->rate;
  single += ShellOptions->retry;
  single += ShellOptions->hostquit;
  single += ShellOptions->ka;
  single += ShellOptions->log;
  single += ShellOptions->errpage;
  //-->
  single += ShellOptions->waittime;
  single += ShellOptions->maxtime;
  single += ShellOptions->maxrate;
  single += ShellOptions->maxconn;
  single += ShellOptions->maxlinks;
  single += ShellOptions->proxyftp;  
  single += "#f";  // flush
  
  if (ShellOptions->user.GetLength() != 0) {
    args.Add("-F");
    args.Add(ShellOptions->user);
  }

  if (ShellOptions->footer.GetLength() != 0) {
    args.Add("-%F");
    args.Add(ShellOptions->footer);
  }

  if (ShellOptions->accept_language.GetLength() != 0) {
    args.Add("-%l");
    args.Add(ShellOptions->accept_language);
  }

  if (ShellOptions->default_referer.GetLength() != 0) {
    args.Add("-%R");
    args.Add(ShellOptions->default_referer);
  }

  // Explode \n in other_headers
  if (ShellOptions->other_headers.GetLength() != 0) {
    splitStringInArray(args, ShellOptions->other_headers, instEOLSeparatorComparator, "-%X");
  }

  if ((int)ShellOptions->proxy.GetLength()>0) {
    args.Add("-P");
    args.Add(ShellOptions->proxy + ":" + ShellOptions->port);
  }
  
  // mode spider, mettre après options
  if (ShellOptions->choixdeb[0]=='!') {
    args.Add("--testlinks");
  } else if (ShellOptions->choixdeb[0]=='Y') {
    args.Add("--mirrorlinks");
  }
  
  // URLs!!
  if (ShellOptions->url.GetLength() != 0) {
    splitStringInArray(args, ShellOptions->url, instSpaceSeparatorComparator);
  }
  
  // file list
  if ((int) ShellOptions->filelist.GetLength()>0) {
    args.Add("-%L");
    args.Add(ShellOptions->filelist);
  }
  
  // chemins
  if(ShellOptions->path != "") {
    args.Add("-O1");
    args.Add(ShellOptions->path);
  }
  
  // buffer -> les + et -
  if(ShellOptions->buff_filtres.GetLength() != 0) {
    splitStringInArray(args, ShellOptions->buff_filtres, instSpaceSeparatorComparator);
  }
  
  // --assume
  if (ShellOptions->buff_MIME.GetLength() != 0) {
    splitStringInArray(args, ShellOptions->buff_MIME, instEOLSeparatorComparator);
  }

  // initial flags
  args.SetAtIndex(0, single);
  
  // ---
  
  ShellOptions->LINE_back = "winhttrack ";

  // Split into argc/argv
  argvAlloc = 1 + args.GetSize() + 1;
  argv = (char**) malloct(argvAlloc * sizeof(char*));
  argv[0] = strdupt("winhttrack");
  for(int i = 0; i < args.GetSize(); i++) {
    argv[i + 1] = strdupt((const char*) args[i]);

    // For debugging only
    ShellOptions->LINE_back += ' ';
    const bool quote = args[i].Find('"') >= 0;
    if (quote) {
      ShellOptions->LINE_back += '"';
    }
    ShellOptions->LINE_back += args[i];
    if (quote) {
      ShellOptions->LINE_back += '"';
    }
  }
  argv[argc = args.GetSize() + 1] = NULL;

  //
  if (fp_debug) {
    fprintf(fp_debug,"Checking doit.log\r\n");
    fflush(fp_debug);
  }
  //
  // Ok, on lance!
  if(!termine) {
    int result=0;
    {
      char path_log[HTS_URLMAXSIZE*2];
      strcpybuff(path_log,CShellApp_app->end_path_complete);
      if (strlen(path_log)>0)
        if ((path_log[strlen(path_log)-1]!='/') && (path_log[strlen(path_log)-1]!='\\'))
          strcatbuff(path_log,"/");
        
        // on efface le doit.log, pour annuler les parametres anciens et en redéfinir de nouveaux
        // c'est ici une logique qui diffère de la version en ligne de commande
        if (fexist(fconcat(catbuff,sizeof(catbuff),path_log,"hts-cache/new.zip"))
          || fexist(fconcat(catbuff2,sizeof(catbuff2),path_log,"hts-cache/new.ndx"))
          ) {    // un cache est présent
          if (fexist(fconcat(catbuff,sizeof(catbuff),path_log,"hts-cache/doit.log")))
            remove(fconcat(catbuff,sizeof(catbuff),path_log,"hts-cache/doit.log"));
          FILE* fp=fopen(fconcat(catbuff,sizeof(catbuff),path_log,"hts-cache/doit.log"),"wb");
          if (fp) fclose(fp);
        }
        //}
    }
    
    
    // ---
    // LANCER LE MIROIR
    // ---
    //
    if (fp_debug) {
      fprintf(fp_debug,"Ready to call httrack engine, launching threads\r\n");
      fflush(fp_debug);
    }
    //
#if SHELL_MULTITHREAD
    Robot_params al;
    al.argc=argc;
    al.argv=argv;
    hts_newthread( RunBackRobot, (void*) &al);
    
    //
    if (fp_debug) {
      fprintf(fp_debug,"Threads launched, displaying main dialog\r\n");
      fflush(fp_debug);
    }
    //
    // domodal du refresh
    /* XXC A SUPPRIMER */
    while(!termine) {
      Sleep(100);
    }
    //inprogress->DoModal();
    WHTT_LOCK();
    shell_terminated=1;
    result=HTTRACK_result;
    termine=1;  
    WHTT_UNLOCK();
    //
    if (fp_debug) {
      fprintf(fp_debug,"Main dialog exited\r\n");
      fflush(fp_debug);
    }
    //
    
    // non multithread
#else
#error "Non supporté"
#endif

    /* Aborted mirror or finished? */
    {
      char path_log[HTS_URLMAXSIZE*2];
      strcpybuff(path_log,CShellApp_app->end_path_complete);
      if (strlen(path_log)>0)
        if ((path_log[strlen(path_log)-1]!='/') && (path_log[strlen(path_log)-1]!='\\'))
          strcatbuff(path_log,"/");
        if (soft_term_requested || termine_requested) {
          FILE* fp=fopen(fconcat(catbuff,sizeof(catbuff),path_log,"hts-cache/interrupted.lock"),"wb");
          if (fp)
            fclose(fp);
        } else
          remove(fconcat(catbuff,sizeof(catbuff),path_log,"hts-cache/interrupted.lock"));
    }
    
    //SetForegroundWindow();   // yop en premier plan!
    //
    if (fp_debug) {
      fprintf(fp_debug,"Displaying end dialog\r\n");
      fflush(fp_debug);
    }
    //
    /* New pannel */
    if (result) {      // erreur?
      strcpybuff(end_mirror_msg,LANG(LANG_F19 /*"A problem occured during the mirror\n  \"","Un problème est survenu pendant le miroir\n  \""*/));
      strcatbuff(end_mirror_msg,"\"");
      if (result != -100) {
        strcatbuff(end_mirror_msg,hts_errmsg(global_opt));
      } else {
				strcatbuff(end_mirror_msg, "The engine unexpectedly crashed.");
      }
      strcatbuff(end_mirror_msg,"\"");
      strcatbuff(end_mirror_msg,LANG(LANG_F20 /*"\" \nDuring:\n  ","\" \nDurant:\n  "*/));
      strcatbuff(end_mirror_msg,"\"");
      strcatbuff(end_mirror_msg,ShellOptions->LINE_back);
      strcatbuff(end_mirror_msg,"\"");
      strcatbuff(end_mirror_msg,LANG(LANG_F21 /*"\nSee the log file if necessary.\n\nClick OK to quit WinHTTrack.\n\nThanks for using WinHTTrack!","\nVoir le fichier log au besoin\n\nCliquez sur OK pour quitter WinHTTrack\n\nMerci d'utiliser WinHTTrack."*/));
      //AfxMessageBox(s,MB_OK+MB_ICONINFORMATION);
    } else {
      strcpybuff(end_mirror_msg,LANG(LANG_F22 /*"The mirror is finished.\nClick OK to quit WinHTTrack.\nSee log file(s) if necessary to ensure that everything is OK.\n\nThanks for using WinHTTrack!","Le miroir est terminé\nCliquez sur OK pour quitter WinHTTrack\nVoir au besoin les fichiers d'audit pour vérifier que tout s'est bien passé\n\nMerci d'utiliser WinHTTrack!"*/));
      //AfxMessageBox("The mirror is finished.\nClic OK to quit WinHTTrack.\nSee log file(s) if necessary to ensure that everything is OK.\n\nThanks for using WinHTTrack!",MB_OK+MB_ICONINFORMATION);
      //        ShellExecute(0,"open",,"","",);
    }
#if USE_RAS
    // erreur ras
    if (connected == -1)
      if ((int) strlen(connected_err) > 0)
        strcpybuff(end_mirror_msg,connected_err);
#endif
      {
        char pathlog[HTS_URLMAXSIZE*2];
        strcpybuff(pathlog,dialog0->GetPath());
        Ciplog form;
        if (strlen(pathlog)>0)
          if ((pathlog[strlen(pathlog)-1]!='/') && (pathlog[strlen(pathlog)-1]!='\\'))
            strcatbuff(pathlog,"/");
          // fichier log existe ou on est télécommandé par un !
          if ( (fsize(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-err.txt")))>0) {
            strcatbuff(end_mirror_msg,LANG(LANG_F23 /*"\n\nTip: Click [View log file] to see warning or error messages","\n\nConseil: [Voir fichiers log] pour voir les erreurs et messages"*/));
          }
      }
      //this_Cinfoend->m_infoend = msg;
      if (inprogress->m_hWnd != NULL) {
        inprogress->SendMessage(WM_USER + 4);      // avertir
      }
      if (fp_debug) {
        fprintf(fp_debug,"DoModal end dialog, waiting\r\n");
        fflush(fp_debug);
      }
      /*
      if (info.DoModal() == IDC_NewProject) {     // relancer programme!
      // copie de onnewproject() de wid1::
      CWinApp* pApp = AfxGetApp();
      CString name = pApp->m_pszHelpFilePath;
      name=name.Left(name.GetLength()-4);
      name += ".EXE";
      ShellExecute(NULL,"open",name,"","",SW_RESTORE);	
      }
      */
      if (fp_debug) {
        fprintf(fp_debug,"Final info OK, leaving..\r\n");
        fflush(fp_debug);
      }
  }
  else {
  }
}

// int LANG_T(int);
// char* LANG(char* english,char* francais);


/* interface lang - lang_string="stringlang0\nstringlang1\n..laststring" */
void SetCombo(CWnd* _this,int id,char* lang_string) {
  CComboBox* combo = (CComboBox*) _this->GetDlgItem(id);
  CString st=lang_string;
  st.TrimLeft(); st.TrimRight();
  if (combo) {
    st+="\n";         /* end */
    combo->ResetContent();
    while(st.GetLength()) {
      int pos=st.Find('\n');
      CString item=st.Left(pos);
      st=st.Mid(pos+1);
      item.TrimLeft(); item.TrimRight();
      if (item.GetLength())
        combo->AddString(item);
    }
  }
}


// Ecriture profiles
CString profile_code(char* from) {
  int i;
  CString result;
  for(i = 0 ; from[i] != '\0' ; i++) {
    switch(from[i]) {
    case '%': 
      result += '%';
      result += '%';
      break;
    case '=': 
      result += '%';
      result += '3';
      result += 'd';
      break;
    case 13:
      result += '%';
      result += '0';
      result += 'd';
      break;
    case 10:
      result += '%';
      result += '0';
      result += 'a';
      break;
    case 9:
      result += '%';
      result += '0';
      result += '9';
      break;
    default:
      result += from[i];
      break;
    }
  }
  return result;
}
CString profile_decode(char* from) {
  int j;
  CString result;
  for(j = 0 ; from[j] != '\0' ; ) {  // oui oui
    if (from[j]=='%') {
      if (from[j + 1] == '%') {
        result += '%';
        j+=2;
      } else {
        if (strncmp(from+j+1,"0d", 2)==0)
          result += (char) 13;
        else if (strncmp(from+j+1,"0a", 2)==0)
          result += (char) 10;
        else if (strncmp(from+j+1,"09", 2)==0)
          result += (char) 9;
        else if (strncmp(from+j+1,"3d", 2)==0)
          result += '=';
        else
          result += ' ';
        j+=3;
      }
    } else
      result += from[j++];
  }
  return result;
}
//
// Ecriture/Lecture profiles
int MyWriteProfileInt(CString path,CString dummy,CString name,int value) {
  if (path.IsEmpty()) {
    CWinApp* pApp = AfxGetApp();
    pApp->WriteProfileInt(dummy,name,value);
  } else if (path=="<mem>") {       // buffer
    tmpm.setInt(name,value);
    return 0;
  } else if (path=="<tmp>") {       // fichier temporaire commun
    if (tmpf) {
      return MyWriteProfileIntFile(tmpf,dummy,name,value);
    }
  } else {
    FILE* fp = fopen(path,"ab");
    if (fp) {
      int r=MyWriteProfileIntFile(fp,dummy,name,value);
      /*
      fprintf(fp,"%s=%d\x0d\x0a",name,value);
      */
      fclose(fp);
      return r;
    }
  }
  return 0;
}
int MyWriteProfileIntFile(FILE* fp,CString dummy,CString name,int value) {
  if (fp) {
    fprintf(fp,"%s=%d\x0d\x0a", name, value);
  }
  return 0;
}
int MyWriteProfileString(CString path,CString dummy,CString name,CString value) {
  if (path.IsEmpty()) {
    CWinApp* pApp = AfxGetApp();
    pApp->WriteProfileString(dummy, name, profile_code(value.GetBuffer(0)));
  } else if (path=="<mem>") {       // buffer
    tmpm.setString(name,value);
    return 0;
  } else if (path=="<tmp>") {       // fichier temporaire commun
    if (tmpf) {
      return MyWriteProfileStringFile(tmpf,dummy,name,value);
    }
  } else {
    FILE* fp = fopen(path,"ab");
    if (fp) {
      int r=MyWriteProfileStringFile(fp,dummy,name,value);
      fclose(fp);
      return r;
    }
  }
  return 0;
}
int MyWriteProfileStringFile(FILE* fp,CString dummy,CString name,CString value) {
  if (fp) {
    fprintf(fp,"%s=%s\x0d\x0a", name, profile_code(value.GetBuffer(0)).GetBuffer(0));
  }
  return 0;
}
//
// lecture
int MyGetProfileInt(CString path,CString dummy,CString name,int value) {
  if (path.IsEmpty()) {
    CWinApp* pApp = AfxGetApp();
    return pApp->GetProfileInt(dummy, name, value);
  } else if (path=="<mem>") {       // buffer
    return tmpm.getInt(name,value);
  } else if (path=="<tmp>") {       // fichier temporaire commun
    if (tmpf) {
      return MyGetProfileIntFile(tmpf,dummy,name,value);
    } else return value;
  } else {
    FILE* fp = fopen(path,"rb");
    if (fp) {
      int r=MyGetProfileIntFile(fp,dummy,name,value);
      fclose(fp);
      return r;
    } else return value;
  }
}
int MyGetProfileIntFile(FILE* fp,CString dummy,CString name,int value) {
  if (fp) {
    char srch[256];
    fseek(fp,0,SEEK_SET);
    sprintf(srch,"%s=",name);
    while(!feof(fp)) {
      char s[2048]; s[0]='\0';
      linput(fp,s,2000);
      if (strlen(s)==0)     // EOF
        return value;
      if (strncmp(s,srch,strlen(srch)) == 0) {    // ligne reconnue
        int val;
        if (sscanf(s+strlen(srch),"%d",&val) == 1)
          return val;
        else
          return value;
      }
    }
    return value;
  } else return value;
}
CString MyGetProfileString(CString path,CString dummy,CString name,CString value) {
  if (path.IsEmpty()) {
    CWinApp* pApp = AfxGetApp();
    return profile_decode(pApp->GetProfileString(dummy, name, profile_code(value.GetBuffer(0)).GetBuffer(0)).GetBuffer(0));
  } else if (path=="<mem>") {       // buffer
    return tmpm.getString(name,value);
  } else if (path=="<tmp>") {       // fichier temporaire commun
    if (tmpf) {
      return MyGetProfileStringFile(tmpf,dummy,name,value);
    } else return value;
  } else {
    FILE* fp = fopen(path,"rb");
    if (fp) {
      CString st=MyGetProfileStringFile(fp,dummy,name,value);
      fclose(fp);
      return st;
    } else return value;
  }
}
CString MyGetProfileStringFile(FILE* fp,CString dummy,CString name,CString value) {
  if (fp) {
    char srch[256];
    fseek(fp,0,SEEK_SET);
    sprintf(srch,"%s",name);
    strcatbuff(srch,"=");
    while(!feof(fp)) {
      char s[32768]; s[0]='\0';
      linput(fp,s,32000);
      if (strlen(s)==0)     // EOF
        return value;
      if (strncmp(s,srch,strlen(srch)) == 0) {    // ligne reconnue
        return profile_decode(s+strlen(srch));
      }
    }
    return value;
  } else return value;
}

//
// Get_profile et Write_profile eux mêmes
//
// path="" -> écrire dans la base (default)
// path="<tmp>" -> écrire dans le fichier tempo commun
// path="<mem>" -> écrire dans le buffer tempo commun
// path="<null>" -> lire default (illégal en écriture)
void Write_profile(CString path,int load_path) {
  CWaitCursor wait;
  CString strSection       = "OptionsValues";
  CString st;
  int n;
  
  // Fichier tempo ou fichier?
  if (path=="<tmp>") {     // fichier temporaire
    if (tmpf)
      fclose(tmpf);
    tmpf=tmpfile();
    if (!tmpf)
      return;
  } else if (path=="<mem>") {     // buffer temporaire
    tmpm.deleteAll();
  } else if (!(path.IsEmpty())) {
    FILE *fp=fopen(path,"wb");
    if (fp) 
      fclose(fp);
  }
  
  //if (dialog3.m_hWnd == NULL) {    // pas initialisé
  if (maintab->m_hWnd == NULL) {    // pas initialisé
    // checkboxes
    MyWriteProfileInt(path,strSection, "Near",maintab->m_option1.m_link);
    MyWriteProfileInt(path,strSection, "Test",maintab->m_option1.m_testall);
    MyWriteProfileInt(path,strSection, "ParseAll",maintab->m_option1.m_parseall);
    MyWriteProfileInt(path,strSection, "HTMLFirst",maintab->m_option1.m_htmlfirst);
    MyWriteProfileInt(path,strSection, "Cache",maintab->m_option3.m_cache);
    MyWriteProfileInt(path,strSection, "NoRecatch",maintab->m_option9.m_norecatch);
    MyWriteProfileInt(path,strSection, "Dos",
      ((maintab->m_option2.m_dos)?1:0)
      +
      (((maintab->m_option2.m_iso9660)?1:0)<<1)
      );
    MyWriteProfileInt(path,strSection, "Index",maintab->m_option9.m_index);
    MyWriteProfileInt(path,strSection, "WordIndex",maintab->m_option9.m_index2);
    MyWriteProfileInt(path,strSection, "MailIndex",maintab->m_option9.m_index_mail);
    MyWriteProfileInt(path,strSection, "Log",maintab->m_option9.m_logf);
    MyWriteProfileInt(path,strSection, "RemoveTimeout",maintab->m_option4.m_remt);
    MyWriteProfileInt(path,strSection, "RemoveRateout",maintab->m_option4.m_rems);
    MyWriteProfileInt(path,strSection, "KeepAlive",maintab->m_option4.m_ka);
    MyWriteProfileInt(path,strSection, "FollowRobotsTxt",maintab->m_option8.m_robots);
    MyWriteProfileInt(path,strSection, "NoErrorPages",maintab->m_option2.m_errpage);
    MyWriteProfileInt(path,strSection, "NoExternalPages",maintab->m_option2.m_external);
    MyWriteProfileInt(path,strSection, "NoPwdInPages",maintab->m_option2.m_hidepwd);
    MyWriteProfileInt(path,strSection, "NoQueryStrings",maintab->m_option2.m_hidequery);
    MyWriteProfileInt(path,strSection, "NoPurgeOldFiles",maintab->m_option2.m_nopurge);
    MyWriteProfileInt(path,strSection, "Cookies",maintab->m_option8.m_cookies);
    MyWriteProfileInt(path,strSection, "CheckType",maintab->m_option8.m_checktype);
    MyWriteProfileInt(path,strSection, "ParseJava",maintab->m_option8.m_parsejava);
    MyWriteProfileInt(path,strSection, "HTTP10",maintab->m_option8.m_http10);
    MyWriteProfileInt(path,strSection, "TolerantRequests",maintab->m_option8.m_toler);
    MyWriteProfileInt(path,strSection, "UpdateHack",maintab->m_option8.m_updhack);
    MyWriteProfileInt(path,strSection, "URLHack",maintab->m_option8.m_urlhack);
    MyWriteProfileInt(path,strSection, "StoreAllInCache",maintab->m_option9.m_Cache2);
    MyWriteProfileInt(path,strSection, "LogType",maintab->m_option9.m_logtype);
    MyWriteProfileInt(path,strSection, "UseHTTPProxyForFTP",maintab->m_option10.m_ftpprox);
    
    // menus
    MyWriteProfileInt(path,strSection, "Build",maintab->m_option2.m_build);
    MyWriteProfileInt(path,strSection, "PrimaryScan",maintab->m_option3.m_filter);
    MyWriteProfileInt(path,strSection, "Travel",maintab->m_option3.m_travel);
    MyWriteProfileInt(path,strSection, "GlobalTravel",maintab->m_option3.m_travel2);
    MyWriteProfileInt(path,strSection, "RewriteLinks",maintab->m_option3.m_travel3);
    MyWriteProfileString(path,strSection, "BuildString",maintab->m_option2.Bopt.m_BuildString);
    
    // champs
    MyWriteProfileString(path,strSection, "Category", this_CSplitterFrame->GetCurrentCategory());

    MyWriteProfileString(path,strSection, "MaxHtml",maintab->m_option5.m_maxhtml);
    MyWriteProfileString(path,strSection, "MaxOther",maintab->m_option5.m_othermax);
    MyWriteProfileString(path,strSection, "MaxAll",maintab->m_option5.m_sizemax);
    MyWriteProfileString(path,strSection, "MaxWait",maintab->m_option5.m_pausebytes);
    MyWriteProfileString(path,strSection, "Sockets",maintab->m_option4.m_connexion);
    MyWriteProfileString(path,strSection, "Retry",maintab->m_option4.m_retry);
    MyWriteProfileString(path,strSection, "MaxTime",maintab->m_option5.m_maxtime);
    MyWriteProfileString(path,strSection, "TimeOut",maintab->m_option4.m_timeout);
    MyWriteProfileString(path,strSection, "RateOut",maintab->m_option4.m_rate);
    MyWriteProfileString(path,strSection, "UserID",maintab->m_option6.m_user);
    MyWriteProfileString(path,strSection, "Footer",maintab->m_option6.m_footer);
    MyWriteProfileString(path,strSection, "AcceptLanguage",maintab->m_option6.m_accept_language);
    MyWriteProfileString(path,strSection, "OtherHeaders",maintab->m_option6.m_other_headers);
    MyWriteProfileString(path,strSection, "DefaultReferer",maintab->m_option6.m_default_referer);
    MyWriteProfileString(path,strSection, "MaxRate",maintab->m_option5.m_maxrate);
    MyWriteProfileString(path,strSection, "WildCardFilters",maintab->m_option7.m_url2);
    MyWriteProfileString(path,strSection, "Proxy",maintab->m_option10.m_proxy);
    MyWriteProfileString(path,strSection, "Port",maintab->m_option10.m_port);
    MyWriteProfileString(path,strSection, "Depth",maintab->m_option5.m_depth);
    MyWriteProfileString(path,strSection, "ExtDepth",maintab->m_option5.m_depth2);
    MyWriteProfileString(path,strSection, "MaxConn",maintab->m_option5.m_maxconn);    
    MyWriteProfileString(path,strSection, "MaxLinks",maintab->m_option5.m_maxlinks);    
    
    // 11
    MyWriteProfileString(path,strSection, "MIMEDefsExt1",maintab->m_option11.m_ext1);    
    MyWriteProfileString(path,strSection, "MIMEDefsExt2",maintab->m_option11.m_ext2);    
    MyWriteProfileString(path,strSection, "MIMEDefsExt3",maintab->m_option11.m_ext3);    
    MyWriteProfileString(path,strSection, "MIMEDefsExt4",maintab->m_option11.m_ext4);    
    MyWriteProfileString(path,strSection, "MIMEDefsExt5",maintab->m_option11.m_ext5);    
    MyWriteProfileString(path,strSection, "MIMEDefsExt6",maintab->m_option11.m_ext6);    
    MyWriteProfileString(path,strSection, "MIMEDefsExt7",maintab->m_option11.m_ext7);    
    MyWriteProfileString(path,strSection, "MIMEDefsExt8",maintab->m_option11.m_ext8);    
    MyWriteProfileString(path,strSection, "MIMEDefsMime1",maintab->m_option11.m_mime1);    
    MyWriteProfileString(path,strSection, "MIMEDefsMime2",maintab->m_option11.m_mime2);    
    MyWriteProfileString(path,strSection, "MIMEDefsMime3",maintab->m_option11.m_mime3);    
    MyWriteProfileString(path,strSection, "MIMEDefsMime4",maintab->m_option11.m_mime4);    
    MyWriteProfileString(path,strSection, "MIMEDefsMime5",maintab->m_option11.m_mime5);    
    MyWriteProfileString(path,strSection, "MIMEDefsMime6",maintab->m_option11.m_mime6);    
    MyWriteProfileString(path,strSection, "MIMEDefsMime7",maintab->m_option11.m_mime7);    
    MyWriteProfileString(path,strSection, "MIMEDefsMime8",maintab->m_option11.m_mime8);
  } else {
    st = this_CSplitterFrame->GetCurrentCategory(); MyWriteProfileString(path,strSection,"Category",st);

    // checkboxes
    // 1
    n=maintab->m_option1.IsDlgButtonChecked(IDC_link);
    MyWriteProfileInt(path,strSection,"Near", n);
    n=maintab->m_option1.IsDlgButtonChecked(IDC_testall);
    MyWriteProfileInt(path,strSection,"Test", n);
    n=maintab->m_option1.IsDlgButtonChecked(IDC_parseall);
    MyWriteProfileInt(path,strSection,"ParseAll", n);
    n=maintab->m_option1.IsDlgButtonChecked(IDC_htmlfirst);
    MyWriteProfileInt(path,strSection,"HTMLFirst", n);
    // 2
    n=maintab->m_option3.IsDlgButtonChecked(IDC_Cache);
    MyWriteProfileInt(path,strSection,"Cache", n);
    n=maintab->m_option2.IsDlgButtonChecked(IDC_norecatch);
    MyWriteProfileInt(path,strSection,"NoRecatch", n);
    n = ((maintab->m_option2.IsDlgButtonChecked(IDC_dos))?1:0)
      + ((maintab->m_option2.IsDlgButtonChecked(IDC_iso9660)?1:0)<<1);
    MyWriteProfileInt(path,strSection,"Dos", n);
    n=maintab->m_option2.IsDlgButtonChecked(IDC_index);
    MyWriteProfileInt(path,strSection,"Index", n);
    n=maintab->m_option2.IsDlgButtonChecked(IDC_index2);
    MyWriteProfileInt(path,strSection,"WordIndex", n);
    n=maintab->m_option2.IsDlgButtonChecked(IDC_index_mail);
    MyWriteProfileInt(path,strSection,"MailIndex", n);
    n=maintab->m_option2.IsDlgButtonChecked(IDC_logf);
    MyWriteProfileInt(path,strSection,"Log", n);
    n=maintab->m_option2.IsDlgButtonChecked(IDC_errpage);
    MyWriteProfileInt(path,strSection,"NoErrorPages", n);
    n=maintab->m_option2.IsDlgButtonChecked(IDC_hidepwd);
    MyWriteProfileInt(path,strSection,"NoPwdInPages", n);
    n=maintab->m_option2.IsDlgButtonChecked(IDC_hidequery);
    MyWriteProfileInt(path,strSection,"NoQueryStrings", n);
    n=maintab->m_option2.IsDlgButtonChecked(IDC_external);
    MyWriteProfileInt(path,strSection,"NoExternalPages", n);
    n=maintab->m_option2.IsDlgButtonChecked(IDC_nopurge);
    MyWriteProfileInt(path,strSection,"NoPurgeOldFiles", n);
    if ((n=maintab->m_option2.m_ctl_build.GetCurSel()) != CB_ERR)
      MyWriteProfileInt(path,strSection, "Build", n);
    st = maintab->m_option2.Bopt.m_BuildString;
    if (st.GetLength()>0)
      MyWriteProfileString(path,strSection, "BuildString",st);
    // 3
    // menus
    if ((n=maintab->m_option3.m_ctl_filter.GetCurSel()) != CB_ERR)
      MyWriteProfileInt(path,strSection, "PrimaryScan", n);
    if ((n=maintab->m_option3.m_ctl_travel.GetCurSel()) != CB_ERR)
      MyWriteProfileInt(path,strSection, "Travel", n);
    if ((n=maintab->m_option3.m_ctl_travel2.GetCurSel()) != CB_ERR)
      MyWriteProfileInt(path,strSection, "GlobalTravel", n);
    if ((n=maintab->m_option3.m_ctl_travel3.GetCurSel()) != CB_ERR)
      MyWriteProfileInt(path,strSection, "RewriteLinks", n);
    //
    maintab->m_option8.GetDlgItemText(IDC_robots,st);
    MyWriteProfileString(path,strSection, "FollowRobotsTxt", st);
    // 4
    maintab->m_option4.GetDlgItemText(IDC_connexion,st);
    MyWriteProfileString(path,strSection, "Sockets", st);
    maintab->m_option4.GetDlgItemText(IDC_timeout,st);
    MyWriteProfileString(path,strSection, "TimeOut", st);
    n=maintab->m_option4.IsDlgButtonChecked(IDC_remt);
    MyWriteProfileInt(path,strSection,"RemoveTimeout", n);
    maintab->m_option4.GetDlgItemText(IDC_retry,st);
    MyWriteProfileString(path,strSection, "Retry", st);
    maintab->m_option4.GetDlgItemText(IDC_rate,st);
    MyWriteProfileString(path,strSection, "RateOut", st);
    n=maintab->m_option4.IsDlgButtonChecked(IDC_rems);
    MyWriteProfileInt(path,strSection,"RemoveRateout", n);
    n=maintab->m_option4.IsDlgButtonChecked(IDC_ka);
    MyWriteProfileInt(path,strSection,"KeepAlive", n);
    // 5
    maintab->m_option5.GetDlgItemText(IDC_maxhtml,st);
    MyWriteProfileString(path,strSection, "MaxHtml", st);
    maintab->m_option5.GetDlgItemText(IDC_othermax,st);
    MyWriteProfileString(path,strSection, "MaxOther", st);
    maintab->m_option5.GetDlgItemText(IDC_sizemax,st);
    MyWriteProfileString(path,strSection, "MaxAll", st);
    maintab->m_option5.GetDlgItemText(IDC_pausebytes,st);
    MyWriteProfileString(path,strSection, "MaxWait", st);
    maintab->m_option5.GetDlgItemText(IDC_maxtime,st);
    MyWriteProfileString(path,strSection, "MaxTime", st);
    maintab->m_option5.GetDlgItemText(IDC_maxrate,st);
    MyWriteProfileString(path,strSection, "MaxRate", st);
    maintab->m_option5.GetDlgItemText(IDC_depth,st);
    MyWriteProfileString(path,strSection,"Depth",st);
    maintab->m_option5.GetDlgItemText(IDC_depth2,st);
    MyWriteProfileString(path,strSection,"ExtDepth",st);
    maintab->m_option5.GetDlgItemText(IDC_maxconn,st);
    MyWriteProfileString(path,strSection, "MaxConn", st);
    maintab->m_option5.GetDlgItemText(IDC_maxlinks,st);
    MyWriteProfileString(path,strSection, "MaxLinks", st);
    // 6
    maintab->m_option6.GetDlgItemText(IDC_user,st);
    MyWriteProfileString(path,strSection, "UserID", st);
    maintab->m_option6.GetDlgItemText(IDC_footer,st);
    MyWriteProfileString(path,strSection, "Footer", st);
    maintab->m_option6.GetDlgItemText(IDC_accept_language, st);
    MyWriteProfileString(path,strSection, "AcceptLanguage", st);
    maintab->m_option6.GetDlgItemText(IDC_other_headers, st);
    // FIXME SHOULD BE MULTILINE!!
    st.Replace("\n", " ");
    MyWriteProfileString(path,strSection, "OtherHeaders", st);
    maintab->m_option6.GetDlgItemText(IDC_default_referer, st);
    MyWriteProfileString(path,strSection, "DefaultReferer", st);
    // 7
    maintab->m_option7.GetDlgItemText(IDC_URL2,st);
    MyWriteProfileString(path,strSection, "WildCardFilters", st);
    // 8
    maintab->m_option8.GetDlgItemText(IDC_cookies,st);
    MyWriteProfileString(path,strSection, "Cookies", st);
    maintab->m_option8.GetDlgItemText(IDC_checktype,st);
    MyWriteProfileString(path,strSection, "CheckType", st);
    n=maintab->m_option8.IsDlgButtonChecked(IDC_parsejava);
    MyWriteProfileInt(path,strSection, "ParseJava", n);
    n=maintab->m_option8.IsDlgButtonChecked(IDC_http10);
    MyWriteProfileInt(path,strSection, "HTTP10", n);
    n=maintab->m_option8.IsDlgButtonChecked(IDC_toler);
    MyWriteProfileInt(path,strSection, "TolerantRequests", n);
    n=maintab->m_option8.IsDlgButtonChecked(IDC_updhack);
    MyWriteProfileInt(path,strSection, "UpdateHack", n);
    n=maintab->m_option8.IsDlgButtonChecked(IDC_urlhack);
    MyWriteProfileInt(path,strSection, "URLHack", n);
    // 9
    maintab->m_option9.GetDlgItemText(IDC_Cache2,st);
    MyWriteProfileString(path,strSection, "StoreAllInCache", st);
    maintab->m_option9.GetDlgItemText(IDC_logtype,st);
    MyWriteProfileString(path,strSection, "LogType", st);
    // 10
    maintab->m_option10.GetDlgItemText(IDC_prox,st);
    MyWriteProfileString(path,strSection,"Proxy",st);
    maintab->m_option10.GetDlgItemText(IDC_proxport,st);
    MyWriteProfileString(path,strSection,"Port",st);
    n=maintab->m_option10.IsDlgButtonChecked(IDC_ftpprox);
    MyWriteProfileInt(path,strSection,"UseHTTPProxyForFTP", n);
    
    // 11
    maintab->m_option11.GetDlgItemText(IDC_ext1,st); MyWriteProfileString(path,strSection,"MIMEDefsExt1",st);
    maintab->m_option11.GetDlgItemText(IDC_ext2,st); MyWriteProfileString(path,strSection,"MIMEDefsExt2",st);
    maintab->m_option11.GetDlgItemText(IDC_ext3,st); MyWriteProfileString(path,strSection,"MIMEDefsExt3",st);
    maintab->m_option11.GetDlgItemText(IDC_ext4,st); MyWriteProfileString(path,strSection,"MIMEDefsExt4",st);
    maintab->m_option11.GetDlgItemText(IDC_ext5,st); MyWriteProfileString(path,strSection,"MIMEDefsExt5",st);
    maintab->m_option11.GetDlgItemText(IDC_ext6,st); MyWriteProfileString(path,strSection,"MIMEDefsExt6",st);
    maintab->m_option11.GetDlgItemText(IDC_ext7,st); MyWriteProfileString(path,strSection,"MIMEDefsExt7",st);
    maintab->m_option11.GetDlgItemText(IDC_ext8,st); MyWriteProfileString(path,strSection,"MIMEDefsExt8",st);
    maintab->m_option11.GetDlgItemText(IDC_mime1,st); MyWriteProfileString(path,strSection,"MIMEDefsMime1",st);
    maintab->m_option11.GetDlgItemText(IDC_mime2,st); MyWriteProfileString(path,strSection,"MIMEDefsMime2",st);
    maintab->m_option11.GetDlgItemText(IDC_mime3,st); MyWriteProfileString(path,strSection,"MIMEDefsMime3",st);
    maintab->m_option11.GetDlgItemText(IDC_mime4,st); MyWriteProfileString(path,strSection,"MIMEDefsMime4",st);
    maintab->m_option11.GetDlgItemText(IDC_mime5,st); MyWriteProfileString(path,strSection,"MIMEDefsMime5",st);
    maintab->m_option11.GetDlgItemText(IDC_mime6,st); MyWriteProfileString(path,strSection,"MIMEDefsMime6",st);
    maintab->m_option11.GetDlgItemText(IDC_mime7,st); MyWriteProfileString(path,strSection,"MIMEDefsMime7",st);
    maintab->m_option11.GetDlgItemText(IDC_mime8,st); MyWriteProfileString(path,strSection,"MIMEDefsMime8",st);
  }
  // liens, jokers etc. si mirror merge
  if (!(path.IsEmpty())) {
    if (dialog1->m_hWnd == NULL) {    // pas initialisé
      //MyWriteProfileString(path,strSection,"CurrentDepth",dialog1->m_depth);
      MyWriteProfileString(path,strSection,"CurrentUrl",dialog1->m_urls);
      if (dialog1->m_todo >= 0)
        MyWriteProfileInt(path, strSection,"CurrentAction", dialog1->m_todo);
      //
      MyWriteProfileString(path,strSection,"CurrentURLList",dialog1->m_filelist);
      
      //if (load_path) {
      //MyWriteProfileString(path,strSection,"CurrentPath1",dialog0->GetPath());
      //MyWriteProfileString(path,strSection,"CurrentPath2",dialog0->GetPath());
      //}
    } else {
      //dialog1->GetDlgItemText(IDC_depth,st);
      //MyWriteProfileString(path,strSection,"CurrentDepth",st);
      dialog1->GetDlgItemText(IDC_URL,st);
      MyWriteProfileString(path,strSection,"CurrentUrl",st);
      if ((n=dialog1->m_ctl_todo.GetCurSel()) != CB_ERR)
        MyWriteProfileInt(path,strSection, "CurrentAction", n);
      //
      dialog1->GetDlgItemText(IDC_filelist,st);
      MyWriteProfileString(path,strSection,"CurrentURLList",st);
      //
      //if (load_path) {
      //dialog1->GetDlgItemText(IDC_pathlog,st);
      //MyWriteProfileString(path,strSection,"CurrentPath1",st);
      //dialog1->GetDlgItemText(IDC_pthmir,st);
      //MyWriteProfileString(path,strSection,"CurrentPath2",st);
      //}
    }
  }
}
// path="" -> lire dans la base (default)
//if not exist: do not load anything but patches projname&co
void Read_profile(CString path,int load_path) {
  CWaitCursor wait;
  CString strSection       = "OptionsValues";
  CString st;
  
  // Vérification <tmp>
  if (path=="<tmp>") {     // fichier temporaire
    if (!tmpf)
      return;
    else
      fflush(tmpf);
  } else if (path=="<null>") {     // options par défaut
    path="<mem>";
    tmpm.deleteAll();              // effacer
  } else {
    int pos=path.ReverseFind('\\');
    if (pos>=0) {
      CString dir=path.Left(pos);           // enlever winprofile.ini
      pos=dir.ReverseFind('\\');
      if (pos>=0) {
        dir=dir.Left(pos);                  // enlever hts-cache
        pos=dir.ReverseFind('\\');
        if (pos>=0) {
          dialog0->m_projname=dir.Mid(pos+1);
          dialog0->m_projpath=dir.Left(pos);
          /*
          if (this_CWizTab)
          this_CWizTab->SetActivePage(2);     // page 3
          */
        }
      }
    }
  }

  // Default browser language
  CString default_lang = "";
  if (strnotempty(LANGUAGE_ISO)) {
    default_lang += LANGUAGE_ISO;
    if (strcmp(LANGUAGE_ISO, "en") != 0)
      default_lang += ", en";
    default_lang += ", *";
  } else {
    default_lang = "en, *";
  }
  
  // checkboxes
  maintab->m_option1.m_link      = MyGetProfileInt(path,strSection, "Near",0);
  maintab->m_option1.m_testall   = MyGetProfileInt(path,strSection, "Test",0);
  maintab->m_option1.m_parseall  = MyGetProfileInt(path,strSection, "ParseAll",1);
  maintab->m_option1.m_htmlfirst = MyGetProfileInt(path,strSection, "HTMLFirst",0);
  maintab->m_option3.m_cache     = MyGetProfileInt(path,strSection, "Cache",1);
  maintab->m_option9.m_norecatch = MyGetProfileInt(path,strSection, "NoRecatch",0);
  maintab->m_option2.m_dos       = (MyGetProfileInt(path,strSection, "Dos",0) & 1);
  maintab->m_option2.m_iso9660   = ((MyGetProfileInt(path,strSection, "Dos",0) & 2)>>1);
  maintab->m_option9.m_index     = MyGetProfileInt(path,strSection, "Index",1);
  maintab->m_option9.m_index2    = MyGetProfileInt(path,strSection, "WordIndex",0);
  maintab->m_option9.m_index_mail= MyGetProfileInt(path,strSection, "MailIndex",0);
  maintab->m_option9.m_logf      = MyGetProfileInt(path,strSection, "Log",1);
  maintab->m_option4.m_remt      = MyGetProfileInt(path,strSection, "RemoveTimeout",0);
  maintab->m_option4.m_rems      = MyGetProfileInt(path,strSection, "RemoveRateout",0);
  maintab->m_option4.m_ka        = MyGetProfileInt(path,strSection, "KeepAlive",1);
  maintab->m_option8.m_robots    = MyGetProfileInt(path,strSection, "FollowRobotsTxt",2);
  maintab->m_option2.m_errpage   = MyGetProfileInt(path,strSection, "NoErrorPages",0);
  maintab->m_option2.m_external  = MyGetProfileInt(path,strSection, "NoExternalPages",0);
  maintab->m_option2.m_hidepwd   = MyGetProfileInt(path,strSection, "NoPwdInPages",0);
  maintab->m_option2.m_hidequery = MyGetProfileInt(path,strSection, "NoQueryStrings",0);
  maintab->m_option2.m_nopurge   = MyGetProfileInt(path,strSection, "NoPurgeOldFiles",0);
  maintab->m_option8.m_cookies    = MyGetProfileInt(path,strSection, "Cookies",1);
  maintab->m_option8.m_checktype  = MyGetProfileInt(path,strSection, "CheckType",1);
  maintab->m_option8.m_parsejava  = MyGetProfileInt(path,strSection, "ParseJava",1);
  maintab->m_option8.m_toler      = MyGetProfileInt(path,strSection, "TolerantRequests",0);
  maintab->m_option8.m_updhack    = MyGetProfileInt(path,strSection, "UpdateHack",1);
  maintab->m_option8.m_urlhack    = MyGetProfileInt(path,strSection, "URLHack",1);
  maintab->m_option8.m_http10     = MyGetProfileInt(path,strSection, "HTTP10",0);
  maintab->m_option9.m_Cache2     = MyGetProfileInt(path,strSection, "StoreAllInCache",0);
  maintab->m_option9.m_logtype    = MyGetProfileInt(path,strSection, "LogType",0);
  
  // menus
  maintab->m_option2.m_build   = MyGetProfileInt(path,strSection, "Build",0);
  maintab->m_option3.m_filter  = MyGetProfileInt(path,strSection, "PrimaryScan",3);
  maintab->m_option3.m_travel  = MyGetProfileInt(path,strSection, "Travel",1);
  maintab->m_option3.m_travel2 = MyGetProfileInt(path,strSection, "GlobalTravel",0);
  maintab->m_option3.m_travel3 = MyGetProfileInt(path,strSection, "RewriteLinks",0);
  maintab->m_option2.Bopt.m_BuildString = MyGetProfileString(path,strSection, "BuildString","%h%p/%n%q.%t");
  
  // champs
  dialog0->m_projcateg =          MyGetProfileString(path,strSection, "Category");

  maintab->m_option5.m_maxhtml =  MyGetProfileString(path,strSection, "MaxHtml");
  maintab->m_option5.m_othermax=  MyGetProfileString(path,strSection, "MaxOther");
  maintab->m_option5.m_sizemax =  MyGetProfileString(path,strSection, "MaxAll");
  maintab->m_option5.m_pausebytes=MyGetProfileString(path,strSection, "MaxWait");  
  maintab->m_option4.m_connexion= MyGetProfileString(path,strSection, "Sockets");
  maintab->m_option4.m_retry   =  MyGetProfileString(path,strSection, "Retry");
  maintab->m_option5.m_maxtime =  MyGetProfileString(path,strSection, "MaxTime");
  maintab->m_option4.m_timeout =  MyGetProfileString(path,strSection, "TimeOut");
  maintab->m_option4.m_rate    =  MyGetProfileString(path,strSection, "RateOut");
  maintab->m_option6.m_user    =  MyGetProfileString(path,strSection, "UserID","Mozilla/4.5 (compatible; HTTrack 3.0x; Windows 98)");
  maintab->m_option6.m_footer  =  MyGetProfileString(path,strSection, "Footer",HTS_DEFAULT_FOOTER);
  maintab->m_option6.m_accept_language =
                                  MyGetProfileString(path,strSection, "AcceptLanguage", default_lang);
  maintab->m_option6.m_other_headers =
                                  MyGetProfileString(path,strSection, "OtherHeaders");
  maintab->m_option6.m_default_referer =
                                  MyGetProfileString(path,strSection, "DefaultReferer");
  maintab->m_option5.m_maxrate =  MyGetProfileString(path,strSection, "MaxRate", "25000");
  maintab->m_option5.m_maxconn =  MyGetProfileString(path,strSection, "MaxConn");
  maintab->m_option5.m_maxlinks = MyGetProfileString(path,strSection, "MaxLinks");
  
  // 7
  maintab->m_option7.m_url2 = MyGetProfileString(path,strSection, "WildCardFilters","+*.png +*.gif +*.jpg +*.jpeg +*.css +*.js -ad.doubleclick.net/* -mime:application/foobar");
  
  // 10
  maintab->m_option10.m_proxy   = MyGetProfileString(path,strSection, "Proxy");
  maintab->m_option10.m_port    = MyGetProfileString(path,strSection, "Port");
  maintab->m_option10.m_ftpprox = MyGetProfileInt(path,strSection, "UseHTTPProxyForFTP",1);
  //
  maintab->m_option5.m_depth    = MyGetProfileString(path,strSection, "Depth");
  maintab->m_option5.m_depth2   = MyGetProfileString(path,strSection, "ExtDepth");
  
  // 11
  maintab->m_option11.m_ext1   = MyGetProfileString(path,strSection, "MIMEDefsExt1");   // php3,php,php2,asp,jsp,pl,cfm,nsf
  maintab->m_option11.m_ext2   = MyGetProfileString(path,strSection, "MIMEDefsExt2");
  maintab->m_option11.m_ext3   = MyGetProfileString(path,strSection, "MIMEDefsExt3");
  maintab->m_option11.m_ext4   = MyGetProfileString(path,strSection, "MIMEDefsExt4");
  maintab->m_option11.m_ext5   = MyGetProfileString(path,strSection, "MIMEDefsExt5");
  maintab->m_option11.m_ext6   = MyGetProfileString(path,strSection, "MIMEDefsExt6");
  maintab->m_option11.m_ext7   = MyGetProfileString(path,strSection, "MIMEDefsExt7");
  maintab->m_option11.m_ext8   = MyGetProfileString(path,strSection, "MIMEDefsExt8");
  maintab->m_option11.m_mime1   = MyGetProfileString(path,strSection, "MIMEDefsMime1");   // text/html
  maintab->m_option11.m_mime2   = MyGetProfileString(path,strSection, "MIMEDefsMime2");
  maintab->m_option11.m_mime3   = MyGetProfileString(path,strSection, "MIMEDefsMime3");
  maintab->m_option11.m_mime4   = MyGetProfileString(path,strSection, "MIMEDefsMime4");
  maintab->m_option11.m_mime5   = MyGetProfileString(path,strSection, "MIMEDefsMime5");
  maintab->m_option11.m_mime6   = MyGetProfileString(path,strSection, "MIMEDefsMime6");
  maintab->m_option11.m_mime7   = MyGetProfileString(path,strSection, "MIMEDefsMime7");
  maintab->m_option11.m_mime8   = MyGetProfileString(path,strSection, "MIMEDefsMime8");
  
  
  //st = MyGetProfileString(path,strSection,"WildCardFilters");
  //ShellOptions->buff_filtres = st;
  
  // liens, jokers etc. si mirror merge
  if (!(path.IsEmpty())) {
    if (dialog1->m_hWnd == NULL) {    // pas initialisé
      //dialog1->m_depth  = MyGetProfileString(path,strSection,"CurrentDepth");
      dialog1->m_urls     = MyGetProfileString(path,strSection,"CurrentUrl");
      dialog1->m_todo     = MyGetProfileInt(path,strSection,"CurrentAction",0);
      dialog1->m_filelist = MyGetProfileString(path,strSection,"CurrentURLList");
      /*
      if (load_path) {
      CString st;
      st = MyGetProfileString(path,strSection,"CurrentPath1");
      if (st != "")
      dialog1->m_pathlog = st;
      st = MyGetProfileString(path,strSection,"CurrentPath2");
      if (st != "")
      dialog1->m_pathmir = st;
      }
      */
    } else {
      //st = MyGetProfileString(path,strSection,"CurrentDepth");
      //dialog1->SetDlgItemText(IDC_depth,st);
      st = MyGetProfileString(path,strSection,"CurrentUrl");
      SetDlgItemTextCP(dialog1, IDC_URL,st);
      int n = MyGetProfileInt(path,strSection,"CurrentAction",0);
      dialog1->m_ctl_todo.SetCurSel(n);
      st = MyGetProfileString(path,strSection,"CurrentURLList");
      SetDlgItemTextCP(dialog1, IDC_filelist,st);
      /*
      if (load_path) {
      st = MyGetProfileString(path,strSection,"CurrentPath1");
      if (st != "")
      dialog1->SetDlgItemText(IDC_pathlog,st);
      st = MyGetProfileString(path,strSection,"CurrentPath2");
      if (st != "")
      dialog1->SetDlgItemText(IDC_pthmir,st);
      }
      */
      dialog1->Refresh();
    }
  }
  
}

// Initialisation du RAS
void InitRAS() {
#if USE_RAS
  if (!LibRas) {
    LibRas=new CDynamicRAS();
    if (LibRas->IsRASLoaded()) 
      LibRasUse=1;
    else
      LibRasUse=0;
  }
#endif
}

// Reconstruire index général!
void Build_TopIndex(BOOL check_empty) {
  CWaitCursor wait;

  //if (toptemplate_header && toptemplate_body && toptemplate_footer) {
  {
    TStamp t_start = mtime_local();
    char path[HTS_URLMAXSIZE*2];
    strcpybuff(path,CShellApp_app->end_path);

    /* Build top index */
    httrackp *opt = hts_create_opt();
    TStamp t_opt = mtime_local();
    opt->log = opt->errlog = NULL;
    hts_buildtopindex(opt,path, "");
    hts_free_opt(opt);
    TStamp t_build = mtime_local();

    /* Check empty dirs and build .whtt */
    // FILE* fpo=fopen(fconcat(path,"/index.html"),"wb");
    //if (fpo) {
    {
      // verif_backblue(opt, path);    // générer gif
      //
      // Header
      //fprintf(fpo,toptemplate_header,
      //  "<!-- Mirror and index made by HTTrack Website Copier/"HTTRACK_VERSION" "HTTRACK_AFF_AUTHORS" -->"
      //  );
      // chargement de la liste
      WIN32_FIND_DATA find;
      HANDLE h = FindFirstFile(CShellApp_app->end_path+"*.*",&find);
      if (h != INVALID_HANDLE_VALUE) {
        CString to_delete="";
        do {
          if (!(find.dwFileAttributes  & (FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_HIDDEN) ))
            if (strcmp(find.cFileName,".."))
              if (strcmp(find.cFileName,"."))
                if (find.dwFileAttributes  & FILE_ATTRIBUTE_DIRECTORY ) {
                  int r;
                  r=CheckDirInfo(CShellApp_app->end_path+find.cFileName);
                  if ((r>=0) && (r<=3)) {       /* vide, fichiers vides etc */
                    to_delete+=(CShellApp_app->end_path+find.cFileName+"\n");
                  } else {      /* non vide */
                    char iname[HTS_URLMAXSIZE*2],iname2[HTS_URLMAXSIZE*2];
                    strcpybuff(iname,CShellApp_app->end_path);
                    strcatbuff(iname,find.cFileName);
                    strcatbuff(iname,"\\");
                    strcpybuff(iname2,iname);
                    strcatbuff(iname,"index.html");
                    strcatbuff(iname2,"\\hts-cache\\winprofile.ini");
                    if (fexist(iname)) {
                      char hname[HTS_URLMAXSIZE*2];
                      escape_check_url(find.cFileName, hname, sizeof(hname));
                      // Body
                      //fprintf(fpo,toptemplate_body,
                      //  hname,
                      //  find.cFileName
                      //  );
                    }
                    
                    if ((fexist(iname)) || (fexist(iname2)) ) {
                      // vérifier existence de .whtt
                      strcpybuff(iname,CShellApp_app->end_path);
                      strcatbuff(iname,find.cFileName);
                      strcatbuff(iname,".whtt");
                      if (!fexist(iname)) {
                        FILE* fp=fopen(iname,"wb");
                        if (fp) fclose(fp);
                      }
                      
                    }
                  }
                } else {    /* effacer .whtt qui ne pointent vers rien */
                  CString iname=CShellApp_app->end_path+find.cFileName;
                  if (iname.Right(5).CompareNoCase(".whtt")==0) {
                    CString rname=iname.Left(iname.GetLength()-5)+"\\index.html";
                    CString rname2=iname.Left(iname.GetLength()-5)+"\\hts-cache\\winprofile.ini";
                    if ( (!fexist((char*)LPCTSTR(rname))) && (!fexist((char*)LPCTSTR(rname2))) ) {
                      remove(iname);
                    }
                  }
                }
        } while(FindNextFile(h,&find));
        FindClose(h);
        if (check_empty) {
          while(to_delete.GetLength()>0) {
            int pos=to_delete.Find('\n');
            if (pos) {
              CString path=to_delete.Left(pos);
              to_delete=to_delete.Mid(pos+1);
              CString str;
              str.Format(LANG_DELETEEMPTYCONF,path);
              if (AfxMessageBox(str,MB_OKCANCEL)==IDOK) {
                /* éliminer au besoin le .whtt */
                DeleteFile(path+".whtt");
                if (!RemoveEmptyDir(path))
                  AfxMessageBox(LANG_ERRORDEL);
              }
            } else
              to_delete="";
          }
        }
      }
      // Footer
      //fprintf(fpo,toptemplate_footer,
      //  "<!-- Mirror and index made by HTTrack Website Copier/"HTTRACK_VERSION" "HTTRACK_AFF_AUTHORS" -->"
      //  );
      //fclose(fpo);
    }

    TStamp t_end = mtime_local();
    TStamp l_opt = t_opt - t_start;
    TStamp l_build = t_build - t_opt;
    TStamp l_dir = t_end - t_build;
    //CString a;
    //a.Format("opt=%dms build=%dms dir=%dms", (int)l_opt, (int)l_build, (int)l_dir);
    //(void) AfxMessageBox(a, MB_OK);
  }

  
  /*
  if (toptemplate_header)
    freet(toptemplate_header);
  if (toptemplate_body)
    freet(toptemplate_body);
  if (toptemplate_footer)
    freet(toptemplate_footer);
  */
}

/*
-1 : error
0 : directory is empty
1 : empty directories inside
2 : empty files inside
3 : 1 + 2
4 : not empty
*/
int CheckDirInfo(CString path) {
  CWaitCursor wait;
  
  if (path.Right(1)!="\\")
    path+="\\";
  
  // Pour FindFirstFile/FindNextFile
  WIN32_FIND_DATA find;
  HANDLE h = FindFirstFile(path+"*.*",&find);
  
  // accessible
  if (h!=INVALID_HANDLE_VALUE) {
    int return_code=0;
    do {
      if (!(find.dwFileAttributes  & FILE_ATTRIBUTE_SYSTEM )) {
        if (strcmp(find.cFileName,"..")) {
          if (strcmp(find.cFileName,".")) {
            if (find.dwFileAttributes  & FILE_ATTRIBUTE_DIRECTORY ) {
              return_code|=1;
              int r=CheckDirInfo(path+find.cFileName);
              if (r==4)
                return_code=4;
              else
                return_code|=r;
            } else {
              return_code|=2;
              if ((find.nFileSizeLow) || (find.nFileSizeHigh))
                return_code=4;
            }
          }
        }
      }
    } while(FindNextFile(h,&find) && !((return_code & 4)) );
    FindClose(h);
    return return_code;
  } else
    return -1;
}

/*
Remove Empty Dir
*/
BOOL RemoveEmptyDir(CString path) {
  CWaitCursor wait;
  
  SetCurrentDirectory("C:\\");
  
  if (path.Right(1)!="\\")
    path+="\\";
  
  // Pour FindFirstFile/FindNextFile
  WIN32_FIND_DATA find;
  HANDLE h = FindFirstFile(path+"*.*",&find);
  
  // accessible
  if (h!=INVALID_HANDLE_VALUE) {
    do {
      if (!(find.dwFileAttributes  & FILE_ATTRIBUTE_SYSTEM )) {
        if (strcmp(find.cFileName,"..")) {
          if (strcmp(find.cFileName,".")) {
            if (find.dwFileAttributes  & FILE_ATTRIBUTE_DIRECTORY ) {
              if (!RemoveEmptyDir(path+find.cFileName)) {
                FindClose(h);
                return 0;
              }
            } else {
              if ((!find.nFileSizeLow) && (!find.nFileSizeHigh))
                if (!DeleteFile(path+find.cFileName)) {
                  FindClose(h);
                  return 0;
                }
            }
          }
        }
      }
    } while(FindNextFile(h,&find));
    FindClose(h);
    
    SetCurrentDirectory("C:\\");
    return RemoveDirectory(path.Left(path.GetLength()-1));
  } else
    return 0;
  return TRUE;
}
