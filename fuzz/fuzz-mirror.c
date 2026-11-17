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
   a site the fuzzer generates. A server thread on 127.0.0.1 serves the
   generated pages to the engine, which runs through hts_main2() with hard
   bounds (-r depth, -c1 socket, -T timeout, -R0 retries, -s0 robots); when
   the serve budget is spent the listener closes, so every run terminates. */
#include "fuzz.h"

#include <dirent.h>
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

struct fuzz_mirror_page {
  char path[FUZZ_MIRROR_MAXPATH];
  char body[FUZZ_MIRROR_MAXBODY];
  size_t len;
};

static struct fuzz_mirror_page fuzz_mirror_pages[FUZZ_MIRROR_MAXPAGES];
static int fuzz_mirror_npages = 0;
static int fuzz_mirror_listen = -1;
static int fuzz_mirror_budget = FUZZ_MIRROR_BUDGET;

static const char *fuzz_mirror_type(const char *path) {
  const size_t len = strlen(path);

  if (len >= 4 && strcmp(path + len - 4, ".css") == 0)
    return "text/css";
  if (len >= 3 && strcmp(path + len - 3, ".js") == 0)
    return "application/javascript";
  if (len >= 4 && strcmp(path + len - 4, ".gif") == 0)
    return "image/gif";
  if (len >= 4 && strcmp(path + len - 4, ".png") == 0)
    return "image/png";
  if (len >= 4 && strcmp(path + len - 4, ".jpg") == 0)
    return "image/jpeg";
  return "text/html";
}

/* One served connection: read the request line, answer from the generated
   site, close. */
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
      const int hl =
          snprintf(hdr, sizeof(hdr),
                   "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: "
                   "%zu\r\nConnection: close\r\n\r\n",
                   fuzz_mirror_type(hit->path), hit->len);

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
  while (fuzz_mirror_budget-- > 0) {
    int c = accept(fuzz_mirror_listen, NULL, NULL);

    if (c < 0)
      break;
    fuzz_mirror_serve_one(c);
  }
  /* budget spent: no more accepts, and further connects are refused */
  (void) shutdown(fuzz_mirror_listen, SHUT_RDWR);
  (void) close(fuzz_mirror_listen);
  fuzz_mirror_listen = -1;
  return NULL;
}

/* Copy the next NUL-separated field into dst (bounded); HTS_FALSE at end. */
static hts_boolean fuzz_mirror_field(const uint8_t **data, size_t *size,
                                     char *dst, size_t dst_size) {
  const uint8_t *nul;
  size_t len;

  if (*size == 0)
    return HTS_FALSE;
  nul = (const uint8_t *) memchr(*data, 0, *size);
  len = nul != NULL ? (size_t) (nul - *data) : *size;
  if (len >= dst_size)
    len = dst_size - 1;
  memcpy(dst, *data, len);
  dst[len] = '\0';
  *data += len;
  *size -= len;
  if (*size > 0) {
    *data += 1;
    *size -= 1;
  }
  return HTS_TRUE;
}

static char fuzz_mirror_dir[256];
static char fuzz_mirror_www[sizeof(fuzz_mirror_dir) + 8];

/* Delete the mirror output tree, bounded. */
static void fuzz_mirror_cleanup_tree(const char *dir, int depth) {
  char path[sizeof(fuzz_mirror_dir) + 64];
  DIR *d;
  struct dirent *e;
  int count = 0;

  if (depth > 3)
    return;
  d = opendir(dir);
  if (d == NULL)
    return;
  while ((e = readdir(d)) != NULL && count++ < 200) {
    if (e->d_name[0] == '.')
      continue;
    snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
    if (unlink(path) == 0)
      continue;
    fuzz_mirror_cleanup_tree(path, depth + 1);
  }
  closedir(d);
  (void) rmdir(dir);
}

static void fuzz_mirror_cleanup(void) {
  fuzz_mirror_cleanup_tree(fuzz_mirror_dir, 0);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static int inited = 0;
  static pthread_t thread;
  const int depth = size > 0 && (data[0] & 1) ? 2 : 1;
  char *index_html;
  size_t index_len;
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  int port;
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
  data++, size--;

  /* The site: pages[0] is /, then (path, body) pairs. The generated index
     links every path, so the crawl has somewhere to go. */
  memset(fuzz_mirror_pages, 0, sizeof(fuzz_mirror_pages));
  fuzz_mirror_npages = 1;
  index_html = fuzz_mirror_pages[0].body;
  index_len = 0;
  snprintf(fuzz_mirror_pages[0].path,
           sizeof(fuzz_mirror_pages[0].path), "/");
  {
    char path[FUZZ_MIRROR_MAXPATH];
    char body[FUZZ_MIRROR_MAXBODY];

    while (fuzz_mirror_npages < FUZZ_MIRROR_MAXPAGES) {
      struct fuzz_mirror_page *const e = &fuzz_mirror_pages[fuzz_mirror_npages];
      size_t blen;

      if (!fuzz_mirror_field(&data, &size, path, sizeof(path)))
        break;
      if (!fuzz_mirror_field(&data, &size, body, sizeof(body)))
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

  /* The loopback server. */
  fuzz_mirror_listen = socket(AF_INET, SOCK_STREAM, 0);
  if (fuzz_mirror_listen < 0)
    return 0;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if (bind(fuzz_mirror_listen, (struct sockaddr *) &addr, sizeof(addr)) != 0 ||
      listen(fuzz_mirror_listen, 8) != 0 ||
      getsockname(fuzz_mirror_listen, (struct sockaddr *) &addr, &addrlen) !=
          0) {
    (void) close(fuzz_mirror_listen);
    fuzz_mirror_listen = -1;
    return 0;
  }
  port = ntohs(addr.sin_port);
  fuzz_mirror_budget = FUZZ_MIRROR_BUDGET;
  if (pthread_create(&thread, NULL, fuzz_mirror_serve, NULL) != 0) {
    (void) close(fuzz_mirror_listen);
    fuzz_mirror_listen = -1;
    return 0;
  }

  /* The engine, bounded. */
  snprintf(url, sizeof(url), "http://127.0.0.1:%d/", port);
  argv[argc++] = (char *) "httrack";
  argv[argc++] = (char *) "-q";
  argv[argc++] = depth == 2 ? (char *) "-r2" : (char *) "-r1";
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
    (void) hts_main2(argc, argv, opt);
    hts_free_opt(opt);
  }

  /* The budget may not have been spent; close whatever is left so a next
     run starts clean. */
  if (fuzz_mirror_listen >= 0) {
    (void) shutdown(fuzz_mirror_listen, SHUT_RDWR);
    (void) close(fuzz_mirror_listen);
    fuzz_mirror_listen = -1;
  }
  pthread_join(thread, NULL);
  return 0;
}
