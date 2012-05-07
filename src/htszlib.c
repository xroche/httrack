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
/* File: Unpacking subroutines using Jean-loup Gailly's Zlib    */
/*       for http compressed data                               */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

/* specific definitions */
#include "htsbase.h"
#include "htscore.h"
#include "htszlib.h"

#if HTS_USEZLIB
/* zlib */
/*
#include <zlib.h>
#include "htszlib.h"
*/

/*
  Unpack file into a new file
  Return value: size of the new file, or -1 if an error occured
*/
/* Note: utf-8 */
int hts_zunpack(char* filename,char* newfile) {
  int ret = -1;
	char catbuff[CATBUFF_SIZE];
  if (gz_is_available && filename && newfile) {
    if (filename[0] && newfile[0]) {
      // not: NOT an UTF-8 filename
      gzFile gz = gzopen(filename, "rb");
      if (gz) {
        FILE*const fpout = FOPEN(fconv(catbuff, newfile), "wb");
        int size=0;
        if (fpout) {
          int nr;
          do {
            char BIGSTK buff[1024];
            nr=gzread (gz, buff, 1024);
            if (nr>0) {
              size+=nr;
              if (fwrite(buff,1,nr,fpout) != nr)
                nr=size=-1;
            }
          } while(nr>0);
          fclose(fpout);
        } else
          size=-1;
        gzclose(gz);
        ret = (int) size;
      }
    }
  }
  return ret;
}

int hts_extract_meta(const char* path) {
	char catbuff[CATBUFF_SIZE];
  unzFile zFile = unzOpen(fconcat(catbuff,path,"hts-cache/new.zip"));
  zipFile zFileOut = zipOpen(fconcat(catbuff,path,"hts-cache/meta.zip"), 0);
  if (zFile != NULL && zFileOut != NULL) {
    if (unzGoToFirstFile(zFile) == Z_OK) {
      zip_fileinfo fi;
      unz_file_info ufi;
      char BIGSTK filename[HTS_URLMAXSIZE * 4];
      char BIGSTK comment[8192];
      memset(comment, 0, sizeof(comment));       // for truncated reads
      memset(&fi, 0, sizeof(fi));
      memset(&ufi, 0, sizeof(ufi));
      do  {
        int readSizeHeader;
        filename[0] = '\0';
        comment[0] = '\0';
        
        if (unzOpenCurrentFile(zFile) == Z_OK) {
          if (
            (readSizeHeader = unzGetLocalExtrafield(zFile, comment, sizeof(comment) - 2)) > 0
            &&
            unzGetCurrentFileInfo(zFile, &ufi, filename, sizeof(filename) - 2, NULL, 0, NULL, 0) == Z_OK
            ) 
          {
            comment[readSizeHeader] = '\0';
            fi.dosDate = ufi.dosDate;
            fi.internal_fa = ufi.internal_fa;
            fi.external_fa = ufi.external_fa;
            if (zipOpenNewFileInZip(zFileOut,
              filename,
              &fi,
              NULL,
              0,
              NULL,
              0,
              NULL, /* comment */
              Z_DEFLATED,
              Z_DEFAULT_COMPRESSION) == Z_OK)
            {
              if (zipWriteInFileInZip(zFileOut, comment, (int) strlen(comment)) != Z_OK) {
              }
              if (zipCloseFileInZip(zFileOut) != Z_OK) {
              }
            }
          }
          unzCloseCurrentFile(zFile);
        }
      } while( unzGoToNextFile(zFile) == Z_OK );
    }
    zipClose(zFileOut, "Meta-data extracted by HTTrack/"HTTRACK_VERSION);
    unzClose(zFile);
    return 1;
  }
  return 0;
}

const char* hts_get_zerror(int err) {
  switch(err) {
    case UNZ_OK:
      return "no error";
      break;
    case UNZ_END_OF_LIST_OF_FILE:
      return "end of list of file";
      break;
    case UNZ_ERRNO:
      return (const char*) strerror(errno);
      break;
    case UNZ_PARAMERROR:
      return "parameter error";
      break;
    case UNZ_BADZIPFILE:
      return "bad zip file";
      break;
    case UNZ_INTERNALERROR:
      return "internal error";
      break;
    case UNZ_CRCERROR:
      return "crc error";
      break;
    default:
      return "unknown error";
      break;
  }
}
#endif
