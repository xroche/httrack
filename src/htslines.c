/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 2026 Xavier Roche and other contributors

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
/* File: htslines.c                                             */
/*       line readers for config files and an owner-only file   */
/*       opener, shared by the engine and htsserver             */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htslines.h"
#include "htslib.h"

#include <errno.h>
#include <string.h>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

FILE *hts_fopen_private(const char *name, const char *mode) {
  char catbuff[CATBUFF_SIZE];

  /* O_TRUNC below would silently turn an append into a rewrite */
  if (strcmp(mode, "w") != 0 && strcmp(mode, "wb") != 0) {
    errno = EINVAL;
    return NULL;
  }
#ifdef _WIN32
  return FOPEN(fconv(catbuff, sizeof(catbuff), name), mode);
#else
  {
    const int fd = open(fconv(catbuff, sizeof(catbuff), name),
                        O_WRONLY | O_CREAT | O_TRUNC, HTS_PROTECT_FILE);
    FILE *fp = fd != -1 ? fdopen(fd, mode) : NULL;

    if (fd != -1 && fp == NULL)
      close(fd);    /* fdopen failed: don't leak the descriptor */
    if (fp != NULL) /* O_CREAT's mode skips pre-existing files: tighten those */
      (void) fchmod(fd, HTS_PROTECT_FILE);
    return fp;
  }
#endif
}

/* hts_readline_raw() keeps every byte. */
#define LINE_RAW 0x100

/* Is c one of the bytes flags drop? */
static hts_boolean line_drops(int c, int flags) {
  return (flags & LINE_RAW) == 0 &&
         (c == '\r' || ((flags & HTS_LINE_DROP_TAB) != 0 && c == '\t') ||
          ((flags & HTS_LINE_DROP_FF) != 0 && c == '\f') ||
          ((flags & HTS_LINE_DROP_NUL) != 0 && c == '\0'));
}

/* How a line ends once trimmed, which decides whether it continues. */
typedef struct line_end {
  size_t run;        /* trailing backslashes */
  hts_boolean other; /* holds a byte besides those backslashes */
} line_end;

/* hts_readline(), also giving the number of bytes stored and how the whole
   line ends once trimmed, from every byte read, stored or not. */
static hts_boolean readline_end(FILE *fp, char *s, size_t max, int flags,
                                size_t *len, line_end *end) {
  hts_boolean cut = HTS_FALSE;
  hts_boolean lead = HTS_TRUE; /* only spaces and TABs so far */
  hts_boolean gap = HTS_FALSE; /* whitespace kept if a byte follows */
  size_t j = 0;
  int c;

  end->run = 0;
  end->other = HTS_FALSE;
  while ((c = fgetc(fp)) != EOF && c != '\n') {
    if (line_drops(c, flags))
      continue;
    if (is_realspace(c)) {
      if (!lead || (c != ' ' && c != '\t')) {
        lead = HTS_FALSE;
        gap = HTS_TRUE;
      }
    } else {
      if (gap) {
        end->other = HTS_TRUE;
        end->run = 0;
      }
      lead = gap = HTS_FALSE;
      if (c == '\\') {
        end->run++;
      } else {
        end->other = HTS_TRUE;
        end->run = 0;
      }
    }
    if (j + 1 < max)
      s[j++] = (char) c;
    else if ((flags & LINE_RAW) != 0 || !is_realspace(c))
      cut = HTS_TRUE;
  }
  s[j] = '\0';
  *len = j;
  return cut;
}

hts_boolean hts_readline(FILE *fp, char *s, size_t max, int flags) {
  size_t len;
  line_end end;

  return readline_end(fp, s, max, flags, &len, &end);
}

hts_boolean hts_readline_raw(FILE *fp, char *s, size_t max, size_t *len) {
  line_end end;

  return readline_end(fp, s, max, LINE_RAW, len, &end);
}

/* readline_end(), then the trim hts_readline_cpp() does on each line. */
static hts_boolean readline_trim(FILE *fp, char *s, size_t max, int flags,
                                 size_t *len, line_end *end) {
  const hts_boolean cut = readline_end(fp, s, max, flags, len, end);
  size_t n = *len;
  size_t i = 0;

  while (n > 0 && is_realspace(s[n - 1]))
    s[--n] = '\0';
  while (i < n && (s[i] == ' ' || s[i] == '\t'))
    i++;
  if (i > 0) {
    memmove(s, s + i, n - i);
    n -= i;
    s[n] = '\0';
  }
  *len = n;
  return cut;
}

hts_boolean hts_readline_cpp(FILE *fp, char *s, size_t max, int flags) {
  hts_boolean cut = HTS_FALSE;
  size_t rlen = 0;
  /* trailing backslashes of the whole joined line, stored or not */
  size_t run = 0;

  s[0] = '\0';
  do {
    size_t n;
    line_end end;

    if (run > 0) {
      run--;
      if (rlen > 0)
        s[--rlen] = '\0';
    }
    if (readline_trim(fp, s + rlen, max - rlen, flags, &n, &end))
      cut = HTS_TRUE;
    rlen += n;
    run = end.other ? end.run : run + end.run;
  } while (run > 0);
  return cut;
}

hts_boolean hts_readline_alloc(FILE *fp, String *line, size_t limit,
                               int flags) {
  hts_boolean cut = HTS_FALSE;
  int c;

  StringClear(*line);
  while ((c = fgetc(fp)) != EOF && c != '\n') {
    if (line_drops(c, flags))
      continue;
    if (StringLength(*line) < limit)
      StringAddchar(*line, (char) c);
    else
      cut = HTS_TRUE;
  }
  return cut;
}
