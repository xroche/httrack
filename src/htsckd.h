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

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: htsckd.h checked size_t arithmetic and narrowing      */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSCKD_DEFH
#define HTSCKD_DEFH

#include "htsglobal.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

/** @file htsckd.h
 *  C23 ckd_add() and ckd_mul() for size_t, plus checked narrowing,
 *  engine-internal and never installed. **/

#if defined(__has_builtin)
#if __has_builtin(__builtin_add_overflow) &&                                   \
    __has_builtin(__builtin_mul_overflow)
#define HTS_CKD_BUILTIN
#endif
#elif defined(__GNUC__) && __GNUC__ >= 5
#define HTS_CKD_BUILTIN
#endif

/* Portable fallbacks, also the self-test's reference. */
static HTS_UNUSED HTS_CHECK_RESULT hts_boolean hts_ckd_add_portable(size_t *r,
                                                                    size_t a,
                                                                    size_t b) {
  *r = a + b;
  return b > SIZE_MAX - a ? HTS_TRUE : HTS_FALSE;
}

static HTS_UNUSED HTS_CHECK_RESULT hts_boolean hts_ckd_mul_portable(size_t *r,
                                                                    size_t a,
                                                                    size_t b) {
  *r = a * b;
  return a != 0 && b > SIZE_MAX / a ? HTS_TRUE : HTS_FALSE;
}

/** Sets *r = a + b, wrapped, and returns HTS_TRUE if it overflowed. **/
static HTS_UNUSED HTS_CHECK_RESULT hts_boolean hts_ckd_add(size_t *r, size_t a,
                                                           size_t b) {
#if defined(HTS_CKD_BUILTIN)
  return __builtin_add_overflow(a, b, r) ? HTS_TRUE : HTS_FALSE;
#else
  return hts_ckd_add_portable(r, a, b);
#endif
}

/** Sets *r = a * b, wrapped, and returns HTS_TRUE if it overflowed. **/
static HTS_UNUSED HTS_CHECK_RESULT hts_boolean hts_ckd_mul(size_t *r, size_t a,
                                                           size_t b) {
#if defined(HTS_CKD_BUILTIN)
  return __builtin_mul_overflow(a, b, r) ? HTS_TRUE : HTS_FALSE;
#else
  return hts_ckd_mul_portable(r, a, b);
#endif
}

/** Sets *r = v and returns HTS_FALSE if v fits an int; otherwise returns
 *  HTS_TRUE and leaves *r alone. **/
static HTS_UNUSED HTS_CHECK_RESULT hts_boolean hts_ckd_int(int *r, int64_t v) {
  if (v < INT_MIN || v > INT_MAX)
    return HTS_TRUE;
  *r = (int) v;
  return HTS_FALSE;
}

/** Sets *r = v and returns HTS_FALSE if v fits a size_t; otherwise
 *  (negative, or too big on a 32-bit target) returns HTS_TRUE and leaves *r
 *  alone. **/
static HTS_UNUSED HTS_CHECK_RESULT hts_boolean hts_ckd_size(size_t *r,
                                                            int64_t v) {
  if (v < 0 || (uint64_t) v > SIZE_MAX)
    return HTS_TRUE;
  *r = (size_t) v;
  return HTS_FALSE;
}

/** Returns v capped at INT_MAX, for an I/O length an int API takes. **/
static HTS_UNUSED int hts_sat_int(size_t v) {
  return v < INT_MAX ? (int) v : INT_MAX;
}

#endif
