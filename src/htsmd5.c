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
/* File: htsmd5.c subroutines:                                  */
/*       generate a md5 hash                                    */
/*                                                              */
/* Written March 1993 by Branko Lankester                       */
/* Modified June 1993 by Colin Plumb for altered md5.c.         */
/* Modified October 1995 by Erik Troan for RPM                  */
/* Modified 2000 by Xavier Roche for domd5mem                   */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "htsmd5.h"
#include "md5.h"

int domd5mem(const unsigned char * buf, size_t len, 
                    unsigned char * digest, int asAscii) {
  int endian = 1;
  unsigned char bindigest[16];
  MD5_CTX ctx;

  MD5Init(&ctx, * ( (char*) &endian));
  MD5Update(&ctx, buf, (unsigned int) len);
  MD5Final(bindigest, &ctx);

  if (!asAscii) {
    memcpy(digest, bindigest, 16);
  } else {
    sprintf(digest, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
      "%02x%02x%02x%02x%02x",
      bindigest[0],  bindigest[1],  bindigest[2],  bindigest[3],
      bindigest[4],  bindigest[5],  bindigest[6],  bindigest[7],
      bindigest[8],  bindigest[9],  bindigest[10], bindigest[11],
      bindigest[12], bindigest[13], bindigest[14], bindigest[15]);
  }
  
  return 0;
}

unsigned long int md5sum32(const char* buff) {
  unsigned char md5digest[16];
  domd5mem(buff,(int)strlen(buff),md5digest,0);
  return *( (long int*)(char*)md5digest );
}
