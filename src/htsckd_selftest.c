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
/* File: htsckd_selftest.c subroutines:                         */
/*       self-tests for the checked size_t arithmetic           */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

#include "htsckd.h"

#include <limits.h>
#include <stdint.h>

typedef hts_boolean (*ckd_fn)(size_t *r, size_t a, size_t b);

struct ckd_case {
  size_t a, b;
  size_t want;          /* the result, wrapped as C23 ckd_* wraps it */
  hts_boolean overflow; /* expected return */
};

/* Count the cases fn gets wrong, printing each. */
static int ckd_check(const char *name, ckd_fn fn, const struct ckd_case *cases,
                     size_t n) {
  int errors = 0;
  size_t i;

  for (i = 0; i < n; i++) {
    size_t r = 0;
    const hts_boolean ovf = fn(&r, cases[i].a, cases[i].b);

    if (ovf != cases[i].overflow || r != cases[i].want) {
      printf("%s case %u: got %d/%" PRIuMAX ", want %d/%" PRIuMAX "\n", name,
             (unsigned) i, ovf, (uintmax_t) r, cases[i].overflow,
             (uintmax_t) cases[i].want);
      errors++;
    }
  }
  return errors;
}

static hts_boolean ckd_add_fast(size_t *r, size_t a, size_t b) {
  return hts_ckd_add(r, a, b);
}

static hts_boolean ckd_mul_fast(size_t *r, size_t a, size_t b) {
  return hts_ckd_mul(r, a, b);
}

static hts_boolean ckd_add_ref(size_t *r, size_t a, size_t b) {
  return hts_ckd_add_portable(r, a, b);
}

static hts_boolean ckd_mul_ref(size_t *r, size_t a, size_t b) {
  return hts_ckd_mul_portable(r, a, b);
}

/* -#test=ckd: both the compiler's and the portable checks at the size_t
   boundaries, whatever the width of size_t. */
static int st_ckd(httrackp *opt, int argc, char **argv) {
  const size_t M = SIZE_MAX, H = M / 2;
  /* 2^(bits/2): its square is the first to wrap */
  const size_t S = (size_t) 1 << (sizeof(size_t) * CHAR_BIT / 2);
  const struct ckd_case add[] = {
      {0, 0, 0, HTS_FALSE},     {M, 0, M, HTS_FALSE},
      {0, M, M, HTS_FALSE},     {M - 1, 1, M, HTS_FALSE},
      {1, M - 1, M, HTS_FALSE}, {M, 1, 0, HTS_TRUE},
      {1, M, 0, HTS_TRUE},      {M, M, M - 1, HTS_TRUE},
      {H + 1, H, M, HTS_FALSE}, {H + 1, H + 1, 0, HTS_TRUE},
  };
  const struct ckd_case mul[] = {
      {0, 0, 0, HTS_FALSE},
      {0, M, 0, HTS_FALSE},
      {M, 0, 0, HTS_FALSE},
      {1, M, M, HTS_FALSE},
      {M, 1, M, HTS_FALSE},
      {2, M, M - 1, HTS_TRUE},
      {M, M, 1, HTS_TRUE},
      {2, H, M - 1, HTS_FALSE},
      {2, H + 1, 0, HTS_TRUE},
      {S - 1, S - 1, M - 2 * S + 2, HTS_FALSE},
      {S + 1, S - 1, M, HTS_FALSE}, /* the largest product: exactly SIZE_MAX */
      {S - 1, S + 1, M, HTS_FALSE},
      {S, S, 0, HTS_TRUE},
      {S + 1, S + 1, 2 * S + 1, HTS_TRUE},
      {M / 3, 3, M, HTS_FALSE},
      {M / 3 + 1, 3, 2, HTS_TRUE},
  };
  const size_t nadd = sizeof(add) / sizeof(add[0]);
  const size_t nmul = sizeof(mul) / sizeof(mul[0]);
  const int errors = ckd_check("add", ckd_add_fast, add, nadd) +
                     ckd_check("add_portable", ckd_add_ref, add, nadd) +
                     ckd_check("mul", ckd_mul_fast, mul, nmul) +
                     ckd_check("mul_portable", ckd_mul_ref, mul, nmul);

  printf("%s\n", errors == 0 ? "ok" : "FAIL");
  return errors == 0 ? 0 : 1;
}

const struct selftest_entry selftests_ckd[] = {
    {"ckd", "", "checked size_t add and multiply at their boundaries", st_ckd},
    {NULL, NULL, NULL, NULL},
};
