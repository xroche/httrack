/*
  The bundle's main executable: hands off to the webhttrack script beside it. A
  Mach-O, not the shell stub it replaces, because Apple treats a script here as
  a bundle resource rather than code, and TCC will not attribute a prompt to one
  (#901).
*/

#include <libgen.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PAYLOAD "/../Resources/bin/webhttrack"

int main(int argc, char *argv[]) {
  char self[PATH_MAX], resolved[PATH_MAX], payload[PATH_MAX];
  uint32_t size = (uint32_t) sizeof(self);

  (void) argc;
  /* A symlink or a translocated bundle both land here, so resolve first. */
  if (_NSGetExecutablePath(self, &size) != 0 ||
      realpath(self, resolved) == NULL) {
    perror("HTTrack: cannot locate the bundle");
    return 1;
  }
  /* dirname() may write through its argument, so it never gets a literal. */
  if (snprintf(payload, sizeof(payload), "%s" PAYLOAD, dirname(resolved)) >=
      (int) sizeof(payload)) {
    fprintf(stderr, "HTTrack: bundle path too long\n");
    return 1;
  }
  /* webhttrack locates its payload from $0, so it must see its own path. */
  argv[0] = payload;
  execv(payload, argv);
  perror(payload);
  return 1;
}
