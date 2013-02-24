// AddFilter.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "AddFilter.h"

extern "C" {
  #include "HTTrackInterface.h"
  //#include "htsbase.h"
};

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern HICON httrack_icon;
// Helper
extern LaunchHelp* HtsHelper;

/////////////////////////////////////////////////////////////////////////////
// CAddFilter dialog


CAddFilter::CAddFilter(CWnd* pParent /*=NULL*/)
	: CDialog(CAddFilter::IDD, pParent)
{
	//{{AFX_DATA_INIT(CAddFilter)
	m_addtype = _T("");
	m_afquery = _T("");
	m_aftype = -1;
	//}}AFX_DATA_INIT
}


void CAddFilter::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAddFilter)
	DDX_Control(pDX, IDC_affkeyword, m_ctl_affkeyword);
	DDX_Control(pDX, IDC_AFext, m_ctl_afext);
	DDX_Control(pDX, IDOK, m_ctl_ok);
	DDX_Control(pDX, IDC_AFtype, m_ctl_aftype);
	DDX_Text(pDX, IDC_EDIT1, m_addtype);
	DDX_Text(pDX, IDC_AFext, m_afquery);
	DDX_CBIndex(pDX, IDC_AFtype, m_aftype);
	//}}AFX_DATA_MAP
  m_ctl_ok.ModifyStyle(0,WS_DISABLED);
}


BEGIN_MESSAGE_MAP(CAddFilter, CDialog)
	//{{AFX_MSG_MAP(CAddFilter)
	ON_CBN_SELCHANGE(IDC_AFtype, OnSelchangeAFtype)
	ON_WM_HELPINFO()
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
  ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
  ON_COMMAND(ID_HELP,OnHelpInfo2)
	ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAddFilter message handlers

BOOL CAddFilter::OnInitDialog() 
{
	CDialog::OnInitDialog();
  SetIcon(httrack_icon,false);
  SetIcon(httrack_icon,true);  
  EnableToolTips(true);     // TOOL TIPS	

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    //SetDlgItemTextCP(this, ,"");
    SetWindowTextCP(this, LANG(LANG_B5) /*"Ajouter un filtre"*/);
    SetDlgItemTextCP(this, IDC_STATIC_rule,LANG(LANG_B6) /*"Choisir une règle:"*/);
    SetDlgItemTextCP(this, IDC_affkeyword,LANG(LANG_B7) /*"Entrer un mot clé:"*/);
    SetDlgItemTextCP(this, IDCANCEL,LANG(LANG_CANCEL) /*"Annuler"*/);
    SetDlgItemTextCP(this, IDOK,LANG(LANG_B8) /*"Ajouter"*/);
    SetCombo(this,IDC_AFtype,LISTDEF_1);
  }

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CAddFilter::OnSelchangeAFtype() 
{
  char hlp[256];
  int r;
  strcpybuff(hlp,LANG(LANG_B22)); //  "Example: ");
  r=m_ctl_aftype.GetCurSel();
  switch(r) {
  case CB_ERR : break;  // pas de sélection
  case 0:
    strcatbuff(hlp,LANG(LANG_B23)); // "gif\x0d\x0aWill detect all gif files (or GIF files)");
    break;
  case 1:
    strcatbuff(hlp,LANG(LANG_B24)); // "blue\x0d\x0aWill detect all files containing blue, such as 'bluesky-small.jpeg'");
    break;
  case 2:
    strcatbuff(hlp,LANG(LANG_B25)); // "bigfile.mov\x0d\x0aWill detect the file 'bigfile.mov', but not 'bigfile2.mov'");
    break;
  case 3:
    strcatbuff(hlp,LANG(LANG_B26)); // "cgi\x0d\x0aWill detect links with folder containing 'cgi' such as /cgi-bin/mycgi.cgi");
    break;
  case 4:
    strcatbuff(hlp,LANG(LANG_B27)); // "cgi-bin\x0d\x0aWill detect links with folder name 'cgi-bin' (but not cgi-bin-2, for example)");
    break;
  case 5:
    strcatbuff(hlp,LANG(LANG_B28)); // "myweb.com\x0d\x0aWill detect all links like www.myweb.com, private.myweb.com etc.");
    break;
  case 6:
    strcatbuff(hlp,LANG(LANG_B29)); // "myweb\x0d\x0aWill detect all links like www.myweb.com, www.myweb.edu, private.myweb.otherweb.com etc.");
    break;
  case 7:
    strcatbuff(hlp,LANG(LANG_B30)); // "www.myweb.com\x0d\x0aWill detect all links like www.myweb.com/... (but not links like private.myweb.com/..)");
    break;
  case 8:
    strcatbuff(hlp,LANG(LANG_B31)); // "myweb\x0d\x0aWill detect all links like www.myweb.com/.., www.test.abc/frommyweb/index.html, www.test.abc/test/myweb.html etc.");
    break;
  case 9:
    strcatbuff(hlp,LANG(LANG_B32)); // "www.test.com/test/myweb.html\x0d\x0aWill only detect the link www.test.com/test/myweb.html. Note that you have to type both address (www.xxx.yyy) and path (/test/myweb.html)");
    break;
  case 10:
    strcatbuff(hlp,LANG(LANG_B33)); // "All links will be forbidden/accepted");
    break;
  }
  SetDlgItemTextCP(this, IDC_EDIT2,hlp);
  if (r!=10) {
    m_ctl_afext.ModifyStyle(WS_DISABLED,0);
    m_ctl_affkeyword.ModifyStyle(WS_DISABLED,0);
  }
  else {
    m_ctl_afext.ModifyStyle(0,WS_DISABLED);
    m_ctl_affkeyword.ModifyStyle(0,WS_DISABLED);
  }
  m_ctl_afext.RedrawWindow();
  m_ctl_affkeyword.RedrawWindow();
  
  if (r!=CB_ERR ) {
    m_ctl_ok.ModifyStyle(WS_DISABLED,0);
  }
  else {
    m_ctl_ok.ModifyStyle(0,WS_DISABLED);
  }
  m_ctl_ok.RedrawWindow();
  
}


void CAddFilter::OnOK() 
{
  CString st;
  int r=m_ctl_aftype.GetCurSel();
	GetDlgItemText(IDC_AFext,st);
  if ((strlen(st)>0) || (r==10)) {
    CDialog::OnOK();
  } else {
    AfxMessageBox(
      LANG(LANG_A2) /*"Please enter one or several keyword(s) for the rule"*/
      ,MB_OK+MB_ICONINFORMATION);
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
BOOL CAddFilter::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
char* CAddFilter::GetTip(int ID)
{
  switch(ID) {
    case IDC_AFtype: return LANG(LANG_B1); /*"Select a rule for the filter","Choisissez une règle pour le filtre");*/ break;
    case IDC_AFext:  return LANG(LANG_B2); /*"Enter here keywords for the filter","Entrez ici un mot clé pour le filtre");*/ break;
    case IDCANCEL:   return LANG(LANG_B3); /*"Cancel","Annuler");*/ break;
    case IDOK:       return LANG(LANG_B4); /*"Add this filter","Ajouter cette règle");*/ break;
    //case : return ""; break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------


// Appel aide
void CAddFilter::OnHelpInfo2() {
  (void)OnHelpInfo(NULL);
}

BOOL CAddFilter::OnHelpInfo(HELPINFO* dummy) 
{
  //return CDialog::OnHelpInfo(pHelpInfo);
  HtsHelper->Help();
  return true;
  //AfxGetApp()->WinHelp(0,HELP_FINDER);    // Index du fichier Hlp
  //return true;
}
