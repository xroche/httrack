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
/* File: htsckd.h checked size_t arithmetic                     */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSCKD_DEFH
#define HTSCKD_DEFH

#include "htsglobal.h"

#include <stddef.h>
#include <stdint.h>

/** @file htsckd.h
 *  C23 ckd_add() and ckd_mul() for size_t on every compiler: *r receives the
 *  result wrapped modulo SIZE_MAX + 1, and the return is HTS_TRUE when the
 *  exact result did not fit. Engine-internal, never installed. **/

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L &&                \
    defined(__has_include)
#if __has_include(<stdckdint.h>)
#include <stdckdint.h>
#define HTS_CKD_STDC
#endif
#endif

#if !defined(HTS_CKD_STDC)
#if defined(__has_builtin)
#if __has_builtin(__builtin_add_overflow) &&                                   \
    __has_builtin(__builtin_mul_overflow)
#define HTS_CKD_BUILTIN
#endif
#elif defined(__GNUC__) && __GNUC__ >= 5
#define HTS_CKD_BUILTIN
#endif
#endif

#if !defined(HTS_CKD_STDC) && !defined(HTS_CKD_BUILTIN) &&                     \
    defined(_MSC_VER) && !defined(__clang__)
#include <intsafe.h>
#define HTS_CKD_INTSAFE
#endif

/** Portable a + b, also the reference the self-test checks the others by. **/
static HTS_UNUSED HTS_CHECK_RESULT hts_boolean hts_ckd_add_portable(size_t *r,
                                                                    size_t a,
                                                                    size_t b) {
  *r = a + b;
  return b > SIZE_MAX - a ? HTS_TRUE : HTS_FALSE;
}

/** Portable a * b, also the reference the self-test checks the others by. **/
static HTS_UNUSED HTS_CHECK_RESULT hts_boolean hts_ckd_mul_portable(size_t *r,
                                                                    size_t a,
                                                                    size_t b) {
  *r = a * b;
  return a != 0 && b > SIZE_MAX / a ? HTS_TRUE : HTS_FALSE;
}

/** *r = a + b; HTS_TRUE if the sum overflowed size_t. **/
static HTS_UNUSED HTS_CHECK_RESULT hts_boolean hts_ckd_add(size_t *r, size_t a,
                                                           size_t b) {
#if defined(HTS_CKD_STDC)
  return ckd_add(r, a, b) ? HTS_TRUE : HTS_FALSE;
#elif defined(HTS_CKD_BUILTIN)
  return __builtin_add_overflow(a, b, r) ? HTS_TRUE : HTS_FALSE;
#elif defined(HTS_CKD_INTSAFE)
  if (SizeTAdd(a, b, r) == S_OK)
    return HTS_FALSE;
  *r = a + b; /* intsafe stores SIZE_T_ERROR, ckd_add the wrapped sum */
  return HTS_TRUE;
#else
  return hts_ckd_add_portable(r, a, b);
#endif
}

/** *r = a * b; HTS_TRUE if the product overflowed size_t. **/
static HTS_UNUSED HTS_CHECK_RESULT hts_boolean hts_ckd_mul(size_t *r, size_t a,
                                                           size_t b) {
#if defined(HTS_CKD_STDC)
  return ckd_mul(r, a, b) ? HTS_TRUE : HTS_FALSE;
#elif defined(HTS_CKD_BUILTIN)
  return __builtin_mul_overflow(a, b, r) ? HTS_TRUE : HTS_FALSE;
#elif defined(HTS_CKD_INTSAFE)
  if (SizeTMult(a, b, r) == S_OK)
    return HTS_FALSE;
  *r = a * b;
  return HTS_TRUE;
#else
  return hts_ckd_mul_portable(r, a, b);
#endif
}

#endif
