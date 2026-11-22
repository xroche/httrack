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

/* P points at SIZE writable bytes, the end NUL included. There is no implicit
   conversion to or from hts_span. The htsbuff of htssafe.h is an append cursor,
   which is a different type. */
typedef struct {
  char *p;
  size_t size;
} hts_buf;

/* Use it only to forward a pointer that comes with its size, never an array. */
static HTS_INLINE HTS_UNUSED hts_buf hts_buf_at(char *p, size_t size) {
  hts_buf b;

  b.p = p;
  b.size = size;
  return b;
}

/* Return B without its first OFF bytes. It aborts unless a byte is left. */
static HTS_INLINE HTS_UNUSED hts_buf hts_buf_tail(hts_buf b, size_t off) {
  assertf(off < b.size);
  return hts_buf_at(b.p + off, b.size - off);
}

/* HTS_BUF(ARR) is the buffer over array ARR, and a pointer does not compile.
   HTS_BUF_N(ARR, N) is its first N bytes, and N past ARR does not compile. */
#ifdef htsbuff_must_be_array_
#define HTS_BUF_GUARD_(ARR) htsbuff_must_be_array_(ARR)
#else
#define HTS_BUF_GUARD_(ARR) 0
#endif
#define HTS_BUF(ARR) hts_buf_at((ARR), sizeof(ARR) + HTS_BUF_GUARD_(ARR))
#define HTS_BUF_N(ARR, N)                                                      \
  (HTS_COMPILE_ASSERT((N) <= sizeof(ARR)),                                     \
   hts_buf_at((ARR), (N) + HTS_BUF_GUARD_(ARR)))

#endif
