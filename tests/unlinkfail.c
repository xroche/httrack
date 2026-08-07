/* Borrows Windows' refusal to unlink a file the process still holds open, which
   POSIX cannot otherwise reach: UNLINKFAIL_MATCH names the basename to refuse
   with EACCES. Every refusal is appended to UNLINKFAIL_LOG, so a shim that
   silently never fires cannot read as a passing test (#581). */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The tree builds with -fvisibility=hidden, which would hide the interposer. */
#define SHIM_EXPORT __attribute__((visibility("default")))

SHIM_EXPORT int unlink(const char *pathname);

static int unlinkfail_refused(const char *pathname) {
  const char *const match = getenv("UNLINKFAIL_MATCH");
  const char *const slash = strrchr(pathname, '/');
  const char *const base = slash != NULL ? slash + 1 : pathname;
  const char *log;

  if (match == NULL || strcmp(base, match) != 0) {
    return 0;
  }
  log = getenv("UNLINKFAIL_LOG");
  if (log != NULL) {
    FILE *const fp = fopen(log, "ab");

    if (fp != NULL) {
      fprintf(fp, "refused %s\n", pathname);
      fclose(fp);
    }
  }
  return 1;
}

SHIM_EXPORT int unlink(const char *pathname) {
  static int (*real_unlink)(const char *) = NULL;

  if (unlinkfail_refused(pathname)) {
    errno = EACCES;
    return -1;
  }
  if (real_unlink == NULL) {
    *(void **) &real_unlink = dlsym(RTLD_NEXT, "unlink");
    if (real_unlink == NULL) {
      errno = ENOSYS;
      return -1;
    }
  }
  return real_unlink(pathname);
}
