/* unlink() fails with EACCES while this process still holds the file open, as
   it does on Windows. UNLINKFAIL_MATCH picks the basename to watch;
   UNLINKFAIL_LOG collects one "open" or "closed" line per attempt on it. */

#define _GNU_SOURCE
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/* The tree builds with -fvisibility=hidden, which would hide the interposer. */
#define SHIM_EXPORT __attribute__((visibility("default")))

SHIM_EXPORT int unlink(const char *pathname);

/* Matched by (dev, ino): a relative path or a symlinked tmpdir would miss. */
static int unlinkfail_held_open(const char *pathname) {
  struct stat target;
  DIR *dir;
  struct dirent *ent;
  int held = 0;

  if (stat(pathname, &target) != 0) {
    return 0;
  }
  dir = opendir("/proc/self/fd");
  if (dir == NULL) {
    return 0;
  }
  while (!held && (ent = readdir(dir)) != NULL) {
    char path[64];
    struct stat st;

    if (ent->d_name[0] == '.') {
      continue;
    }
    snprintf(path, sizeof(path), "/proc/self/fd/%s", ent->d_name);
    if (stat(path, &st) == 0 && st.st_dev == target.st_dev &&
        st.st_ino == target.st_ino) {
      held = 1;
    }
  }
  closedir(dir);
  return held;
}

static void unlinkfail_log(const char *state, const char *pathname) {
  const char *const log = getenv("UNLINKFAIL_LOG");
  FILE *fp;

  if (log == NULL || (fp = fopen(log, "ab")) == NULL) {
    return;
  }
  fprintf(fp, "%s %s\n", state, pathname);
  fclose(fp);
}

SHIM_EXPORT int unlink(const char *pathname) {
  static int (*real_unlink)(const char *) = NULL;
  const char *const match = getenv("UNLINKFAIL_MATCH");
  const char *const slash = strrchr(pathname, '/');
  const char *const base = slash != NULL ? slash + 1 : pathname;

  if (match != NULL && strcmp(base, match) == 0) {
    if (unlinkfail_held_open(pathname)) {
      unlinkfail_log("open", pathname);
      errno = EACCES;
      return -1;
    }
    unlinkfail_log("closed", pathname);
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
