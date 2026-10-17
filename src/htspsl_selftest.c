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
/* File: htspsl_selftest.c subroutines:                         */
/*       self-tests for the Public Suffix List lookups          */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

#include "htspsl.h"

/* Print the registrable domain of host, lowercased, or "null". */
static void print_registrable(const char *host) {
  char buf[256];
  const char *reg;

  buf[0] = '\0';
  strlncatbuff(buf, host, sizeof(buf), sizeof(buf) - 1);
  hts_lowcase(buf);
  reg = hts_psl_registrable_domain(buf);
  printf("%s\n", reg != NULL ? reg : "null");
}

/* -#test=psl <host>: the registrable domain of host under the list in use. */
static int st_psl(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc != 1) {
    fprintf(stderr, "psl: takes <host>\n");
    return 1;
  }
  print_registrable(argv[0]);
  return 0;
}

/* -#test=pslfile <file> <host>: look host up under a list file, print the
   file's stamp, then restore the default list. */
static int st_pslfile(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc != 2) {
    fprintf(stderr, "pslfile: takes <file> <host>\n");
    return 1;
  }
  if (hts_psl_load_file(argv[0], 1)) {
    print_registrable(argv[1]);
    printf("%s\n", hts_psl_version());
  } else {
    printf("unreadable\n");
  }
  hts_psl_init();
  return 0;
}

/* -#test=pslinit <file>: the stamp of the list a build configured with
   --with-psl-file=<file> would use, then restore the default list. */
static int st_pslinit(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc != 1) {
    fprintf(stderr, "pslinit: takes <file>\n");
    return 1;
  }
  hts_psl_init_from(argv[0]);
  printf("%s\n", hts_psl_version());
  hts_psl_init();
  return 0;
}

/* -#test=pslversion: the VERSION stamp of the list in use. */
static int st_pslversion(httrackp *opt, int argc, char **argv) {
  (void) opt;
  (void) argc;
  (void) argv;
  printf("%s\n", hts_psl_version());
  return 0;
}

const struct selftest_entry selftests_psl[] = {
    {"psl", "<host>", "registrable domain under the Public Suffix List",
     st_psl},
    {"pslfile", "<file> <host>", "registrable domain under a list file",
     st_pslfile},
    {"pslinit", "<file>", "VERSION stamp chosen for a list file", st_pslinit},
    {"pslversion", "", "VERSION stamp of the Public Suffix List in use",
     st_pslversion},
    {NULL, NULL, NULL, NULL},
};
