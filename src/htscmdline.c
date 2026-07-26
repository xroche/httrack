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

char **hts_split_cmdline(char *cmd, int *nargs) {
  size_t nsep = 0;
  size_t capacity;
  size_t r;
  size_t w;
  int argc = 0;
  hts_boolean quoted = HTS_FALSE;
  char **argv;

  *nargs = 0;

  /* fold the other separators, so counting them sizes the vector exactly */
  for (r = 0; cmd[r] != '\0'; r++) {
    if (cmd[r] == '\t' || cmd[r] == '\r' || cmd[r] == '\n') {
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

  argv[argc++] = cmd;
  for (r = 0, w = 0; cmd[r] != '\0';) {
    if (quoted && cmd[r] == '\\' &&
        (cmd[r + 1] == '\\' || cmd[r + 1] == '\"')) {
      r++;
      cmd[w++] = cmd[r++];
    } else if (cmd[r] == '\"') {
      quoted = !quoted;
      cmd[w++] = cmd[r++];
    } else if (cmd[r] == ' ' && !quoted) {
      cmd[w++] = '\0';
      assertf((size_t) argc < capacity - 1); /* the last slot holds the NULL */
      argv[argc++] = cmd + w;
      r++;
    } else {
      cmd[w++] = cmd[r++];
    }
  }
  cmd[w] = '\0';
  argv[argc] = NULL; /* callers may rely on argv[argc] == NULL */

  *nargs = argc;
  return argv;
}
