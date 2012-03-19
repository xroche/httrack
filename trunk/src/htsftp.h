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
/* File: basic FTP protocol manager .h                          */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */


#ifndef HTSFTP_DEFH
#define HTSFTP_DEFH 

#include "htsbase.h"
#include "htsbasenet.h"
#include "htsthread.h"

// lien_back
#include "htscore.h"

#if USE_BEGINTHREAD
void launch_ftp(lien_back* back);
PTHREAD_TYPE back_launch_ftp( void* pP );
#else
void launch_ftp(lien_back* back,char* path,char* exec);
int back_launch_ftp(lien_back* back);
#endif

int run_launch_ftp(lien_back* back);
int send_line(T_SOC soc,char* data);
int get_ftp_line(T_SOC soc,char* line,int timeout);
T_SOC get_datasocket(char* to_send);
int stop_ftp(lien_back* back);
char* linejmp(char* line);
int check_socket(T_SOC soc);
int check_socket_connect(T_SOC soc);
int wait_socket_receive(T_SOC soc,int timeout);


#endif

