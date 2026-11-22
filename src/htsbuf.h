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

#include <stdarg.h>
#include <stddef.h>

/* P holds SIZE writable bytes. It never converts implicitly to hts_span, and
   unlike htssafe.h's htsbuff it has no cursor. */
typedef struct {
  char *p;
  size_t size;
} hts_buf;

/* Returns SIZE bytes at P, for forwarding a pointer that came with its size. */
static HTS_INLINE HTS_UNUSED hts_buf hts_buf_at(char *p, size_t size) {
  hts_buf b;

  b.p = p;
  b.size = size;
  return b;
}

/* Return B without its first OFF bytes, aborting unless a byte is left. */
static HTS_INLINE HTS_UNUSED hts_buf hts_buf_skip(hts_buf b, size_t off) {
  assertf(off < b.size);
  return hts_buf_at(b.p + off, b.size - off);
}

static HTS_INLINE HTS_UNUSED void hts_buf_cpy_(hts_buf b, const char *s,
                                               const char *file, int line) {
  (void) strcpy_safe_(b.p, b.size, s, (size_t) -1,
                      "overflow while copying into a buffer", file, line);
}

static HTS_INLINE HTS_UNUSED void
hts_buf_catn_(hts_buf b, const char *s, size_t n, const char *file, int line) {
  (void) strncat_safe_(b.p, b.size, s, (size_t) -1, n,
                       "overflow while appending to a buffer", file, line);
}

/* Copy, append, or append at most N bytes of S to B's text, aborting unless it
   fits, as strlcpybuff, strlcatbuff and strlncatbuff. The abort names the
   caller's line. */
#define hts_buf_cpy(B, S) hts_buf_cpy_((B), (S), __FILE__, __LINE__)
#define hts_buf_cat(B, S)                                                      \
  hts_buf_catn_((B), (S), (size_t) -1, __FILE__, __LINE__)
#define hts_buf_catn(B, S, N) hts_buf_catn_((B), (S), (N), __FILE__, __LINE__)

/* Copy S into B, clipped to fit, as strclipbuff. TRUE if it all fit. */
static HTS_INLINE HTS_UNUSED hts_boolean hts_buf_clip(hts_buf b,
                                                      const char *s) {
  return strclipbuff(b.p, b.size, s);
}

/* Print into B, clipped to fit, as slprintfbuff. TRUE if it all fit. */
static HTS_INLINE HTS_UNUSED HTS_CHECK_RESULT HTS_PRINTF_FUN(2, 3) hts_boolean
    hts_buf_printf(hts_buf b, HTS_FORMAT_STRING const char *fmt, ...) {
  va_list args;
  hts_boolean fit;

  va_start(args, fmt);
  fit = vslprintfbuff(b.p, b.size, fmt, args);
  va_end(args);
  return fit;
}

/* hts_buf_printf for text meant to be clipped, as slprintfbuff_clip. */
static HTS_INLINE HTS_UNUSED HTS_PRINTF_FUN(2, 3) void hts_buf_printf_clip(
    hts_buf b, HTS_FORMAT_STRING const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  (void) vslprintfbuff(b.p, b.size, fmt, args);
  va_end(args);
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
