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

/* Fuzz the HTTP receive path: one back_wait() slot waits for headers on a
   socketpair whose peer holds the input and is closed, so every read ends in
   data or EOF and nothing waits on a timer. Two leading bytes pick the request
   and the options; the rest is the server's response, status line first. */
#include "fuzz.h"

#include "httrack-library.h"
#include "htscore.h"
#include "htsback.h"
#include "coucal.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

/* the peer write must fit the socket buffer, or it blocks */
#define FUZZ_RESP_MAXSIZE (64 * 1024)

/* option byte */
#define OPT_TEST 0x01     /* a test GET (the crawler probing a link) */
#define OPT_HEAD 0x02     /* a HEAD request */
#define OPT_DISK 0x04     /* save to a file, which decoding needs */
#define OPT_TOLERANT 0x08 /* -%B: accept a bad length */
#define OPT_HTTP10 0x10   /* no keep-alive */
#define OPT_UPDATE 0x20   /* an update of a file already mirrored */
#define OPT_SIZEHACK 0x40 /* -%s */
#define OPT_PARTIAL 0x80  /* a partial file on disk, resumed with Range */

static char resp_dir[256];

/* remove what dir holds, keeping dir itself */
static void resp_wipe_dir(const char *dir) {
  DIR *const d = opendir(dir);
  struct dirent *e;

  if (d == NULL)
    return;
  while ((e = readdir(d)) != NULL) {
    char path[sizeof(resp_dir) + 512];
    struct stat st;

    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
      continue;
    snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
    if (lstat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
      resp_wipe_dir(path);
      (void) rmdir(path);
    } else {
      (void) unlink(path);
    }
  }
  closedir(d);
}

static void resp_wipe(void) { resp_wipe_dir(resp_dir); }

static void resp_cleanup(void) {
  resp_wipe();
  (void) rmdir(resp_dir);
}

static int resp_setup(void) {
  const char *const tmp = getenv("TMPDIR");

  snprintf(resp_dir, sizeof(resp_dir), "%s/fuzz-response-XXXXXX",
           tmp != NULL && *tmp != '\0' ? tmp : "/tmp");
  if (mkdtemp(resp_dir) == NULL)
    return -1;
  atexit(resp_cleanup);
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static int inited = 0;

  httrackp *opt;
  cache_back cache;
  struct_back *sback;
  lien_back *back;
  htsblk r;
  int sv[2];
  unsigned int flags, extra;
  const char *fil;
  char save[sizeof(resp_dir) + 32];
  size_t waits;
  char **filters = NULL;
  int filptr = 0;

  if (size < 2 || size > FUZZ_RESP_MAXSIZE)
    return 0;
  flags = data[0];
  extra = data[1];
  data += 2, size -= 2;

  if (!inited) {
    hts_init();
    if (resp_setup() != 0)
      abort();
    inited = 1;
  }
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    abort();
  if (size > 0 && write(sv[1], data, size) != (ssize_t) size)
    abort();
  close(sv[1]);

  opt = hts_create_opt();
  opt->log = opt->errlog = NULL;
  opt->maxrate = 0; /* a rate cap sleeps */
  opt->timeout = 0;
  opt->tolerant = (flags & OPT_TOLERANT) ? HTS_TRUE : HTS_FALSE;
  opt->sizehack = (flags & OPT_SIZEHACK) ? HTS_TRUE : HTS_FALSE;
  opt->nokeepalive = (flags & OPT_HTTP10) ? HTS_TRUE : HTS_FALSE;
  /* the resume path wants a cache, and its reference file lands in path_log */
  opt->cache = (flags & OPT_PARTIAL) ? 1 : 0;
  StringCopy(opt->path_html, resp_dir);
  StringCat(opt->path_html, "/");
  StringCopy(opt->path_html_utf8, StringBuff(opt->path_html));
  StringCopy(opt->path_log, StringBuff(opt->path_html));

  /* no filter, but the size and type checks walk the list */
  filters_init(&filters, opt->maxfilter, 0);
  opt->filters.filters = &filters;
  opt->filters.filptr = &filptr;

  memset(&cache, 0, sizeof(cache)); /* no cache to read or write */
  cache.hashtable = coucal_new(0);

  /* a second slot takes the connection if keep-alive keeps it */
  sback = back_new(opt, 2);
  if (sback == NULL)
    abort();
  back = sback->lnk;

  /* the extension decides what is hypertext when the type does not */
  fil = (extra & 0x80) ? "/file.bin" : "/index.html";
  save[0] = '\0';
  if (flags & (OPT_DISK | OPT_PARTIAL))
    snprintf(save, sizeof(save), "%s/example.com%s", resp_dir, fil);

  hts_init_htsblk(&r);
  r.soc = sv[0];
  r.totalsize = -1;
  back_slot_attach(sback, 0, STATUS_WAIT_HEADERS, &r, "example.com", fil);
  strcpybuff(back[0].url_sav, save);
  back[0].maxfile_html = opt->maxfile_html;
  back[0].maxfile_nonhtml = opt->maxfile_nonhtml;
  back[0].http11 = (flags & OPT_HTTP10) ? 0 : 1;
  back[0].ka_time_start = time_local();
  back[0].testmode = (flags & (OPT_TEST | OPT_HEAD)) ? 1 : 0;
  back[0].head_request = (flags & OPT_HEAD) ? 1 : (flags & OPT_TEST) ? 2 : 0;
  back[0].is_update = (flags & OPT_UPDATE) ? 1 : 0;
  if ((flags & OPT_PARTIAL) && save[0] != '\0') {
    /* what an interrupted run left: the range request resumes after it */
    const size_t have = extra & 0x7f;
    FILE *fp;

    if (structcheck_utf8(save) == 0 && (fp = fopen(save, "wb")) != NULL) {
      size_t k;

      for (k = 0; k < have; k++)
        fputc('A' + (int) (k % 26), fp);
      fclose(fp);
      back[0].range_req_size = (LLint) have;
    }
  }

  /* each wait reads at least one byte or ends the slot */
  for (waits = 0; back_status(&back[0]) > STATUS_READY; waits++) {
    if (waits > size + 64) {
      fprintf(stderr, "fuzz-response: slot stuck in status %d\n",
              back_status(&back[0]));
      abort();
    }
    back_wait(sback, opt, &cache, 0);
  }
  /* as the crawler does once it knows the type */
  if (back_status(&back[0]) == STATUS_READY && !back[0].finalized)
    back_finalize(opt, &cache, sback, 0);

  back_delete_all(opt, &cache, sback);
  back_free(&sback);
  coucal_delete(&cache.hashtable);
  if (filters != NULL) {
    freet(filters[0]);
    freet(filters);
  }
  hts_free_opt(opt);
  if (save[0] != '\0')
    resp_wipe();
  return 0;
}
