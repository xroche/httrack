/* Borrows Windows' rename() for 01_engine-renameover.test, since POSIX cannot
   reach hts_rename_over()'s aside fallback: an existing target is refused with
   EEXIST, and RENAMEFAIL_MODE=locked reports EACCES instead, as the CRT does
   for a source another process holds. RENAMEFAIL_ASIDE_FAILS=N refuses the
   first N moves back out of the parked ".hts-old" name, which is the only way
   to reach hts_rename_over()'s restore retry (#790). */

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
  const int locked = mode != NULL && strcmp(mode, "locked") == 0;
  struct stat st;

  if (locked) {
    errno = EACCES;
    return -1;
  }
  if (aside_fails != NULL && strstr(oldpath, ".hts-old") != NULL &&
      aside_failures < atoi(aside_fails)) {
    aside_failures++;
    errno = EACCES;
    return -1;
  }
  if (aside_fails == NULL && stat(newpath, &st) == 0) {
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
