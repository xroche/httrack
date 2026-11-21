/* Stalls getaddrinfo() on GAISTALL_HOST (default 127.0.0.1) for GAISTALL
   seconds, for 527_local-maxtime-dns-stall.test (#1840) and
   2415_local-dns-nonblocking.test. */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
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
  const char *const host = getenv("GAISTALL_HOST");

  if (stall != NULL && node != NULL &&
      strcmp(node, host != NULL ? host : "127.0.0.1") == 0) {
    unsigned int left = (unsigned int) atoi(stall);

    while (left > 0) /* sleep() returns early on a signal */
      left = sleep(left);
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
