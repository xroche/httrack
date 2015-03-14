/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2015 Xavier Roche and other contributors

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
/* File: Unpacking subroutines using Jean-loup Gailly's Zlib    */
/*       for http compressed data                               */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_DEFZLIB
#define HTS_DEFZLIB

/* ZLib */
#include "zlib.h"
//#include "zutil.h"

/* MiniZip */
#include "minizip/zip.h"
#include "minizip/unzip.h"
#include "minizip/mztools.h"

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE
extern int hts_zunpack(char *filename, char *newfile);
extern int hts_extract_meta(const char *path);
extern const char *hts_get_zerror(int err);
#endif

#endif
