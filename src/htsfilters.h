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
/* File: httrack.c subroutines:                                 */
/*       filters ("regexp")                                     */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */



#ifndef HTSFILT_DEFH
#define HTSFILT_DEFH 

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

#include "htsbase.h"

int fa_strjoker(int type,char** filters,int nfil,char* nom,LLint* size,int* size_flag,int* depth);
HTS_INLINE char* strjoker(char* chaine,char* joker,LLint* size,int* size_flag);
char* strjokerfind(char* chaine,char* joker);
#endif

#endif
