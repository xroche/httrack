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
/* File: htsmd5.c subroutines:                                  */
/*       generate a md5 hash                                    */
/*                                                              */
/* Written March 1993 by Branko Lankester                       */
/* Modified June 1993 by Colin Plumb for altered md5.c.         */
/* Modified October 1995 by Erik Troan for RPM                  */
/* Modified 2000 by Xavier Roche for domd5mem                   */
/* ------------------------------------------------------------ */

#ifndef HTSMD5_DEFH
#define HTSMD5_DEFH

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE
int domd5mem(const char *buf, size_t len, char *digest, int asAscii);
unsigned long int md5sum32(const char *buff);
void md5selftest(void);
#endif

#endif
