/* Borrows Windows' rename() for the tests POSIX cannot otherwise reach:
   hts_rename_over()'s aside fallback needs an existing target refused with
   EEXIST, and RENAMEFAIL_MODE=locked reports EACCES instead, as the CRT does
   for a source another process holds. =repair and =repair-eexist narrow the
   refusal to the cache-repair move (#786), the latter through the fallback so
   the retry inside it is the failing one. RENAMEFAIL_ASIDE_FAILS=N refuses the
   first N moves back out of a parked ".hts-old" name (#790). */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* The tree builds with -fvisibility=hidden, which would hide the interposer. */
#define SHIM_EXPORT __attribute__((visibility("default")))

SHIM_EXPORT int rename(const char *oldpath, const char *newpath);

SHIM_EXPORT int rename(const char *oldpath, const char *newpath) {
  static int (*real_rename)(const char *, const char *) = NULL;
  static int aside_failures = 0;
  const char *const mode = getenv("RENAMEFAIL_MODE");
  const char *const aside_fails = getenv("RENAMEFAIL_ASIDE_FAILS");
  const char *const slash = strrchr(oldpath, '/');
  const char *const base = slash != NULL ? slash + 1 : oldpath;
  const int repaired = strcmp(base, "repair.zip") == 0;
  struct stat st;

  if (mode != NULL && (strcmp(mode, "locked") == 0 ||
                       (repaired && strcmp(mode, "repair") == 0))) {
    errno = EACCES;
    return -1;
  }
  if (repaired && mode != NULL && strcmp(mode, "repair-eexist") == 0) {
    errno = stat(newpath, &st) == 0 ? EEXIST : EACCES;
    return -1;
  }
  if (aside_fails != NULL && strstr(base, ".hts-old") != NULL &&
      aside_failures < atoi(aside_fails)) {
    aside_failures++;
    errno = EACCES;
    return -1;
  }
  /* Only the bare shim emulates Windows for every rename; a narrowed mode
     leaves the rest of the run on plain POSIX semantics. */
  if (mode == NULL && aside_fails == NULL && stat(newpath, &st) == 0) {
    errno = EEXIST;
    return -1;
  }
  if (real_rename == NULL) {
    *(void **) &real_rename = dlsym(RTLD_NEXT, "rename");
    if (real_rename == NULL) {
      errno = ENOSYS;
      return -1;
    }
  }
  return real_rename(oldpath, newpath);
}
