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

Please visit our Website: http://www.httrack.com
*/

/* Drives the String allocation-failure path through a realloc stub: asking a
   real allocator for a size it should refuse is a guess about the machine, not
   a test (#915). One case per run, named by argv[1]. */

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int alloc_fails = 0;
static int oom_jumps = 0;
static int oom_calls = 0;
static size_t oom_size = 0;
static size_t last_request = 0;
static jmp_buf oom_jump;

static char *test_realloc(char *buff, size_t size) {
  last_request = size;
  if (alloc_fails) {
    return NULL;
  }
  return (char *) realloc(buff, size);
}

/* Declared before the include: StringBuffN_ and StringSprintf_ expand
   STRING_OOM inside the header itself. */
static void test_oom(size_t size);

#define STRING_REALLOC(BUFF, SIZE) test_realloc(BUFF, SIZE)
#define STRING_FREE(BUFF) free(BUFF)
#define STRING_OOM(SIZE) test_oom(SIZE)

#include "htsstrings.h"

/* Either returns to the caller, leaving the String observable, or runs the
   shipped handler. */
static void test_oom(size_t size) {
  oom_calls++;
  oom_size = size;
  if (oom_jumps) {
    longjmp(oom_jump, 1);
  }
  StringOom_(size);
}

/* File scope, so longjmp cannot leave them indeterminate. */
static String room = STRING_EMPTY;
static const char *kept = NULL;
static size_t kept_capacity = 0;

/* The handler must run once, and be told the size that was actually asked of
   the allocator: an under-allocation reports a size the String does not have.
   Compared against the stub's own record rather than a literal, so the initial
   capacity stays a policy the test does not pin. */
static int failure_reported(const char *name) {
  if (oom_calls != 1) {
    printf("%s: FAIL (handler ran %d times)\n", name, oom_calls);
    return 0;
  }
  if (oom_size != last_request) {
    printf("%s: FAIL (reported %u bytes, allocator was asked for %u)\n", name,
           (unsigned) oom_size, (unsigned) last_request);
    return 0;
  }
  return 1;
}

/* Control: with the stub allocating for real, nothing must reach the handler.
   Without this, a stub stuck in failing mode would "prove" every case. */
static int grow_case(void) {
  size_t cap, i;

  StringRoomTotal(room, 100);
  if (oom_calls != 0) {
    printf("grow: FAIL (handler ran %d times)\n", oom_calls);
    return 1;
  }
  cap = StringCapacity(room);
  if (StringBuff(room) == NULL || cap < 100) {
    printf("grow: FAIL (capacity %u)\n", (unsigned) cap);
    return 1;
  }
  /* A capacity never written to is a number, not a buffer: fill it to the
     last byte, so an allocation short of the announced capacity is a heap
     overflow the sanitizer legs catch. */
  for (i = 0; i + 1 < cap; i++) {
    StringBuffRW(room)[i] = (char) ('a' + (i % 26));
  }
  StringBuffRW(room)[cap - 1] = '\0';
  for (i = 0; i + 1 < cap; i++) {
    if (StringBuff(room)[i] != (char) ('a' + (i % 26))) {
      printf("grow: FAIL (byte %u of %u read back as 0x%02x)\n", (unsigned) i,
             (unsigned) cap, (unsigned char) StringBuff(room)[i]);
      return 1;
    }
  }
  if (strlen(StringBuff(room)) != cap - 1) {
    printf("grow: FAIL (%u bytes readable, capacity %u)\n",
           (unsigned) strlen(StringBuff(room)), (unsigned) cap);
    return 1;
  }
  StringFree(room);
  printf("grow: OK\n");
  return 0;
}

static int hook_case(void) {
  alloc_fails = oom_jumps = 1;
  if (setjmp(oom_jump) == 0) {
    StringRoomTotal(room, 100);
    printf("hook: FAIL (grew through a failing allocator)\n");
    return 1;
  }
  if (!failure_reported("hook")) {
    return 1;
  }
  if (StringBuff(room) != NULL || StringCapacity(room) != 0 ||
      StringLength(room) != 0) {
    printf("hook: FAIL (capacity %u, buffer %s)\n",
           (unsigned) StringCapacity(room),
           StringBuff(room) == NULL ? "null" : "set");
    return 1;
  }
  printf("hook: OK\n");
  return 0;
}

/* The one the old code got wrong: a failed realloc must not overwrite the live
   buffer with NULL nor bump the capacity past what was allocated. */
static int keep_case(void) {
  StringCopy(room, "abc");
  kept = StringBuff(room);
  kept_capacity = StringCapacity(room);
  alloc_fails = oom_jumps = 1;
  if (setjmp(oom_jump) == 0) {
    StringRoomTotal(room, 1000);
    printf("keep: FAIL (grew through a failing allocator)\n");
    return 1;
  }
  if (!failure_reported("keep")) {
    return 1;
  }
  if (StringBuff(room) != kept || StringCapacity(room) != kept_capacity ||
      StringLength(room) != 3 || strcmp(StringBuff(room), "abc") != 0) {
    printf("keep: FAIL (buffer %s, capacity %u was %u)\n",
           StringBuff(room) == kept ? "kept" : "moved",
           (unsigned) StringCapacity(room), (unsigned) kept_capacity);
    return 1;
  }
  alloc_fails = 0;
  StringFree(room);
  printf("keep: OK\n");
  return 0;
}

/* The header's own growers expand STRING_OOM as well, so drive each of them
   into the same failure rather than only the macro they call. */
static int sprintf_case(void) {
  alloc_fails = oom_jumps = 1;
  if (setjmp(oom_jump) == 0) {
    StringSprintf(room, "%s", "x");
    printf("sprintf: FAIL (formatted through a failing allocator)\n");
    return 1;
  }
  if (!failure_reported("sprintf") || StringBuff(room) != NULL) {
    return 1;
  }
  printf("sprintf: OK\n");
  return 0;
}

static int buffn_case(void) {
  alloc_fails = oom_jumps = 1;
  if (setjmp(oom_jump) == 0) {
    (void) StringBuffN(room, 10);
    printf("buffn: FAIL (reserved through a failing allocator)\n");
    return 1;
  }
  if (!failure_reported("buffn") || StringBuff(room) != NULL) {
    return 1;
  }
  printf("buffn: OK\n");
  return 0;
}

/* Runs the shipped handler, which must print and abort. */
static int abort_case(void) {
  alloc_fails = 1;
  StringRoomTotal(room, 100);
  printf("abort: NOT aborted\n");
  return 1;
}

int main(int argc, char **argv) {
  const char *const mode = argc > 1 ? argv[1] : "";

  if (strcmp(mode, "grow") == 0) {
    return grow_case();
  } else if (strcmp(mode, "hook") == 0) {
    return hook_case();
  } else if (strcmp(mode, "keep") == 0) {
    return keep_case();
  } else if (strcmp(mode, "sprintf") == 0) {
    return sprintf_case();
  } else if (strcmp(mode, "buffn") == 0) {
    return buffn_case();
  } else if (strcmp(mode, "abort") == 0) {
    return abort_case();
  }
  fprintf(stderr, "usage: %s grow|hook|keep|sprintf|buffn|abort\n", argv[0]);
  return 2;
}
