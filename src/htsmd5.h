/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998 Xavier Roche and other contributors

SPDX-License-Identifier: GPL-3.0-or-later

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

Ethical use: we kindly ask that you NOT use this software to harvest email
addresses or to collect any other private information about people. Doing so
would dishonor our work and waste the many hours we have spent on it.

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
struct MD5Context;

/* MD5Init() with the flag that gives the standard digest on this host. */
void hts_md5_init(struct MD5Context *ctx);

/* Writes the 32 lowercase hex digits of 'bindigest' and a NUL into 'hex'. */
void hts_md5_hex(const unsigned char bindigest[16], char hex[33]);

int domd5mem(const char *buf, size_t len, char *digest, int asAscii);
void md5selftest(void);
#endif

#endif
