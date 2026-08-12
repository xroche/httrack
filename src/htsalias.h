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

/* One token-block allocation, its bytes following the link. */
typedef struct cmdl_chunk cmdl_chunk;

/* Command line rebuilt from argv, config files and doit.log: argc slots
   pointing into the token chunks, where the tokens are packed back-to-back.
   Both grow on demand, since a config file and a doit.log each add a token
   count only the input knows. A chunk is never resized, so every argv[] slot,
   and every pointer a caller kept into one, survives the growth. */
typedef struct {
  char **argv; /* argc slots used out of capacity allocated */
  int argc;
  int capacity;
  cmdl_chunk *chunks; /* newest first; tokens are cut from the newest */
  size_t blk_size;    /* the newest chunk, blk_used of blk_size in use */
  size_t blk_used;
} cmdl_argv;

/* Allocate a first token chunk of at least blk_size bytes and room for slots
   entries. HTS_FALSE if either fails, leaving cmd empty. */
hts_boolean cmdl_init(cmdl_argv *cmd, size_t blk_size, int slots);

/* Release cmd; the tokens its argv pointed at die with it. */
void cmdl_free(cmdl_argv *cmd);

/* Append token as the last entry. HTS_FALSE if it does not fit and neither the
   slots nor the chunks can be grown. */
hts_boolean cmdl_add(cmdl_argv *cmd, const char *token);

/* Insert token at 0 <= pos <= argc, shifting the entries above it up by one.
   HTS_FALSE if it does not fit and neither the slots nor the chunks can be
   grown. */
hts_boolean cmdl_ins(cmdl_argv *cmd, const char *token, int pos);

typedef enum {
  CMDL_FILE_MISSING, /* not found, or unreadable */
  CMDL_FILE_READ,    /* expanded into the command line */
  CMDL_FILE_NOMEM    /* out of memory, leaving the command line half expanded */
} cmdl_file_result;

/* Expand a config file into cmd, inserting after its program name. */
cmdl_file_result optinclude_file(const char *name, cmdl_argv *cmd);
#endif

#endif
