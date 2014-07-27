// NewProj.cpp : implementation file
//

// pour lecture dir
//#include "kernel32.h"

#include "stdafx.h"
#include "Shell.h"
#include "NewProj.h"

/* Externe C */
#include <WS2tcpip.h>  // Note: weird C2894 error if not included here
extern "C" {
  #include "HTTrackInterface.h"
  #include "htscore.h"
}

#include "XSHBrowseForFolder.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern HICON httrack_icon;
extern CNewProj* dialog0;

// Helper
extern LaunchHelp* HtsHelper;

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

/* reference sur objet Wid1 */
#include "Wid1.h"
extern Wid1* dialog1;

/* shellapp */
extern CShellApp* CShellApp_app;

/* création structure */
extern "C" HTSEXT_API int structcheck(const char* s);


/////////////////////////////////////////////////////////////////////////////
// CNewProj dialog

IMPLEMENT_DYNCREATE(CNewProj, CPropertyPage)

CNewProj::CNewProj()
	: CPropertyPage(CNewProj::IDD)
{
  dialog0=this;    /* NOTER REFERENCE */
  can_click_next=TRUE;
	//{{AFX_DATA_INIT(CNewProj)
	m_projname = _T("");
	m_projpath = _T("");
	m_projcateg = _T("");
	//}}AFX_DATA_INIT
}

CNewProj::~CNewProj() {
  dialog0=NULL;
}


void CNewProj::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNewProj)
	DDX_Control(pDX, IDC_projcateg, m_ctl_projcateg);
	DDX_Control(pDX, IDC_projname, m_ctl_projname);
	DDX_Text(pDX, IDC_projname, m_projname);
	DDX_Text(pDX, IDC_projpath, m_projpath);
	DDX_CBString(pDX, IDC_projcateg, m_projcateg);
	//}}AFX_DATA_MAP
}


#define wm_CEasyDropTargetCallback (WM_USER + 1)
BEGIN_MESSAGE_MAP(CNewProj, CPropertyPage)
	//{{AFX_MSG_MAP(CNewProj)
	ON_BN_CLICKED(IDC_br, Onbr)
	ON_EN_CHANGE(IDC_projpath, OnChangeprojpath)
	ON_CBN_EDITCHANGE(IDC_projname, OnChangeprojname)
	ON_WM_CREATE()
	ON_CBN_SELCHANGE(IDC_projname, OnSelchangeprojname)
	//}}AFX_MSG_MAP
  ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
  ON_COMMAND(ID_HELP,OnHelpInfo2)
	ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
  //
  ON_MESSAGE( wm_CEasyDropTargetCallback, DragDropText)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNewProj message handlers

CString CNewProj::GetBasePath() {
  CString st=m_projpath;
  st.Replace('/','\\');
  st+="\\";
  return st;
}

CString CNewProj::GetName() {
  return m_projname;
}

CString CNewProj::GetPath0() {
  CString st;
  if (m_projpath.GetLength() < MAX_PATH) {
    st=m_projpath;
    st.Replace('/','\\');
    st+="\\";
    st+=m_projname;
  }
  return st;
}

CString CNewProj::GetPath() {
  CString st=GetPath0();
  st+="\\";
  return st;
}

LRESULT CNewProj::OnWizardNext() {
  CString st,stp="";
  char tempo[8192];

  GetDlgItemText(IDC_projpath,stp);
  if (st.GetLength() > MAX_PATH) {
    return -1;
  }
  strcpybuff(tempo,stp);
  if ((tempo[strlen(tempo)-1]=='/') || (tempo[strlen(tempo)-1]=='\\'))
    tempo[strlen(tempo)-1]='\0';
  stp=tempo;

  // ecrire
  CString strSection       = "DefaultValues";    
  CWinApp* pApp = AfxGetApp();
  pApp->WriteProfileString(strSection,"BasePath",stp);

  GetDlgItemText(IDC_projname,st);
  if (st.GetLength() > MAX_PATH) {
    return -1;
  }
  st.TrimLeft();
  st.TrimRight();
  strcpybuff(tempo,st);
  // caractères interdits
  {
    int i;
    char* a=NULL;
    for(i=0;i<9;i++)
      while(a=strchr(tempo,("\\/:*?\"<>|")[i])) 
        *a=' ';
  }
  //
  //while(a=strchr(tempo,' ')) *a='_';      // PAS de _
  SetDlgItemTextCP(this, IDC_projname,tempo);

  // Modifié et sauvable
  GetDlgItemText(IDC_projpath,stp);
  if (stp.GetLength() > MAX_PATH) {
    return -1;
  }
  if ((stp.Right(1)!="\\") && (stp.Right(1)!="/"))    // ajouter /
    stp+="\\";
  GetDlgItemText(IDC_projname,st);
  if (st.GetLength() > MAX_PATH) {
    return -1;
  }
  this_CSplitterFrame->SetNewName(stp+st+".whtt");
  //GetDlgItemText(IDC_projpath,st);
  //this_CSplitterFrame->GetActiveDocument()->SetPathName(st);

  // Hack
  {
    CString st;
    GetDlgItemText(IDC_projcateg, st);
    if (st.GetLength() > MAX_PATH) {
      return -1;
    }
    this_CSplitterFrame->SetCurrentCategory(st);
  }

  if (can_click_next)
    return 0;
  else
    return -1;
}

void CNewProj::Onbr() 
{
  CString st,spth;
  char pth[MAX_PATH + 32];
  GetDlgItemText(IDC_projpath,spth);
  if (spth.GetLength() > MAX_PATH) {
    return ;
  }
  strcpybuff(pth,spth);
  st=XSHBrowseForFolder(this->m_hWnd,"Select a path name for mirror",pth);
  if (st.GetLength()>0) {
    if (st.Right(1)=='\\')
      st=st.Left(st.GetLength()-1);
    SetDlgItemTextCP(this, IDC_projpath,st);
  }

}

BOOL CNewProj::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
  SetIcon(httrack_icon,false);
  SetIcon(httrack_icon,true);
  EnableToolTips(true);     // TOOL TIPS
  //SetForegroundWindow();   // yop en premier plan!

  // disabled
  OnChangeprojname();

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    SetWindowTextCP(this,  LANG(LANG_S10));
    // SetDlgItemTextCP(this, IDOK,LANG(LANG_NEXT )); // "SUIVANT ->");
    // SetDlgItemTextCP(this, IDCANCEL,LANG(LANG_QUIT));  // exit 
    SetDlgItemTextCP(this, IDC_STATIC_projname,LANG(LANG_S11));
    SetDlgItemTextCP(this, IDC_STATIC_basepath,LANG(LANG_S12));
    SetDlgItemTextCP(this, IDC_STATIC_projcateg,LANG(LANG_S13));
    //SetDlgItemTextCP(this, IDC_STATIC_updtcontinue,LANG(LANG_S13));
  }
  
  CString strSection       = "DefaultValues";    
  CWinApp* pApp = AfxGetApp();
  CString st;

  st = pApp->GetProfileString(strSection, "BasePath");
  if (m_projpath.GetLength()==0) {
    if (st != "")
      SetDlgItemTextCP(this, IDC_projpath,st);    
    else
      SetDlgItemTextCP(this, IDC_projpath,LANG(LANG_S20));    
  }

  return TRUE;
}

void CNewProj::OnChangeprojpath() 
{
  CWaitCursor wait;
  CString st;
  CString old_name;
  char tempo[HTS_URLMAXSIZE*2];

  GetDlgItemText(IDC_projname,old_name);
  GetDlgItemText(IDC_projpath,st);
  if (st.GetLength() > MAX_PATH) {
    SetDlgItemText(IDC_projpath, "");
    return;
  }

  tempo[0] = '\0';
  strcatbuff(tempo, st);
  if ((tempo[strlen(tempo)-1]=='/') || (tempo[strlen(tempo)-1]=='\\')) {
    tempo[strlen(tempo)-1]='\0';
    //SetDlgItemTextCP(this, IDC_projpath,tempo);
  }
  strcatbuff(tempo,"\\");

  TStamp t_start = mtime_local();

  // chargement de la liste
  m_ctl_projname.ResetContent();
  m_ctl_projcateg.ResetContent();
  WIN32_FIND_DATA find;
  char  pth[MAX_PATH + 32];
  strcpybuff(pth,tempo);
  strcatbuff(pth,"*.*");
  HANDLE h = FindFirstFile(pth,&find);
  if (h != INVALID_HANDLE_VALUE) {
    do {
      if (find.dwFileAttributes  & FILE_ATTRIBUTE_DIRECTORY )
      if (!(find.dwFileAttributes  & (FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_HIDDEN) ))
      if (strcmp(find.cFileName,".."))
        if (strcmp(find.cFileName,".")) {
          CString st;
          st=tempo;
          st=st+find.cFileName;
          st=st+"\\hts-cache";
          
          WIN32_FIND_DATA find2;
          HANDLE h2 = FindFirstFile(st,&find2);
          if (h2 != INVALID_HANDLE_VALUE) {
            FindClose(h2);
            m_ctl_projname.AddString(find.cFileName);
            
            // Read category
            st += "\\winprofile.ini";
            CString strSection       = "OptionsValues";
            CString categ = MyGetProfileString(st, strSection, "Category");
            if (categ.GetLength() > 0 && m_ctl_projcateg.FindStringExact(0, categ) < 0) {
              m_ctl_projcateg.AddString(categ);
            }
          }
        }
    } while(FindNextFile(h,&find));
    FindClose(h);
  }

  // nouveau nom!
  SetDlgItemTextCP(this, IDC_projname,old_name);

  TStamp t_proj = mtime_local();

  OnChangeprojname();

  TStamp t_end = mtime_local();
  TStamp l_dir = t_proj - t_start;
  TStamp l_proj = t_end - t_proj;
  //CString a;
  //a.Format("%s: build=%dms proj=%dms", __FUNCTION__, (int)l_dir, (int)l_proj);
  //(void) AfxMessageBox(a, MB_OK);
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
BOOL CNewProj::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
char* CNewProj::GetTip(int ID)
{
  switch(ID) {
    case IDC_projname:  return LANG(LANG_S1); break;
    case IDC_projpath:  return LANG(LANG_S2); break;
    case IDC_projcateg: return LANG(LANG_S5); break;
    //case IDC_projlist:  return LANG(LANG_S3); break;
    case IDC_br:        return LANG(LANG_S4); break;
    case IDCANCEL:     return LANG(LANG_G20); break; // "Quit WinHTTrack","Quittter WinHTTrack"); break;
    case IDOK:         return LANG(LANG_G23); break; // "Click to continue","Clic pour continuer"); break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------


void CNewProj::OnChangeprojname() 
{
  CString stl;
  BOOLEAN modified = FALSE;
  int i;
  GetDlgItemText(IDC_projname,stl);
  for(i = 0 ; i < stl.GetLength() ; i++) {
    if (stl[i] == '\"') {
      stl.GetBuffer(0)[i] = '\'';
      modified = TRUE;
    }
    else if (((unsigned char)stl[i]) < 32) {
      stl.GetBuffer(0)[i] = ' ';
      modified = TRUE;
    }
    else if (stl[i] == '~'
      || stl[i] == '\\'
      || stl[i] == '/'
      || stl[i] == ':'
      || stl[i] == '*'
      || stl[i] == '?'
      || stl[i] == '<'
      || stl[i] == '>'
      || stl[i] == '|'
      ) {
      stl.GetBuffer(0)[i] = '_';
      modified = TRUE;
    }
  }
  if (modified) {
    DWORD dwSel=0;
    dwSel = m_ctl_projname.GetEditSel();
    SetDlgItemTextCP(this, IDC_projname, stl);
    m_ctl_projname.SetEditSel(HIWORD(dwSel), LOWORD(dwSel));
  }
  Changeprojname(stl);
}
 

void CNewProj::Changeprojname(CString stl) {
	char catbuff[CATBUFF_SIZE];
	CWaitCursor wait;
  CString st;
  //
  if (stl.GetLength()==0 || stl.GetLength() > HTS_URLMAXSIZE) {
    //m_ctl_idok.ModifyStyle(0,WS_DISABLED);
    SetDlgItemTextCP(this, IDC_STATIC_comments,LANG(LANG_S30));
    SetDlgItemTextCP(this, IDC_STATIC_projname,LANG(LANG_S11c));
    this_CWizTab->SetWizardButtons(PSWIZB_BACK);
    can_click_next=FALSE;
  } else {
    char tempo[HTS_URLMAXSIZE*2];
    GetDlgItemText(IDC_projpath,st);
    if (st.GetLength() + stl.GetLength() + 32 > sizeof(tempo)) {
      can_click_next=FALSE;
    } else {
      strcpybuff(tempo,st);
      strcatbuff(tempo,"/");
      strcatbuff(tempo,stl);
      strcatbuff(tempo,"/");
      if (fexist(fconcat(catbuff,sizeof(catbuff),tempo,"hts-cache/winprofile.ini"))     // un cache est présent
        && fsize(fconcat(catbuff,sizeof(catbuff),tempo,"hts-cache/winprofile.ini"))>0) {   // taille log contrôle>0
        CString strSection       = "OptionsValues";
        CString st  = MyGetProfileString(fconcat(catbuff,sizeof(catbuff),tempo,"hts-cache/winprofile.ini"),strSection,"CurrentUrl");
        CString st2 = MyGetProfileString(fconcat(catbuff,sizeof(catbuff),tempo,"hts-cache/winprofile.ini"),strSection,"Category");
        //
        SetDlgItemTextCP(this, IDC_STATIC_comments, st);
        SetDlgItemTextCP(this, IDC_projcateg, st2);
        // Static
        SetDlgItemTextCP(this, IDC_STATIC_projname,LANG(LANG_S11b));
      } else {
        SetDlgItemTextCP(this, IDC_STATIC_comments,LANG(LANG_S31));
        SetDlgItemTextCP(this, IDC_STATIC_projname,LANG(LANG_S11));
      }
      //m_ctl_idok.ModifyStyle(WS_DISABLED,0);
      //if (!can_click_next)
      this_CWizTab->SetWizardButtons(PSWIZB_BACK|PSWIZB_NEXT);
      can_click_next=TRUE;
    }
  }
  //m_ctl_idok.RedrawWindow();
}


// help
// Appel aide
void CNewProj::OnHelpInfo2() {
  (void)  OnHelpInfo(NULL);
}

BOOL CNewProj::OnHelpInfo(HELPINFO* dummy) 
{
  //return CPropertyPage::OnHelpInfo(pHelpInfo);
  //AfxGetApp()->WinHelp(0,HELP_FINDER);    // Index du fichier Hlp
  HtsHelper->Help("step1.html");
  //HtsHelper->Help();
  return true;
}


extern HWND App_Main_HWND;
int CNewProj::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
  //lpCreateStruct->hwndParent=AfxGetApp()->GetMainWnd()->m_hWnd;
  lpCreateStruct->hwndParent=App_Main_HWND;
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
LRESULT CNewProj::DragDropText(WPARAM wParam,LPARAM lParam) {
  if (lParam) {
    CString st=*((CString*) lParam);
    CLIPFORMAT cfFormat = (CLIPFORMAT) wParam;
    if (cfFormat==CF_HDROP) {
      if (st.Right(5).CompareNoCase(".whtt")==0)
        st=st.Left(st.GetLength()-5);
      if(st.GetLength()<256) {
        char s[256];
        strcpybuff(s,st);
        if (!fexist(s)) {
          if (st.Find('\n')<0) {
            int pos=st.ReverseFind('\\');
            SetDlgItemTextCP(this, IDC_projpath,st.Mid(0,pos));
            SetDlgItemTextCP(this, IDC_projname,st.Mid(pos+1));
            //
            OnChangeprojpath();
            OnChangeprojname();
          } else
            AfxMessageBox(LANG(LANG_DIAL3),MB_SYSTEMMODAL);
        } else
          AfxMessageBox(LANG(LANG_DIAL4),MB_SYSTEMMODAL);
      }
    } else
      AfxMessageBox(LANG(LANG_DIAL5),MB_SYSTEMMODAL);
  }
  return 0;
}


void CNewProj::OnSelchangeprojname() 
{
  CString stl;
  int r;
  if ((r=m_ctl_projname.GetCurSel()) != CB_ERR) {
    m_ctl_projname.GetLBText(r,stl);
  } else
    GetDlgItemText(IDC_projname,stl);
  Changeprojname(stl);
}

BOOL CNewProj::OnSetActive( ) {
  //this_CWizTab->SetWizardButtons(PSWIZB_BACK);

  WHTT_LOCATION("NewProj");
  //if (!can_click_next) {
  // structure visible
  CString st;
  GetDlgItemText(IDC_projpath,st);    
  TStamp t_start = mtime_local();
  // FIXME: VERY SLOW!!
  this_DirTreeView->EnsureVisible(st+"\\index.html");
  TStamp t_end = mtime_local();

  //TStamp l_dir = t_end - t_start;
  //CString a;
  //a.Format("t=%dms", (int)l_dir);
  //(void) AfxMessageBox(a, MB_OK);

  SetDlgItemTextCP(this_CWizTab, IDCANCEL,LANG_CANCEL);
  //} else
  //this_CWizTab->PressButton(PSBTN_NEXT);
  
  return 1;
}

BOOL CNewProj::OnQueryCancel( ) {
  this_CSplitterFrame->SetNewView(0,1,RUNTIME_CLASS(CDialogContainer));
  return 0;
}

BOOL CNewProj::OnKillActive( ) {
  CString st;
  char tempo[HTS_URLMAXSIZE*2];
  GetDlgItemText(IDC_projpath,st);
  if (st.GetLength() > MAX_PATH) {
    return FALSE;
  }
  strcpybuff(tempo,st);
  if ((tempo[strlen(tempo)-1]=='/') || (tempo[strlen(tempo)-1]=='\\')) {
    tempo[strlen(tempo)-1]='\0';
    SetDlgItemTextCP(this, IDC_projpath,tempo);
  }

  UpdateData(TRUE);         // DoDataExchange

  // créer structure
  {
    char dest[HTS_URLMAXSIZE*2];
    int i=0;
    strcpybuff(dest,GetPath()+"hts-cache\\");
    {
      char* a;
      while(a=strchr(dest,'\\')) *a='/';
      structcheck(dest);
    }
  }

  CShellApp_app->end_path=dialog0->GetBasePath();
  CShellApp_app->end_path_complete=dialog0->GetPath();
  Build_TopIndex(FALSE);

  // structure visible
  TStamp t_start = mtime_local();
  // FIXME: VERY SLOW!!
  this_DirTreeView->EnsureVisible(GetPath()+"hts-cache");
  TStamp t_end = mtime_local();

  //TStamp l_dir = t_end - t_start;
  //CString a;
  //a.Format("t=%dms", (int)l_dir);
  //(void) AfxMessageBox(a, MB_OK);

  // charger préfs
  dialog1->OnChangepathlog();

  return 1;
}


