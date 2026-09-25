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
/* File: htsselftest.h                                          */
/*       named dispatch for the hidden engine self-tests        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSSELFTEST_DEFH
#define HTSSELFTEST_DEFH

#ifdef HTS_INTERNAL_BYTECODE

#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif

/* One self-test: the name `-#test=` takes, a usage hint and a description for
   the listing, and the handler that runs it over argv[0..argc-1] and returns
   the process exit code. A module publishes a table of these, ended by a NULL
   name. */
struct selftest_entry {
  const char *name;
  const char *args;
  const char *desc;
  int (*fn)(httrackp *opt, int argc, char **argv);
};

/* Run engine self-test `name` over the positional args argv[0..argc-1], or list
   the available tests when name is NULL, empty, or "list". Prints the result;
   returns the process exit code (0 == success). The caller owns option cleanup.
   Reached through the hidden `httrack -#test[=NAME ...]` subcommand. */
int hts_selftest(httrackp *opt, const char *name, int argc, char **argv);

#endif

#endif
