/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 2026 Xavier Roche and other contributors

SPDX-License-Identifier: GPL-3.0-or-later

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

Ethical use: we kindly ask that you NOT use this software to harvest email
addresses or to collect any other private information about people. Doing so
would dishonor our work and waste the many hours we have spent on it.

Please visit our Website: http://www.httrack.com
*/

/* Fuzz proxytrack's .arc reader the way `--convert` drives it: the record loop
   seeks on lengths read from the file, and every entry reaches a writer. */
#include "fuzz.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "coucal.h"
#include "proxy/store.h"

/* An .arc past this size says nothing a smaller one cannot. */
#define FUZZ_ARC_MAXSIZE (1024 * 1024)

static char arc_dir[256];
static char arc_in[sizeof(arc_dir) + sizeof("/in.arc")];
static char arc_out[sizeof(arc_dir) + sizeof("/out.arc")];

static void fuzz_arc_cleanup(void) {
  (void) unlink(arc_in);
  (void) unlink(arc_out);
  (void) rmdir(arc_dir);
}

/* proxytrack's main() installs one; without it coucal logs stats per free */
static void fuzz_arc_coucal_log(coucal_opaque arg, coucal_loglevel level,
                                const char *format, va_list args) {
  (void) arg;
  (void) level;
  (void) format;
  (void) args;
}

/* PT_GetType() picks the format from the extension, so the names end in .arc */
static int fuzz_arc_setup(void) {
  if (arc_in[0] == '\0') {
    const char *const tmp = getenv("TMPDIR");

    coucal_set_global_assert_handler(fuzz_arc_coucal_log, NULL);

    snprintf(arc_dir, sizeof(arc_dir), "%s/fuzz-arc-XXXXXX",
             tmp != NULL && *tmp != '\0' ? tmp : "/tmp");
    if (mkdtemp(arc_dir) == NULL) {
      return -1;
    }
    snprintf(arc_out, sizeof(arc_out), "%s/out.arc", arc_dir);
    snprintf(arc_in, sizeof(arc_in), "%s/in.arc", arc_dir);
    atexit(fuzz_arc_cleanup);
  }
  return 0;
}

/* PT_Enumerate() returns a NULL-terminated array of pointers into the same
   block, so touching each string is what proves the references land in it. */
static void fuzz_arc_walk(char **list) {
  static volatile size_t sink; /* keeps the reads from being optimised away */
  size_t i;

  if (list == NULL) {
    return;
  }
  for (i = 0; list[i] != NULL; i++) {
    sink += strlen(list[i]);
  }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  PT_Indexes indexes;
  char **flat = NULL, **tree = NULL, **none = NULL;
  FILE *fp;
  hts_boolean written;

  if (size > FUZZ_ARC_MAXSIZE || fuzz_arc_setup() != 0) {
    return 0;
  }
  if ((fp = fopen(arc_in, "wb")) == NULL) {
    return 0;
  }
  written = fwrite(data, 1, size, fp) == size ? HTS_TRUE : HTS_FALSE;
  if (fclose(fp) != 0 || !written) {
    return 0;
  }

  indexes = PT_New();
  if (indexes != NULL) {
    if (PT_AddIndex(indexes, arc_in) > 0) {
      /* the writer reads back every entry the loader indexed */
      (void) PT_SaveCache(indexes, arc_out);
      /* An empty prefix takes every entry, subtree=1 takes the other shape,
         and a prefix almost no input indexes takes the no-match return. */
      flat = PT_Enumerate(indexes, "", 0);
      tree = PT_Enumerate(indexes, "http://", 1);
      none = PT_Enumerate(indexes, "http://255.255.255.255./", 0);
    }
    PT_Delete(indexes);
  }
  fuzz_arc_walk(flat);
  fuzz_arc_walk(tree);
  fuzz_arc_walk(none);
  PT_Enumerate_Delete(&flat);
  PT_Enumerate_Delete(&tree);
  PT_Enumerate_Delete(&none);
  return 0;
}
