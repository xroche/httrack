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

#include "htsglobal.h"

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
const char *optreal_value(int p);
const char *optalias_value(int p);
const char *opttype_value(int p);
const char *opthelp_value(int p);
const char *hts_gethome(void);
void expand_home(String * str);

/* The command line being rebuilt by htscoremain.c (CLI parsing) and
   htsalias.c (config-file alias expansion): argc slots, each pointing into
   blk, where the tokens are packed back-to-back. Config files and doit.log
   append an input-defined number of tokens, so the slot array is grown on
   demand and the invariant argc <= size is re-established before every write.
   blk does not grow: a token that no longer fits aborts in the bounded copy
   rather than being written past the end. */
typedef struct {
  char **argv; /* argc slots used out of size allocated */
  int argc;
  int size;
  char *blk; /* token bytes, blk_used of blk_size in use */
  size_t blk_size;
  size_t blk_used;
} cmdl_argv;

/* Grow the slot array to hold at least count entries. HTS_FALSE if that many
   slots can not be allocated, leaving cmd usable and unchanged. */
hts_boolean cmdl_reserve(cmdl_argv *cmd, int count);

/* Append token as the last entry. HTS_FALSE if the array can not be grown. */
hts_boolean cmdl_add(cmdl_argv *cmd, const char *token);

/* Insert token at 0 <= pos <= argc, shifting the entries above it up by one.
   HTS_FALSE if the array can not be grown. */
hts_boolean cmdl_ins(cmdl_argv *cmd, const char *token, int pos);

/* Expand a config file into cmd, inserting after its program name. Returns
   whether the file could be read. */
hts_boolean optinclude_file(const char *name, cmdl_argv *cmd);
#endif

#endif
