/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998 Xavier Roche and other contributors

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

/* Fuzz the whole mirror engine (htscore.c's httpmirror() loop and everything
   it drags in: link extraction, the acceptance wizard, name generation, the
   back slots, the receive path, the cache and the indexes) end to end, over
   a site the fuzzer generates. A server thread on a fixed 127.0.0.1 port
   serves the generated pages to the engine, which runs through hts_main2()
   with hard bounds (-r2 depth, -c1 socket, -T timeout, -R0 retries, -s0 to
   leave robots off). The main thread wakes the server with one connect when
   the crawl returns; the server alone closes the listen socket. */
#include "fuzz.h"

#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "httrack-library.h"

#define FUZZ_MIRROR_MAXPAGES 8
#define FUZZ_MIRROR_MAXBODY 8192
#define FUZZ_MIRROR_MAXPATH 128
#define FUZZ_MIRROR_BUDGET 24
/* Fixed so cache keys and mirror paths do not learn ephemeral ports. */
#define FUZZ_MIRROR_PORT 18765

struct fuzz_mirror_page {
  char path[FUZZ_MIRROR_MAXPATH];
  char body[FUZZ_MIRROR_MAXBODY];
  size_t len;
};

static struct fuzz_mirror_page fuzz_mirror_pages[FUZZ_MIRROR_MAXPAGES];
static int fuzz_mirror_npages = 0;
static int fuzz_mirror_listen = -1;
static int fuzz_mirror_budget = FUZZ_MIRROR_BUDGET;
static volatile int fuzz_mirror_done = 0;

/* One served connection: read the request line, answer from the generated
   site, close. Everything is served as text/html; the type table did not
   add coverage. */
static void fuzz_mirror_serve_one(int c) {
  char req[1024];
  ssize_t n = recv(c, req, sizeof(req) - 1, 0);

  if (n > 0) {
    const char *p;
    char *e;
    const struct fuzz_mirror_page *hit = NULL;
    int i;

    req[n] = '\0';
    p = strncmp(req, "GET ", 4) == 0 ? req + 4 : req;
    e = strchr(p, ' ');
    if (e != NULL)
      *e = '\0';
    if (*p != '/')
      p = "/";
    for (i = 0; i < fuzz_mirror_npages; i++) {
      if (strcmp(fuzz_mirror_pages[i].path, p) == 0) {
        hit = &fuzz_mirror_pages[i];
        break;
      }
    }
    if (hit != NULL) {
      char hdr[256];
      const int hl = snprintf(
          hdr, sizeof(hdr),
          "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "
          "%zu\r\nConnection: close\r\n\r\n",
          hit->len);

      (void) !write(c, hdr, (size_t) hl);
      if (hit->len > 0)
        (void) !write(c, hit->body, hit->len);
    } else {
      static const char notfound[] =
          "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n"
          "Connection: close\r\n\r\n";

      (void) !write(c, notfound, sizeof(notfound) - 1);
    }
  }
  (void) shutdown(c, SHUT_WR);
  (void) close(c);
}

static void *fuzz_mirror_serve(void *arg) {
  (void) arg;
  while (fuzz_mirror_budget-- > 0 && !fuzz_mirror_done) {
    int c = accept(fuzz_mirror_listen, NULL, NULL);

    if (c < 0)
      break;
    if (fuzz_mirror_done) {
      (void) close(c);
      break;
    }
    fuzz_mirror_serve_one(c);
  }
  /* Only this thread closes the listen socket. */
  if (fuzz_mirror_listen >= 0) {
    (void) close(fuzz_mirror_listen);
    fuzz_mirror_listen = -1;
  }
  return NULL;
}

static char fuzz_mirror_dir[256];
static char fuzz_mirror_www[sizeof(fuzz_mirror_dir) + 8];

static void fuzz_mirror_cleanup(void) {
  fuzz_wipe_dir(fuzz_mirror_dir);
  (void) rmdir(fuzz_mirror_dir);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static int inited = 0;
  static pthread_t thread;
  char *index_html;
  size_t index_len;
  struct sockaddr_in addr;
  int one = 1;
  char url[64];
  char *argv[16];
  int argc = 0;
  httrackp *opt;
  int i;

  if (size == 0)
    return 0;
  if (!inited) {
    hts_init();
    if (!fuzz_tmpdir(fuzz_mirror_dir, sizeof(fuzz_mirror_dir), "fuzz-mirror",
                     fuzz_mirror_cleanup))
      return 0;
    snprintf(fuzz_mirror_www, sizeof(fuzz_mirror_www), "%s/www",
             fuzz_mirror_dir);
    inited = 1;
  }

  /* Wipe the previous mirror so a crash reproduces from this input alone
     (including libFuzzer's second leak-check run of the same input). */
  fuzz_wipe_dir(fuzz_mirror_www);
  (void) mkdir(fuzz_mirror_www, 0700);

  /* The site: pages[0] is /, then (path, body) pairs. The generated index
     links every path, so the crawl has somewhere to go. */
  memset(fuzz_mirror_pages, 0, sizeof(fuzz_mirror_pages));
  fuzz_mirror_npages = 1;
  index_html = fuzz_mirror_pages[0].body;
  index_len = 0;
  snprintf(fuzz_mirror_pages[0].path, sizeof(fuzz_mirror_pages[0].path), "/");
  {
    char path[FUZZ_MIRROR_MAXPATH];
    char body[FUZZ_MIRROR_MAXBODY];

    while (fuzz_mirror_npages < FUZZ_MIRROR_MAXPAGES) {
      struct fuzz_mirror_page *const e = &fuzz_mirror_pages[fuzz_mirror_npages];
      size_t blen;

      if (!fuzz_field(&data, &size, path, sizeof(path)))
        break;
      if (!fuzz_field(&data, &size, body, sizeof(body)))
        break;
      if (path[0] == '\0' || path[0] != '/')
        break;
      if (strcmp(path, "/") == 0)
        continue;
      blen = strlen(body);
      memcpy(e->body, body, blen);
      e->body[blen] = '\0';
      e->len = blen;
      snprintf(e->path, sizeof(e->path), "%s", path);
      fuzz_mirror_npages++;
    }
    for (i = 1; i < fuzz_mirror_npages; i++) {
      index_len += (size_t) snprintf(index_html + index_len,
                                     FUZZ_MIRROR_MAXBODY - index_len,
                                     "<a href=\"%s\">p%d</a><br>",
                                     fuzz_mirror_pages[i].path, i);
      if (index_len >= FUZZ_MIRROR_MAXBODY - 64)
        break;
    }
  }
  /* the leftover input trails the generated links on /: hostile index HTML */
  {
    size_t used = strlen(fuzz_mirror_pages[0].body);
    size_t avail = FUZZ_MIRROR_MAXBODY - 1 - used;
    size_t take = size < avail ? size : avail;

    if (take > 0) {
      memcpy(fuzz_mirror_pages[0].body + used, data, take);
      fuzz_mirror_pages[0].body[used + take] = '\0';
    }
  }
  fuzz_mirror_pages[0].len = strlen(fuzz_mirror_pages[0].body);

  /* The loopback server on a fixed port. */
  fuzz_mirror_listen = socket(AF_INET, SOCK_STREAM, 0);
  if (fuzz_mirror_listen < 0)
    return 0;
  (void) setsockopt(fuzz_mirror_listen, SOL_SOCKET, SO_REUSEADDR, &one,
                    sizeof(one));
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(FUZZ_MIRROR_PORT);
  if (bind(fuzz_mirror_listen, (struct sockaddr *) &addr, sizeof(addr)) != 0 ||
      listen(fuzz_mirror_listen, 8) != 0) {
    (void) close(fuzz_mirror_listen);
    fuzz_mirror_listen = -1;
    return 0;
  }
  fuzz_mirror_budget = FUZZ_MIRROR_BUDGET;
  fuzz_mirror_done = 0;
  if (pthread_create(&thread, NULL, fuzz_mirror_serve, NULL) != 0) {
    (void) close(fuzz_mirror_listen);
    fuzz_mirror_listen = -1;
    return 0;
  }

  /* The engine, bounded. Fixed -r2: the depth knob lost coverage. */
  snprintf(url, sizeof(url), "http://127.0.0.1:%d/", FUZZ_MIRROR_PORT);
  argv[argc++] = (char *) "httrack";
  argv[argc++] = (char *) "-q";
  argv[argc++] = (char *) "-r2";
  argv[argc++] = (char *) "-c1";
  argv[argc++] = (char *) "-T8";
  argv[argc++] = (char *) "-R0";
  argv[argc++] = (char *) "-s0";
  argv[argc++] = (char *) "-O";
  argv[argc++] = fuzz_mirror_www;
  argv[argc++] = url;
  argv[argc] = NULL;
  opt = hts_create_opt();
  if (opt != NULL) {
    opt->log = opt->errlog = NULL;
    (void) hts_main2(argc, argv, opt);
    hts_free_opt(opt);
  }

  /* Wake a blocking accept; only the server thread closes the listen fd. */
  fuzz_mirror_done = 1;
  {
    int poke = socket(AF_INET, SOCK_STREAM, 0);

    if (poke >= 0) {
      (void) connect(poke, (struct sockaddr *) &addr, sizeof(addr));
      (void) close(poke);
    }
  }
  pthread_join(thread, NULL);
  return 0;
}
