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
/* On-demand crash, for crash-handler testing. See htscrashtest.h.
   Several kinds, because a handler coping with one fault is not proven to
   cope with the others. */
/* ------------------------------------------------------------ */

#define HTS_INTERNAL_BYTECODE

#include "htscrashtest.h"

#include "htssafe.h"
#include "htsthread.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#define CRASH_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define CRASH_NOINLINE __attribute__((noinline))
#else
#define CRASH_NOINLINE
#endif

/* Read through volatiles so the optimizer cannot see the UB and delete it. */
static volatile uintptr_t crash_address = 0x42;
static volatile int crash_keep_recursing = 1;

static CRASH_NOINLINE void fourty_two(void) {
  volatile char *const ptr = (volatile char *) crash_address;

  (*ptr)++;
}

/* Nested, so the backtrace under test has more than one frame to name. */
static CRASH_NOINLINE void crash_segv(void) { fourty_two(); }

/* The engine's own fatal path (htssafe assertions land here). */
static CRASH_NOINLINE void crash_abort(void) {
  abortLog("deliberate crash test");
}

static CRASH_NOINLINE void crash_trap(void) {
#if defined(_MSC_VER)
  __debugbreak();
#elif defined(__GNUC__)
  __builtin_trap();
#else
  abort();
#endif
}

/* Reading the frame *after* the call keeps it live across it: returning
   frame[0] + f(...) instead lets gcc accumulate and loop, never overflowing. */
static CRASH_NOINLINE char blow_the_stack(size_t depth) {
  volatile char frame[4096];
  char deeper;

  frame[0] = (char) depth;
  if (!crash_keep_recursing) {
    return frame[0];
  }
  deeper = blow_the_stack(depth + 1);
  return (char) (frame[0] + deeper);
}

/* Faults with no stack left for the handler, unless it runs on an altstack. */
static CRASH_NOINLINE void crash_stack(void) { (void) blow_the_stack(0); }

static void crash_stack_thread(void *arg) {
  (void) arg;
  fprintf(stderr, "** Crash test worker thread started\n");
  fflush(stderr);
  crash_stack();
}

/* Same runaway recursion in an engine worker: the fatal handler needs an
   alternate stack in every thread, not just the main one (#969). */
static CRASH_NOINLINE void crash_threadstack(void) {
  if (hts_newthread(crash_stack_thread, NULL) == 0)
    htsthread_wait_n(0); /* the worker takes the process down from there */
}

static const struct {
  const char *name;
  void (*fn)(void);
} crash_kinds[] = {
    {"segv", crash_segv},
    {"abort", crash_abort},
    {"trap", crash_trap},
    {"stack", crash_stack},
    {"threadstack", crash_threadstack},
};

#define CRASH_KINDS_COUNT (sizeof(crash_kinds) / sizeof(crash_kinds[0]))

const char *hts_crash_test_kinds(void) {
  static char list[64];

  if (list[0] == '\0') {
    size_t i;

    for (i = 0; i < CRASH_KINDS_COUNT; i++) {
      if (i != 0) {
        strcatbuff(list, ", ");
      }
      strcatbuff(list, crash_kinds[i].name);
    }
  }
  return list;
}

hts_boolean hts_crash_test(const char *kind) {
  size_t i;

  if (kind == NULL || *kind == '\0') {
    kind = crash_kinds[0].name;
  }
  for (i = 0; i < CRASH_KINDS_COUNT; i++) {
    if (strcmp(kind, crash_kinds[i].name) == 0) {
      fprintf(stderr,
              "** Deliberate '%s' crash requested (-#c): crash handler test\n",
              kind);
      fflush(stderr);
      crash_kinds[i].fn();
      /* Reached only if a handler swallowed the fault and resumed. */
      fprintf(stderr, "** Crash test '%s' did not crash the process\n", kind);
      fflush(stderr);
      return HTS_TRUE;
    }
  }
  return HTS_FALSE;
}
