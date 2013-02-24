// NewFolder.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "NewFolder.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNewFolder dialog


CNewFolder::CNewFolder(CWnd* pParent /*=NULL*/)
	: CDialog(CNewFolder::IDD, pParent)
{
	//{{AFX_DATA_INIT(CNewFolder)
	m_folder = _T("");
	//}}AFX_DATA_INIT
}


void CNewFolder::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNewFolder)
	DDX_Text(pDX, IDC_Folder, m_folder);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CNewFolder, CDialog)
	//{{AFX_MSG_MAP(CNewFolder)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_HELP, OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNewFolder message handlers
