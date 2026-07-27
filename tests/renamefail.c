/* LD_PRELOAD shim giving POSIX rename() the Windows shape: it refuses an
   existing target with EACCES, and reports that ahead of a missing source.
   Without it hts_rename_over()'s unlink fallback is unreachable on POSIX. */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>

/* The tree builds with -fvisibility=hidden, which would hide the interposer. */
#define SHIM_EXPORT __attribute__((visibility("default")))

SHIM_EXPORT int rename(const char *oldpath, const char *newpath);

SHIM_EXPORT int rename(const char *oldpath, const char *newpath) {
  static int (*real_rename)(const char *, const char *) = NULL;
  struct stat st;

  if (stat(newpath, &st) == 0) {
    errno = EACCES;
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
