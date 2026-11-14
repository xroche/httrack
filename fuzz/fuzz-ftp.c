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

/* Fuzz the FTP control channel (htsftp.c): get_ftp_line() reads the server's
   replies off a socketpair, and each reply goes through the PASV, EPSV, SIZE
   and MDTM parsers. The same bytes, as a URL, go through the user, host and
   command splitters. Byte 0 sets the reply buffer size; the rest is the input.
 */
#include "fuzz.h"

#include "htscore.h"
#include "htsftp.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

/* fits the socketpair buffer, so the whole reply is written before reading */
#define FUZZ_FTP_MAXSIZE (16 * 1024)

static void ftp_fail(const char *what, const char *line) {
  fprintf(stderr, "fuzz-ftp: %s: \"%s\"\n", what, line);
  abort();
}

/* every parser on one reply line, checking what each one promises */
static void ftp_parse_reply(char *line) {
  char ip[16]; /* "255.255.255.255" */
  int port = -1;
  LLint fsize = -1;
  struct tm tm;

  if (ftp_parse_pasv(line, ip, sizeof(ip), &port) &&
      (port < 1 || port > 65535 || strlen(ip) < 7))
    ftp_fail("pasv", line);
  port = -1;
  if (ftp_parse_epsv(line, &port) && (port < 1 || port > 65535))
    ftp_fail("epsv", line);
  if (ftp_parse_size(line, &fsize) && fsize < 0)
    ftp_fail("size", line);
  if (ftp_parse_mdtm(line, &tm) &&
      (tm.tm_mon < 0 || tm.tm_mon > 11 || tm.tm_mday < 1 || tm.tm_mday > 31))
    ftp_fail("mdtm", line);
  (void) linejmp(line);
}

/* the URL splitting run_launch_ftp() does before it connects */
static void ftp_split_url(char *url) {
  char user[256], pass[256];
  char host[HTS_URLMAXSIZE * 2];
  char err[256];
  char line[FTP_LINE_SIZE];
  char *adr = ftp_jump_authority(url);
  char *const real_adr = adr;
  char *path;
  int port = 21;

  if ((adr = jump_identification(real_adr)) != real_adr &&
      ftp_split_userpass_buf(real_adr, adr, user, pass) &&
      (strlen(user) >= sizeof(user) || strlen(pass) >= sizeof(pass)))
    ftp_fail("userpass", real_adr);
  path = strchr(adr, '/');
  if (path != NULL)
    *path++ = '\0';
  if (ftp_split_hostport(adr, host, sizeof(host), &port, err, sizeof(err)) &&
      (port < 1 || port > 65535))
    ftp_fail("hostport", adr);
  if (path != NULL && ftp_command_line(line, "RETR", path) &&
      strncmp(line, "RETR ", 5) != 0)
    ftp_fail("command", path);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static int inited = 0;
  T_SOC sv[2];
  size_t line_size, calls;
  char *line, *url;
  int pending;

  if (size < 1 || size > FUZZ_FTP_MAXSIZE)
    return 0;
  /* odd: the size every caller passes; even: a short one, to exercise clips */
  line_size = (data[0] & 1) ? FTP_LINE_SIZE : 1 + (data[0] >> 1);
  data++, size--;

  if (!inited) {
    hts_init();
    inited = 1;
  }

  url = fuzz_strdup(data, size);
  ftp_split_url(url);
  freet(url);

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    abort();
  if (size > 0 && send(sv[1], data, size, MSG_NOSIGNAL) != (ssize_t) size)
    abort();
  close(sv[1]);

  /* heap, so ASan bounds the copy into the caller's buffer */
  line = malloct(line_size);
  /* each call reads at least one byte, so the bound is the input size */
  for (calls = 0; ioctl(sv[0], FIONREAD, &pending) == 0 && pending > 0;
       calls++) {
    if (calls > size)
      ftp_fail("get_ftp_line read nothing", "");
    line[0] = '\0';
    (void) get_ftp_line(NULL, sv[0], line, line_size, 1, NULL);
    if (strlen(line) >= line_size)
      ftp_fail("line overflows its buffer", "");
    ftp_parse_reply(line);
  }
  freet(line);
  close(sv[0]);
  return 0;
}
