// CatchUrl.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "CatchUrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CCatchUrl dialog


CCatchUrl::CCatchUrl(CWnd* pParent /*=NULL*/)
	: CDialog(CCatchUrl::IDD, pParent)
{
	//{{AFX_DATA_INIT(CCatchUrl)
	m_info = _T("");
	//}}AFX_DATA_INIT
}


void CCatchUrl::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CCatchUrl)
	DDX_Text(pDX, IDC_info, m_info);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CCatchUrl, CDialog)
	//{{AFX_MSG_MAP(CCatchUrl)
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
 ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCatchUrl message handlers

void CCatchUrl::OnClose() 
{
	// TODO: Add your message handler code here and/or call default
	
	CDialog::OnClose();
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
BOOL CCatchUrl::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
char* CCatchUrl::GetTip(int ID)
{
  switch(ID) {
    case IDC_info: return LANG_V10; break;
    case IDCANCEL: return LANG_V11; break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------



BOOL CCatchUrl::OnInitDialog() 
{
	CDialog::OnInitDialog();
  EnableToolTips(true);     // TOOL TIPS

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    SetWindowTextCP(this, LANG(LANG_V1));
    SetDlgItemTextCP(this, IDC_STATIC_info,LANG_V2);
    SetDlgItemTextCP(this, IDC_STATIC_info2,LANG_V3);
    SetDlgItemTextCP(this, IDCANCEL,LANG_V4);
  }
  
	return TRUE;
}
