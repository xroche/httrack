// OptionTab4.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "OptionTab4.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COptionTab4 property page

IMPLEMENT_DYNCREATE(COptionTab4, CPropertyPage)

COptionTab4::COptionTab4() : CPropertyPage(COptionTab4::IDD)
{
  // Patcher titre
  if (LANG_T(-1)) {    // Patcher en français
    m_psp.pszTitle=LANG(LANG_IOPT4); // titre
    m_psp.dwFlags|=PSP_USETITLE;
  }
  m_psp.dwFlags|=PSP_HASHELP;
  //
	//{{AFX_DATA_INIT(COptionTab4)
	m_connexion = _T("");
	m_remt = FALSE;
	m_retry = _T("");
	m_timeout = _T("");
	m_rems = FALSE;
	m_rate = _T("");
	m_ka = FALSE;
	//}}AFX_DATA_INIT
}

COptionTab4::~COptionTab4()
{
}

void COptionTab4::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionTab4)
	DDX_CBString(pDX, IDC_connexion, m_connexion);
	DDX_Check(pDX, IDC_remt, m_remt);
	DDX_CBString(pDX, IDC_retry, m_retry);
	DDX_CBString(pDX, IDC_timeout, m_timeout);
	DDX_Check(pDX, IDC_rems, m_rems);
	DDX_Text(pDX, IDC_rate, m_rate);
	DDX_Check(pDX, IDC_ka, m_ka);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionTab4, CPropertyPage)
	//{{AFX_MSG_MAP(COptionTab4)
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionTab4 message handlers

BOOL COptionTab4::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
  EnableToolTips(true);     // TOOL TIPS
	
  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    SetDlgItemTextCP(this, IDC_STATIC_flowc,LANG(LANG_I41)); // "Contrôle du flux");
    SetDlgItemTextCP(this, IDC_STATIC_mc,LANG(LANG_I44)); // "N# connexions");
    SetDlgItemTextCP(this, IDC_remt,LANG(LANG_I45)); // "Abandon hote si erreur");
    SetDlgItemTextCP(this, IDC_STATIC_retr,LANG(LANG_I48)); // "Essais");
    SetDlgItemTextCP(this, IDC_STATIC_mintr,LANG(LANG_I46)); // "Taux min transfert (B/s)");
    SetDlgItemTextCP(this, IDC_rems,LANG(LANG_I47)); // "Abandon hote si lent");
    SetDlgItemTextCP(this, IDC_ka,LANG(LANG_I47e));
    SetDlgItemTextCP(this, IDC_STATIC_timeout,LANG_I47d); // TimeOut(s)
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
BOOL COptionTab4::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
char* COptionTab4::GetTip(int ID)
{
  switch(ID) {
    case IDC_connexion: return LANG(LANG_I12); break; // "Maximum number of connections","Nombre maximal de connexions"); break;
    case IDC_timeout: return LANG(LANG_I13); break; // "Maximum idle time for a file","Temps mort maximal pour un fichier"); break;
    case IDC_remt:    return LANG(LANG_I14); break; // "Cancel all links from a host if a timeout occurs","Annuler tous les liens sur un domaine en cas de temps mort"); break;
    case IDC_retry:   return LANG(LANG_I17); break; // "Maximum number of retries if a non-fatal error occurs","Nombre maximum d'essais en cas d'erreur non fatale"); break;
    case IDC_rate:    return LANG(LANG_I15); break; // "Minimum transfer rate tolerated","Taux de transfert minimal toléré"); break;
    case IDC_rems:    return LANG(LANG_I16); break; // "Cancel all links from a host if it is too slow","Annuler tous les liens sur un domaine en cas de transfert trop lent"); break;
    case IDC_ka:      return LANG(LANG_I47f); break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------

