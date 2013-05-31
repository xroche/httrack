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
/* File: Encoding conversion functions                          */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htscharset.h"
#include "htsencoding.h"

/* static int decode_entity(const unsigned int hash, const size_t len);
*/
#include "htsentities.h"

/* hexadecimal conversion */
static int get_hex_value(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  else if (c >= 'a' && c <= 'f')
    return (c - 'a' + 10);
  else if (c >= 'A' && c <= 'F')
    return (c - 'A' + 10);
  else
    return -1;
}

/* Numerical Recipes,
   see <http://en.wikipedia.org/wiki/Linear_congruential_generator> */
#define HASH_PRIME ( 1664525 )
#define HASH_CONST ( 1013904223 )
#define HASH_ADD(HASH, C) do {                  \
    (HASH) *= HASH_PRIME;                       \
    (HASH) += HASH_CONST;                       \
    (HASH) += (C);                              \
  } while(0)

int hts_unescapeEntitiesWithCharset(const char *src, char *dest, const size_t max, const char *charset) {
  size_t i, j, ampStart, ampStartDest;
  int uc;
  int hex;
  unsigned int hash;
  for(i = 0, j = 0, ampStart = (size_t) -1, ampStartDest = 0,
        uc = -1, hex = 0, hash = 0 ; src[i] != '\0' ; i++) {
    /* start of entity */
    if (src[i] == '&') {
      ampStart = i;
      ampStartDest = j;
      hash = 0;
      uc = -1;
    }
    /* inside a potential entity */
    else if (ampStart != (size_t) -1) {
      /* &#..; entity */
      if (ampStart + 1 == i && src[ampStart + 1] == '#') {
        uc = 0;
        hex = 0;
      }
      /* &#x..; entity */
      else if (ampStart + 2 == i && src[ampStart + 1] == '#'
               && src[ampStart + 2] == 'x') {
        hex = 1;
      }
      /* end of entity */
      else if (src[i] == ';') {
        size_t len;
        
        /* decode entity */
        if (uc == -1) {
          /* &foo; */
          uc = decode_entity(hash, /*&src[ampStart + 1],*/
                             i - ampStart - 1);
          /* FIXME: TEMPORARY HACK FROM PREVIOUS VERSION TO BE INVESTIGATED */
          if (uc == 160) {
            uc = 32;
          }
        }
        
        /* end */
        ampStart = (size_t) -1;
        
        /* success ? */
        if (uc > 0) {
          const size_t maxOut = max - ampStartDest;
          /* write at position */
          if (charset != NULL && hts_isCharsetUTF8(charset)) {
            len = hts_writeUTF8(uc, &dest[ampStartDest], maxOut);
          } else {
            size_t ulen;
            char buffer[32];
            len = 0;
            if ( ( ulen = hts_writeUTF8(uc, buffer, sizeof(buffer)) ) != 0) {
              char *s;
              buffer[ulen] = '\0';
              s = hts_convertStringFromUTF8(buffer, strlen(buffer), charset);
              if (s != NULL) {
                const size_t sLen = strlen(s);
                if (sLen < maxOut) {
                  // Do not copy \0.
                  memcpy(&dest[ampStartDest], s, sLen);
                  len = sLen;
                }
                free(s);
              }
            }
          }
          if (len > 0) {
            /* new dest position */
            j = ampStartDest + len;
            /* do not copy ; */
            continue;
          }
        }
      }
      /* numerical entity */
      else if (uc != -1) {
        /* decimal */
        if (!hex) {
          if (src[i] >= '0' && src[i] <= '9') {
            const int h = src[i] - '0';
            uc *= 10;
            uc += h;
          } else {
            /* abandon */
            ampStart = (size_t) -1;
          }
        }
        /* hex */
        else {
          const int h = get_hex_value(src[i]);
          if (h != -1) {
            uc *= 16;
            uc += h;
          } else {
            /* abandon */
            ampStart = (size_t) -1;
          }
        }
      }
      /* alphanumerical entity */
      else {
        /* alphanum and not too far ('&thetasym;' is the longest) */
        if (i <= ampStart + 10 &&
            (
             (src[i] >= '0' && src[i] <= '9')
             || (src[i] >= 'A' && src[i] <= 'Z')
             || (src[i] >= 'a' && src[i] <= 'z')
             )
            ) {
          /* compute hash */
          HASH_ADD(hash, (unsigned char) src[i]);
        } else {
          /* abandon */
          ampStart = (size_t) -1;
        }
      }
    }
    
    /* copy */
    if (j + 1 > max) {
      /* overflow */
      return -1;
    }
    if (src != dest || i != j) {
      dest[j] = src[i];
    }
    j++;
  }
  dest[j] = '\0';

  return 0;
}

int hts_unescapeEntities(const char *src, char *dest, const size_t max) {
  return hts_unescapeEntitiesWithCharset(src, dest, max, "UTF-8");
}
