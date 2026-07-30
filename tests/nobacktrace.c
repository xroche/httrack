/* Forces the empty backtrace() of an unwind-tableless build, for
   143_engine-backtrace-empty.test. */

#define _GNU_SOURCE

/* The tree builds with -fvisibility=hidden, which would hide the interposer. */
#define SHIM_EXPORT __attribute__((visibility("default")))

SHIM_EXPORT int backtrace(void **buffer, int size);

SHIM_EXPORT int backtrace(void **buffer, int size) {
  (void) buffer;
  (void) size;
  return 0;
}
