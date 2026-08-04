/* Says what happened to the alternate signal stack a process was handed, for
   181_altstack-honoured.test. ALTSTACK_MODE picks the state main() inherits:
   "keep" installs one first, "none" leaves none installed. */

#define _GNU_SOURCE

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define PROBE_STACK_SIZE (128 * 1024)

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
