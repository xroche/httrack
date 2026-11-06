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

Please visit our Website: http://www.httrack.com
*/

/* Fuzz the login-bundle file reader (htsbundle.c). The fuzzer bytes are the
   bundle file: they go to <bundle_dir>/fuzz.hts (and fuzz2.hts, so one input
   also drives the two-bundle overlap/merge checks), the bundles are named the
   way a start URL names them (hts_bundle_take on "#httrack-bundle=<name>",
   https vs http picked from the input), and the real hts_bundle_load_all()
   parses the "# HTTrack bundle 1" header, the url/ua/header/scope key lines
   and the Netscape cookie records behind "cookies" through the bundle cookie
   filter. hts_bundle_url() and hts_bundle_identity() then walk the loaded
   state. The bundle file is a user-imported file, so its parser faces hostile
   input: memory safety here is the point. */
#include "fuzz.h"

#include "htsbauth.h"
#include "htsbundle.h"
#include "htscore.h"
#include "htsstrings.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static int inited = 0;
  httrackp *opt;
  t_cookie jar;
  char dir[] = "/tmp/fuzz-bundle-XXXXXX";
  char path[256];
  char url[HTS_URLMAXSIZE * 2];
  char err[1024] = "";
  FILE *fp;
  size_t i;

  if (size == 0)
    return 0;
  if (!inited) {
    hts_init();
    inited = 1;
  }
  if (mkdtemp(dir) == NULL)
    return 0;

  opt = hts_create_opt();
  opt->log = opt->errlog = NULL;

  /* Bundles are read from bundle_dir when set, else from path_log. */
  StringCopy(opt->bundle_dir, dir);

  /* The engine's jar: bundle cookies are merged into it (max_len is what
     httpmirror gives the jar). */
  memset(&jar, 0, sizeof(jar));
  jar.max_len = COOKIE_MAX_JAR;
  opt->cookie = &jar;

  /* The fuzzer bytes are the bundle file, twice under two names so a single
     input reaches the scope-overlap and identity-merge checks. */
  snprintf(path, sizeof(path), "%s/fuzz.hts", dir);
  fp = fopen(path, "wb");
  if (fp != NULL) {
    fwrite(data, 1, size, fp);
    fclose(fp);
  }
  snprintf(path, sizeof(path), "%s/fuzz2.hts", dir);
  fp = fopen(path, "wb");
  if (fp != NULL) {
    fwrite(data, 1, size, fp);
    fclose(fp);
  }

  /* Name the bundles the way a start URL does; the https naming URL makes the
     merged headers https-only when the input contains an https url line. */
  strcpybuff(url, (data[0] & 1) ? "https://fuzz.example/members/"
                                : "http://fuzz.example/members/");
  (void) hts_bundle_take(opt, url);
  strcpybuff(url, "https://fuzz.example/other#httrack-bundle=fuzz2");
  (void) hts_bundle_take(opt, url);

  (void) hts_bundle_load_all(opt, NULL, err, sizeof(err));

  for (i = 0; hts_bundle_url(opt, i) != NULL; i++)
    continue;
  {
    htsrequest req;

    memset(&req, 0, sizeof(req));
    hts_bundle_identity(opt, "https://fuzz.example/x", &req);
    hts_bundle_identity(opt, "http://other.example/x", &req);
  }
  (void) hts_bundle_has_control((const char *) data, size);

  hts_bundle_free(opt);
  opt->cookie = NULL;
  hts_free_opt(opt);

  snprintf(path, sizeof(path), "%s/fuzz.hts", dir);
  (void) unlink(path);
  snprintf(path, sizeof(path), "%s/fuzz2.hts", dir);
  (void) unlink(path);
  (void) rmdir(dir);
  return 0;
}
