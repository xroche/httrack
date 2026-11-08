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


/* ------------------------------------------------------------ */
/* File: htsindex_selftest.c subroutines:                       */
/*       self-tests for the keyword indexer                     */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

#include "htsindex.h"

#include <dirent.h>

/* How many descriptors this process holds, or -1 if it cannot be told. */
static int open_fd_count(void) {
#if defined(_WIN32)
  return -1;
#else
  DIR *d = opendir("/proc/self/fd");
  int n = 0;

  if (d == NULL)
    return -1;
  while (readdir(d) != NULL)
    n++;
  closedir(d);
  return n;
#endif
}

/* -#test=indexfd: a repeated index_init, then index_finish, must not leave a
   temporary file open. Prints SKIP when descriptors cannot be counted. */
static int st_indexfd(httrackp *opt, int argc, char **argv) {
  const int before = open_fd_count();
  int after;

  if (before < 0) {
    printf("indexfd SKIP\n");
    return 0;
  }
  index_init("");
  index_init("");
  index_finish("", 1);
  after = open_fd_count();
  if (after != before) {
    fprintf(stderr, "indexfd: %d descriptors before, %d after\n", before,
            after);
    return 1;
  }
  printf("indexfd OK\n");
  return 0;
}

const struct selftest_entry selftests_index[] = {
    {"indexfd", "", "keyword index temp file is closed", st_indexfd},
    {NULL, NULL, NULL, NULL},
};
