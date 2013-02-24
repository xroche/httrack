// about.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "about.h"
//#include "about_sh.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//extern "C" {
//  #include "htsbase.h"
//}

extern HICON httrack_icon;
extern CString _HTTRACK_VERSION;

extern int LANG_T(int);
// Helper
extern LaunchHelp* HtsHelper;
extern "C" {
  #include "HTTrackInterface.h"
  #include "httrack-library.h"
}

/////////////////////////////////////////////////////////////////////////////
// Cabout dialog


Cabout::Cabout(CWnd* pParent /*=NULL*/)
	: CDialog(Cabout::IDD, pParent)
{
	//{{AFX_DATA_INIT(Cabout)
	m_infover = _T("");
	m_lang = -1;
	//}}AFX_DATA_INIT
}


void Cabout::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(Cabout)
	DDX_Control(pDX, IDC_SPLASH, m_splash);
	DDX_Control(pDX, IDC_lang, m_ctl_lang);
	DDX_Text(pDX, IDC_INFOVER, m_infover);
	DDX_CBIndex(pDX, IDC_lang, m_lang);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(Cabout, CDialog)
	//{{AFX_MSG_MAP(Cabout)
	ON_CBN_SELCHANGE(IDC_lang, OnSelchangelang)
	ON_WM_HELPINFO()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
  ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
  ON_COMMAND(ID_HELP,OnHelpInfo2)
	ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Cabout message handlers

BOOL Cabout::OnInitDialog() 
{
  //m_lang=LANG_T(-1);    // langue?
  CDialog::OnInitDialog();

  WINDOWPLACEMENT wp;
  m_splash.GetWindowPlacement(&wp);
  wp.rcNormalPosition.right=wp.rcNormalPosition.left+300+1;
  wp.rcNormalPosition.bottom=wp.rcNormalPosition.top+69+1; 
  m_splash.SetWindowPlacement(&wp);

  m_ctl_lang.ResetContent();
  {
    char lang_str[1024];
    //int old_lang=LANG_T(-1);
    int i=0;
    int curr_lng=LANG_T(-1);
    QLANG_T(0);
    strcpybuff(lang_str,"LANGUAGE_NAME");
    LANG_LOAD(lang_str);    // 0 english 1 français..
    while(strlen(lang_str)) {
      m_ctl_lang.AddString(lang_str);
      i++;
      QLANG_T(i);
      strcpybuff(lang_str,"LANGUAGE_NAME");
      LANG_LOAD(lang_str);    // 0 english 1 français..
    }
    QLANG_T(curr_lng);
    //LANG_T(min(old_lang,i-1));
  }

  /* sel */
  int i;
  int max=m_ctl_lang.GetCount();
  m_ctl_lang.SetCurSel(-1);
  for(i=0;i<max;i++) {
    CString st;
    m_ctl_lang.GetLBText(i,st);
    if (strcmp(st,LANGUAGE_NAME)==0) {
      m_ctl_lang.SetCurSel(i);
    }
  }
  
  EnableToolTips(true);     // TOOL TIPS
  setlang();
  SetIcon(httrack_icon,false);
  SetIcon(httrack_icon,true);  

  SetForegroundWindow();   // yop en premier plan!

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void Cabout::setlang() {
  const char* avail = hts_is_available();
  CString info = "WinHTTrack Website Copier ";
  info+= _HTTRACK_VERSION;
  if (avail && *avail) {
    info+=" (";
    info+= avail;
    info+=")";
  }
  info+=LANG(LANG_K1);
  info+="\r\n\r\n";
  info+=LANG_K3;
  info+=" :\r\n";
  info+=HTTRACK_WEB;
  SetDlgItemTextCP(this, IDC_INFOVER,info);
  //
  SetWindowText(LANG_K2);
  //SetDlgItemTextCP(this, IDC_visit,LANG_K3);
}

void Cabout::OnOK() 
{
  /*
  DWORD a;
  a=m_ctl_hot.GetHotKey();
  int vcc=LOWORD(a);
  int mod=HIWORD(a);
  */
	CDialog::OnOK();
}

void Cabout::OnSelchangelang() 
{
  int r;
  int old_lang=LANG_T(-1);
  r = m_ctl_lang.GetCurSel();
  if (r!=CB_ERR) {
    CString st;
    m_ctl_lang.GetLBText(r,st);
    int i;
    int max=m_ctl_lang.GetCount();
    for(i=0;i<max;i++) {
      char lang_str[1024];

      QLANG_T(i);
      strcpybuff(lang_str,"LANGUAGE_NAME");
      LANG_LOAD(lang_str);    // 0 english 1 français..

      //LANG_T(i);    // 0 english 1 français..
      if (strcmp(st,lang_str)==0) {
        LANG_T(i);
        setlang();
        if (i != old_lang)
          AfxMessageBox("Please restart WinHTTrack so that your language preferences can be updated!");
        return;
      }
    }
    /* error */
    LANG_T(0);    // 0 english 1 français..
  }
  setlang();
}

// Appel aide
void Cabout::OnHelpInfo2() {
  (void)OnHelpInfo(NULL);
}

BOOL Cabout::OnHelpInfo(HELPINFO* dummy) 
{
  //return CDialog::OnHelpInfo(pHelpInfo);
  HtsHelper->Help();
  return true;
  //AfxGetApp()->WinHelp(0,HELP_FINDER);    // Index du fichier Hlp
  //return true;
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
BOOL Cabout::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
char* Cabout::GetTip(int ID)
{
  switch(ID) {
    case IDC_lang:   return LANG_U1; break;
    case IDC_abouticon: return LANG_K3 ; break;
    case IDCANCEL:   return LANG(LANG_TIPCANCEL); break;
    case IDOK:       return LANG(LANG_TIPOK); break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------

void Cabout::OnLButtonDown(UINT nFlags, CPoint point) 
{
  int id=0;
  CWnd* w=ChildWindowFromPoint(point);
  if (w)
    id=w->GetDlgCtrlID();

  // Select
  switch(id) {
    case 0: break;
    case IDC_SPLASH:
      this->ClientToScreen(&point);
      w->ScreenToClient(&point);
    if ((point.y>=100) && (point.y<=110) || (point.y<=80)) {
      if (!ShellExecute(NULL,"open","http://www.httrack.com","","",SW_RESTORE)) {
      }
    }
    break;
  }

  CDialog::OnLButtonDown(nFlags, point);
}

void Cabout::OnMouseMove(UINT nFlags, CPoint point) 
{
  int id=0;
  CWnd* w=ChildWindowFromPoint(point);
  if (w)
    id=w->GetDlgCtrlID();

  // Select
  switch(id) {
  case 0: break;
  case IDC_SPLASH:
    this->ClientToScreen(&point);
    w->ScreenToClient(&point);
    HCURSOR curs=NULL;
    if (
         (point.y>=120) && (point.y<=140)
      || (point.y<=80)
      || (point.y>=100) && (point.y<=110)
      ) {
      curs=AfxGetApp()->LoadCursor(IDC_CURSWWW);
    } else {
      curs=AfxGetApp()->LoadStandardCursor(IDC_ARROW);
    }
    if (curs) {
      //if (curs != currentCurs) {
        SetCursor(curs);
        // currentCurs=curs;
      //}
    }
  }  

	CDialog::OnMouseMove(nFlags, point);
}
