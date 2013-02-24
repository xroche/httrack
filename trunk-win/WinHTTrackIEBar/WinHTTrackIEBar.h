

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 7.00.0500 */
/* at Thu Feb 18 21:48:38 2010
 */
/* Compiler settings for .\WinHTTrackIEBar.idl:
    Oicf, W1, Zp8, env=Win32 (32b run)
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif // __RPCNDR_H_VERSION__

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __WinHTTrackIEBar_h__
#define __WinHTTrackIEBar_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IWinHTTrackLauncher_FWD_DEFINED__
#define __IWinHTTrackLauncher_FWD_DEFINED__
typedef interface IWinHTTrackLauncher IWinHTTrackLauncher;
#endif 	/* __IWinHTTrackLauncher_FWD_DEFINED__ */


#ifndef __WinHTTrackLauncher_FWD_DEFINED__
#define __WinHTTrackLauncher_FWD_DEFINED__

#ifdef __cplusplus
typedef class WinHTTrackLauncher WinHTTrackLauncher;
#else
typedef struct WinHTTrackLauncher WinHTTrackLauncher;
#endif /* __cplusplus */

#endif 	/* __WinHTTrackLauncher_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __IWinHTTrackLauncher_INTERFACE_DEFINED__
#define __IWinHTTrackLauncher_INTERFACE_DEFINED__

/* interface IWinHTTrackLauncher */
/* [unique][helpstring][dual][uuid][object] */ 


EXTERN_C const IID IID_IWinHTTrackLauncher;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("67ADF06B-7E1A-4743-B221-E7AA6E4D1FA8")
    IWinHTTrackLauncher : public IDispatch
    {
    public:
    };
    
#else 	/* C style interface */

    typedef struct IWinHTTrackLauncherVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IWinHTTrackLauncher * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ 
            __RPC__deref_out  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IWinHTTrackLauncher * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IWinHTTrackLauncher * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IWinHTTrackLauncher * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IWinHTTrackLauncher * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IWinHTTrackLauncher * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IWinHTTrackLauncher * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        END_INTERFACE
    } IWinHTTrackLauncherVtbl;

    interface IWinHTTrackLauncher
    {
        CONST_VTBL struct IWinHTTrackLauncherVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IWinHTTrackLauncher_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IWinHTTrackLauncher_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IWinHTTrackLauncher_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IWinHTTrackLauncher_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IWinHTTrackLauncher_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IWinHTTrackLauncher_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IWinHTTrackLauncher_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IWinHTTrackLauncher_INTERFACE_DEFINED__ */



#ifndef __WINHTTRACKIEBARLib_LIBRARY_DEFINED__
#define __WINHTTRACKIEBARLib_LIBRARY_DEFINED__

/* library WINHTTRACKIEBARLib */
/* [helpstring][version][uuid] */ 


EXTERN_C const IID LIBID_WINHTTRACKIEBARLib;

EXTERN_C const CLSID CLSID_WinHTTrackLauncher;

#ifdef __cplusplus

class DECLSPEC_UUID("86529161-034E-4F8A-88D2-3C625E612E04")
WinHTTrackLauncher;
#endif
#endif /* __WINHTTRACKIEBARLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


