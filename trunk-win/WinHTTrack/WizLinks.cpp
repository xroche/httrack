// WizLinks.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "WizLinks.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
extern HICON httrack_icon;

/////////////////////////////////////////////////////////////////////////////
// WizLinks dialog


WizLinks::WizLinks(CWnd* pParent /*=NULL*/)
	: CDialog(WizLinks::IDD, pParent)
{
	//{{AFX_DATA_INIT(WizLinks)
	m_lnk = -1;
	m_url = _T("");
	//}}AFX_DATA_INIT
}


void WizLinks::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(WizLinks)
	DDX_Radio(pDX, IDC_ch1, m_lnk);
	DDX_Text(pDX, IDC_URL, m_url);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(WizLinks, CDialog)
	//{{AFX_MSG_MAP(WizLinks)
	ON_BN_CLICKED(IDskipall, Onskipall)
	ON_WM_DESTROY()
	ON_WM_TIMER()
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_HELP, OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// WizLinks message handlers

void WizLinks::Onskipall() 
{
	// TODO: Add your control notification handler code here
  EndDialog(IDskipall);	
}

BOOL WizLinks::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
  SetIcon(httrack_icon,false);
  SetIcon(httrack_icon,true);  
	
  tm=SetTimer(WM_TIMER,1000,NULL);
  SetForegroundWindow();   // yop en premier plan!

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    //SetDlgItemTextCP(this, ,"");
    SetWindowTextCP(this, LANG(LANG_M1)); // "Un lien a été détecté");
    SetDlgItemTextCP(this, IDC_STATIC_rule,LANG(LANG_M2)); // "Choisir une règle:");
    SetDlgItemTextCP(this, IDC_ch1,LANG(LANG_M3)); // "Ignorer ce lien");
    SetDlgItemTextCP(this, IDC_ch2,LANG(LANG_M4)); // "Ignorer répertoire");
    SetDlgItemTextCP(this, IDC_ch3,LANG(LANG_M5)); // "Ignorer domaine");
    SetDlgItemTextCP(this, IDC_ch4,LANG(LANG_M6)); // "Prendre cette page uniquement");
    SetDlgItemTextCP(this, IDC_ch5,LANG(LANG_M7)); // "Miroir du site");
    SetDlgItemTextCP(this, IDC_ch6,LANG(LANG_M8)); // "Miroir du domaine entier");
    SetDlgItemTextCP(this, IDskipall,LANG(LANG_M9)); // "Ignorer tout");
  }

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void WizLinks::OnDestroy() 
{
  if (tm!=0) {
    KillTimer(tm); 
    tm=-1; 
  }
	CDialog::OnDestroy();
}

void WizLinks::OnTimer(UINT_PTR nIDEvent) 
{
  wflag=!wflag;
	FlashWindow(wflag);
	CDialog::OnTimer(nIDEvent);
}


INT_PTR WizLinks::DoModal() 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CDialog::DoModal();
}

