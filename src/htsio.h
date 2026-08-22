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
/* File: htsio.h all-or-nothing stdio helpers                   */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSIO_DEFH
#define HTSIO_DEFH

#include <stdio.h>

#include "htsglobal.h"

/** Read exactly size bytes into dest. HTS_TRUE only when the whole block was
    read; a short read, EOF or error alike, is a failure, with errno left as
    fread() set it. A zero size succeeds without touching dest. Wrong for a
    read-to-EOF loop, which must keep the count fread() returns. **/
static HTS_INLINE HTS_UNUSED hts_boolean hts_fread_exact(void *dest,
                                                         size_t size,
                                                         FILE *fp) {
  return fread(dest, 1, size, fp) == size ? HTS_TRUE : HTS_FALSE;
}

/** Write exactly size bytes from src. HTS_TRUE only when the whole block
    reached fp, with errno left as fwrite() set it; a zero size succeeds.
    Best-effort writers ignore the verdict, so it is not HTS_CHECK_RESULT. **/
static HTS_INLINE HTS_UNUSED hts_boolean hts_fwrite_exact(const void *src,
                                                          size_t size,
                                                          FILE *fp) {
  return fwrite(src, 1, size, fp) == size ? HTS_TRUE : HTS_FALSE;
}

#endif
