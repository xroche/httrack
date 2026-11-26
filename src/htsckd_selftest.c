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
  size_t want; /* wrapped result */
  hts_boolean overflow;
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

/* Count narrowing mismatches; a refused value must leave *r untouched. */
static int ckd_narrow_check(void) {
  struct {
    int64_t v;
    hts_boolean overflow;
  } ints[] = {
      {0, HTS_FALSE},        {-1, HTS_FALSE},
      {INT_MAX, HTS_FALSE},  {(int64_t) INT_MAX + 1, HTS_TRUE},
      {INT_MIN, HTS_FALSE},  {(int64_t) INT_MIN - 1, HTS_TRUE},
      {INT64_MAX, HTS_TRUE}, {INT64_MIN, HTS_TRUE},
  };

  struct {
    int64_t v;
    hts_boolean overflow;
  } sizes[] = {
      {0, HTS_FALSE},
      {-1, HTS_TRUE},
      {INT64_MIN, HTS_TRUE},
      {INT_MAX, HTS_FALSE},
#if SIZE_MAX < INT64_MAX
      {(int64_t) SIZE_MAX, HTS_FALSE},
      {(int64_t) SIZE_MAX + 1, HTS_TRUE},
#else
      {INT64_MAX, HTS_FALSE},
#endif
  };
  struct {
    size_t v;
    int want;
  } sats[] = {
      {0, 0},
      {(size_t) INT_MAX - 1, INT_MAX - 1},
      {INT_MAX, INT_MAX},
      {(size_t) INT_MAX + 1, INT_MAX},
      {SIZE_MAX, INT_MAX},
  };

  int errors = 0;
  size_t i;

  for (i = 0; i < sizeof(ints) / sizeof(ints[0]); i++) {
    int r = 42;
    const hts_boolean ovf = hts_ckd_int(&r, ints[i].v);
    const int want = ovf ? 42 : (int) ints[i].v;

    if (ovf != ints[i].overflow || r != want) {
      printf("int case %u: got %d/%d\n", (unsigned) i, ovf, r);
      errors++;
    }
  }
  for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
    size_t r = 42;
    const hts_boolean ovf = hts_ckd_size(&r, sizes[i].v);
    const size_t want = ovf ? 42 : (size_t) sizes[i].v;

    if (ovf != sizes[i].overflow || r != want) {
      printf("size case %u: got %d/%" PRIuMAX "\n", (unsigned) i, ovf,
             (uintmax_t) r);
      errors++;
    }
  }
  for (i = 0; i < sizeof(sats) / sizeof(sats[0]); i++) {
    if (hts_sat_int(sats[i].v) != sats[i].want) {
      printf("sat case %u: got %d\n", (unsigned) i, hts_sat_int(sats[i].v));
      errors++;
    }
  }
  return errors;
}

/* -#test=ckd: checks both implementations at the size_t boundaries, and
   the narrowing helpers. */
static int st_ckd(httrackp *opt, int argc, char **argv) {
  const size_t M = SIZE_MAX, H = M / 2;
  /* S = 2^(bits/2) squares to the first wrap */
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
      /* (S-1)^2 = S^2 - 2S + 1, and S^2 = M + 1 */
      {S - 1, S - 1, M - 2 * S + 2, HTS_FALSE},
      {S + 1, S - 1, M, HTS_FALSE},
      {S - 1, S + 1, M, HTS_FALSE},
      {S, S, 0, HTS_TRUE},
      /* (S+1)^2 = S^2 + 2S + 1, and S^2 wraps to 0 */
      {S + 1, S + 1, 2 * S + 1, HTS_TRUE},
      {M / 3, 3, M, HTS_FALSE},
      {M / 3 + 1, 3, 2, HTS_TRUE},
  };
  const size_t nadd = sizeof(add) / sizeof(add[0]);
  const size_t nmul = sizeof(mul) / sizeof(mul[0]);
  const int errors =
      ckd_check("add", hts_ckd_add, add, nadd) +
      ckd_check("add_portable", hts_ckd_add_portable, add, nadd) +
      ckd_check("mul", hts_ckd_mul, mul, nmul) +
      ckd_check("mul_portable", hts_ckd_mul_portable, mul, nmul) +
      ckd_narrow_check();

  printf("%s\n", errors == 0 ? "ok" : "FAIL");
  return errors == 0 ? 0 : 1;
}

const struct selftest_entry selftests_ckd[] = {
    {"ckd", "", "checked size_t arithmetic and narrowing at their edges",
     st_ckd},
    {NULL, NULL, NULL, NULL},
};
