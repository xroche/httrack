// WinHTTrackLauncher.h: Definition of the WinHTTrackLauncher class
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_WINHTTRACKLAUNCHER_H__08EA4A9D_DC5C_487F_8196_18B699DB7E08__INCLUDED_)
#define AFX_WINHTTRACKLAUNCHER_H__08EA4A9D_DC5C_487F_8196_18B699DB7E08__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "resource.h"       // main symbols
#include "docobj.h"
#include <SHLGUID.h>

/////////////////////////////////////////////////////////////////////////////
// WinHTTrackLauncher

class WinHTTrackLauncher : 
	public IDispatchImpl<IWinHTTrackLauncher, &IID_IWinHTTrackLauncher, &LIBID_WINHTTRACKIEBARLib>,
	public ISupportErrorInfo,
	public CComObjectRoot,
	public CComCoClass<WinHTTrackLauncher,&CLSID_WinHTTrackLauncher>,
  public IOleCommandTarget,
  public IObjectWithSite
{
private:
  CComPtr<IWebBrowser2> browser;
public:
	WinHTTrackLauncher() {}
BEGIN_COM_MAP(WinHTTrackLauncher)
	COM_INTERFACE_ENTRY(IDispatch)
	COM_INTERFACE_ENTRY(IOleCommandTarget)
  COM_INTERFACE_ENTRY(IObjectWithSite)
	COM_INTERFACE_ENTRY(IWinHTTrackLauncher)
	COM_INTERFACE_ENTRY(ISupportErrorInfo)
END_COM_MAP()
//DECLARE_NOT_AGGREGATABLE(WinHTTrackLauncher) 
// Remove the comment from the line above if you don't want your object to 
// support aggregation. 

DECLARE_REGISTRY_RESOURCEID(IDR_WinHTTrackLauncher)
// ISupportsErrorInfo
	STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);


// IOleCommandTarget
STDMETHOD(QueryStatus)(const GUID __RPC_FAR *pguidCmdGroup, ULONG cCmds, OLECMD __RPC_FAR prgCmds[  ], OLECMDTEXT __RPC_FAR *pCmdText);
STDMETHOD(Exec)(const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdExecOpt, VARIANTARG *pvaIn, VARIANTARG *pvaOut);

// IObjectWithSite
STDMETHOD(SetSite)(IUnknown* pUnkSite);
STDMETHOD(GetSite)(REFIID riid, void** ppvSite);


// IWinHTTrackLauncher
public:
};

#endif // !defined(AFX_WINHTTRACKLAUNCHER_H__08EA4A9D_DC5C_487F_8196_18B699DB7E08__INCLUDED_)
