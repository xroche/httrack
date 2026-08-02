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

/* Drives StringRoomTotal's allocation-failure path through a realloc stub:
   asking a real allocator for a size it should refuse is a guess about the
   machine, not a test (#915). One case per run, named by argv[1]. */

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int alloc_fails = 0;
static int oom_jumps = 0;
static int oom_calls = 0;
static size_t oom_size = 0;
static jmp_buf oom_jump;

static char *test_realloc(char *buff, size_t size) {
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

/* Control: with the stub allocating for real, nothing must reach the handler.
   Without this, a stub stuck in failing mode would "prove" every case. */
static int grow_case(void) {
  StringRoomTotal(room, 100);
  if (oom_calls != 0) {
    printf("grow: FAIL (handler ran %d times)\n", oom_calls);
    return 1;
  }
  if (StringBuff(room) == NULL || StringCapacity(room) < 100) {
    printf("grow: FAIL (capacity %u)\n", (unsigned) StringCapacity(room));
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
  if (StringBuff(room) != NULL || StringCapacity(room) != 0 ||
      StringLength(room) != 0) {
    printf("hook: FAIL (capacity %u, buffer %s)\n",
           (unsigned) StringCapacity(room),
           StringBuff(room) == NULL ? "null" : "set");
    return 1;
  }
  if (oom_size != 16) { /* the first doubling, and the size we report */
    printf("hook: FAIL (reported %u bytes, expected 16)\n",
           (unsigned) oom_size);
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
  alloc_fails = oom_jumps = 1;
  if (setjmp(oom_jump) == 0) {
    StringRoomTotal(room, 1000);
    printf("keep: FAIL (grew through a failing allocator)\n");
    return 1;
  }
  if (StringBuff(room) != kept || StringCapacity(room) != 16 ||
      StringLength(room) != 3 || strcmp(StringBuff(room), "abc") != 0) {
    printf("keep: FAIL (buffer %s, capacity %u)\n",
           StringBuff(room) == kept ? "kept" : "moved",
           (unsigned) StringCapacity(room));
    return 1;
  }
  alloc_fails = 0;
  StringFree(room);
  printf("keep: OK\n");
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
  } else if (strcmp(mode, "abort") == 0) {
    return abort_case();
  }
  fprintf(stderr, "usage: %s grow|hook|keep|abort\n", argv[0]);
  return 2;
}
