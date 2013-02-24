// OptionTab2.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "OptionTab2.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COptionTab2 property page

IMPLEMENT_DYNCREATE(COptionTab2, CPropertyPage)

COptionTab2::COptionTab2() : CPropertyPage(COptionTab2::IDD)
{
  // Patcher titre
  if (LANG_T(-1)) {    // Patcher en français
    m_psp.pszTitle=LANG(LANG_IOPT2); // titre
    m_psp.dwFlags|=PSP_USETITLE;
  }
  m_psp.dwFlags|=PSP_HASHELP;
  //
	//{{AFX_DATA_INIT(COptionTab2)
	m_build = -1;
	m_dos = FALSE;
	m_errpage = FALSE;
	m_external = FALSE;
	m_nopurge = FALSE;
	m_hidepwd = FALSE;
	m_hidequery = FALSE;
	m_iso9660 = FALSE;
	//}}AFX_DATA_INIT
}

COptionTab2::~COptionTab2()
{
}

void COptionTab2::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionTab2)
	DDX_Control(pDX, IDC_build, m_ctl_build);
	DDX_Control(pDX, IDC_buildopt, m_buildopt);
	DDX_CBIndex(pDX, IDC_build, m_build);
	DDX_Check(pDX, IDC_dos, m_dos);
	DDX_Check(pDX, IDC_errpage, m_errpage);
	DDX_Check(pDX, IDC_external, m_external);
	DDX_Check(pDX, IDC_nopurge, m_nopurge);
	DDX_Check(pDX, IDC_hidepwd, m_hidepwd);
	DDX_Check(pDX, IDC_hidequery, m_hidequery);
	DDX_Check(pDX, IDC_iso9660, m_iso9660);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionTab2, CPropertyPage)
	//{{AFX_MSG_MAP(COptionTab2)
	ON_BN_CLICKED(IDC_buildopt, Onbuildopt)
	ON_CBN_SELCHANGE(IDC_build, OnSelchangebuild)
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionTab2 message handlers

void COptionTab2::Onbuildopt() 
{
  if (Bopt.DoModal()==IDOK) {
    if (AfxMessageBox(LANG(LANG_DIAL6),MB_OKCANCEL+MB_ICONQUESTION)==IDOK) {
      m_ctl_build.SetCurSel(14);
      AfxMessageBox(LANG(LANG_DIAL7),MB_OK);
    }
  }
}

BOOL COptionTab2::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
  EnableToolTips(true);     // TOOL TIPS
	
  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    //SetDlgItemTextCP(this, ,"");
    SetDlgItemTextCP(this, IDC_STATIC_buildopt,LANG(LANG_I33)); // "Type de structure (manière dont les liens sont sauvés)");
    SetDlgItemTextCP(this, IDC_dos,LANG(LANG_I37)); // "Noms DOS");
    SetDlgItemTextCP(this, IDC_iso9660,LANG(LANG_I37b)); // "Noms DOS");
    SetDlgItemTextCP(this, IDC_errpage,LANG(LANG_I38)); // "Pas de page d'erreur");
    SetDlgItemTextCP(this, IDC_external,LANG(LANG_I56)); // "Pas de page externes");
    SetDlgItemTextCP(this, IDC_nopurge,LANG(LANG_I57)); // "nopurge");
    SetDlgItemTextCP(this, IDC_buildopt,LANG_O2);
    SetDlgItemTextCP(this, IDC_hidepwd,LANG(LANG_I66)); // "Hide passwords");
    SetDlgItemTextCP(this, IDC_hidequery,LANG_I67);
    SetCombo(this,IDC_build,LISTDEF_3);
  }

  // mode modif à la volée
  if (modify==1) {
    GetDlgItem(IDC_build)   ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_buildopt)->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_dos)     ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_iso9660) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_errpage) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_nopurge) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_external)->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_hidepwd) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_hidequery)->ModifyStyle(0,WS_DISABLED);
  } else {
    GetDlgItem(IDC_build)   ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_buildopt)->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_dos)     ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_iso9660) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_errpage) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_nopurge) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_external)->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_hidepwd) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_hidequery)->ModifyStyle(WS_DISABLED,0);
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
BOOL COptionTab2::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
char* COptionTab2::GetTip(int ID)
{
  switch(ID) {
    case IDC_build:   return LANG(LANG_I3); break; // "Choose local site structure","Choisir la structure de fichiers local"); break;
    case IDC_buildopt:return LANG(LANG_I4); break; // "Set user-defined structure on disk","Définir la structure du site sur disque"); break;
    case IDC_dos:     return LANG(LANG_I8); break; // "Generate ONLY 8-3 filenames","Générer uniquement des fichiers courts 8-3"); break;
    case IDC_iso9660: return LANG(LANG_I8b); break; // "Generate ONLY ISO9660 filenames","Générer uniquement des fichiers courts 8-3"); break;
    case IDC_errpage: return LANG(LANG_I9); break; // "Do not write html error pages","Ne pas générer les fichiers d'erreur html"); break;
    case IDC_external: return LANG(LANG_I29); break; // extern
    case IDC_nopurge: return LANG(LANG_I1a); break; // erase old files
    case IDC_hidepwd: return LANG(LANG_I30); break;
    case IDC_hidequery: return LANG_I30b; break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------


void COptionTab2::OnSelchangebuild() 
{
  static int old=0;
  int r;
  r = m_ctl_build.GetCurSel();
  if (r!=CB_ERR) {
    if (r==m_ctl_build.GetCount()-1) {
      if (AfxMessageBox(LANG(LANG_DIAL8),MB_YESNO)==IDNO) {
        m_ctl_build.SetCurSel(old);
      }
    } else
      old=r;
  }
}

