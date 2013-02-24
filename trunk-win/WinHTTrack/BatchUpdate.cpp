// BatchUpdate.cpp : implementation file
//

#include "stdafx.h"
#include "winhttrack.h"
#include "BatchUpdate.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CBatchUpdate property page

IMPLEMENT_DYNCREATE(CBatchUpdate, CPropertyPage)

CBatchUpdate::CBatchUpdate() : CPropertyPage(CBatchUpdate::IDD)
{
	//{{AFX_DATA_INIT(CBatchUpdate)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CBatchUpdate::~CBatchUpdate()
{
}

void CBatchUpdate::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CBatchUpdate)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CBatchUpdate, CPropertyPage)
	//{{AFX_MSG_MAP(CBatchUpdate)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBatchUpdate message handlers
