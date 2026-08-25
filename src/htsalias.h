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
#include "htsarena.h"

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE
/* Longest digit run -N reads as a preset number; a longer one is out of range
   on every int width and falls back to the default layout. */
#define HTS_SAVENAME_PRESET_MAX_DIGITS 9

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
const char *hts_gethome(void);
void expand_home(String * str);

/* Command line rebuilt from argv, config files and doit.log. The slots and the
   token arena each grow on demand; the arena is what keeps every argv[] slot,
   and every pointer a caller kept into one, valid. */
typedef struct {
  char **argv; /* argc slots used out of capacity allocated */
  /* per slot: the token arrived already unquoted, so the parser must not strip
     a quote pair off it again (doit.log tokens, unquoted by next_token) */
  hts_boolean *unquoted;
  int argc;
  int capacity;
  hts_arena tokens;
} cmdl_argv;

/* Prepare an empty command line with room for slots entries. HTS_FALSE if the
   slots cannot be allocated. */
hts_boolean cmdl_init(cmdl_argv *cmd, int slots);

/* Release cmd; the tokens its argv pointed at die with it. */
void cmdl_free(cmdl_argv *cmd);

/* Append token as the last entry. HTS_FALSE if it does not fit and neither the
   slots nor the arena can be grown. */
hts_boolean cmdl_add(cmdl_argv *cmd, const char *token);

/* Insert token at 0 <= pos <= argc, shifting the entries above it up by one.
   HTS_FALSE if it does not fit and neither the slots nor the arena can be
   grown. */
hts_boolean cmdl_ins(cmdl_argv *cmd, const char *token, int pos);

/* cmdl_ins for a token that has already been unquoted by its reader, marking
   it so the parser leaves its quotes alone. */
hts_boolean cmdl_ins_unquoted(cmdl_argv *cmd, const char *token, int pos);

typedef enum {
  CMDL_FILE_MISSING, /* not found, or unreadable */
  CMDL_FILE_READ,    /* expanded into the command line */
  CMDL_FILE_NOMEM    /* out of memory, leaving the command line half expanded */
} cmdl_file_result;

/* Expand a config file into cmd, inserting after its program name. */
cmdl_file_result optinclude_file(const char *name, cmdl_argv *cmd);
#endif

#endif
