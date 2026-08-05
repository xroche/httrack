/* Crash-report driver for 218_crash-nopie-frames.test: links htsbacktrace.c
   into a non-PIE program of its own, which the build tree cannot produce. */

#include "htsbacktrace.h"

static void probe_frame(void) { hts_print_backtrace(); }

int main(void) {
  hts_backtrace_init();
  probe_frame();
  return 0;
}
