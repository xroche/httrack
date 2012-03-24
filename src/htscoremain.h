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
/*       main routine (first called)                            */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */



#ifndef HTSMAINHSR_DEFH
#define HTSMAINHSR_DEFH 

// --assume standard
#define HTS_ASSUME_STANDARD \
  "php2 php3 php4 php cgi asp jsp pl cfm nsf=text/html"

#include "htsglobal.h"
#include "htsopt.h"

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE
// Main, récupère les paramètres et appelle le robot
#ifndef HTTRACK_DEFLIB
HTSEXT_API int hts_main(int argc, char **argv);
HTSEXT_API int hts_main2(int argc, char **argv, httrackp *opt);
#endif

int cmdl_opt(char* s);
int check_path(String* s,char* defaultname);

#endif


#endif
