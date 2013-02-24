// WinHTTrackLauncher.cpp : Implementation of CWinHTTrackIEBarApp and DLL registration.

#include "stdafx.h"
#include "WinHTTrackIEBar.h"
#include "WinHTTrackLauncher.h"

extern "C" {
#include <direct.h>
};

#include <commctrl.h>
#include <mshtml.h>

#include "projectInfo.h"

/////////////////////////////////////////////////////////////////////////////
//

static char* BSTRtoPCHAR(BSTR str, UINT codepage = CP_ACP) {
  if (str != NULL) {
	  int multiByteLength = ::WideCharToMultiByte( codepage, 0, str, (int) wcslen(str), NULL, NULL, NULL, NULL); 
    if (multiByteLength > 0) 
    { 
      char *pcharMutiByteBuffer = (char*) malloc(multiByteLength + 1); 
      ::WideCharToMultiByte( codepage, 0, str, (int) wcslen(str), pcharMutiByteBuffer, (int) multiByteLength, NULL, NULL); 
      pcharMutiByteBuffer[multiByteLength] = '\0';
      return pcharMutiByteBuffer;
    }
  }
  return NULL;
}

static int fexist(char* s) {
  FILE* fp = fopen(s, "rb");
  if (fp) fclose(fp);
  return fp != NULL;
} 

STDMETHODIMP WinHTTrackLauncher::InterfaceSupportsErrorInfo(REFIID riid)
{
  static const IID* arr[] = 
  {
    &IID_IWinHTTrackLauncher,
  };
  
  for (int i=0;i<sizeof(arr)/sizeof(arr[0]);i++)
  {
    if (InlineIsEqualGUID(*arr[i],riid))
      return S_OK;
  }
  return S_FALSE;
}

// IOleCommandTarget
STDMETHODIMP WinHTTrackLauncher::QueryStatus(const GUID __RPC_FAR *pguidCmdGroup, ULONG cCmds, OLECMD __RPC_FAR prgCmds[  ], OLECMDTEXT __RPC_FAR *pCmdText) {
	HRESULT hr = E_POINTER;

	if ( prgCmds != NULL)
	{
		for ( ULONG i = 0; i < cCmds; i++)
			prgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED; 
		
		hr = S_OK;
	}
	return hr;
}

STDMETHODIMP WinHTTrackLauncher::Exec(const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdExecOpt, VARIANTARG *pvaIn, VARIANTARG *pvaOut) {
  HKEY key;
  DWORD type;
  char path[256];
  char appPath[256];
  DWORD size = sizeof(path) - 2;
  BSTR url = NULL;
  BSTR title = NULL;
  char* urlPCHAR = NULL;
  char* titlePCHAR = NULL;

  if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\WinHTTrack Website Copier\\WinHTTrack Website Copier\\DefaultValues",
    0, KEY_READ,
    &key) == ERROR_SUCCESS) {
    if (RegQueryValueExA(key, "BasePath", 0, &type,
      (LPBYTE) path, &size) == ERROR_SUCCESS)
    {
      path[size] = '\0';
    }
    RegCloseKey(key);
  }
  size = sizeof(appPath) - 2;
  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\WinHTTrack Website Copier\\WinHTTrack Website Copier",
    0, KEY_READ,
    &key) == ERROR_SUCCESS) {
    if (RegQueryValueExA(key, "Path", 0, &type,
      (LPBYTE) appPath, &size) == ERROR_SUCCESS)
    {
      appPath[size] = '\0';
      strcat(appPath, "\\WinHTTrack.exe");
    }
    RegCloseKey(key);
  }
  
  if (path[0] != '\0' && appPath[0] != '\0') {

    CComPtr<IDispatch> doc;
    if (SUCCEEDED(browser->get_Document(&doc))) {
      CComQIPtr<IHTMLDocument2> hDoc(doc);
      if (hDoc != NULL) {
        if (SUCCEEDED(hDoc->get_title(&title))) {
          titlePCHAR = BSTRtoPCHAR(title);
        }
      }
    }
    if (SUCCEEDED(browser->get_LocationURL(&url))) {
      urlPCHAR = BSTRtoPCHAR(url);
    }
    
    if (urlPCHAR != NULL && titlePCHAR != NULL) {
      int i, j;     
	  char lastCh = 0;
      for(i = 0 ; i < (int) strlen(titlePCHAR) ; i++) {
        if (titlePCHAR[i] == '\"') {
          titlePCHAR[i] = '\'';
        }
        else if (titlePCHAR[i] == ',' 
          || titlePCHAR[i] == '~'
          || titlePCHAR[i] == '\\'
          || titlePCHAR[i] == '/'
          || titlePCHAR[i] == ':'
          || titlePCHAR[i] == '*'
		  || titlePCHAR[i] == '?'
		  || titlePCHAR[i] == '<'
		  || titlePCHAR[i] == '>'
		  || titlePCHAR[i] == '|'
		  || ((unsigned char)titlePCHAR[i]) < 32
		  ) {
			  titlePCHAR[i] = ' ';
		  }
	  }
	  for(i = 0, j = 0 ; titlePCHAR[i] != '\0' ; i++) {
		  if (titlePCHAR[i] != ' ' || lastCh != ' ') {
			  titlePCHAR[j++] = titlePCHAR[i];
			  lastCh = titlePCHAR[i];
		  }
	  }
	  titlePCHAR[j++] = '\0';
	  if (strlen(titlePCHAR) > 128)
		  titlePCHAR[128] = '\0';

	  _bstr_t titleWtl = titlePCHAR;
	  CprojectInfo info(&titleWtl);
	  if (info.DoModal() == IDOK) {
		  /* Form name */
		  strcat(path, "\\");
		  strcat(path, (char*)titleWtl);
		  int backPos = (int) strlen(path);
		  strcat(path, ".whtt");
		  /* Open */
		  if (!fexist(path)) {
			  FILE* fp = fopen(path, "wb");
			  if (fp != NULL) {
				  fclose(fp);
			  }
			  path[backPos] = '\0';
			  _mkdir(path);
			  strcat(path, "\\hts-cache");
			  _mkdir(path);
			  strcat(path, "\\winprofile.ini");
			  fp = fopen(path, "wb");
			  if (fp != NULL) {
				  fprintf(fp, "CurrentUrl=%s\r\n", urlPCHAR);
				  fprintf(fp, "CurrentAction=1\r\n");
				  fclose(fp);
			  }
			  path[backPos] = '\0';
			  strcat(path, ".whtt");
		  }
		  if (fexist(path)) {       
			  if (!ShellExecuteA(NULL, "open", path, "", "", SW_RESTORE)) {
			  }
		  }
	  }

    } else
      MessageBoxW(NULL, L"Can not find any URL", L"WinHTTrack", MB_SETFOREGROUND);
  }
  if (url != NULL) {
    SysFreeString(url);
  }   
  if (title != NULL) {
    SysFreeString(title);
  }
  if (urlPCHAR != NULL) {
    free(urlPCHAR);
  }
  if (titlePCHAR != NULL) {
    free(titlePCHAR);
  }
  return S_OK;
}

// IObjectWithSite - See KB257717
STDMETHODIMP WinHTTrackLauncher::SetSite(IUnknown* pClientSite) {
  HRESULT hr = S_OK;
  CComPtr<IServiceProvider> isp, isp2;
  if (pClientSite != NULL)
  {
    if (SUCCEEDED(pClientSite->QueryInterface(IID_IServiceProvider, reinterpret_cast<void **>(&isp))))
    {
      if (SUCCEEDED(isp->QueryService(SID_STopLevelBrowser, IID_IServiceProvider, reinterpret_cast<void **>(&isp2))))
      {
        if (SUCCEEDED(isp2->QueryService(SID_SWebBrowserApp, IID_IWebBrowser2, reinterpret_cast<void **>(&browser))))
        {
        }
      }
    }
  }
  return S_OK;
}

STDMETHODIMP WinHTTrackLauncher::GetSite(REFIID riid, void** ppvSite) {
	if (NULL != ppvSite) {
		*ppvSite = browser;
		return S_OK;
	}
	return E_NOTIMPL;
}

