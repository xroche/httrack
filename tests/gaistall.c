/* Stalls getaddrinfo() on GAISTALL_HOST (default 127.0.0.1) for GAISTALL
   seconds, or GAISTALL_MS milliseconds, appending a line to GAISTALL_LOG per
   stalled call. For 527_local-maxtime-dns-stall.test (#1840) and
   2415_local-dns-nonblocking.test. */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* The tree builds with -fvisibility=hidden, which would hide the interposer. */
#define SHIM_EXPORT __attribute__((visibility("default")))

SHIM_EXPORT int getaddrinfo(const char *node, const char *service,
                            const struct addrinfo *hints,
                            struct addrinfo **res);

SHIM_EXPORT int getaddrinfo(const char *node, const char *service,
                            const struct addrinfo *hints,
                            struct addrinfo **res) {
  static int (*real_gai)(const char *, const char *, const struct addrinfo *,
                         struct addrinfo **) = NULL;
  const char *const stall = getenv("GAISTALL");
  const char *const stall_ms = getenv("GAISTALL_MS");
  const char *const host = getenv("GAISTALL_HOST");
  const char *const log = getenv("GAISTALL_LOG");

  if ((stall != NULL || stall_ms != NULL) && node != NULL &&
      strcmp(node, host != NULL ? host : "127.0.0.1") == 0) {
    const long ms =
        stall_ms != NULL ? atol(stall_ms) : (long) atoi(stall) * 1000L;
    struct timespec left;

    if (log != NULL) {
      const int fd = open(log, O_WRONLY | O_CREAT | O_APPEND, 0644);

      if (fd >= 0) {
        const ssize_t n = write(fd, "stalled\n", 8);

        (void) n; /* a short write shows as a missing line */
        close(fd);
      }
    }
    left.tv_sec = ms / 1000;
    left.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&left, &left) != 0 && errno == EINTR) {
    }
  }
  int (*gai)(const char *, const char *, const struct addrinfo *,
             struct addrinfo **) = __atomic_load_n(&real_gai, __ATOMIC_ACQUIRE);

  /* two resolver workers may get here at once */
  if (gai == NULL) {
    *(void **) &gai = dlsym(RTLD_NEXT, "getaddrinfo");
    if (gai == NULL)
      return EAI_FAIL;
    __atomic_store_n(&real_gai, gai, __ATOMIC_RELEASE);
  }
  return gai(node, service, hints, res);
}
