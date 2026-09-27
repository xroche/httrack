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
/* File: httrack.c subroutines:                                 */
/*       wrapper system (for shell                              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/** @file htswrap.h
    Engine-only copy of the htswrap_init() and htswrap_free() declarations.
    Everything below sits behind HTS_INTERNAL_BYTECODE, so a consumer reads them
    from httrack-library.h instead, next to htswrap_add() and htswrap_read(). */

#ifndef HTSWRAP_DEFH
#define HTSWRAP_DEFH

/* Engine-internal declarations */
#ifdef HTS_INTERNAL_BYTECODE

#include "htsglobal.h"

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Does nothing and returns 1, kept for old clients. */
HTSEXT_API int htswrap_init(void); // LEGACY

/** Does nothing and returns 1, kept for old clients. */
HTSEXT_API int htswrap_free(void); // LEGACY

#ifdef __cplusplus
}
#endif

// HTSEXT_API int htswrap_add(httrackp * opt, const char *name, void *fct);
// HTSEXT_API uintptr_t htswrap_read(httrackp * opt, const char *name);

#endif

#endif
