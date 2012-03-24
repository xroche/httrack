/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
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
/* File: basic FTP protocol manager .h                          */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */


#ifndef HTSFTP_DEFH
#define HTSFTP_DEFH 

#include "htsbase.h"
#include "htsbasenet.h"
#include "htsthread.h"

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_lien_back
#define HTS_DEF_FWSTRUCT_lien_back
typedef struct lien_back lien_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif

/* Download structure */
#ifndef HTS_DEF_FWSTRUCT_FTPDownloadStruct
#define HTS_DEF_FWSTRUCT_FTPDownloadStruct
typedef struct FTPDownloadStruct FTPDownloadStruct;
#endif
struct FTPDownloadStruct {
	lien_back *pBack;
	httrackp *pOpt;
};

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE
#if USE_BEGINTHREAD
void launch_ftp(FTPDownloadStruct *params);
void back_launch_ftp( void* pP );
#else
void launch_ftp(FTPDownloadStruct *params,char* path,char* exec);
int back_launch_ftp(FTPDownloadStruct *params);
#endif

int run_launch_ftp(FTPDownloadStruct *params);
int send_line(T_SOC soc,char* data);
int get_ftp_line(T_SOC soc,char* line,int timeout);
T_SOC get_datasocket(char* to_send);
int stop_ftp(lien_back* back);
char* linejmp(char* line);
int check_socket(T_SOC soc);
int check_socket_connect(T_SOC soc);
int wait_socket_receive(T_SOC soc,int timeout);
#endif

#endif

