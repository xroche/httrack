/* fseeko() fails with EFBIG past SEEKFAIL_CAP bytes, standing in for GNU/Hurd
   ext2fs's low file-size ceiling. SEEKFAIL_ERROR=eio refuses for an unrelated
   reason instead, which nothing may excuse. */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The tree builds with -fvisibility=hidden, which would hide the interposer. */
#define SHIM_EXPORT __attribute__((visibility("default")))

/* glibc redirects fseeko to fseeko64 under _FILE_OFFSET_BITS=64, and the engine
   calls whichever its own build resolved to: interpose both names. */
#undef fseeko

SHIM_EXPORT int fseeko(FILE *stream, off_t offset, int whence);
SHIM_EXPORT int fseeko64(FILE *stream, int64_t offset, int whence);

static int seekfail_capped(int64_t offset) {
  const char *const cap = getenv("SEEKFAIL_CAP");
  const char *const kind = getenv("SEEKFAIL_ERROR");
  char *end;

  /* a cap that is not a number would otherwise read as zero and refuse
   * everything */
  if (cap == NULL || *cap == '\0') {
    return 0;
  }
  if (offset <= (int64_t) strtoll(cap, &end, 10) || *end != '\0') {
    return 0;
  }
  errno = kind != NULL && strcmp(kind, "eio") == 0 ? EIO : EFBIG;
  return 1;
}

SHIM_EXPORT int fseeko(FILE *stream, off_t offset, int whence) {
  static int (*real_fseeko)(FILE *, off_t, int) = NULL;

  if (whence == SEEK_SET && seekfail_capped((int64_t) offset)) {
    return -1;
  }
  if (real_fseeko == NULL) {
    *(void **) &real_fseeko = dlsym(RTLD_NEXT, "fseeko");
    if (real_fseeko == NULL) {
      errno = ENOSYS;
      return -1;
    }
  }
  return real_fseeko(stream, offset, whence);
}

SHIM_EXPORT int fseeko64(FILE *stream, int64_t offset, int whence) {
  static int (*real_fseeko64)(FILE *, int64_t, int) = NULL;

  if (whence == SEEK_SET && seekfail_capped(offset)) {
    return -1;
  }
  if (real_fseeko64 == NULL) {
    *(void **) &real_fseeko64 = dlsym(RTLD_NEXT, "fseeko64");
    if (real_fseeko64 == NULL) {
      errno = ENOSYS;
      return -1;
    }
  }
  return real_fseeko64(stream, offset, whence);
}
