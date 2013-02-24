// wizard.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "wizard.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
extern HICON httrack_icon;

/////////////////////////////////////////////////////////////////////////////
// wizard dialog


wizard::wizard(CWnd* pParent /*=NULL*/)
	: CDialog(wizard::IDD, pParent)
{
	//{{AFX_DATA_INIT(wizard)
	m_reponse = _T("");
	m_question = _T("");
	//}}AFX_DATA_INIT
}


void wizard::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(wizard)
	DDX_Text(pDX, IDC_reponse, m_reponse);
	DDX_Text(pDX, IDC_question, m_question);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(wizard, CDialog)
	//{{AFX_MSG_MAP(wizard)
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_HELP, OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// wizard message handlers

BOOL wizard::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
  SetIcon(httrack_icon,false);
  SetIcon(httrack_icon,true);  
  SetForegroundWindow();   // yop en premier plan!

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    //SetDlgItemText(,"");
    SetWindowTextCP(this, LANG(LANG_L1) /*"Question du wizard"*/);
    SetDlgItemTextCP(this, IDC_STATIC_answer,LANG(LANG_L2) /*"Votre réponse:"*/);
  }

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
