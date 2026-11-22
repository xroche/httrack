/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 2026 Xavier Roche and other contributors

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
This would disgrace our work, and spoil the many hard hours we spent on it.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: htsbuf.h                                               */
/*       writable buffer passed with its size                   */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSBUF_DEFH
#define HTSBUF_DEFH

#include "htssafe.h"

#include <stddef.h>

/* P points at SIZE writable bytes, the end NUL included. It does not convert
   to or from hts_span, the read-only view. */
typedef struct {
  char *p;
  size_t size;
} hts_buf;

/* Return the buffer of SIZE bytes at P. */
static HTS_INLINE HTS_UNUSED hts_buf hts_buf_at(char *p, size_t size) {
  hts_buf b;

  b.p = p;
  b.size = size;
  return b;
}

/* Return B without its first OFF bytes. Aborts unless a byte is left. */
static HTS_INLINE HTS_UNUSED hts_buf hts_buf_tail(hts_buf b, size_t off) {
  assertf(off < b.size);
  return hts_buf_at(b.p + off, b.size - off);
}

/* The buffer over the in-scope array ARR. A pointer does not compile, as with
   htsbuff_array(). */
#ifdef htsbuff_must_be_array_
#define HTS_BUF(ARR)                                                           \
  hts_buf_at((ARR), sizeof(ARR) + htsbuff_must_be_array_(ARR))
#else
#define HTS_BUF(ARR) hts_buf_at((ARR), sizeof(ARR))
#endif

/* HTS_BUF(ARR) that does not compile when ARR holds fewer than N bytes. */
#define HTS_BUF_MIN(ARR, N)                                                    \
  hts_buf_at((ARR), HTS_BUF(ARR).size +                                        \
                        (sizeof(char[sizeof(ARR) >= (N) ? 1 : -1]) - 1))

#endif
