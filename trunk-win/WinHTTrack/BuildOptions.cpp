// BuildOptions.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "BuildOptions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Helper
extern LaunchHelp* HtsHelper;


/////////////////////////////////////////////////////////////////////////////
// CBuildOptions dialog


CBuildOptions::CBuildOptions(CWnd* pParent /*=NULL*/)
	: CDialog(CBuildOptions::IDD, pParent)
{
	//{{AFX_DATA_INIT(CBuildOptions)
	m_BuildString = _T("");
	//}}AFX_DATA_INIT
}


void CBuildOptions::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CBuildOptions)
	DDX_Text(pDX, IDC_BuildString, m_BuildString);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CBuildOptions, CDialog)
	//{{AFX_MSG_MAP(CBuildOptions)
	ON_WM_HELPINFO()
	//}}AFX_MSG_MAP
  ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
  ON_COMMAND(ID_HELP,OnHelpInfo2)
	ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBuildOptions message handlers

// Appel aide
void CBuildOptions::OnHelpInfo2() {
  (void) OnHelpInfo(NULL);
}

BOOL CBuildOptions::OnHelpInfo(HELPINFO* dummy) 
{
  //return CDialog::OnHelpInfo(pHelpInfo);
  HtsHelper->Help();
  return true;
  //AfxGetApp()->WinHelp(0,HELP_FINDER);    // Index du fichier Hlp
  //return true;
}

BOOL CBuildOptions::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    SetWindowTextCP(this, LANG(LANG_Q1)); // struct
    SetDlgItemTextCP(this, IDOK,LANG(LANG_OK ));
    SetDlgItemTextCP(this, IDC_STATIC_options,LANG_O2);
    SetDlgItemTextCP(this, IDCANCEL,LANG(LANG_CANCEL));  // cancel 
  }
  SetDlgItemTextCP(this, IDC_STATIC_bo1,LANG(LANG_Q2));
  SetDlgItemTextCP(this, IDC_STATIC_bo2,LANG(LANG_Q3));

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
