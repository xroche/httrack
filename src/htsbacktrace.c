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
#include <link.h>
#include <signal.h>
#include <spawn.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define USES_BACKTRACE
#endif

#ifdef _WIN32
#define BT_REPORT_FD 2 /* MSVC ships no <unistd.h> */
#else
#define BT_REPORT_FD STDERR_FILENO
#endif

#ifdef USES_BACKTRACE
#define BT_MAX_FRAMES 64  /* frames we try to name */
#define BT_MAX_MODULES 8  /* distinct modules, one child each */
#define BT_HEX_SIZE 19    /* "0x" + 16 nibbles + NUL */
#define BT_PATH_SIZE 1024 /* module path; longer is skipped */
#define BT_WAIT_TICKS 300 /* 10ms ticks, shared: cap a slow child */

static hts_boolean symbolize_crash = HTS_TRUE;

/* Built at init: assembling file actions allocates; the crash path cannot. */
static posix_spawn_file_actions_t spawn_actions;
static posix_spawn_file_actions_t *spawn_redirect = NULL;

/* dladdr() names the main program after argv[0], which the symbolizer cannot
   open when the binary came off PATH; /proc/self/exe always names the file.
   hts_self_path() (htscoremain.c) resolves the same path, out of reach here:
   it is library-side and hidden by -fvisibility=hidden (#997). */
static char main_path[BT_PATH_SIZE];
/* Main program's mapped range, and the bias to subtract for addr2line (0 on an
   ET_EXEC). */
static uintptr_t main_lo, main_hi, main_bias;

static int record_main_range(struct dl_phdr_info *info, size_t size,
                             void *data) {
  size_t i;

  (void) size;
  (void) data;
  main_bias = (uintptr_t) info->dlpi_addr;
  main_lo = (uintptr_t) -1; /* an empty range matches no frame */
  main_hi = 0;
  for (i = 0; i < info->dlpi_phnum; i++) {
    const ElfW(Phdr) *const phdr = &info->dlpi_phdr[i];
    const uintptr_t start = main_bias + (uintptr_t) phdr->p_vaddr;

    if (phdr->p_type != PT_LOAD)
      continue;
    if (start < main_lo)
      main_lo = start;
    if (start + phdr->p_memsz > main_hi)
      main_hi = start + phdr->p_memsz;
  }
  return 1; /* dl_iterate_phdr starts at the main program */
}

/* Address range, not load base: the two disagree on an ET_EXEC (#995). */
static hts_boolean is_main_frame(const void *addr) {
  const uintptr_t value = (uintptr_t) addr;

  return value >= main_lo && value < main_hi ? HTS_TRUE : HTS_FALSE;
}

/* Non-zero once a named link-map entry is the file *data stats to: the main
   program's entry is the unnamed one, so a hit means a shared object. */
static int names_a_shared_object(struct dl_phdr_info *info, size_t size,
                                 void *data) {
  const struct stat *const self = (const struct stat *) data;
  struct stat st;

  (void) size;
  if (info->dlpi_name == NULL || info->dlpi_name[0] == '\0')
    return 0;
  if (stat(info->dlpi_name, &st) != 0)
    return 0; /* vDSO and the like have no file */
  return st.st_dev == self->st_dev && st.st_ino == self->st_ino;
}

static void find_main_object(void) {
  const ssize_t len = readlink("/proc/self/exe", main_path, sizeof(main_path));
  struct stat self;

  dl_iterate_phdr(record_main_range, NULL);
  /* A full buffer is a clipped path, which readlink() cannot report: its prefix
     names another file, and the symbolizer would happily open that one. */
  if (len > 0 && (size_t) len < sizeof(main_path)) {
    main_path[len] = '\0';
    /* Started through the loader, /proc/self/exe is ld.so, which the link map
       carries under its own name (#996). Identity, not dladdr()'s name: that is
       argv[0] verbatim, so a symlinked or renamed program would lose its
       module. */
    if (stat(main_path, &self) == 0 &&
        dl_iterate_phdr(names_a_shared_object, &self) == 0)
      return;
  }
  main_path[0] = '\0'; /* unresolved: keep whatever dladdr() reported */
}

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

/* Run the symbolizer on argv within *budget ticks; HTS_FALSE only if none could
   be run at all. Not fork(): it runs the pthread_atfork handlers, and glibc's
   malloc registers one taking every arena lock a signal inside malloc holds
   (#968). posix_spawn() clones with CLONE_VFORK instead, and counts as
   async-signal-safe as of POSIX.1-2024. */
static hts_boolean spawn_symbolizer(char **argv, int *budget) {
  static char llvm_prog[] = "llvm-symbolizer";
  static char llvm_opts[] = "-p";
  pid_t pid;

  if (posix_spawnp(&pid, argv[0], spawn_redirect, NULL, argv, environ) != 0) {
    argv[0] = llvm_prog; /* an LLVM-only install ships no addr2line */
    argv[1] = llvm_opts;
    if (posix_spawnp(&pid, argv[0], spawn_redirect, NULL, argv, environ) != 0)
      return HTS_FALSE;
  }
  for (; *budget > 0; (*budget)--) {
    const struct timespec tick = {0, 10 * 1000 * 1000};
    const pid_t reaped = waitpid(pid, NULL, WNOHANG);

    if (reaped == pid || (reaped == -1 && errno != EINTR))
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
static void symbolize_backtrace(void *const *stack, int size) {
  static char prog[] = "addr2line";
  static char opts[] = "-Cfipa";
  static char dashe[] = "-e";
  char hex[BT_MAX_FRAMES][BT_HEX_SIZE];
  const void *base[BT_MAX_FRAMES];
  const char *name[BT_MAX_FRAMES];
  hts_boolean grouped[BT_MAX_FRAMES];
  hts_boolean is_main[BT_MAX_FRAMES];
  char module[BT_PATH_SIZE];
  char *argv[4 + BT_MAX_FRAMES + 1];
  int budget = BT_WAIT_TICKS;
  int i, spawned;

  if (spawn_redirect == NULL) /* init could not prepare the child's redirect */
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
    is_main[i] = is_main_frame(stack[i]);
    print_hex(hex[i],
              (uintptr_t) stack[i] -
                  (is_main[i] ? main_bias : (uintptr_t) info.dli_fbase));
    grouped[i] = HTS_FALSE;
  }

  /* One child per module: addr2line takes a single -e. Each frame is claimed
     once, so argc cannot exceed argv[]. */
  for (spawned = 0; spawned < BT_MAX_MODULES; spawned++) {
    int first, j, argc = 0;
    const char *path;

    for (first = 0; first < size && grouped[first]; first++)
      ;
    if (first >= size)
      break;
    path = main_path[0] != '\0' && is_main[first] ? main_path : name[first];
    argv[argc++] = prog;
    argv[argc++] = opts;
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
    if (copy_bounded(module, sizeof(module), path) &&
        access(module, R_OK) == 0) {
      const size_t len = strlen(module);

      /* addr2line -a prints offsets only: say which module they are in. */
      (void) (write(BT_REPORT_FD, module, len) == (ssize_t) len);
      (void) (write(BT_REPORT_FD, ":\n", 2) == 2);
      if (!spawn_symbolizer(argv, &budget))
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
  if (symbolize_crash && posix_spawn_file_actions_init(&spawn_actions) == 0 &&
      posix_spawn_file_actions_adddup2(&spawn_actions, BT_REPORT_FD,
                                       STDOUT_FILENO) == 0)
    spawn_redirect = &spawn_actions; /* both symbolizers write on stdout */
  find_main_object();
  /* Pay for the unwinder now: glibc's first backtrace() dlopen()s libgcc_s,
     which allocates and takes the loader lock the crashing thread may hold. */
  (void) backtrace(frame, 1);
#endif
}

void *hts_backtrace_altstack(void) {
#ifdef USES_SIGALTSTACK
  /* Not a constant since glibc 2.34: SIGSTKSZ is a sysconf() call. */
  const size_t size =
      (size_t) SIGSTKSZ > BT_ALTSTACK_MIN ? (size_t) SIGSTKSZ : BT_ALTSTACK_MIN;
  stack_t ss;
  void *sp;

  /* Never take one over, whatever its size: a sanitizer runtime installs its
     own and sizes it for its own handlers (ASan: 32kB, ours needs ~10kB). */
  if (sigaltstack(NULL, &ss) == 0 && (ss.ss_flags & SS_DISABLE) == 0)
    return NULL;
  /* Mapped, not allocated: a stack for the handler must not live in the heap
     whose corruption we may be reporting. */
  sp = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1,
            0);
  if (sp == MAP_FAILED)
    return NULL;
  ss.ss_sp = sp;
  ss.ss_size = size;
  ss.ss_flags = 0;
  if (sigaltstack(&ss, NULL) != 0) {
    munmap(sp, size);
    return NULL;
  }
  return sp;
#else
  return NULL;
#endif
}

void hts_backtrace_altstack_release(void *stack) {
#ifdef USES_SIGALTSTACK
  stack_t ss;
  size_t size;

  if (stack == NULL)
    return;
  /* Whatever replaced ours since is not ours to unmap. */
  if (sigaltstack(NULL, &ss) != 0 || ss.ss_sp != stack)
    return;
  size = ss.ss_size;
  ss.ss_flags = SS_DISABLE;
  /* Disable first: the kernel must never send a handler to an unmapped page. */
  if (sigaltstack(&ss, NULL) == 0)
    munmap(stack, size);
#else
  (void) stack;
#endif
}

unsigned int hts_print_num(char *buffer, int num) {
  const int neg = num < 0;
  /* negate in unsigned: INT_MIN has no positive counterpart */
  unsigned int val = neg ? -(unsigned int) num : (unsigned int) num;
  unsigned int i, j;

  if (neg) {
    *(buffer++) = '-';
  }
  for (i = 0; val != 0 || i == 0; i++, val /= 10) {
    buffer[i] = '0' + (val % 10);
  }
  /* j stops at i/2: going to i swaps each pair twice and undoes the reversal */
  for (j = 0; j < i / 2; j++) {
    const char c = buffer[i - j - 1];
    buffer[i - j - 1] = buffer[j];
    buffer[j] = c;
  }
  buffer[i] = '\0';
  return i + (unsigned int) neg;
}

/* Why the report has no frames: a silent gap reads as a handler that died. */
static void print_no_trace(int fd, const char *msg, unsigned int len) {
  if (write(fd, msg, len) != len) { /* no ssize_t: this is built on MSVC too */
    /* sorry GCC */
  }
}

void hts_print_backtrace(void) {
#ifdef USES_BACKTRACE
  void *stack[256];
  const int size = backtrace(stack, sizeof(stack) / sizeof(stack[0]));

  /* A fault inside the handler lands back here: symbolizing twice interleaves
     two traces on the report fd and spends a second budget. */
  static volatile sig_atomic_t entered = 0;

  if (size != 0) {
    backtrace_symbols_fd(stack, size, BT_REPORT_FD);
    if (symbolize_crash && entered == 0) {
      entered = 1;
      symbolize_backtrace(stack, size);
      entered = 0;
    }
  } else {
    /* An empty trace means the build carries no unwind tables. */
    const char msg[] = "No stack trace available: unwinding failed\n";

    print_no_trace(BT_REPORT_FD, msg, sizeof(msg) - 1);
  }
#else
  const char msg[] = "No stack trace available on this OS :(\n";

  print_no_trace(BT_REPORT_FD, msg, sizeof(msg) - 1);
#endif
}
