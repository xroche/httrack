/* Stalls getaddrinfo() on 127.0.0.1 for GAISTALL seconds, for
   527_local-maxtime-dns-stall.test (#1840). */

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

  if (stall != NULL && node != NULL && strcmp(node, "127.0.0.1") == 0) {
    unsigned int left = (unsigned int) atoi(stall);

    while (left > 0) /* sleep() returns early on a signal */
      left = sleep(left);
  }
  if (real_gai == NULL) {
    *(void **) &real_gai = dlsym(RTLD_NEXT, "getaddrinfo");
    if (real_gai == NULL)
      return EAI_FAIL;
  }
  return real_gai(node, service, hints, res);
}
