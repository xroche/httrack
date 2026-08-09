/* close() observer for 253_local-ftp-close-once.test: one "fd rc status" line
   per call in CLOSETRACK_LOG, so a double-close shows up as EBADF. Writes with
   write(2) rather than stdio, which would close a descriptor and recurse. */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* The tree builds with -fvisibility=hidden, which would hide the interposer. */
#define SHIM_EXPORT __attribute__((visibility("default")))

SHIM_EXPORT int close(int fd);

static int closetrack_log = -1;
static int closetrack_engine = 0;

/* Opened from the constructor: the process is still single-threaded there, so
   the descriptor needs no lock, and O_APPEND keeps concurrent writes whole. */
static void __attribute__((constructor)) closetrack_open(void) {
  const char *const path = getenv("CLOSETRACK_LOG");

  if (path != NULL) {
    closetrack_log =
        open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
  }
}

/* The preload also lands on the libtool wrapper, a /bin/sh that closes -1 on
   its own; the engine is the process holding libhttrack. Never caches a no, so
   a close issued before the library is mapped cannot blind the rest. */
static int closetrack_is_engine(void) {
  if (!closetrack_engine && dlsym(RTLD_DEFAULT, "hts_init") != NULL) {
    closetrack_engine = 1;
  }
  return closetrack_engine;
}

static const char *closetrack_status(int rc, int err) {
  if (rc == 0) {
    return "ok";
  }
  return err == EBADF ? "EBADF" : "other";
}

SHIM_EXPORT int close(int fd) {
  static int (*real_close)(int) = NULL;
  int rc, err;

  if (real_close == NULL) {
    *(void **) &real_close = dlsym(RTLD_NEXT, "close");
    if (real_close == NULL) {
      errno = ENOSYS;
      return -1;
    }
  }
  if (fd == closetrack_log) {
    /* Refused rather than obeyed: losing the log would blind the test. */
    rc = -1;
    err = EBADF;
  } else {
    rc = real_close(fd);
    err = errno;
  }
  if (closetrack_log != -1 && closetrack_is_engine()) {
    char line[64];
    const int len = snprintf(line, sizeof(line), "%d %d %s\n", fd, rc,
                             closetrack_status(rc, err));

    if (len > 0) {
      const ssize_t written = write(closetrack_log, line, (size_t) len);

      (void) written;
    }
  }
  errno = err;
  return rc;
}
