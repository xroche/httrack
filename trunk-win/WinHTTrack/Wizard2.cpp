// wizard2.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "wizard2.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
extern HICON httrack_icon;


/////////////////////////////////////////////////////////////////////////////
// wizard2 dialog


wizard2::wizard2(CWnd* pParent /*=NULL*/)
	: CDialog(wizard2::IDD, pParent)
{
	//{{AFX_DATA_INIT(wizard2)
	m_question = _T("");
	//}}AFX_DATA_INIT
}


void wizard2::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(wizard2)
	DDX_Text(pDX, IDC_question, m_question);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(wizard2, CDialog)
	//{{AFX_MSG_MAP(wizard2)
	ON_WM_TIMER()
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_HELP, OnHelp)
END_MESSAGE_MAP()



BOOL wizard2::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
  SetIcon(httrack_icon,false);
  SetIcon(httrack_icon,true);  
	
  tm=SetTimer(WM_TIMER,1000,NULL);
  SetForegroundWindow();   // yop en premier plan!

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    //SetDlgItemText(,"");
    SetWindowTextCP(this, LANG(LANG_N1)); // "Question du wizard");
    SetDlgItemTextCP(this, IDCANCEL,LANG(LANG_N2)); // "NON");
  }


  return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void wizard2::OnTimer(UINT_PTR nIDEvent) 
{
  wflag=!wflag;
	FlashWindow(wflag);
	CDialog::OnTimer(nIDEvent);
}

void wizard2::OnDestroy() 
{
  if (tm!=0) {
    KillTimer(tm); 
    tm=-1; 
  }
	CDialog::OnDestroy();	
}
