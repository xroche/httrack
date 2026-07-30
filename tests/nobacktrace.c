/* backtrace() finding nothing is what a build without unwind tables does
   (armhf emits no .ARM.exidx by default), and the frameless crash report that
   follows is what 143_engine-backtrace-empty.test pins. */

#define _GNU_SOURCE

/* The tree builds with -fvisibility=hidden, which would hide the interposer. */
#define SHIM_EXPORT __attribute__((visibility("default")))

SHIM_EXPORT int backtrace(void **buffer, int size);

SHIM_EXPORT int backtrace(void **buffer, int size) {
  (void) buffer;
  (void) size;
  return 0;
}
