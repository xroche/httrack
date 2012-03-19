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
/* File: Unpacking subroutines using Jean-loup Gailly's Zlib    */
/*       for http compressed data                               */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */


/* specific definitions */
#include <stdio.h>
#include <stdlib.h>
#include "htsbase.h"
#include "htscore.h"

#if HTS_USEZLIB

/* zlib */
#include <zlib.h>
#include "htszlib.h"

/*
  Unpack file into a new file
  Return value: size of the new file, or -1 if an error occured
*/
int hts_zunpack(char* filename,char* newfile) {
  if (filename && newfile) {
    if (filename[0] && newfile[0]) {
      gzFile gz = gzopen (filename, "rb");
      if (gz) {
        FILE* fpout=fopen(fconv(newfile),"wb");
        int size=0;
        if (fpout) {
          int nr;
          do {
            char buff[1024];
            nr=gzread (gz, buff, 1024);
            if (nr>0) {
              size+=nr;
              if ((int)fwrite(buff,1,nr,fpout) != nr)
                nr=size=-1;
            }
          } while(nr>0);
          fclose(fpout);
        } else
          size=-1;
        gzclose(gz);
        return size;
      }
    }
  }
  return -1;
}

#endif
