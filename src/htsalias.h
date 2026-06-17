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
/* File: htsalias.h subroutines:                                */
/*       alias for command-line options and config files        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSALIAS_DEFH
#define HTSALIAS_DEFH

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE
extern const char *hts_optalias[][4];
int optalias_check(int argc, const char *const *argv, int n_arg,
                   int *return_argc, char **return_argv,
                   size_t return_argv_size, char *return_error,
                   size_t return_error_size);
int optalias_find(const char *token);
const char *optalias_help(const char *token);
int optreal_find(const char *token);
int optinclude_file(const char *name, int *argc, char **argv, char *x_argvblk,
                    size_t x_argvblk_size, int *x_ptr);
const char *optreal_value(int p);
const char *optalias_value(int p);
const char *opttype_value(int p);
const char *opthelp_value(int p);
const char *hts_gethome(void);
void expand_home(String * str);

/* Command-line argv-block builders, shared by htscoremain.c (the CLI parser)
   and htsalias.c (config-file alias expansion). Tokens are packed back-to-back
   into x_argvblk (total capacity bufsize); each argv[] entry points into the
   block. cmdl_room bounds every copy: the running offset ptr can outrun the
   block (alias / doit.log expansion outpacing the +32768 slack), so it yields
   0 rather than a wrapped size_t and the bounded copy aborts cleanly. */
#define cmdl_room(bufsize, ptr)                                                \
  ((ptr) < (size_t) (bufsize) ? (size_t) (bufsize) - (ptr) : 0)

/* Append a token as a new argv[argc]. */
#define cmdl_add(token, argc, argv, buff, bufsize, ptr)                        \
  argv[argc] = (buff + ptr);                                                   \
  strlcpybuff(argv[argc], token, cmdl_room(bufsize, ptr));                     \
  ptr += (int) (strlen(argv[argc]) + 1);                                       \
  argc++

/* Insert a token at argv[0], shifting the existing argc entries up by one. */
#define cmdl_ins(token, argc, argv, buff, bufsize, ptr)                        \
  {                                                                            \
    int i;                                                                     \
    for (i = argc; i > 0; i--)                                                 \
      argv[i] = argv[i - 1];                                                   \
  }                                                                            \
  argv[0] = (buff + ptr);                                                      \
  strlcpybuff(argv[0], token, cmdl_room(bufsize, ptr));                        \
  ptr += (int) (strlen(argv[0]) + 1);                                          \
  argc++
#endif

#endif
