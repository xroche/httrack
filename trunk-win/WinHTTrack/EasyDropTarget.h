#if !defined(AFX_CEasyDropTarget_H__5EBE1984_98CD_11D2_A2B1_0000E84E7CA1__INCLUDED_)
#define AFX_CEasyDropTarget_H__5EBE1984_98CD_11D2_A2B1_0000E84E7CA1__INCLUDED_

#include <afxole.h>

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// CEasyDropTarget.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CEasyDropTarget document

static int ReadString(CArchive& ar, char* pString, int nMaxLen);

class CEasyDropTarget : public COleDropTarget
{
public: 
  CEasyDropTarget();
  CEasyDropTarget(CWnd* wnd);
  BOOL IsRegistered();
  void SetTextCallback(UINT msg);
  //
  static char** StringToArray(CString st);
  static int ReleaseStringToArray(char** st);

  // OLE
  DROPEFFECT OnDragEnter( CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point ); 
  DROPEFFECT OnDragOver( CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point ); 
  DROPEFFECT OnDragScroll( CWnd* pWnd, DWORD dwKeyState, CPoint point );     
  BOOL OnDrop( CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point ); 
  CString DoImportText(CArchive &ar);
  CString DoImportFile(HDROP hDropInfo);
private:
  BOOL registered;
  void SendWndText(CString DroppedData,CLIPFORMAT cfFormat);
  int DataToString(COleDataObject* pDataObject,CString &DroppedData);
  //
  CWnd* wnd;
  int cedt_text;
};


#endif // !defined(AFX_CEasyDropTarget_H__5EBE1984_98CD_11D2_A2B1_0000E84E7CA1__INCLUDED_)
