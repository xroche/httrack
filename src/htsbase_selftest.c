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
/* File: htsbase_selftest.c subroutines:                        */
/*       self-tests for the typed min/max helpers               */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

#include "htsbase.h"

#include <limits.h>
#include <inttypes.h>
#include <stdint.h>

static int minmax_calls;

/* Returns v and counts the call, to prove each argument is evaluated once. */
static int64_t counted(int64_t v) {
  minmax_calls++;
  return v;
}

/* Prints and counts a mismatch at the self-test's line. */
static int minmax_fail(int line, int64_t got, int64_t want) {
  if (got == want)
    return 0;
  printf("FAIL line %d: got %" PRId64 ", want %" PRId64 "\n", line, got, want);
  return 1;
}

/* -#test=minmax: each helper at its type's bounds, each argument once. */
static int st_minmax(httrackp *opt, int argc, char **argv) {
  int errors = 0;

  errors += minmax_fail(__LINE__, hts_min_int(INT_MIN, INT_MAX), INT_MIN);
  errors += minmax_fail(__LINE__, hts_min_int(INT_MAX, INT_MIN), INT_MIN);
  errors += minmax_fail(__LINE__, hts_max_int(INT_MIN, INT_MAX), INT_MAX);
  errors += minmax_fail(__LINE__, hts_max_int(INT_MAX, INT_MIN), INT_MAX);
  errors += minmax_fail(__LINE__, hts_min_int(-1, 0), -1);
  errors += minmax_fail(__LINE__, hts_max_int(-1, 0), 0);
  errors += minmax_fail(__LINE__, hts_max_int(7, 7), 7);
  /* a size_t compare must not see SIZE_MAX as -1 */
  if (hts_min_size(SIZE_MAX, 1) != 1 || hts_min_size(1, SIZE_MAX) != 1 ||
      hts_max_size(SIZE_MAX, 0) != SIZE_MAX ||
      hts_max_size(0, SIZE_MAX) != SIZE_MAX || hts_min_size(3, 3) != 3) {
    printf("FAIL size_t bounds\n");
    errors++;
  }
  /* an int64_t compare must not truncate to int */
  errors += minmax_fail(__LINE__, hts_min_i64(INT64_MIN, INT64_MAX), INT64_MIN);
  errors += minmax_fail(__LINE__, hts_min_i64(INT64_MAX, INT64_MIN), INT64_MIN);
  errors += minmax_fail(__LINE__, hts_max_i64(INT64_MIN, INT64_MAX), INT64_MAX);
  errors += minmax_fail(__LINE__, hts_max_i64(INT64_MAX, INT64_MIN), INT64_MAX);
  errors += minmax_fail(__LINE__, hts_max_i64((int64_t) 1 << 40, 1),
                        (int64_t) 1 << 40);
  errors += minmax_fail(__LINE__, hts_min_i64(-((int64_t) 1 << 40), -1),
                        -((int64_t) 1 << 40));

  minmax_calls = 0;
  errors += minmax_fail(__LINE__, hts_max_i64(counted(1), counted(5)), 5);
  errors += minmax_fail(__LINE__, hts_min_i64(counted(1), counted(5)), 1);
  errors +=
      minmax_fail(__LINE__, hts_max_int((int) counted(9), (int) counted(2)), 9);
  errors += minmax_fail(
      __LINE__, hts_min_size((size_t) counted(9), (size_t) counted(2)), 2);
  if (minmax_calls != 8) {
    printf("FAIL %d argument evaluations, want 8\n", minmax_calls);
    errors++;
  }

  printf("%s\n", errors == 0 ? "ok" : "FAIL");
  return errors == 0 ? 0 : 1;
}

const struct selftest_entry selftests_base[] = {
    {"minmax", "", "typed min/max at their bounds, each argument once",
     st_minmax},
    {NULL, NULL, NULL, NULL},
};
