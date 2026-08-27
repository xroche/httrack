/* Renders each argv value through hts_print_num() for 372_crash-signal-number.
   Prints "<returned length>:<text>", which the test grades against printf. */

#include "htsbacktrace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  int i;

  for (i = 1; i < argc; i++) {
    char buf[64];
    char *end;
    const long value = strtol(argv[i], &end, 10);
    unsigned int len;

    if (*end != '\0') {
      fprintf(stderr, "printnumprobe: '%s' is not a number\n", argv[i]);
      return 1;
    }
    /* poisoned, so a stray terminator one past the end is visible */
    memset(buf, 'X', sizeof(buf));
    len = hts_print_num(buf, (int) value);
    if (len >= sizeof(buf) - 2) {
      fprintf(stderr, "printnumprobe: '%s' wrote %u bytes\n", argv[i], len);
      return 1;
    }
    if (buf[len] != '\0' || buf[len + 1] != 'X') {
      fprintf(stderr, "printnumprobe: '%s' wrote past its %u bytes\n", argv[i],
              len);
      return 1;
    }
    printf("%u:%s\n", len, buf);
  }
  return 0;
}
