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
/* File: htsindex_selftest.c subroutines:                       */
/*       self-tests for the keyword indexer                     */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

#include "htsindex.h"

#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif

/* Returns the descriptors this process holds, or -1 if unknown. */
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

/* -#test=indexfd: a repeated index_init and index_finish must not leave a
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

/* Index N bytes that end right before an unreadable page, so a read past them
   faults on every build. Windows only gets an exact heap copy, for ASan. */
static int st_index_guarded(const char *body, size_t n, const char *mime) {
  int ok;
#ifndef _WIN32
  const size_t pg = (size_t) sysconf(_SC_PAGESIZE);
  char *const map = mmap(NULL, 2 * pg, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  assertf(map != MAP_FAILED && n <= pg);
  assertf(mprotect(map + pg, pg, PROT_NONE) == 0);
  memcpy(map + pg - n, body, n);
  ok = index_keyword(hts_span_at(map + pg - n, n), mime, "page.html", "");
  munmap(map, 2 * pg);
#else
  char *page = malloct(n != 0 ? n : 1);

  assertf(page != NULL);
  memcpy(page, body, n);
  ok = index_keyword(hts_span_at(page, n), mime, "page.html", "");
  freet(page);
#endif
  return ok;
}

/* -#test=indexkw <mime> <body>: index BODY (st_decode_body) as one page, then
   print the 0/1 result and index.txt from the cwd. */
static int st_indexkw(httrackp *opt, int argc, char **argv) {
  char body[4096];
  size_t n, len = 0;
  char *out;
  int ok;

  if (argc != 2) {
    fprintf(stderr, "usage: indexkw <mime> <body>\n");
    return 1;
  }
  n = st_decode_body(argv[1], HTS_BUF(body));
  UNLINK("index.txt"); /* a skipped page writes none */
  index_init("");
  ok = st_index_guarded(body, n, argv[0]);
  index_finish("", 1);
  printf("indexkw %d\n", ok);
  out = st_slurp("index.txt", &len);
  if (out != NULL) {
    fwrite(out, 1, len, stdout);
    freet(out);
  }
  return 0;
}

const struct selftest_entry selftests_index[] = {
    {"indexfd", "", "keyword index temp file is closed", st_indexfd},
    {"indexkw", "<mime> <body>", "index one page and print index.txt",
     st_indexkw},
    {NULL, NULL, NULL, NULL},
};
