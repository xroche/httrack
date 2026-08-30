/* A consumer's view of the structs HTS_INET6 and HTS_USEOPENSSL shape. 381
   compiles this against the installed headers alone and diffs it against
   "httrack -#test=pubheaders", which the library prints from its own build. */
#include <stddef.h>
#include <stdio.h>

#include <httrack/httrack-library.h>
#include <httrack/htsnet.h>
#include <httrack/htsopt.h>

#define PUBH_SIZE(t) printf("sizeof %s = %lu\n", #t, (unsigned long) sizeof(t))
#define PUBH_OFF(t, f)                                                         \
  printf("offsetof %s.%s = %lu\n", #t, #f, (unsigned long) offsetof(t, f))

int main(void) {
  PUBH_SIZE(SOCaddr);
  PUBH_SIZE(INTsys);
  PUBH_SIZE(htsblk);
  PUBH_OFF(htsblk, soc);
  PUBH_OFF(htsblk, address);
  PUBH_OFF(htsblk, fp);
  PUBH_OFF(htsblk, lastmodified);
  PUBH_OFF(htsblk, etag);
  PUBH_OFF(htsblk, debugid);
  PUBH_SIZE(httrackp);
  printf("HTS_INET6 = %d\n", (int) HTS_INET6);
  printf("HTS_USEOPENSSL = %d\n", (int) HTS_USEOPENSSL);
  return 0;
}
