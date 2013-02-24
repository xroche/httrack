// OptionTab5.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "OptionTab5.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COptionTab5 property page

IMPLEMENT_DYNCREATE(COptionTab5, CPropertyPage)

COptionTab5::COptionTab5() : CPropertyPage(COptionTab5::IDD)
{
  // Patcher titre
  if (LANG_T(-1)) {    // Patcher en français
    m_psp.pszTitle=LANG(LANG_IOPT5); // titre
    m_psp.dwFlags|=PSP_USETITLE;
  }
  m_psp.dwFlags|=PSP_HASHELP;
  //
	//{{AFX_DATA_INIT(COptionTab5)
	m_maxhtml = _T("");
	m_maxrate = _T("");
	m_maxtime = _T("");
	m_othermax = _T("");
	m_sizemax = _T("");
	m_depth = _T("");
	m_maxconn = _T("");
	m_depth2 = _T("");
	m_pausebytes = _T("");
	m_maxlinks = _T("");
	//}}AFX_DATA_INIT
}

COptionTab5::~COptionTab5()
{
}

void COptionTab5::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionTab5)
	DDX_Control(pDX, IDC_pausebytes, m_ctl_pausebytes);
	DDX_Control(pDX, IDC_depth2, m_ctl_depth2);
	DDX_Control(pDX, IDC_depth, m_ctl_depth);
	DDX_Text(pDX, IDC_maxhtml, m_maxhtml);
	DDX_CBString(pDX, IDC_maxrate, m_maxrate);
	DDX_CBString(pDX, IDC_maxtime, m_maxtime);
	DDX_Text(pDX, IDC_othermax, m_othermax);
	DDX_Text(pDX, IDC_sizemax, m_sizemax);
	DDX_CBString(pDX, IDC_depth, m_depth);
	DDX_CBString(pDX, IDC_maxconn, m_maxconn);
	DDX_CBString(pDX, IDC_depth2, m_depth2);
	DDX_Text(pDX, IDC_pausebytes, m_pausebytes);
	DDX_CBString(pDX, IDC_maxlinks, m_maxlinks);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionTab5, CPropertyPage)
	//{{AFX_MSG_MAP(COptionTab5)
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionTab5 message handlers

BOOL COptionTab5::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
  EnableToolTips(true);     // TOOL TIPS
	
  depth_status=-1;

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    SetDlgItemTextCP(this, IDC_STATIC_limits,LANG(LANG_I42)); // "Limites");
    SetDlgItemTextCP(this, IDC_STATIC_fillim,LANG(LANG_I49)); // "Limite en taille");
    SetDlgItemTextCP(this, IDC_STATIC_autre,LANG(LANG_I50)); // "Autres:");
    SetDlgItemTextCP(this, IDC_STATIC_autre2,LANG(LANG_I50b)); // "Autres:");
    SetDlgItemTextCP(this, IDC_STATIC_sitesize,LANG(LANG_I51)); // "Taille site max");
    SetDlgItemTextCP(this, IDC_STATIC_tempmax,LANG(LANG_I52)); // "Temps max");
    SetDlgItemTextCP(this, IDC_STATIC_ratemax,LANG(LANG_I54));
    SetDlgItemTextCP(this, IDC_infodepth,LANG(LANG_G32)); // "Profondeur maximale:");
    SetDlgItemTextCP(this, IDC_infodepth2,LANG(LANG_G32b));
    SetDlgItemTextCP(this, IDC_STATIC_maxconn,LANG_I64);
    SetDlgItemTextCP(this, IDC_STATIC_pausebytes,LANG_I65); // Pause after downloading..
    SetDlgItemTextCP(this, IDC_STATIC_maxlinks,LANG_I64b);
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
BOOL COptionTab5::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
char* COptionTab5::GetTip(int ID)
{
  switch(ID) {
    case IDC_depth: return LANG_I1g; break;
    case IDC_depth2: return LANG_I1g2; break;
    case IDC_maxhtml: return LANG(LANG_I18); break; // "Maximum size for an html page","Taille maximale pour une page html"); break;
    case IDC_othermax: return LANG(LANG_I19); break; // "Maximum size for a file","Taille maximale pour un fichier"); break;
    case IDC_sizemax: return LANG(LANG_I20); break; // "Maximum amount of bytes to retrieve from the Web","Taille totale maximale d'un miroir"); break;
    case IDC_pausebytes: return LANG(LANG_I20b); break;
    case IDC_maxtime: return LANG(LANG_I21); break; // "Maximum time for the mirror","Temps total maximum pour un miroir"); break;
    case IDC_maxrate: return LANG(LANG_I22); break; // "Maximum transfer rate","Taux de transfert maximum"); break;
    case IDC_maxconn: return LANG(LANG_I22b); break; // "Maximum transfer rate","Taux de transfert maximum"); break;
    case IDC_maxlinks: return LANG(LANG_I22c); break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------

/*
void COptionTab5::OnSelchangedepth() 
{
  CString st="";
  char tempo[8192];
  // TODO: If this is a RICHEDIT control, the control will not
  // send this notification unless you override the CDialog::OnInitDialog()
  // function to send the EM_SETEVENTMASK message to the control
  // with the ENM_CHANGE flag ORed into the lParam mask.
  
  // TODO: Add your control notification handler code here
  GetDlgItemText(IDC_depth,st);
  strcpybuff(tempo,st);
  
  if ((strlen(tempo)>0)!=depth_status) {
    depth_status=!depth_status;
    
    if (strlen(tempo)>0) {
      m_ctl_depth.ModifyStyle(WS_DISABLED,0);
    }
    else {
      m_ctl_depth.ModifyStyle(0,WS_DISABLED);
    }
    m_ctl_depth.RedrawWindow();
  }  
}
*/
