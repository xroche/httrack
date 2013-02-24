// WinHTTrackDoc.cpp : implementation of the CWinHTTrackDoc class
//

#include "stdafx.h"
#include "WinHTTrack.h"

#include "WinHTTrackDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern "C" {
  #include "HTTrackInterface.h"
}

/* dialog0 */
#include "NewProj.h"
extern CNewProj* dialog0;

/* Main WizTab frame */
#include "WizTab.h"
extern CWizTab* this_CWizTab;

// dirtreeview
#include "DirTreeView.h"
extern CDirTreeView* this_DirTreeView;

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackDoc

IMPLEMENT_DYNCREATE(CWinHTTrackDoc, CDocument)

BEGIN_MESSAGE_MAP(CWinHTTrackDoc, CDocument)
	//{{AFX_MSG_MAP(CWinHTTrackDoc)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackDoc construction/destruction

CWinHTTrackDoc::CWinHTTrackDoc()
{
	// TODO: add one-time construction code here

}

CWinHTTrackDoc::~CWinHTTrackDoc()
{
}

BOOL CWinHTTrackDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: add reinitialization code here
	// (SDI documents will reuse this document)

	return TRUE;
}



/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackDoc serialization

void CWinHTTrackDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackDoc diagnostics

#ifdef _DEBUG
void CWinHTTrackDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CWinHTTrackDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackDoc commands

BOOL CWinHTTrackDoc::OnSaveDocument(LPCTSTR lpszPathName) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	//return CDocument::OnSaveDocument(lpszPathName);
  //AfxMessageBox(lpszPathName);

  if (dialog0) {
    CString PathName=lpszPathName;
    int pos=PathName.ReverseFind('.');
    if (pos>=0) {
      if (PathName.Mid(pos)==".whtt") {
        if (dialog0->GetPath0()+".whtt" != LPCSTR(lpszPathName)) {
          switch(AfxMessageBox(LANG_G26b,MB_YESNOCANCEL)) {
          case IDYES:
            {
              CString path=lpszPathName;
              int pos=path.ReverseFind('.');
              if (pos>=0) {
                CString dir=path.Left(pos);           // enlever .whtt
                pos=path.ReverseFind('\\');
                dialog0->m_projname=dir.Mid(pos+1);
                dialog0->m_projpath=dir.Left(pos);
              }
            }
            break;
          case IDNO:
            break;
          default:
            return 0;
            break;
          }
        }
      }
    }
    
    Save_current_profile(0);
  }
 
  return 1;
}

BOOL CWinHTTrackDoc::OnOpenDocument(LPCTSTR lpszPathName) 
{
	/*if (!CDocument::OnOpenDocument(lpszPathName))
		return FALSE;
    */
  CFileStatus status;
  if (CFile::GetStatus(lpszPathName,status)) {
    CString PathName=lpszPathName;

    /* si répertoire, convertir d'abord en .whtt */
    if (status.m_attribute & 0x10 ) {       /* directory = 0x10 */
      PathName+=".whtt";
    }
    
    int pos=PathName.ReverseFind('.');
    if (pos>=0) {
      if (PathName.Mid(pos)==".whtt") {
        CString dir=PathName.Left(pos)+"\\";
        CString profile=dir+"hts-cache\\winprofile.ini";    
        Read_profile((char*) LPCTSTR(profile),1);
        //if (fexist((char*) LPCTSTR(profile))) {
        //} else {
          //AfxMessageBox(LANG(LANG_G26 /*"File not found!","Fichier introuvable!"*/));
          /*return FALSE;*/
        //}
		if (dialog0->m_projname.GetLength() > 0) {
          this_CWizTab->PressButton(PSBTN_NEXT);
          this_CWizTab->PressButton(PSBTN_NEXT);
		}
      } else {
        AfxMessageBox(LANG(LANG_G26 /*"File not found!","Fichier introuvable!"*/));
        return FALSE;
      }
    } else {
      AfxMessageBox(LANG(LANG_G26 /*"File not found!","Fichier introuvable!"*/));
      return FALSE;
    }
  } else {
    AfxMessageBox(LANG(LANG_G26 /*"File not found!","Fichier introuvable!"*/));
    return FALSE;
  }
  
  return TRUE;
}

void CWinHTTrackDoc::OnCloseDocument() 
{
	// TODO: Add your specialized code here and/or call the base class
	CDocument::OnCloseDocument();
}
