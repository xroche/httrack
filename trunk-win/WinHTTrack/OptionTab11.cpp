// OptionTab11.cpp : implementation file
//

#include "stdafx.h"
#include "winhttrack.h"
#include "OptionTab11.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COptionTab11 property page

IMPLEMENT_DYNCREATE(COptionTab11, CPropertyPage)

COptionTab11::COptionTab11() : CPropertyPage(COptionTab11::IDD)
{
  // Patcher titre
  if (LANG_T(-1)) {    // Patcher en français
    m_psp.pszTitle=LANG(LANG_IOPT11); // titre
    m_psp.dwFlags|=PSP_USETITLE;
  }
  m_psp.dwFlags|=PSP_HASHELP;
	//{{AFX_DATA_INIT(COptionTab11)
	m_ext1 = _T("");
	m_ext2 = _T("");
	m_ext3 = _T("");
	m_ext4 = _T("");
	m_ext5 = _T("");
	m_ext6 = _T("");
	m_ext7 = _T("");
	m_ext8 = _T("");
	m_mime1 = _T("");
	m_mime2 = _T("");
	m_mime3 = _T("");
	m_mime4 = _T("");
	m_mime5 = _T("");
	m_mime6 = _T("");
	m_mime7 = _T("");
	m_mime8 = _T("");
	//}}AFX_DATA_INIT
}

COptionTab11::~COptionTab11()
{
}

void COptionTab11::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionTab11)
	DDX_CBString(pDX, IDC_ext1, m_ext1);
	DDX_CBString(pDX, IDC_ext2, m_ext2);
	DDX_CBString(pDX, IDC_ext3, m_ext3);
	DDX_CBString(pDX, IDC_ext4, m_ext4);
	DDX_CBString(pDX, IDC_ext5, m_ext5);
	DDX_CBString(pDX, IDC_ext6, m_ext6);
	DDX_CBString(pDX, IDC_ext7, m_ext7);
	DDX_CBString(pDX, IDC_ext8, m_ext8);
	DDX_CBString(pDX, IDC_mime1, m_mime1);
	DDX_CBString(pDX, IDC_mime2, m_mime2);
	DDX_CBString(pDX, IDC_mime3, m_mime3);
	DDX_CBString(pDX, IDC_mime4, m_mime4);
	DDX_CBString(pDX, IDC_mime5, m_mime5);
	DDX_CBString(pDX, IDC_mime6, m_mime6);
	DDX_CBString(pDX, IDC_mime7, m_mime7);
	DDX_CBString(pDX, IDC_mime8, m_mime8);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionTab11, CPropertyPage)
	//{{AFX_MSG_MAP(COptionTab11)
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionTab11 message handlers

BOOL COptionTab11::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	
  EnableToolTips(true);     // TOOL TIPS

  if (LANG_T(-1)) {    // Patcher en français
    SetDlgItemTextCP(this, IDC_STATIC_asso,LANG_W1);
    SetDlgItemTextCP(this, IDC_STATIC_filetype,LANG_W2);
    SetDlgItemTextCP(this, IDC_STATIC_mime,LANG_W3);
  }  

  SetWindowTextCP(this, LANG_IOPT11);

  // mode modif à la volée
  if (modify==1) {
    GetDlgItem(IDC_ext1           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_ext2           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_ext3           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_ext4           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_ext5           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_ext6           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_ext7           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_ext8           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_mime1           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_mime2           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_mime3           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_mime4           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_mime5           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_mime6           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_mime7           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_mime8           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_up           ) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_down           ) ->ModifyStyle(0,WS_DISABLED);
  } else {
    GetDlgItem(IDC_ext1           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_ext2           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_ext3           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_ext4           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_ext5           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_ext6           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_ext7           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_ext8           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_mime1           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_mime2           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_mime3           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_mime4           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_mime5           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_mime6           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_mime7           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_mime8           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_up           ) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_down           ) ->ModifyStyle(WS_DISABLED,0);
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
BOOL COptionTab11::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
char* COptionTab11::GetTip(int ID)
{
  switch(ID) {
    case IDC_ext1:
    case IDC_ext2:
    case IDC_ext3:
    case IDC_ext4:
    case IDC_ext5:
    case IDC_ext6:
    case IDC_ext7:
    case IDC_ext8:
      return LANG_W4; break;
    case IDC_mime1:
    case IDC_mime2:
    case IDC_mime3:
    case IDC_mime4:
    case IDC_mime5:
    case IDC_mime6:
    case IDC_mime7:
    case IDC_mime8:
      return LANG_W5; break;
    case IDC_up:
      return LANG_W6; break;
    case IDC_down:
      return LANG_W7; break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------


