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
  Unpack file into a new file (gzip, zlib RFC1950 or raw deflate RFC1951).
  A body with no compressed framing at all is copied verbatim (identity).
  Return value: size of the new file, or -1 if an error occurred
*/
/* Note: utf-8 */
int hts_zunpack(char *filename, char *newfile) {
  int ret = -1;

  if (filename != NULL && newfile != NULL && filename[0] && newfile[0]) {
    char catbuff[CATBUFF_SIZE];
    FILE *const in = FOPEN(fconv(catbuff, sizeof(catbuff), filename), "rb");

    if (in != NULL) {
      unsigned char BIGSTK inbuf[8192];
      size_t navail = fread(inbuf, 1, sizeof(inbuf), in);
      /* gzip/zlib headers -> +32 windowBits; else raw deflate (RFC1951) */
      const hts_boolean wrapped =
          (navail >= 2 &&
           ((inbuf[0] == 0x1f && inbuf[1] == 0x8b) ||
            ((inbuf[0] & 0x0f) == Z_DEFLATED &&
             (((unsigned) inbuf[0] << 8 | inbuf[1]) % 31) == 0)));
      int attempt;

      /* deflate is ambiguous; on failure retry with the other windowBits */
      for (attempt = 0; attempt < 2 && ret < 0; attempt++) {
        const int windowBits =
            (attempt == 0 ? wrapped : !wrapped) ? (32 + MAX_WBITS) : -MAX_WBITS;
        FILE *fpout;
        z_stream strm;

        if (attempt > 0) {
          /* rewind input; reopening fpout "wb" discards the partial output */
          if (fseek(in, 0, SEEK_SET) != 0)
            break;
          navail = fread(inbuf, 1, sizeof(inbuf), in);
        }
        fpout = FOPEN(fconv(catbuff, sizeof(catbuff), newfile), "wb");
        if (fpout == NULL)
          break;
        memset(&strm, 0, sizeof(strm));
        if (inflateInit2(&strm, windowBits) != Z_OK) {
          fclose(fpout);
          break;
        }
        {
          hts_boolean ok = HTS_TRUE;
          int size = 0;
          int zerr = Z_OK;

          /* chunked inflate; first chunk in inbuf, single member */
          do {
            strm.next_in = inbuf;
            strm.avail_in = (uInt) navail;
            do {
              unsigned char BIGSTK outbuf[8192];
              size_t produced;

              strm.next_out = outbuf;
              strm.avail_out = sizeof(outbuf);
              zerr = inflate(&strm, Z_NO_FLUSH);
              if (zerr == Z_NEED_DICT || zerr == Z_DATA_ERROR ||
                  zerr == Z_MEM_ERROR || zerr == Z_STREAM_ERROR) {
                ok = HTS_FALSE;
                break;
              }
              produced = sizeof(outbuf) - strm.avail_out;
              if (produced > 0 &&
                  fwrite(outbuf, 1, produced, fpout) != produced) {
                ok = HTS_FALSE;
                break;
              }
              size += (int) produced;
            } while (strm.avail_out == 0);
            if (!ok || zerr == Z_STREAM_END)
              break;
            navail = fread(inbuf, 1, sizeof(inbuf), in);
          } while (navail > 0);
          if (ok && zerr == Z_STREAM_END)
            ret = size;
        }
        inflateEnd(&strm);
        fclose(fpout);
      }
      /* neither framing decodes and no gzip/zlib header: server mislabeled
         an identity body as compressed; keep it verbatim (#47) */
      if (ret < 0 && !wrapped) {
        FILE *const fpout =
            FOPEN(fconv(catbuff, sizeof(catbuff), newfile), "wb");

        if (fpout != NULL && fseek(in, 0, SEEK_SET) == 0) {
          int size = 0;

          while ((navail = fread(inbuf, 1, sizeof(inbuf), in)) > 0) {
            if (fwrite(inbuf, 1, navail, fpout) != navail) {
              size = -1;
              break;
            }
            size += (int) navail;
          }
          if (size >= 0 && !ferror(in))
            ret = size;
        }
        if (fpout != NULL)
          fclose(fpout);
      }
      fclose(in);
    }
  }
  return ret;
}

int hts_extract_meta(const char *path) {
  char catbuff[CATBUFF_SIZE];
  unzFile zFile = unzOpen(fconcat(catbuff, sizeof(catbuff), path, "hts-cache/new.zip"));
  zipFile zFileOut = zipOpen(fconcat(catbuff, sizeof(catbuff), path, "hts-cache/meta.zip"), 0);

  if (zFile != NULL && zFileOut != NULL) {
    if (unzGoToFirstFile(zFile) == Z_OK) {
      zip_fileinfo fi;
      unz_file_info ufi;
      char BIGSTK filename[HTS_URLMAXSIZE * 4];
      char BIGSTK comment[8192];

      memset(comment, 0, sizeof(comment));      // for truncated reads
      memset(&fi, 0, sizeof(fi));
      memset(&ufi, 0, sizeof(ufi));
      do {
        int readSizeHeader;

        filename[0] = '\0';
        comment[0] = '\0';

        if (unzOpenCurrentFile(zFile) == Z_OK) {
          if ((readSizeHeader =
               unzGetLocalExtrafield(zFile, comment, sizeof(comment) - 2)) > 0
              && unzGetCurrentFileInfo(zFile, &ufi, filename,
                                       sizeof(filename) - 2, NULL, 0, NULL,
                                       0) == Z_OK) {
            comment[readSizeHeader] = '\0';
            fi.dosDate = ufi.dosDate;
            fi.internal_fa = ufi.internal_fa;
            fi.external_fa = ufi.external_fa;
            if (zipOpenNewFileInZip(zFileOut, filename, &fi, NULL, 0, NULL, 0, NULL,    /* comment */
                                    Z_DEFLATED, Z_DEFAULT_COMPRESSION) == Z_OK) {
              if (zipWriteInFileInZip(zFileOut, comment, (int) strlen(comment))
                  != Z_OK) {
              }
              if (zipCloseFileInZip(zFileOut) != Z_OK) {
              }
            }
          }
          unzCloseCurrentFile(zFile);
        }
      } while(unzGoToNextFile(zFile) == Z_OK);
    }
    zipClose(zFileOut, "Meta-data extracted by HTTrack/" HTTRACK_VERSION);
    unzClose(zFile);
    return 1;
  }
  return 0;
}

const char *hts_get_zerror(int err) {
  switch (err) {
  case UNZ_OK:
    return "no error";
    break;
  case UNZ_END_OF_LIST_OF_FILE:
    return "end of list of file";
    break;
  case UNZ_ERRNO:
    return (const char *) strerror(errno);
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
