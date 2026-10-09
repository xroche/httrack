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
/* File: htstools_selftest.c subroutines:                       */
/*       self-tests for the HTML attribute-value scanner        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

/* endtoken TAIL: what rech_endtoken() reads as the value at TAIL, which is a
   tag's bytes from just after `attr=`. An empty TAIL arrives as no argument. */
static int st_endtoken(httrackp *opt, int argc, char **argv) {
  const char *const tail = argc >= 1 ? argv[0] : "";
  const char *value = NULL;
  const int len = rech_endtoken(tail, &value);

  (void) opt;
  printf("len=%d value=[%.*s]\n", len, len, value);
  return 0;
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

const struct selftest_entry selftests_tools[] = {
    {"endtoken", "<tag bytes after attr=>", "value rech_endtoken() reads",
     st_endtoken},
    {NULL, NULL, NULL, NULL},
};
