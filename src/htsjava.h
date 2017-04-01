/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2017 Xavier Roche and other contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: Java classes parser .h                                 */
/* Author: Yann Philippot                                       */
/* ------------------------------------------------------------ */

#ifndef HTSJAVA_DEFH
#define HTSJAVA_DEFH

#ifndef HTS_DEF_FWSTRUCT_JAVA_HEADER
#define HTS_DEF_FWSTRUCT_JAVA_HEADER
typedef struct JAVA_HEADER JAVA_HEADER;
#endif
struct JAVA_HEADER {
  unsigned long int magic;
  unsigned short int minor;
  unsigned short int major;
  unsigned short int count;
};

#ifndef HTS_DEF_FWSTRUCT_RESP_STRUCT
#define HTS_DEF_FWSTRUCT_RESP_STRUCT
typedef struct RESP_STRUCT RESP_STRUCT;
#endif
struct RESP_STRUCT {
  int file_position;
  //
  unsigned int index1;
  unsigned int type;
  char name[1024];
};

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

EXTERNAL_FUNCTION int hts_plug_java(httrackp * opt, const char *argv);

#endif

#endif
