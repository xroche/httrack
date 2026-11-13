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
/* File: command line splitter, shared by the engine and         */
/*       htsserver                                               */
/* Author: Xavier Roche                                          */
/* ------------------------------------------------------------ */

#include "htscmdline.h"

#include "htssafe.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

char **hts_split_args(char *cmd, int *nargs, int flags) {
  const hts_boolean fold = (flags & HTS_SPLIT_FOLD_WS) != 0;
  const hts_boolean strip = (flags & HTS_SPLIT_STRIP_QUOTES) != 0;
  const hts_boolean drop_empty = (flags & HTS_SPLIT_DROP_EMPTY) != 0;
  size_t nsep = 0;
  size_t capacity;
  size_t r;
  size_t w;
  size_t start = 0;
  hts_boolean has_input = HTS_FALSE; /* any byte since the last separator */
  int argc = 0;
  hts_boolean quoted = HTS_FALSE;
  char **argv;

  *nargs = 0;

  /* fold TAB, CR and LF to spaces when asked, so the space count sizes argv */
  for (r = 0; cmd[r] != '\0'; r++) {
    if (fold && (cmd[r] == '\t' || cmd[r] == '\r' || cmd[r] == '\n')) {
      cmd[r] = ' ';
    }
    if (cmd[r] == ' ') {
      nsep++;
    }
  }

  /* at most one argument per separator, plus the leading one and the NULL */
  if (nsep > (size_t) INT_MAX - 1 || nsep > SIZE_MAX / sizeof(char *) - 2) {
    return NULL;
  }
  capacity = nsep + 2;
  argv = (char **) malloct(capacity * sizeof(char *));
  if (argv == NULL) {
    return NULL;
  }

  for (r = 0, w = 0; cmd[r] != '\0';) {
    if (quoted && cmd[r] == '\\' &&
        (cmd[r + 1] == '\\' || cmd[r + 1] == '\"')) {
      r++;
      cmd[w++] = cmd[r++];
    } else if (cmd[r] == '\"') {
      quoted = !quoted;
      if (!strip) {
        cmd[w++] = cmd[r];
      }
      r++;
      has_input = HTS_TRUE;
    } else if (cmd[r] == ' ' && !quoted) {
      cmd[w++] = '\0';
      if (has_input || !drop_empty) {
        /* the last slot holds the NULL */
        assertf((size_t) argc < capacity - 1);
        argv[argc++] = cmd + start;
      }
      start = w;
      has_input = HTS_FALSE;
      r++;
    } else {
      cmd[w++] = cmd[r++];
      has_input = HTS_TRUE;
    }
  }
  cmd[w] = '\0';
  if (has_input || !drop_empty) {
    /* the loop stored at most one per counted space, so this slot is free */
    assertf((size_t) argc < capacity - 1);
    argv[argc++] = cmd + start;
  }
  argv[argc] = NULL; /* callers may rely on argv[argc] == NULL */

  *nargs = argc;
  return argv;
}

char **hts_split_cmdline(char *cmd, int *nargs) {
  return hts_split_args(cmd, nargs, HTS_SPLIT_FOLD_WS);
}

char **hts_split_webcmd(char *cmd, int *nargs) {
  return hts_split_args(cmd, nargs, HTS_SPLIT_FOLD_WS | HTS_SPLIT_STRIP_QUOTES);
}

void hts_escape_arg(String *out, const char *arg, size_t len) {
  size_t i;

  for (i = 0; i < len; i++) {
    if (arg[i] == '\\' || arg[i] == '\"') {
      StringAddchar(*out, '\\');
    }
    StringAddchar(*out, arg[i]);
  }
}

void hts_quote_arg(String *out, const char *arg) {
  if (arg[0] == '\0') {
    StringCat(*out, "\"\"");
  } else if (strpbrk(arg, " \"\\") != NULL) {
    StringAddchar(*out, '\"');
    hts_escape_arg(out, arg, strlen(arg));
    StringAddchar(*out, '\"');
  } else {
    StringCat(*out, arg);
  }
}

hts_boolean hts_is_quoted(const char *arg, size_t len) {
  return len >= 2 && arg[0] == '\"' && arg[len - 1] == '\"';
}

hts_boolean hts_unquote_arg(char *arg) {
  const size_t len = strlen(arg);

  if (arg[0] != '\"') {
    return HTS_TRUE;
  }
  if (!hts_is_quoted(arg, len)) {
    return HTS_FALSE;
  }
  memmove(arg, arg + 1, len - 2);
  arg[len - 2] = '\0';
  return HTS_TRUE;
}
