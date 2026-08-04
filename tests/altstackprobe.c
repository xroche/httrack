/* Says what happened to the alternate signal stack a process was handed, for
   181_altstack-honoured.test. ALTSTACK_MODE picks the state main() inherits:
   "keep" installs one first, "none" leaves none installed.

   ALTSTACK_TRACE names a file to log the per-thread lifecycle to, for
   183_altstack-worker.test: "<pid> <tid> query|set on|off|own <sp>" for every
   sigaltstack(), "<pid> <tid> munmap - <sp>" for every unmap of a stack seen
   installed. Only the ordering across those two syscalls can show that a worker
   gives its stack back, and gives it back disabled first.

   "own" is an install of a mapping this shim watched mmap() hand out, which is
   what tells httrack's stacks apart from a sanitizer runtime's: those come from
   a raw syscall no interposer sees, though installed the same way. */

#define _GNU_SOURCE

#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#define PROBE_STACK_SIZE (128 * 1024)

/* -fvisibility=hidden across the tree would otherwise hide the interposers. */
#define SHIM_EXPORT __attribute__((visibility("default")))

static void *probe_sp = NULL;

static void say(const char *msg, size_t len) {
  (void) (write(2, msg, len) == (ssize_t) len);
}

#define SAY(s) say(s, sizeof(s) - 1)

static void __attribute__((constructor)) probe_setup(void) {
  const char *const mode = getenv("ALTSTACK_MODE");
  stack_t ss;

  if (mode == NULL) {
    return;
  }
  memset(&ss, 0, sizeof(ss));
  if (strcmp(mode, "keep") == 0) {
    probe_sp = mmap(NULL, PROBE_STACK_SIZE, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (probe_sp == MAP_FAILED) {
      _exit(1);
    }
    ss.ss_sp = probe_sp;
    ss.ss_size = PROBE_STACK_SIZE;
  } else {
    ss.ss_flags = SS_DISABLE; /* drop the sanitizer's own, if any */
  }
  if (sigaltstack(&ss, NULL) != 0) {
    _exit(1);
  }
}

/* Compares the mapping the process ends up with against the one it was given,
   which no count of sigaltstack() calls can do: a sanitizer runtime makes
   plenty of its own. */
static void __attribute__((destructor)) probe_report(void) {
  stack_t ss;

  if (getenv("ALTSTACK_MODE") == NULL || sigaltstack(NULL, &ss) != 0) {
    return;
  }
  if ((ss.ss_flags & SS_DISABLE) != 0 || ss.ss_sp == NULL) {
    SAY("ALTSTACK-NONE\n");
  } else if (ss.ss_sp == probe_sp) {
    SAY("ALTSTACK-KEPT\n");
  } else {
    SAY("ALTSTACK-OWN\n");
  }
}

/* One per worker, so several crawls worth of them fit. */
#define TRACE_STACKS 256

static int trace_fd = -1;
static void *trace_stacks[TRACE_STACKS];
static int trace_stacks_used = 0;
static __thread void *trace_last_mmap = NULL;
static int (*real_sigaltstack)(const stack_t *, stack_t *) = NULL;
static int (*real_munmap)(void *, size_t) = NULL;
static void *(*real_mmap)(void *, size_t, int, int, int, off_t) = NULL;
static void *(*real_mmap64)(void *, size_t, int, int, int, off64_t) = NULL;

/* Runs ahead of probe_setup(), whose own sigaltstack() call would otherwise
   reach an unresolved interposer. */
static void __attribute__((constructor(101))) trace_setup(void) {
  const char *const path = getenv("ALTSTACK_TRACE");

  *(void **) &real_sigaltstack = dlsym(RTLD_NEXT, "sigaltstack");
  *(void **) &real_munmap = dlsym(RTLD_NEXT, "munmap");
  *(void **) &real_mmap = dlsym(RTLD_NEXT, "mmap");
  *(void **) &real_mmap64 = dlsym(RTLD_NEXT, "mmap64");
  if (path == NULL || real_sigaltstack == NULL || real_munmap == NULL ||
      real_mmap == NULL) {
    return;
  }
  /* Appended to by every process under the preload, hence the pid per line. */
  trace_fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
}

static void trace_line(const char *event, const char *state, const void *sp) {
  char line[128];
  int len;

  if (trace_fd < 0) {
    return;
  }
  len = snprintf(line, sizeof(line), "%ld %ld %s %s %p\n", (long) getpid(),
                 (long) syscall(SYS_gettid), event, state, sp);
  /* One write per line, so O_APPEND keeps the threads from interleaving. */
  if (len > 0 && (size_t) len < sizeof(line)) {
    (void) (write(trace_fd, line, (size_t) len) == len);
  }
}

static void trace_remember(void *sp) {
  const int slot = __atomic_fetch_add(&trace_stacks_used, 1, __ATOMIC_RELAXED);

  if (slot < TRACE_STACKS) {
    __atomic_store_n(&trace_stacks[slot], sp, __ATOMIC_RELEASE);
  }
}

static int trace_is_stack(const void *sp) {
  const int used = __atomic_load_n(&trace_stacks_used, __ATOMIC_RELAXED);
  int i;

  for (i = 0; i < used && i < TRACE_STACKS; i++) {
    if (__atomic_load_n(&trace_stacks[i], __ATOMIC_ACQUIRE) == sp) {
      return 1;
    }
  }
  return 0;
}

SHIM_EXPORT int sigaltstack(const stack_t *ss, stack_t *old);

SHIM_EXPORT int sigaltstack(const stack_t *ss, stack_t *old) {
  const int rc = real_sigaltstack != NULL
                     ? real_sigaltstack(ss, old)
                     : (int) syscall(SYS_sigaltstack, ss, old);

  if (rc != 0 || trace_fd < 0) {
    return rc;
  }
  if (ss != NULL) {
    const int off = (ss->ss_flags & SS_DISABLE) != 0;

    trace_line("set",
               off                            ? "off"
               : ss->ss_sp == trace_last_mmap ? "own"
                                              : "on",
               ss->ss_sp);
    if (!off) {
      trace_remember(ss->ss_sp);
    }
  } else if (old != NULL) {
    trace_line("query", (old->ss_flags & SS_DISABLE) != 0 ? "off" : "on",
               old->ss_sp);
  }
  return rc;
}

static void *trace_mapped(void *sp) {
  if (sp != MAP_FAILED) {
    trace_last_mmap = sp;
  }
  return sp;
}

SHIM_EXPORT void *mmap(void *addr, size_t len, int prot, int flags, int fd,
                       off_t off);
SHIM_EXPORT void *mmap64(void *addr, size_t len, int prot, int flags, int fd,
                         off64_t off);

/* hts_backtrace_altstack() installs what it just mapped, with nothing in
   between, so the thread's last mapping is the whole of the check. Both names:
   _FILE_OFFSET_BITS=64 is what the engine's own mmap() call ends up as. */
SHIM_EXPORT void *mmap(void *addr, size_t len, int prot, int flags, int fd,
                       off_t off) {
  return trace_mapped(
      real_mmap != NULL
          ? real_mmap(addr, len, prot, flags, fd, off)
          : (void *) syscall(SYS_mmap, addr, len, prot, flags, fd, off));
}

SHIM_EXPORT void *mmap64(void *addr, size_t len, int prot, int flags, int fd,
                         off64_t off) {
  return real_mmap64 != NULL
             ? trace_mapped(real_mmap64(addr, len, prot, flags, fd, off))
             : mmap(addr, len, prot, flags, fd, (off_t) off);
}

SHIM_EXPORT int munmap(void *addr, size_t len);

SHIM_EXPORT int munmap(void *addr, size_t len) {
  /* Only the stacks: a process unmaps plenty else, and the trace is read as an
     unbroken per-thread sequence. Logged before the call, so a stack the kernel
     is still pointed at leaves its record behind. */
  if (trace_fd >= 0 && trace_is_stack(addr)) {
    trace_line("munmap", "-", addr);
  }
  return real_munmap != NULL ? real_munmap(addr, len)
                             : (int) syscall(SYS_munmap, addr, len);
}
