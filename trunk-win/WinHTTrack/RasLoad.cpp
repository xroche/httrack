/*----------------------------------------------------------------------
Copyright (c) 1998,1999 Gipsysoft. All Rights Reserved.
File:	DynamicRAS.h
Owner:	russf@gipsysoft.com
Purpose:	Dynamically loaded RAS.
----------------------------------------------------------------------*/

/* Thanks to Russ Freeman from gipsymedia */

#include "stdafx.h"
#include "RasLoad.h"

CDynamicRAS::CDynamicRAS()
	: m_hInst( LoadLibrary( _T("rasapi32") ) )
	, pRasEnumConnections( NULL )
	, pRasHangUp( NULL )
	, pRasGetConnectStatus( NULL )
  , pRasDial( NULL )
  , pRasEnumEntries( NULL )
  , pRasGetEntryDialParams( NULL )
{
	if( IsRASLoaded() )
	{
		pRasEnumConnections = (PRASENUMCONNECTIONS)GetProcAddress( m_hInst, "RasEnumConnectionsA" );
		pRasHangUp = (PRASHANGUP)GetProcAddress( m_hInst, "RasHangUpA" );
		pRasGetConnectStatus = (PRASGETCONNECTSTATUS)GetProcAddress( m_hInst, "RasGetConnectStatusA" );
		pRasDial = (PRASDIAL)GetProcAddress( m_hInst, "RasDialA" );
    pRasEnumEntries = (PRASENUMENTRIES)GetProcAddress( m_hInst, "RasEnumEntriesA" );
    pRasGetEntryDialParams = (PRASGETENTRYDIALPARAMS)GetProcAddress( m_hInst, "RasGetEntryDialParamsA" );
	}
}

CDynamicRAS::~CDynamicRAS()
{
	if( IsRASLoaded() )
	{
		FreeLibrary( m_hInst );
	}
}



