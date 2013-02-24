/*
  Implementation example:
  
  1. In the .h file: add

#include "EasyDropTarget.h"
...
private: 
  CEasyDropTarget* drag;
...
  // Generated message map functions
  afx_msg LRESULT DragDropText(WPARAM wParam,LPARAM lParam);
...

  2. In the cpp file: add

#include "EasyDropTarget.h"
...
#define wm_CEasyDropTargetCallback (WM_USER + 1)
BEGIN_MESSAGE_MAP(Wid1, CDialog)
...
  ON_MESSAGE( wm_CEasyDropTargetCallback, DragDropText)
...
END_MESSAGE_MAP()


...
int CMyDialogClass::OnCreate(LPCREATESTRUCT lpCreateStruct) 
...
  // Drag&Drop
  drag=new CEasyDropTarget(this);
  if (drag->IsRegistered()) {
    drag->SetTextCallback(wm_CEasyDropTargetCallback);
  }
...
}

  2b. Add the callback function

// Message from CEasyDropTarget
LRESULT CMyDialogClass::DragDropText(WPARAM wParam,LPARAM lParam) {
  if (lParam) {
    CString st=*((CString*) lParam);
    CLIPFORMAT cfFormat=wParam;
    if (cfFormat==CF_TEXT)
      AfxMessageBox("Raw text : "+st);
    else if (cfFormat==CF_HDROP)
      AfxMessageBox("File list (dropped) : "+st);
    else
      AfxMessageBox("Unknown source : "+st);
  }
  return 0;
}




*/

// CEasyDropTarget.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "EasyDropTarget.h"
#include <afxole.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CEasyDropTarget
CEasyDropTarget::CEasyDropTarget() {
  this->registered=false;
  //
  this->wnd=NULL;
  this->cedt_text=0;
}

CEasyDropTarget::CEasyDropTarget(CWnd* wnd) {
  this->wnd=wnd;
  if (this->wnd) {
    if (Register(this->wnd)) {
      registered=true;
    }
  }
}

void CEasyDropTarget::SetTextCallback(UINT fnc) {
  cedt_text=fnc;
}

BOOL CEasyDropTarget::IsRegistered() {
  return registered;
}


/////////////////////////////////////////////////////////////////////////////
// CEasyDropTarget - tools functions
// Convert a String to char** (each lines in each char*)
// Return: NULL if error, else a char** (last char* item is NULL)
char** CEasyDropTarget::StringToArray(CString st) {
  char** tab;
  int i;
  int n=2;                        // one line, + null term
  for(i=0;i<st.GetLength();i++)
    if (st[i]=='\n')
      n++;
  tab=(char**) calloc(n,sizeof(char*));
  if (tab) {
    char* buff=(char*) malloc(st.GetLength()+2);
    if (buff) {
      int index=0;
      buff[st.GetLength()]='\0';        // NULL at the end of buff for last line
      tab[index++]=(char*) buff;
      for(i=0;i<st.GetLength();i++) {
        if (st[i]=='\n') {              // next line (next char*)
          buff[i]='\0';
          tab[index++]=(char*) buff+i+1;  // note: avoid \n\r or else there may be problems (empty lines)
        } else if (st[i]=='\r') {     // ignore \r
          buff[i]='\0';
        } else
          buff[i]=st[i];
      }
      tab[index++]=(char*) NULL;
      return tab;
    } else
      free(tab);
  }
  return (char**) NULL;
}
int CEasyDropTarget::ReleaseStringToArray(char** st) {
  free((void*) st[0]);  // char buffer
  free((void*) st);     // char* buffer
  return 1;
}


/////////////////////////////////////////////////////////////////////////////
// CEasyDropTarget - Drag&Drop functions
void CEasyDropTarget::SendWndText(CString DroppedData,CLIPFORMAT cfFormat) {
  if (wnd) {
    if (this->cedt_text) {
      wnd->SendMessage(this->cedt_text,cfFormat,(LPARAM) &DroppedData);      // Send Message
    }
  }
}


/////////////////////////////////////////////////////////////////////////////
// CEasyDropTarget - Drag&Drop general functions

DROPEFFECT CEasyDropTarget::OnDragScroll( CWnd* pWnd, DWORD dwKeyState, CPoint point ) 
{     
    return DROPEFFECT_NONE; 
} 

DROPEFFECT CEasyDropTarget::OnDragOver( CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point ) 
{ 
    return DROPEFFECT_COPY; 
} 

DROPEFFECT CEasyDropTarget::OnDragEnter( CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point ) 
{ 
  return DROPEFFECT_COPY; 
} 

BOOL CEasyDropTarget::OnDrop( CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point ) 
{ 
  COleDataObject *MyObject = pDataObject;  
  CString DroppedData;
  int ntype=DataToString(pDataObject,DroppedData);
  if (ntype) {
    SendWndText(DroppedData,ntype);
  }
  return TRUE; 
} 

int CEasyDropTarget::DataToString(COleDataObject* pDataObject,CString &DroppedData) 
{ 
  COleDataObject *MyObject = pDataObject; 
  
  // get file refering to clipboard data
  if (pDataObject->IsDataAvailable(CF_TEXT)) {
    CFile *pFile = pDataObject->GetFileData(CF_TEXT);
    if (pFile != NULL) {
      // connect the file to the archive and read the contents
      CArchive ar(pFile, CArchive::load);
      DroppedData=DoImportText(ar);
      ar.Close();
      delete pFile;                   // desalloc
      return CF_TEXT;
    }
  }
  else if (pDataObject->IsDataAvailable(CF_HDROP)) {
    STGMEDIUM StgMedium;
    if (pDataObject->GetData(CF_HDROP, &StgMedium)) {
      if (StgMedium.tymed==TYMED_HGLOBAL) {
        HGLOBAL hGlobal = (HGLOBAL) StgMedium.hGlobal;
        HDROP hDropInfo = (HDROP) hGlobal;
        DroppedData=DoImportFile(hDropInfo);
      }
      ReleaseStgMedium(&StgMedium);        // desalloc
      return CF_HDROP;
    }
  }
  return -1;
} 

CString CEasyDropTarget::DoImportText(CArchive &ar)
{
  char szNode[256];
  CString st="";  
  while (ReadString(ar, szNode, sizeof(szNode)) != NULL)
  {
    int cch = (int) strlen(szNode);
    if (cch) {
      szNode[cch] = '\0';
    }
    if (st.GetLength())
      st+="\r\n";
    st+=szNode;
  }
  return st;  
}

CString CEasyDropTarget::DoImportFile(HDROP hDropInfo)
{
  CString st="";
  int nf;
  int i;
  nf=DragQueryFile(hDropInfo,0xFFFFFFFF,NULL,0);    // nb of files
  for(i=0;i<nf;i++) {
    char* buff;
    int size;
    size=DragQueryFile(hDropInfo,i,NULL,0);    // buffer size
    if (size>0) {
      size+=16;    // marge de sécurité
      buff=(char*) calloc(size,1);
      if (buff) {
        if (DragQueryFile(hDropInfo,i,buff,size)>0) {    // ok copied
          if (st.GetLength())
            st+="\r\n";
          st+=buff;
        }
        free(buff);
      }
    }
  }
  return st;
}



static int ReadString(CArchive& ar, char* pString, int nMaxLen)
{
	int nCount = 0;
	char ch;
	pString[nCount] = (char)0;
	TRY
	{
		do
		{
			ar >> (BYTE&)ch;

			// watch out for ^Z (EOF under DOS)
			if (ch == 0x1A)
				break;

			// combine "\r\n" pair into single "\n"
			if (ch == '\n' && nCount != 0 && pString[nCount-1] == '\r')
				nCount--;

			pString[nCount++] = ch;

		} while (nCount < nMaxLen-1 && ch != '\n');
	}
	END_TRY

#ifdef _OLD
	// insert newline if missing
	if (nCount != 0 && pString[nCount-1] != '\n' && nCount < nMaxLen-1)
		pString[nCount++] = '\n';
	pString[nCount] = (char)0;
#else
	// insert newline if missing
	if (nCount != 0 && pString[nCount-1] != '\n' && nCount < nMaxLen-1)
	{
		if (pString[nCount-1] == '\0')
			pString[nCount-1] = '\n';
		else
			pString[nCount++] = '\n';
	}
	pString[nCount] = (char)0;
#endif
	return nCount;
}
