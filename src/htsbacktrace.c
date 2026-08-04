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
/* File: crash backtrace printer                                 */
/* Author: Xavier Roche                                          */
/* ------------------------------------------------------------ */

/* Before every header: glibc gates dladdr() on it. */
#if defined(__linux) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "htsbacktrace.h"

#include "htsglobal.h"

#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h> /* write */
#else
#include <signal.h>
#include <sys/mman.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif
#if defined(SA_ONSTACK) && defined(MAP_ANONYMOUS)
#define USES_SIGALTSTACK
#define BT_ALTSTACK_MIN (64 * 1024) /* floor: the report needs ~10kB */
#endif
#if (defined(__linux) && defined(HAVE_EXECINFO_H))
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#define USES_BACKTRACE
#endif

#ifdef USES_BACKTRACE
#define BT_MAX_FRAMES 64     /* frames we try to name */
#define BT_MAX_MODULES 8     /* distinct modules, one child each */
#define BT_HEX_SIZE 19       /* "0x" + 16 nibbles + NUL */
#define BT_PATH_SIZE 1024    /* module path; longer is skipped */
#define BT_WAIT_TICKS 300    /* 10ms ticks, shared: cap a slow child */
#define BT_NO_SYMBOLIZER 127 /* child exit: execv() failed */

static hts_boolean symbolize_crash = HTS_TRUE;

/* Resolved before any crash: a PATH search allocates, and the child of a
   vfork() must not. NULL opts means no symbolizer, skip the whole step. */
static char symbolizer_path[BT_PATH_SIZE];
static char *symbolizer_opts = NULL;

/* "0x"-prefixed hex: the handler must stay stdio-free. */
static void print_hex(char *buffer, uintptr_t value) {
  static const char digits[] = "0123456789abcdef";
  size_t i = 2, a, b;

  buffer[0] = '0';
  buffer[1] = 'x';
  do {
    buffer[i++] = digits[value & 0xf];
    value >>= 4;
  } while (value != 0);
  buffer[i] = '\0';
  for (a = 2, b = i - 1; a < b; a++, b--) {
    const char c = buffer[a];

    buffer[a] = buffer[b];
    buffer[b] = c;
  }
}

/* HTS_FALSE if src does not fit: a truncated module path would point the
   symbolizer at the wrong file. */
static hts_boolean copy_bounded(char *dest, size_t size, const char *src) {
  size_t i;

  for (i = 0; i < size - 1 && src[i] != '\0'; i++) {
    dest[i] = src[i];
  }
  dest[i] = '\0';
  return src[i] == '\0' ? HTS_TRUE : HTS_FALSE;
}

/* Store the first 'prog' found on PATH in symbolizer_path. Startup only: not
   signal-safe, and pointless there since the answer cannot change. */
static hts_boolean find_on_path(const char *prog) {
  const size_t proglen = strlen(prog);
  const char *dir = getenv("PATH");

  while (dir != NULL && *dir != '\0') {
    const char *const sep = strchr(dir, ':');
    const size_t len = sep != NULL ? (size_t) (sep - dir) : strlen(dir);

    /* An empty element means the current directory: never look there. */
    if (len != 0 && len <= sizeof(symbolizer_path) - 2 &&
        proglen < sizeof(symbolizer_path) - len - 1) {
      memcpy(symbolizer_path, dir, len);
      symbolizer_path[len] = '/';
      memcpy(&symbolizer_path[len + 1], prog, proglen + 1);
      if (access(symbolizer_path, X_OK) == 0)
        return HTS_TRUE;
    }
    dir = sep != NULL ? sep + 1 : NULL;
  }
  symbolizer_path[0] = '\0';
  return HTS_FALSE;
}

static void resolve_symbolizer(void) {
  static char addr2line_opts[] = "-Cfipa";
  static char llvm_opts[] = "-p";

  if (find_on_path("addr2line"))
    symbolizer_opts = addr2line_opts;
  /* An LLVM-only install ships neither addr2line nor its option spelling. */
  else if (find_on_path("llvm-symbolizer"))
    symbolizer_opts = llvm_opts;
}

/* Run the symbolizer on argv, output on fd, within *budget ticks. HTS_FALSE
   only if it could not be run at all; otherwise silent, the raw trace stands.
   vfork(), not fork(): fork() runs the pthread_atfork prepare handlers, and
   glibc's malloc registers one taking every arena lock, so a signal raised
   inside malloc deadlocks the handler and loses the report (#968). */
static hts_boolean spawn_symbolizer(char **argv, int fd, int *budget) {
  int status = 0;
  pid_t pid;

  pid = vfork();
  if (pid == -1)
    return HTS_FALSE;
  if (pid == 0) {
    /* Syscalls only: the address space here is still the parent's. */
    dup2(fd, 1); /* both symbolizers write on stdout */
    execv(symbolizer_path, argv);
    _exit(BT_NO_SYMBOLIZER);
  }
  for (; *budget > 0; (*budget)--) {
    const struct timespec tick = {0, 10 * 1000 * 1000};
    const pid_t reaped = waitpid(pid, &status, WNOHANG);

    if (reaped == pid)
      return WIFEXITED(status) && WEXITSTATUS(status) == BT_NO_SYMBOLIZER
                 ? HTS_FALSE
                 : HTS_TRUE;
    if (reaped == -1 && errno != EINTR)
      return HTS_TRUE;
    nanosleep(&tick, NULL);
  }
  kill(pid, SIGKILL);
  waitpid(pid, NULL, 0);
  return HTS_TRUE;
}

/* Name the frames backtrace_symbols_fd() leaves as module+offset:
   -fvisibility=hidden keeps them out of .dynsym, but DWARF has them. dladdr()
   is not formally async-signal-safe; accepted, this path is already fatal. */
static void symbolize_backtrace(void *const *stack, int size, int fd) {
  static char dashe[] = "-e";
  char hex[BT_MAX_FRAMES][BT_HEX_SIZE];
  const void *base[BT_MAX_FRAMES];
  const char *name[BT_MAX_FRAMES];
  hts_boolean grouped[BT_MAX_FRAMES];
  char module[BT_PATH_SIZE];
  char *argv[4 + BT_MAX_FRAMES + 1];
  int budget = BT_WAIT_TICKS;
  int i, spawned;

  if (symbolizer_opts == NULL) /* none on PATH at startup */
    return;
  if (size > BT_MAX_FRAMES)
    size = BT_MAX_FRAMES;

  for (i = 0; i < size; i++) {
    Dl_info info;

    grouped[i] = HTS_TRUE; /* skipped unless dladdr() places the frame */
    if (dladdr(stack[i], &info) == 0 || info.dli_fname == NULL ||
        info.dli_fname[0] == '\0')
      continue;
    base[i] = info.dli_fbase;
    name[i] = info.dli_fname;
    print_hex(hex[i], (uintptr_t) ((const char *) stack[i] -
                                   (const char *) info.dli_fbase));
    grouped[i] = HTS_FALSE;
  }

  /* One child per module: addr2line takes a single -e. Each frame is claimed
     once, so argc cannot exceed argv[]. */
  for (spawned = 0; spawned < BT_MAX_MODULES; spawned++) {
    int first, j, argc = 0;

    for (first = 0; first < size && grouped[first]; first++)
      ;
    if (first >= size)
      break;
    argv[argc++] = symbolizer_path;
    argv[argc++] = symbolizer_opts;
    argv[argc++] = dashe;
    argv[argc++] = module;
    for (j = first; j < size; j++) {
      if (grouped[j] || base[j] != base[first])
        continue;
      grouped[j] = HTS_TRUE;
      argv[argc++] = hex[j];
    }
    argv[argc] = NULL;
    /* access(): skip pseudo-modules like linux-vdso, which have no file and
       would draw nothing but an addr2line complaint. */
    if (copy_bounded(module, sizeof(module), name[first]) &&
        access(module, R_OK) == 0) {
      const size_t len = strlen(module);

      /* addr2line -a prints offsets only: say which module they are in. */
      (void) (write(fd, module, len) == (ssize_t) len);
      (void) (write(fd, ":\n", 2) == 2);
      if (!spawn_symbolizer(argv, fd, &budget))
        break; /* no symbolizer: stop at one header */
    }
  }
}
#endif

void hts_backtrace_init(void) {
#ifdef USES_BACKTRACE
  void *frame[1];

  symbolize_crash =
      getenv("HTTRACK_NO_SYMBOLIZE") == NULL ? HTS_TRUE : HTS_FALSE;
  if (symbolize_crash)
    resolve_symbolizer();
  /* Pay for the unwinder now: glibc's first backtrace() dlopen()s libgcc_s,
     which allocates and takes the loader lock the crashing thread may hold. */
  (void) backtrace(frame, 1);
#endif
}

hts_boolean hts_backtrace_altstack(void) {
#ifdef USES_SIGALTSTACK
  /* Not a constant since glibc 2.34: SIGSTKSZ is a sysconf() call. */
  const size_t size =
      (size_t) SIGSTKSZ > BT_ALTSTACK_MIN ? (size_t) SIGSTKSZ : BT_ALTSTACK_MIN;
  stack_t ss;
  void *sp;

  /* Never take one over, whatever its size: a sanitizer runtime installs its
     own and sizes it for its own handlers (ASan: 32kB, ours needs ~10kB). */
  if (sigaltstack(NULL, &ss) == 0 && (ss.ss_flags & SS_DISABLE) == 0)
    return HTS_TRUE;
  /* Mapped, not allocated: a stack for the handler must not live in the heap
     whose corruption we may be reporting. */
  sp = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1,
            0);
  if (sp == MAP_FAILED)
    return HTS_FALSE;
  ss.ss_sp = sp;
  ss.ss_size = size;
  ss.ss_flags = 0;
  if (sigaltstack(&ss, NULL) != 0) {
    munmap(sp, size);
    return HTS_FALSE;
  }
  return HTS_TRUE;
#else
  return HTS_FALSE;
#endif
}

/* Why the report has no frames: a silent gap reads as a handler that died. */
static void print_no_trace(int fd, const char *msg, size_t len) {
  if (write(fd, msg, len) != len) { /* no ssize_t: this is built on MSVC too */
    /* sorry GCC */
  }
}

void hts_print_backtrace(int fd) {
#ifdef USES_BACKTRACE
  void *stack[256];
  const int size = backtrace(stack, sizeof(stack) / sizeof(stack[0]));

  /* A fault inside the handler lands back here: symbolizing twice interleaves
     two traces on fd and spends a second budget. */
  static volatile sig_atomic_t entered = 0;

  if (size != 0) {
    backtrace_symbols_fd(stack, size, fd);
    if (symbolize_crash && entered == 0) {
      entered = 1;
      symbolize_backtrace(stack, size, fd);
      entered = 0;
    }
  } else {
    /* An empty trace means the build carries no unwind tables. */
    const char msg[] = "No stack trace available: unwinding failed\n";

    print_no_trace(fd, msg, sizeof(msg) - 1);
  }
#else
  const char msg[] = "No stack trace available on this OS :(\n";

  print_no_trace(fd, msg, sizeof(msg) - 1);
#endif
}
