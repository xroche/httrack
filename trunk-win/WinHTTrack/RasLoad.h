/*----------------------------------------------------------------------
Copyright (c) 1998,1999 Gipsysoft. All Rights Reserved.
File:	DynamicRAS.h
Owner:	russf@gipsysoft.com
Purpose:	Dynamically loaded RAS.
----------------------------------------------------------------------*/

/* Thanks to Russ Freeman from gipsymedia */

#if !defined(RAS_LOAD_HTS_DYNAMIC)
#define RAS_LOAD_HTS_DYNAMIC

#include "ras.h"

typedef	DWORD (APIENTRY *PRASENUMCONNECTIONS)( LPRASCONNA, LPDWORD, LPDWORD );
typedef	DWORD (APIENTRY *PRASHANGUP)( HRASCONN );
typedef	DWORD (APIENTRY *PRASGETCONNECTSTATUS)( HRASCONN , LPRASCONNSTATUSA );
typedef	DWORD (APIENTRY *PRASDIAL)(LPRASDIALEXTENSIONS ,LPTSTR ,LPRASDIALPARAMS ,DWORD ,LPVOID ,LPHRASCONN);
typedef	DWORD (APIENTRY *PRASENUMENTRIES)(LPTSTR ,LPTSTR ,LPRASENTRYNAME ,LPDWORD ,LPDWORD );
typedef	DWORD (APIENTRY *PRASGETENTRYDIALPARAMS)(LPTSTR ,LPRASDIALPARAMS ,LPBOOL ); 

class CDynamicRAS {
public:
	CDynamicRAS();
	virtual ~CDynamicRAS();

	inline bool IsRASLoaded() const { return m_hInst ? true : false; }

	inline DWORD RasEnumConnections( LPRASCONN lprasconn, LPDWORD lpcb, LPDWORD lpcConnections) { ASSERT( pRasEnumConnections ); return pRasEnumConnections( lprasconn, lpcb, lpcConnections ); }
	inline DWORD RasHangUp( HRASCONN hrasconn ) { ASSERT( pRasHangUp ); return pRasHangUp( hrasconn ); }
	inline DWORD RasGetConnectStatus( HRASCONN hrasconn , LPRASCONNSTATUS lprasconnstatus ) { ASSERT( pRasGetConnectStatus ); return pRasGetConnectStatus( hrasconn , lprasconnstatus ); }
	inline DWORD RasDial( LPRASDIALEXTENSIONS lpRasDialExtensions, LPTSTR lpszPhonebook, LPRASDIALPARAMS lpRasDialParams, DWORD dwNotifierType, LPVOID lpvNotifier, LPHRASCONN lphRasConn) { 
    ASSERT( pRasDial ); 
    return pRasDial(lpRasDialExtensions, lpszPhonebook, lpRasDialParams, dwNotifierType, lpvNotifier, lphRasConn);
  }
  inline DWORD RasEnumEntries (LPTSTR reserved, LPTSTR lpszPhonebook, LPRASENTRYNAME lprasentryname, LPDWORD lpcb, LPDWORD lpcEntries) {
    ASSERT( pRasEnumEntries ); 
    return pRasEnumEntries(reserved, lpszPhonebook, lprasentryname, lpcb, lpcEntries);
  }
  inline DWORD RasGetEntryDialParams (LPTSTR lpszPhonebook, LPRASDIALPARAMS lprasdialparams, LPBOOL lpfPassword) {
    ASSERT( pRasGetEntryDialParams ); 
    return pRasGetEntryDialParams(lpszPhonebook, lprasdialparams, lpfPassword);
  }

private:
	HINSTANCE m_hInst;

	PRASENUMCONNECTIONS pRasEnumConnections;
	PRASHANGUP pRasHangUp;
	PRASGETCONNECTSTATUS pRasGetConnectStatus;
  PRASDIAL pRasDial;
  PRASENUMENTRIES pRasEnumEntries;
  PRASGETENTRYDIALPARAMS pRasGetEntryDialParams;
};


#endif


