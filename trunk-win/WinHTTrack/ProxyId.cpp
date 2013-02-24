// ProxyId.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "ProxyId.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern HICON httrack_icon;

// Helper
extern LaunchHelp* HtsHelper;

/////////////////////////////////////////////////////////////////////////////
// CProxyId dialog


CProxyId::CProxyId(CWnd* pParent /*=NULL*/)
	: CDialog(CProxyId::IDD, pParent)
{
	//{{AFX_DATA_INIT(CProxyId)
	m_proxadr = _T("");
	m_proxlogin = _T("");
	m_proxpass = _T("");
	m_proxport = _T("");
	//}}AFX_DATA_INIT
}


void CProxyId::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CProxyId)
	DDX_Text(pDX, IDC_proxadr, m_proxadr);
	DDX_Text(pDX, IDC_proxlogin, m_proxlogin);
	DDX_Text(pDX, IDC_proxpass, m_proxpass);
	DDX_Text(pDX, IDC_proxport, m_proxport);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CProxyId, CDialog)
	//{{AFX_MSG_MAP(CProxyId)
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
  ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
  ON_COMMAND(ID_HELP,OnHelpInfo2)
	ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
END_MESSAGE_MAP()


BOOL CProxyId::OnInitDialog() 
{
	CDialog::OnInitDialog();
  SetIcon(httrack_icon,false);
  SetIcon(httrack_icon,true);
  EnableToolTips(true);     // TOOL TIPS
  SetForegroundWindow();   // yop en premier plan!
	
  if (LANG_T(-1)) {    // Patcher en français
    SetWindowTextCP(this,  LANG(LANG_R1));
    SetDlgItemTextCP(this, IDC_STATIC_adr,LANG(LANG_R2));
    SetDlgItemTextCP(this, IDC_STATIC_port,LANG(LANG_R3));
    SetDlgItemTextCP(this, IDC_STATIC_auth,LANG(LANG_R4));
    SetDlgItemTextCP(this, IDC_STATIC_login,LANG(LANG_R5));
    SetDlgItemTextCP(this, IDC_STATIC_pass,LANG(LANG_R6));
    SetDlgItemTextCP(this, IDOK,LANG(LANG_OK));
    SetDlgItemTextCP(this, IDCANCEL,LANG(LANG_CANCEL));
  }

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


// Appel aide
void CProxyId::OnHelpInfo2() {
  (void) OnHelpInfo(NULL);
}

BOOL CProxyId::OnHelpInfo(HELPINFO* dummy) 
{
  //return CDialog::OnHelpInfo(pHelpInfo);
  //AfxGetApp()->WinHelp(0,HELP_FINDER);    // Index du fichier Hlp
  //LaunchHelp(pHelpInfo);
  HtsHelper->Help();
  return true;
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
BOOL CProxyId::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
char* CProxyId::GetTip(int ID)
{
  switch(ID) {
    case IDC_proxadr:    return LANG(LANG_R10); break;
    case IDC_proxport:   return LANG(LANG_R11); break;
    case IDC_proxlogin:  return LANG(LANG_R12); break;
    case IDC_proxpass:   return LANG(LANG_R13); break;
    case IDOK:           return LANG(LANG_TIPOK); break;      
    case IDCANCEL:       return LANG(LANG_TIPCANCEL); break;
    //case : return ""; break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------

