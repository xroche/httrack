/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.


Please visit our Website: http://www.httrack.com
*/


/* ------------------------------------------------------------ */
/* File: basic mms protocol manager .h                          */
/* Author: Xavier Roche                                         */
/*                                                              */
/* The mms routines were written by Nicolas BENOIT,             */
/* based on the work of SDP Multimedia and Major MMS		        */
/* Thanks to all of them!                                       */
/* ------------------------------------------------------------ */


#ifndef HTSMMS_DEFH
#define HTSMMS_DEFH 

#include "htsglobal.h"

#if HTS_USEMMS

#include "htsbase.h"
#include "htsbasenet.h"
#include "htsthread.h"

// lien_back
#include "htscore.h"

typedef struct MMSDownloadStruct {
	lien_back *pBack;
	httrackp *pOpt;
} MMSDownloadStruct;

void launch_mms(const MMSDownloadStruct* pStruct);
#endif

#endif
