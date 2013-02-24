// OptionTab3.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "OptionTab3.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COptionTab3 property page

IMPLEMENT_DYNCREATE(COptionTab3, CPropertyPage)

COptionTab3::COptionTab3() : CPropertyPage(COptionTab3::IDD)
{
  // Patcher titre
  if (LANG_T(-1)) {    // Patcher en français
    m_psp.pszTitle=LANG(LANG_IOPT3); // titre
    m_psp.dwFlags|=PSP_USETITLE;
  }
  m_psp.dwFlags|=PSP_HASHELP;
  //
	//{{AFX_DATA_INIT(COptionTab3)
	m_filter = -1;
	m_travel = -1;
	m_travel2 = -1;
	m_windebug = FALSE;
	m_cache = FALSE;
	m_travel3 = -1;
	//}}AFX_DATA_INIT
}

COptionTab3::~COptionTab3()
{
}

void COptionTab3::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionTab3)
	DDX_Control(pDX, IDC_travel3, m_ctl_travel3);
	DDX_Control(pDX, IDC_travel2, m_ctl_travel2);
	DDX_Control(pDX, IDC_travel, m_ctl_travel);
	DDX_Control(pDX, IDC_filter, m_ctl_filter);
	DDX_CBIndex(pDX, IDC_filter, m_filter);
	DDX_CBIndex(pDX, IDC_travel, m_travel);
	DDX_CBIndex(pDX, IDC_travel2, m_travel2);
	DDX_Check(pDX, IDC_windebug, m_windebug);
	DDX_Check(pDX, IDC_Cache, m_cache);
	DDX_CBIndex(pDX, IDC_travel3, m_travel3);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionTab3, CPropertyPage)
	//{{AFX_MSG_MAP(COptionTab3)
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionTab3 message handlers

BOOL COptionTab3::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
  EnableToolTips(true);     // TOOL TIPS
	
  // mode modif à la volée
  if (modify==1) {
    GetDlgItem(IDC_Cache)   ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_filter)  ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_travel)  ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_travel2) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_travel3) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_STATIC_warning) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_STATIC_primf)   ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_STATIC_travel)  ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_STATIC_travel2) ->ModifyStyle(0,WS_DISABLED);
  } else {
    GetDlgItem(IDC_Cache)   ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_filter)  ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_travel)  ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_travel2) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_STATIC_warning) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_STATIC_primf)   ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_STATIC_travel)  ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_STATIC_travel2) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_STATIC_travel3) ->ModifyStyle(WS_DISABLED,0);
  }
  
  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    SetDlgItemTextCP(this, IDC_Cache,LANG(LANG_I34)); // "Utiliser un cache");
    SetDlgItemTextCP(this, IDC_STATIC_primf,LANG(LANG_I39)); // "Filtre primaire");
    SetDlgItemTextCP(this, IDC_STATIC_travel,LANG(LANG_I40)); // "Mode de parcours");
    SetDlgItemTextCP(this, IDC_STATIC_travel2,LANG(LANG_I40b)); // "Mode de parcours");
    SetDlgItemTextCP(this, IDC_STATIC_warning,LANG(LANG_I40c));
    SetDlgItemTextCP(this, IDC_STATIC_travel3,LANG(LANG_I40e));
    SetDlgItemTextCP(this, IDC_windebug,LANG_I40d);  // Activate debug mode (winhttrack.log)
    SetCombo(this,IDC_filter,LISTDEF_4);
    SetCombo(this,IDC_travel,LISTDEF_5);
    SetCombo(this,IDC_travel2,LISTDEF_6);
    SetCombo(this,IDC_travel3,LISTDEF_11);
  }
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
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
BOOL COptionTab3::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
char* COptionTab3::GetTip(int ID)
{
  switch(ID) {
    case IDC_Cache:   return LANG(LANG_I5); break; // "Use a cache for updates and retries","Utiliser un cache pour les mise à jour et reprises"); break;
    case IDC_filter:  return LANG(LANG_I10); break; // "Select file types to be saved on disk","Sélection des types de fichier à sauver"); break;
    case IDC_travel:  return LANG(LANG_I11); break; // "Select the travel direction in the site","Sélection de la direction du miroir"); break;
    case IDC_travel2:  return LANG(LANG_I11b); break; // "Select the travel direction in the site","Sélection de la direction du miroir"); break;
    case IDC_travel3:  return LANG(LANG_I11c); break;
    case IDC_windebug: return LANG_I1h; break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------

