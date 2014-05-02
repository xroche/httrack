// Wid1.cpp : implementation file
//

#include "stdafx.h"
#include <afxole.h>
#include "oledlg.h"
#include "afxodlgs.h"

#include "Shell.h"
#include "Wid1.h"

#include "NewProj.h"
#include "XSHBrowseForFolder.h"

#include <WS2tcpip.h>  // Note: weird C2894 error if not included here
extern "C" {
  #include "HTTrackInterface.h"
}

#include "about.h"
#include "InsertUrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern CNewProj* dialog0;

extern int binput(char* buff,char* s,int max);

extern int check_continue(char* path_log);
extern int cmdl_opt(char* s);
extern void Write_profile(CString path,int load_path);
extern void Read_profile(CString path,int load_path);
extern CShellApp* CShellApp_app;

// Helper
extern LaunchHelp* HtsHelper;

// reference sur objet
extern Wid1* dialog1;

/* Main WizTab frame */
#include "WizTab.h"
extern CWizTab* this_CWizTab;

/* Main splitter frame */
#include "DialogContainer.h"
#include "splitter.h"
extern CSplitterFrame* this_CSplitterFrame;

/* Back to FirstInfo */
//#include "FirstInfo.h"

// pour lire ini qu'une fois
int first_time=1;

/////////////////////////////////////////////////////////////////////////////
// Wid1 dialog


/*class FileSelPath : public CFileDialog {
public:
  BOOL OnFileNameOK( ) { return 0; }
  FileSelPath(BOOL a):CFileDialog(a) { };
};*/

IMPLEMENT_DYNCREATE(Wid1, CPropertyPage)

//Cfilter filter;
CString change(CString,char);
 
Wid1::Wid1()
	: CPropertyPage(Wid1::IDD)
{
  dialog1=this;    /* NOTER REFERENCE */
  cancel=0;
	//{{AFX_DATA_INIT(Wid1)
	m_C1 = -1;
	m_urls = _T("");
  m_todo = 0;
	m_infomain = _T("");
	m_filelist = _T("");
	//}}AFX_DATA_INIT
}

Wid1::~Wid1() {
  dialog1=NULL;
}

void Wid1::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(Wid1)
	DDX_Control(pDX, IDC_todo, m_ctl_todo);
	DDX_Text(pDX, IDC_URL, m_urls);
	DDX_CBIndex(pDX, IDC_todo, m_todo);
	DDX_Text(pDX, IDC_INFOMAIN, m_infomain);
	DDX_Text(pDX, IDC_filelist, m_filelist);
	//}}AFX_DATA_MAP
}

//IMPLEMENT_DYNAMIC(Wid1, CPropertyPage)

#define wm_CEasyDropTargetCallback (WM_USER + 1)
BEGIN_MESSAGE_MAP(Wid1, CPropertyPage)
	//{{AFX_MSG_MAP(Wid1)
	ON_EN_CHANGE(IDC_URL, OnChangeUrl)
	ON_CBN_SELCHANGE(IDC_todo, OnSelchangetodo)
	ON_WM_CREATE()
	ON_WM_HELPINFO()
	ON_BN_CLICKED(ID_setopt, Onsetopt)
	ON_BN_CLICKED(IDC_login2, Onlogin2)
	ON_BN_CLICKED(IDC_br, Onbr)
  ON_WM_DROPFILES()
	ON_WM_DRAWITEM()
	ON_WM_SHOWWINDOW()
	ON_EN_CHANGE(IDC_filelist, OnChangefilelist)
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
	ON_COMMAND(ID_HELP,OnHelpInfo2)
	//ON_COMMAND(ID_CONTEXT_HELP,OnContextHelp)
	ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
  //
  ON_BN_CLICKED(ID_LOAD_OPTIONS,OnLoadprofile)
  ON_BN_CLICKED(ID_FILE_SAVE_OPTIONS_AS,OnSaveprofile)
	ON_BN_CLICKED(ID_LoadDefaultOptions, OnLoaddefault)
	ON_BN_CLICKED(ID_SaveDefaultOptions, OnSavedefault)
	ON_BN_CLICKED(ID_ClearDefaultOptions,OnResetdefault)
	//ON_BN_CLICKED(ID_NewProjectImport, OnNewProject)
	ON_BN_CLICKED(ID_SaveProject, OnSaveProject)
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
  //
  ON_MESSAGE( wm_CEasyDropTargetCallback, DragDropText)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Wid1 message handlers

BOOL Wid1::OnInitDialog( ) {
  CPropertyPage::OnInitDialog();
  EnableToolTips(true);     // TOOL TIPS

  // inits après affichage
  url_status=-1;
  filelist_status=-1;
  //prox_status=-1;
  log_flip=-1;
  mir_status=-1;
  proj_status=-1;
  continue_status=-1;
  OnChangeUrl();
  OnChangefilelist();
  //OnChangeprox();  // update disabled/normal
  //OnSelchangedepth();  // update disabled/normal
  OnSelchangetodo();

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    //SetDlgItemTextCP(this, ,"");
    SetWindowTextCP(this,  LANG(LANG_G30)); // "Bienvenue dans WinHTTrack!");
    SetDlgItemTextCP(this, IDC_STATIC_action,LANG(LANG_G31)); // "Action:");
    //SetDlgItemTextCP(this, IDC_STATIC_filters,LANG(LANG_G33)); // "Filtres (refuser/accepter liens) :");
    SetDlgItemTextCP(this, IDC_STATIC_filters2,LANG(LANG_G41)); // "Options & prefs :");
    SetDlgItemTextCP(this, IDC_STATIC_paths,LANG(LANG_G34)); // "Chemins");
    //SetDlgItemTextCP(this, IDC_regdef,LANG(LANG_G37)); // "Sauver préfs");
    //SetDlgItemTextCP(this, IDOK,LANG(LANG_NEXT )); // "SUIVANT ->");
    //SetDlgItemTextCP(this, IDC_avant,LANG(LANG_BACK));
    //SetDlgItemTextCP(this, IDCANCEL,LANG(LANG_QUIT));  // exit 
    //SetDlgItemTextCP(this, IDfilter,LANG(LANG_G39)); // "Définir..");
    SetDlgItemTextCP(this, ID_setopt,LANG(LANG_G40)); // "Définir les options..");
    //SetDlgItemTextCP(this, IDC_mirtitle2,LANG(LANG_G42)); // "Nom du projet");
    SetDlgItemTextCP(this, IDC_login2,LANG_G43);
    //SetDlgItemTextCP(this, IDC_urls,LANG_G44);
    SetDlgItemTextCP(this, IDC_STATIC_webaddr,LANG_G44);
    SetCombo(this,IDC_todo,LISTDEF_10);
  }

  // inits
  LAST_ACTION = m_ctl_todo.GetCount()-1;  // dernier item (update)

  // fichier ini
  // lire default
  if (first_time) {
    first_time=0;

    //SetDlgItemTextCP(this, IDC_INFOMAIN,"Please select an action, and fill the addresse(s) in the URL box.\x0d\x0aThe mirror will be saved in the location indicated below");

    /*
    CString strSection       = "DefaultValues";    
    CWinApp* pApp = AfxGetApp();
    CString st;
    int n;
    */

    /*
    if ((n = pApp->GetProfileInt(strSection, "Action",CB_ERR)) != CB_ERR)
      m_ctl_todo.SetCurSel(n);

    if ((st = pApp->GetProfileString(strSection, "Depth")))
      SetDlgItemTextCP(this, IDC_depth,st);    
    */

    /*
    if ((st = pApp->GetProfileString(strSection, "MirrorPath","default")) != (CString) "default")
      SetDlgItemTextCP(this, IDC_pthmir,st);
    if ((st = pApp->GetProfileString(strSection, "LogPath","default")) != (CString) "default")
      SetDlgItemTextCP(this, IDC_pathlog,st);
    */

    // Infos la 1ere fois!
    /*if (!pApp->GetProfileInt("Interface","FirstRun",0)) {
      pApp->WriteProfileInt("Interface","FirstRun",1);
      Onipabout();
    }*/

  } else {
    log_flip=0;
  }
  // fichier ini  

  //OnChangepathlog();  // update répertoire log
  //OnChangepthmir();  // update répertoire miroir

  return TRUE;
}

void Wid1::AfterInitDialog( ) {
  WHTT_LOCATION("Wid1");
  if (load_after_changes) {
    AfterChangepathlog();
    OnSelchangetodo();
    load_after_changes=0;
  }
  OnChangeUrl();
  OnChangefilelist();
}

/*
void Wid1::Onurls() {
  CleanUrls();
}
*/

// nettoyer les URLs
void Wid1::CleanUrls() 
{
  CWaitCursor wait;
  char* tempo, *ch,*str;
  CString	st="";
  // TODO: Add your control notification handler code here
  
  GetDlgItemText(IDC_URL,st);
  tempo=(char*) malloc(st.GetLength()+1);
  ch=(char*) malloc(st.GetLength()+1);
  str=(char*) malloc(st.GetLength()*2+8192);
  tempo[0]=ch[0]=str[0]='\0';
  if ( (tempo) && (ch) && (str) ) {
    strcpybuff(tempo,st);
    int i;
    for(i=0;i < (int) strlen(tempo);i++)  {
      if(tempo[i]==10) tempo[i]=' ';
      else if(tempo[i]==13) tempo[i]=' ';
    }
    
    strcpybuff(ch,"");
    int j=0;
    for(i=0;i <= (int) strlen(tempo);i++) {
      if ((tempo[i]==' ') || (tempo[i]==0)) {
        ch[j++]='\0';
        if ((strlen(ch)>0) && ((int) strlen(ch)<HTS_URLMAXSIZE) ) {
          // vérifier URL
          /*
          char adr[HTS_URLMAXSIZE*2],fil[HTS_URLMAXSIZE*2];
          adr[0]=fil[0]='\0';
          if (ident_url_absolute(ch,adr,fil)==-1) {
            htsblk r;
            char loc[HTS_URLMAXSIZE*2]; loc[0]='\0';    // éventuelle nouvelle position
            r=http_test(ch,fil,loc);
            if (HTTP_IS_REDIRECT(r.statuscode)) {
              strcpybuff(ch,loc);
            }
          }
          */
          // recopier adresse
          if (strstr(ch,":/")==NULL) {
            strcatbuff(str,"http://");
          }
          strcatbuff(str,ch);
          strcatbuff(str,"\x0d\x0a");
        }
        j=0;
      } else ch[j++]=tempo[i];
    }
    SetDlgItemTextCP(this, IDC_URL,str);
    free(tempo);
    free(ch);
    free(str);
  } else
    AfxMessageBox(LANG(LANG_DIAL10));
  //m_urls=str;
}

void Wid1::OnChangeUrl()
{
  CString st="";
  //char tempo[8192];
  char* tempo;
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CPropertyPage::OnInitDialog()
	// function to send the EM_SETEVENTMASK message to the control
  // with the ENM_CHANGE flag ORed into the lParam mask.
  
  // TODO: Add your control notification handler code here
  GetDlgItemText(IDC_URL,st);
  tempo=(char*) malloc(st.GetLength()+1);
  if (tempo) {
    strcpybuff(tempo,st);
    int ex=0;
    do {
      if (strlen(tempo)>0) {
        switch (tempo[strlen(tempo)-1]) {
        case 10: case 13: case 32: tempo[strlen(tempo)-1]='\0';
          break;
        default: ex=1;
          break;
        }
      } else ex=1;
    } while(!ex);
    
    if ( ((strlen(tempo)>0) ) != url_status) { // || (m_ctl_todo.GetCurSel()==4)
      url_status=!url_status;
      
      if (url_status) {
        //m_ctl_next.ModifyStyle(WS_DISABLED,0);
      }
      else {
        //m_ctl_next.ModifyStyle(0,WS_DISABLED);
      }
      //m_ctl_next.RedrawWindow();
    }
    free(tempo); tempo=NULL;
  } else
    AfxMessageBox(LANG(LANG_DIAL10));
}

void Wid1::Refresh() {
  OnChangeUrl();
  //OnChangeprox();  // update disabled/normal
  //OnSelchangedepth();  // update disabled/normal
}

void Wid1::OnSelchangetodo() 
{
  int r;
  r = m_ctl_todo.GetCurSel();
  if (r!=CB_ERR) {
    switch(r) {
    case 0:
      SetDlgItemTextCP(this, IDC_INFOMAIN,
      LANG(LANG_G2 /*"Mirror mode, fill the addresse(s) in the URL box.\x0d\x0aThe mirror will be saved in the location indicated below",
      "Mode miroir, remplissez les addresse(s) dans la liste d'URLs.\x0d\x0aLe mirroir sera sauvé à l'emplacement indiqué plus bas"*/)
      );
      break;
    case 1:
      SetDlgItemTextCP(this, IDC_INFOMAIN,
      LANG(LANG_G3 /* "Mirror mode with wizard (asks questions), fill the addresse(s) in the URL box.\x0d\x0aThe mirror will be saved in the location indicated below",
      "Mode miroir semi automatique (pose des questions), remplissez les addresse(s) dans la liste d'URLs.\x0d\x0aLe mirroir sera sauvé à l'emplacement indiqué plus bas"*/)
      );
      break;
    case 2:
      SetDlgItemTextCP(this, IDC_INFOMAIN,
      LANG(LANG_G4 /* "Get files mode, fill the addresse(s) of the files in the URL box.\x0d\x0aThe mirror will be saved in the location indicated below",
      "Mode téléchargement de fichier, remplissez les addresse(s) des fichiers dans la liste d'URLs.\x0d\x0aLe mirroir sera sauvé à l'emplacement indiqué plus bas"*/)
      );
      SetDlgItemTextCP(this, IDC_depth,"");
      break;
    case 3:
      SetDlgItemTextCP(this, IDC_INFOMAIN,
        LANG(LANG_G1B)
        );
      SetDlgItemTextCP(this, IDC_depth,"");
      break;
    case 4:
      SetDlgItemTextCP(this, IDC_INFOMAIN,
      LANG(LANG_G5 /* "Test links mode, fill the addresse(s) of the pages containing links to test in the URL box.\x0d\x0aThe log report will be saved in the location indicated below",
      "Mode test de liens, remplissez les adresse(s) des pages contenant les liens à tester dans la liste d'URLs.\x0d\x0aLes fichiers d'audit seront sauvés à l'emplacement indiqué plus bas"*/)
      );
      SetDlgItemTextCP(this, IDC_depth,"");
      break;
    default:
      if (r == LAST_ACTION )
        SetDlgItemTextCP(this, IDC_INFOMAIN,
        LANG(LANG_G6 /* "Update/Continue a mirror mode, check addresse(s) in the URL box, then click to the 'NEXT' button and check parameters.\x0d\x0aThe mirror will be saved in the location indicated below",
        "Mode mise à jour/continuer un miroir, vérifiez les adresse(s) dans la liste d'URLs, puis cliquez sur le bouton 'NEXT' et vérifiez les paramètres.\x0d\x0aLe mirroir sera sauvé à l'emplacement indiqué plus bas"*/)
        );
      else if (r == LAST_ACTION-1 )
        SetDlgItemTextCP(this, IDC_INFOMAIN,
        LANG(LANG_G6b /* "Update/Continue a mirror mode, check addresse(s) in the URL box, then click to the 'NEXT' button and check parameters.\x0d\x0aThe mirror will be saved in the location indicated below",
        "Mode mise à jour/continuer un miroir, vérifiez les adresse(s) dans la liste d'URLs, puis cliquez sur le bouton 'NEXT' et vérifiez les paramètres.\x0d\x0aLe mirroir sera sauvé à l'emplacement indiqué plus bas"*/)
        );
      break;
    }
    
    if ( ((r==LAST_ACTION)||(r==LAST_ACTION-1)) != continue_status) {
      continue_status=((r==LAST_ACTION)||(r==LAST_ACTION-1));
      OnChangeUrl();
    }
  }
  
}


void Wid1::OnChangepathlog() {
  load_after_changes=1;
}

void Wid1::AfterChangepathlog() 
{
  CString st="";
  char tempo[8192];
  BOOL modify;
	char catbuff[CATBUFF_SIZE];

  strcpybuff(tempo,dialog0->GetPath());
  {
    if (fexist(fconcat(catbuff,sizeof(catbuff),tempo,"hts-cache/winprofile.ini"))) {    // un cache est présent
      if (fsize(fconcat(catbuff,sizeof(catbuff),tempo,"hts-cache/winprofile.ini"))>0) {   // taille log contrôle>0
        int i;
        for(i=0;i<(int) strlen(tempo);i++)
          if (tempo[i]=='/')
            tempo[i]='\\';
          Read_profile(fconcat(catbuff,sizeof(catbuff),tempo,"hts-cache\\winprofile.ini"),0);

          // peut on modifier?
          int pos=m_ctl_todo.GetCurSel();
          if ((pos==LAST_ACTION) || (pos==LAST_ACTION-1) || (pos==0))
            modify=true;
          else
            modify=false;
          
          // existe: update
          if (
            fexist(fconcat(catbuff,sizeof(catbuff),tempo,"hts-cache/new.zip"))
            ||
            (fexist(fconcat(catbuff,sizeof(catbuff),tempo,"hts-cache/new.dat")))
            && (fexist(fconcat(catbuff,sizeof(catbuff),tempo,"hts-cache/new.ndx")))
            ) {  // il existe déja un cache précédent.. renommer
            if (modify) {
              if (
                (!fexist(fconcat(catbuff,sizeof(catbuff),tempo,"hts-in_progress.lock")))
                &&
                (!fexist(fconcat(catbuff,sizeof(catbuff),tempo,"hts-cache/interrupted.lock")))
                )
                m_ctl_todo.SetCurSel(LAST_ACTION);
              else
                m_ctl_todo.SetCurSel(LAST_ACTION-1);
            }
            log_flip=1;
          } else if (log_flip) {
            if (modify)
              m_ctl_todo.SetCurSel(0);
            log_flip = 0;
          }
          OnSelchangetodo();
      }
    } else if (log_flip) {
      m_ctl_todo.SetCurSel(0);
      log_flip = 0;
    }
  }
}
/*
void Wid1::OnClose() 
{
  // TODO: Add your message handler code here and/or call default
  //Onfin();
  //::OnClose();
  if (AfxMessageBox(
  LANG(LANG_J1)
  ,MB_OKCANCEL)==IDOK) {
    if (Save_current_profile(1)!=IDCANCEL) {
      cancel=1;
      CPropertyPage::OnCancel();
    }
  }
}
*/



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
BOOL Wid1::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
char* Wid1::GetTip(int ID)
{
  switch(ID) {
    case IDC_todo:  return LANG(LANG_G9); break; // "Choose an action","Choisissez une action"); break; 
    case IDC_depth: return LANG(LANG_G10); break; // "Maximum link depth to scan","Profondeur maximale"); break;
    case IDC_URL:   return LANG(LANG_G11); break; // "Enter addresses here","Entrez les adresses ici"); break;
    //case IDC_urls:  return LANG(LANG_G12); break; // "Check spelling","Vérifier syntaxe"); break;
    //case IDfilter:  return LANG(LANG_G13); break; // "Define additional filters","Définir les filtres supplémentaires"); break;
    //case IDC_pathlog:  return LANG(LANG_G16); break; // "Path","Chemin"); break;
    //case IDC_br2:      return LANG(LANG_G17); break; // "Select path","Choix du chemin"); break;
    //case IDC_pthmir:   return LANG(LANG_G18); break; // "Path","Chemin"); break;
    //case IDC_br1:      return LANG(LANG_G19); break; // "Select path","Choix du chemin"); break;
    case IDCANCEL:     return LANG(LANG_G20); break; // "Quit WinHTTrack","Quittter WinHTTrack"); break;
    case IDC_ipabout:  return LANG(LANG_G21); break; // "About WinHTTrack","A propos de WinHTTrack"); break;
    case IDC_regdef:   return LANG(LANG_G22); break; // "Save preferences as default values","Sauver les réglages par défaut"); break;
    case IDOK:         return LANG(LANG_G23); break; // "Click to continue","Clic pour continuer"); break;
    case IDC_avant:     return LANG_TIPPREV; break;
    case ID_setopt:    return LANG(LANG_G24); break; // "Click to define option","Clic pour définir les options"); break;
    case IDC_login2:   return LANG_G24b; break;
    case IDC_filelist: return LANG_G24c; break;
    //case : return ""; break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------


/*
void Wid1::OnDropFiles(HDROP hDropInfo) 
{
  int nf;
  int i;
  nf=DragQueryFile(hDropInfo,0xFFFFFFFF,NULL,0);    // nbre de fichiers

  for(i=0;i<nf;i++) {
    char* buff;
    int size;
    size=DragQueryFile(hDropInfo,i,NULL,0);    // taille buffer nécessaire
    if (size>0) {
      size+=16;    // marge de sécurité
      buff=(char*) calloc(size,1);
      if (buff) {
        if (DragQueryFile(hDropInfo,i,buff,size)>0) {    // ok copié
          AfxMessageBox(buff,MB_OKCANCEL+MB_ICONQUESTION);
        }
        free(buff);
      }
    }
  }

  CPropertyPage::OnDropFiles(hDropInfo);
}
*/

int Wid1::OnCreate(LPCREATESTRUCT lpCreateStruct) 
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


CString Wid1::TextToUrl(CString st,CLIPFORMAT cfFormat) {
  // yop
  // on va convertir les chaines à la suite
  CString res;
  if (cfFormat==CF_TEXT) {
    char *buff, *last;
    for(buff = st.GetBuffer(0), last = buff ; *buff != '\0' ; buff++) {
      if (*buff == 10 || *buff == 13) {
        *buff = 0;
        if (*last) {
          res += last;
          res += "\r\n";
        }
        last = buff + 1;
      }
    }
    return res;
  }
  else if (cfFormat==CF_HDROP) {
    char* last = st.GetBuffer(0);
    int len = (int) strlen(last);
    if (stricmp(last + len - 4, ".url") == 0) {
      FILE* fp = fopen(last, "rb");
      if (fp != NULL) {
        char line[256];
        if (linput(fp, line, sizeof(line) - 2) > 0
          && linput(fp, line, sizeof(line) - 2) > 0)
        {
          /* BASEURL=http://.. */
          char* pos = strchr(line, '=');
          if (pos != NULL) {
            res += (pos + 1);
            res += "\r\n";
          }
        }
        fclose(fp);
      } else {
        res += last;
        res += "\r\n";
      }
    } 
    else if (st.GetLength()<256) {
      char s[256];
      escape_check_url(st, s, sizeof(s));     // coder %
      res = "file://";
      res += s;
    }
  }
  return res;
}

// Message from CEasyDropTarget
LRESULT Wid1::DragDropText(WPARAM wParam,LPARAM lParam) {
  if (lParam) {
    CString st=*((CString*) lParam);
    CLIPFORMAT cfFormat = (CLIPFORMAT) wParam;
    st=Wid1::TextToUrl(st,cfFormat);
    if (st=="")
      AfxMessageBox(LANG(LANG_DIAL11),MB_SYSTEMMODAL);
    else
      AddText(st);
  }
  return 0;
}

// ajout d'URLs
void Wid1::AddText(CString add_st) {
  CString st;
  GetDlgItemText(IDC_URL,st);
  SetDlgItemTextCP(this, IDC_URL,st+"\r\n"+add_st);
  CleanUrls();
  OnChangeUrl();
}

// Appel aide
void Wid1::OnHelpInfo2() {
  (void) OnHelpInfo(NULL);
}

BOOL Wid1::OnHelpInfo(HELPINFO* dummy) 
{
  //return CPropertyPage::OnHelpInfo(pHelpInfo);
  //AfxGetApp()->WinHelp(0,HELP_FINDER);    // Index du fichier Hlp
  //LaunchHelp(pHelpInfo);
  HtsHelper->Help("step2.html");
  //HtsHelper->Help();
  return true;
}

void Wid1::OnLoadprofile() {
  static char BASED_CODE szFilter[256];
  strcpybuff(szFilter,LANG(LANG_G25 /*"WinHTTrack preferences (*.opt)|*.opt||","Réglages de WinHTTrack (*.opt)|*.opt||"*/));
  CFileDialog* dial = new CFileDialog(true,"opt",NULL,OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,szFilter);
  if (dial->DoModal() == IDOK) {
    CString st=dial->GetPathName();
    char s[256];
    strcpybuff(s,st);
    if (fexist(s))
      Read_profile(st,1);
    else
      AfxMessageBox(LANG(LANG_G26 /*"File not found!","Fichier introuvable!"*/));
  }
  delete dial;
}

void Wid1::OnSaveprofile() {
  static char BASED_CODE szFilter[256];
  strcpybuff(szFilter,LANG(LANG_G25 /*"WinHTTrack preferences (*.opt)|*.opt||","Réglages de WinHTTrack (*.opt)|*.opt||"*/));
  CFileDialog* dial = new CFileDialog(false,"opt",NULL,OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,szFilter);
  if (dial->DoModal() == IDOK) {
    CString st=dial->GetPathName();
    Write_profile(st,1);
  }
  delete dial;
}

void Wid1::OnLoaddefault() {
  if (AfxMessageBox(LANG(LANG_G27),MB_OKCANCEL)==IDOK) {
    Read_profile("",0);
  }
}

void Wid1::OnSavedefault() {
  if (AfxMessageBox(LANG(LANG_G28),MB_OKCANCEL)==IDOK) {
    Write_profile("",0);
  }
}

void Wid1::OnResetdefault() {
  if (AfxMessageBox(LANG(LANG_G29),MB_OKCANCEL)==IDOK) {
    Read_profile("<null>",0);    // charger default
    Write_profile("",0);         // et sauver
  }
}

/*
void Wid1::OnNewProject() {
  if (AfxMessageBox(LANG(LANG_G1C),MB_OKCANCEL)==IDOK) {
    CWinApp* pApp = AfxGetApp();
    CString name = pApp->m_pszHelpFilePath;
    name=name.Left(name.GetLength()-4);
    name += ".EXE";
    ShellExecute(NULL,"open",name,"","",SW_RESTORE);
    cancel=1;
    EndDialog(1);
  }
}
*/

void Wid1::OnSaveProject() {
  Save_current_profile(0);
}

void Wid1::Onsetopt() 
{
  CShellApp_app->OptPannel();
}

/*
HCURSOR Wid1::OnQueryDragIcon() 
{
	// TODO: Add your message handler code here and/or call default
	
	return CPropertyPage::OnQueryDragIcon();
}
*/

void Wid1::Onlogin2() 
{
  char catbuff[CATBUFF_SIZE];
  CInsertUrl url;
  url.dest_path=dialog0->GetPath();
  if (url.DoModal() == IDOK) {
    if (url.m_urladr.Left(7)=="http://")
      url.m_urladr=url.m_urladr.Mid(7);
    CString st;
    GetDlgItemText(IDC_URL,st);
    if (url.m_urllogin.GetLength()==0) {
      SetDlgItemTextCP(this, IDC_URL,st+"\r\n"+url.m_urladr);
    } else {
      CString str = st+"\r\n";
      escape_in_url(LPCTSTR(url.m_urllogin), catbuff, sizeof(catbuff));
      str += catbuff;
      str += ":";
      escape_in_url(LPCTSTR(url.m_urlpass), catbuff, sizeof(catbuff));
      str += catbuff;
      str += "@";
      str += url.m_urladr;
      SetDlgItemTextCP(this, IDC_URL, str);
    }
    CleanUrls();
    OnChangeUrl();
  }
}

LRESULT Wid1::OnWizardNext() {
  CString st;
  UpdateData(TRUE);         // DoDataExchange
  m_urls.TrimLeft(); m_urls.TrimRight();
  m_filelist.TrimLeft(); m_filelist.TrimRight();
  if ((m_urls.GetLength()>0) || (m_filelist.GetLength()>0)) {
    if ((m_todo==LAST_ACTION)||(m_todo==LAST_ACTION-1)) {
      char path[HTS_URLMAXSIZE*2];
      CString st;
      st=dialog0->GetPath();
      strcpybuff(path,st);
      if (check_continue(path))
        return 0;
      else {
        GetDlgItem(IDC_URL)->SetFocus();
        return -1;        // impossible de continuer
      }
    }
  } else {
    GetDlgItem(IDC_URL)->SetFocus();
    return -1;
  }
  return 0;
}

BOOL Wid1::OnSetActive( ) {
  //OnInitDialog();
  AfterInitDialog();
  this_CWizTab->SetWizardButtons(PSWIZB_BACK|PSWIZB_NEXT);
  return 1;
}

BOOL Wid1::OnQueryCancel( ) {
  this_CSplitterFrame->SetNewView(0,1,RUNTIME_CLASS(CDialogContainer));
  return 0;
}

BOOL Wid1::OnKillActive( ) {
  UpdateData(TRUE);         // DoDataExchange
  return 1;
}

void Wid1::Onbr() 
{
  static char BASED_CODE szFilter[256];
  strcpybuff(szFilter,LANG(LANG_G25b));
  CFileDialog* dial = new CFileDialog(true,"txt",NULL,OFN_HIDEREADONLY,szFilter);
  if (dial->DoModal() == IDOK) {
    if (fexist((char*)LPCSTR(dial->GetPathName()))) {
      SetDlgItemTextCP(this, IDC_filelist,dial->GetPathName());
    } else {
      AfxMessageBox(LANG(LANG_G26 /*"File not found!","Fichier introuvable!"*/));
      SetDlgItemTextCP(this, IDC_filelist,"");
    }
  }
  delete dial;
  OnChangefilelist();
}

void Wid1::OnChangefilelist() 
{
  CString st;
  GetDlgItemText(IDC_filelist,st);
  if ((st.GetLength()>0) != filelist_status) {
    filelist_status=(st.GetLength()>0);
    if (filelist_status) {
      GetDlgItem(IDC_STATIC_filelist)->ModifyStyle(WS_DISABLED,0);
    } else {
      GetDlgItem(IDC_STATIC_filelist)->ModifyStyle(0,WS_DISABLED);
    }
    GetDlgItem(IDC_STATIC_filelist)->RedrawWindow();
  }
}
