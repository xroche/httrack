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
/* File: htsalias.h subroutines:                                */
/*       alias for command-line options and config files        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */


#ifndef HTSALIAS_DEFH
#define HTSALIAS_DEFH

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE
extern const char* hts_optalias[][4];
int optalias_check(int argc,const char * const * argv,int n_arg,
                   int* return_argc,char** return_argv,
                   char* return_error);
int optalias_find(const char* token);
const char* optalias_help(const char* token);
int optreal_find(const char* token);
int optinclude_file(const char* name,
                    int* argc,char** argv,char* x_argvblk,int* x_ptr);
const char* optreal_value(int p);
const char* optalias_value(int p);
const char* opttype_value(int p);
const char* opthelp_value(int p);
char* hts_gethome(void);
void expand_home(String *str);
#endif

#endif
