// iplog.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "iplog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern "C" {
  #include "HTTrackInterface.h"
}
extern HICON httrack_icon;

// Helper
extern LaunchHelp* HtsHelper;

#define LOW_MARK 127000
#define HIGH_MARK 128000

/////////////////////////////////////////////////////////////////////////////
// Ciplog dialog


Ciplog::Ciplog(CWnd* pParent /*=NULL*/)
	: CDialog(Ciplog::IDD, pParent)
{
  timer=0;
  txt="\n";
  type=-1;
  type_filter=0;
  filter=0;
	//{{AFX_DATA_INIT(Ciplog)
	m_iplog = _T("");
	//}}AFX_DATA_INIT
}


void Ciplog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(Ciplog)
	DDX_Control(pDX, IDC_HIDEINFO, m_ctl_hideinfo);
	DDX_Control(pDX, IDC_log, m_ctl_iplog);
	DDX_Text(pDX, IDC_log, m_iplog);
	//}}AFX_DATA_MAP
}


// typedef void (AFX_MSG_CALL CCmdTarget::*AFX_PMSG)(void);
BEGIN_MESSAGE_MAP(Ciplog, CDialog)
	//{{AFX_MSG_MAP(Ciplog)
	ON_EN_VSCROLL(IDC_log, OnScroll)
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_HELPINFO()
	ON_BN_CLICKED(IDC_changelog, Onchangelog)
	ON_WM_TIMER()
	ON_CBN_SELCHANGE(IDC_HIDEINFO, OnSelchangeHideinfo)
	ON_EN_HSCROLL(IDC_log, OnScroll)
	ON_BN_CLICKED(IDC_FIND, OnFind)
	//}}AFX_MSG_MAP
  ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
  ON_COMMAND(ID_HELP,OnHelpInfo2)
	ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Ciplog message handlers


// refresh
void Ciplog::AffLogRefresh() {
 	char catbuff[CATBUFF_SIZE];

  if (wait_me) {
    wait_me=0;
    return;
  }
  if (type_log != type) {
    type=type_log;
    if (fp)
      fclose(fp);
    fp=NULL;
  }
  if (type_filter != filter) {
    filter=type_filter;
    if (fp)
      fclose(fp);
    fp=NULL;
  }
  
  if (fp==NULL) {
    txt="";
    if (type_log) {
      fp=fopen(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-log.txt"),"rb");
      SetWindowTextCP(this, LANG_E10);
      SetWindowTextCP(m_ctl_iplog, LANG_E10);
      SetDlgItemTextCP(this, IDC_changelog,LANG_E11);
    }
    else {
      fp=fopen(fconcat(catbuff,sizeof(catbuff),pathlog,"hts-err.txt"),"rb");
      SetWindowTextCP(this, LANG_E11);
      SetWindowTextCP(m_ctl_iplog, LANG_E11);
      SetDlgItemTextCP(this, IDC_changelog,LANG_E10);
      if (!fp) {        /* pas de err : switcher (en cas de log unique) */
        type_log=1;
        GetDlgItem(IDC_changelog)->ModifyStyle(0,WS_DISABLED);
      }
    }
    SetDlgItemTextUTF8(this, IDC_log,txt);
  }
  
  if (fp) {
    char dat[HIGH_MARK+4]; dat[0]='\0';
    clearerr(fp);
    if (!feof(fp)) {
      char* a=dat;
      char* startline=dat;
      int n=0;
      int c;
      while ( (!feof(fp)) && (n<HIGH_MARK) ) {
        c = fgetc(fp);
        n++;
        if ((c>0) && (c!=EOF)) {
          switch(c) {
          case 13: break;
          case 10: {
            *a='\0';
            if (filter & 1) {
              if (strstr(startline,"Debug:")) {       // éliminer débug
                a=startline;
              }
            }
            if (filter & 2) {
              if (strstr(startline,"Info:")) {       // éliminer info
                a=startline;
              }
            }
            if (a!=startline) {
              *a++=13; 
              *a++=10;
            }
            startline=a;
            /* couper en fin de ligne */
            if (n>=LOW_MARK)
              n=HIGH_MARK;
                   }
            break;
          case 9: *a++=' '; break;
          default:
            *a++=c;
            break;
          }
        }
      }
      *a='\0';
    }
    if (strlen(dat)>0) {
      // avant d'écrire on sauvegarde la position (bah oui.. c chiant)
      txt += dat;
      int lh = m_ctl_iplog.GetScrollPos(SB_HORZ);
      int lv = m_ctl_iplog.GetScrollPos(SB_VERT);
      SetDlgItemTextUTF8(this, IDC_log,txt);
      m_ctl_iplog.SetScrollPos(SB_HORZ,lh);
      m_ctl_iplog.SetScrollPos(SB_VERT,lv);
    }
  }
}

BOOL Ciplog::DestroyWindow() 
{
  StopTimer();
  if (fp)
    fclose(fp);
  fp=NULL;
	return CDialog::DestroyWindow();
}

void Ciplog::OnScroll() 
{
  wait_me=1;  // attendre pour le scrolling
}

BOOL Ciplog::OnInitDialog() 
{
  wait_me=0;
  fp=NULL;

	CDialog::OnInitDialog();	
  SetIcon(httrack_icon,false);
  SetIcon(httrack_icon,true);
  EnableToolTips(true);     // TOOL TIPS
  SetForegroundWindow();   // yop en premier plan!
  //m_ctl_iplog.ModifyStyle(WS_DLGFRAME,WS_CAPTION|WS_BORDER);
  ///m_ctl_iplog.ModifyStyle(WS_DLGFRAME,WS_BORDER);
  ///m_ctl_iplog.ModifyStyleEx(0,WS_EX_CLIENTEDGE|WS_EX_DLGMODALFRAME);
  //m_ctl_iplog.SetReadOnly(FALSE);     // en blanc

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    //SetDlgItemTextCP(this, ,"");
    SetDlgItemTextCP(this, IDC_STATIC_type,LANG(LANG_E4)); // "Type d'infos:");
    SetDlgItemTextCP(this, IDC_logtype,LANG(LANG_E5)); // "Erreurs");
    SetDlgItemTextCP(this, IDC_logtype2,LANG(LANG_E6)); // "Infos");
    SetDlgItemTextCP(this, IDC_FIND,LANG(LANG_E7)); // "Find");
    //SetDlgItemTextCP(this, IDOK,LANG(LANG_CLOSE)); // "FERMER");
    SetCombo(this,IDC_HIDEINFO,LISTDEF_2);
  }

  // démarrer timer
  StartTimer();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

// Resize
void Ciplog::OnSize(UINT nType, int cx, int cy) 
{
  CDialog::OnSize(nType, cx, cy);

  HWND ip = ::GetDlgItem(this->m_hWnd,IDC_log);
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
  }
}


void Ciplog::OnDestroy() 
{
  if (fp) {
    fclose(fp);	
    fp=NULL;
  }
	CDialog::OnDestroy();	
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
BOOL Ciplog::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
char* Ciplog::GetTip(int ID)
{
  switch(ID) {
    case IDC_logtype:  return LANG(LANG_E1 /*"View error and warning reports"*/); break;
    case IDC_logtype2: return LANG(LANG_E2 /*"View info report"*/); break;
    case IDC_FIND:     return LANG(LANG_E8); break; 
    //case IDOK:         return LANG(LANG_E3 /*"Close the log window"*/); break;
    //case : return ""; break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------


// Appel aide
void Ciplog::OnHelpInfo2() {
  (void) OnHelpInfo(NULL);
}

BOOL Ciplog::OnHelpInfo(HELPINFO* dummy) 
{
  //return CDialog::OnHelpInfo(pHelpInfo);
  HtsHelper->Help();
  return true;
  //AfxGetApp()->WinHelp(0,HELP_FINDER);    // Index du fichier Hlp
  //return true;
}

void Ciplog::Onchangelog() 
{
  if (type_log)
    type_log=0;
  else
    type_log=1;  
}

void Ciplog::OnTimer(UINT_PTR nIDEvent) 
{
  AffLogRefresh();
	CDialog::OnTimer(nIDEvent);
}

void Ciplog::StartTimer() {
  if (!timer) {
    timer=SetTimer(WM_TIMER,100,NULL);
  }
}
void Ciplog::StopTimer() {
  if (timer) {
    KillTimer(timer);
    timer=0;
  }
}

void Ciplog::OnSelchangeHideinfo() 
{
  int r=m_ctl_hideinfo.GetCurSel();
  switch(r) {
  case CB_ERR : break;  // pas de sélection
  default:
    type_filter=r;
  break;
  }
}

void Ciplog::OnFind() 
{
  CWaitCursor wait;
  CString find;
  GetDlgItemText(IDC_FINDSTR,find);
  find.MakeLower();
  if (find.GetLength()) {
    CString data;
    GetDlgItemText(IDC_log,data);
    data.MakeLower();
    int pos=data.Find(find);
    if (pos>=0) {
      CHARRANGE cr;
      cr.cpMin = pos;
      cr.cpMax = pos + 1;
      //m_ctl_iplog.SetScrollPos(SB_VERT,m_ctl_iplog.LineFromChar(pos));
      m_ctl_iplog.Clear();
      //m_ctl_iplog.SetSel(pos,pos+100);
      m_ctl_iplog.SendMessage(EM_SETSEL,0,pos);
      m_ctl_iplog.SendMessage(EM_EXSETSEL,0,(LPARAM) &cr);
      m_ctl_iplog.SendMessage(EM_SCROLLCARET,0,0);
      m_ctl_iplog.SendMessage(EM_SETSEL,-1,-1);

      //m_ctl_iplog.RedrawWindow();
    }
  }
}
