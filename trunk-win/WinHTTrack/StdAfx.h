// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__ED81E639_E017_11D1_A97E_006097BC6150__INCLUDED_)
#define AFX_STDAFX_H__ED81E639_E017_11D1_A97E_006097BC6150__INCLUDED_

// #define NTDDI_VERSION 0x05000000 // NTDDI_WIN2K

#ifndef WINVER
#define WINVER       0x0500 // _WIN32_WINNT_WIN2K
//#define WINVER       0x0400 // _WIN32_WINNT_NT4
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 // _WIN32_WINNT_WIN2K
//#define _WIN32_WINNT 0x0400 // _WIN32_WINNT_NT4
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0500
#endif

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC OLE automation classes
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT


//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__ED81E639_E017_11D1_A97E_006097BC6150__INCLUDED_)
