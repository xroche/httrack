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

/* Fuzz the SOCKS5 client handshake (htsproxy.c) against a hostile proxy:
   socks5_handshake_scripted() replays the proxy's byte stream from a buffer.
   The input is "origin\nproxy\nreply": the first two lines are the origin
   address and the proxy URL, so the port parser and the RFC 1929 credential
   decoder run too, and everything after the second newline is what the proxy
   sends back. Without two newlines the whole input is the reply. */
#include "fuzz.h"

#include <stdlib.h>

#include "htscore.h"
#include "htsproxy.h"

static unsigned long socks5_sink;

/* Is the CONNECT frame, behind the greeting and the optional auth frame, the
   last thing sent? */
static hts_boolean connect_frame_ends_script(const socks5_test_io *io) {
  size_t c;

  if (io->sent_len < 2)
    return HTS_FALSE;
  c = 2 + (size_t) io->sent[1];
  if (c < io->sent_len && io->sent[c] == 0x01) {
    if (c + 2 > io->sent_len)
      return HTS_FALSE;
    c += 2 + io->sent[c + 1];
    if (c >= io->sent_len)
      return HTS_FALSE;
    c += 1 + io->sent[c];
  }
  return c + 5 <= io->sent_len &&
         memcmp(io->sent + c, "\x05\x01\x00\x03", 4) == 0 &&
         c + 7 + io->sent[c + 4] == io->sent_len;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static httrackp *opt = NULL;
  socks5_test_io io;
  char *buf;
  const char *adr = "origin.test";
  const char *proxy = "socks5://127.0.0.1";
  const unsigned char *reply;
  char *nl1, *nl2;
  int ok;

  if (opt == NULL) {
    hts_init();
    opt = hts_create_opt();
    opt->log = opt->errlog = NULL;
  }

  buf = fuzz_strdup(data, size);
  reply = (const unsigned char *) buf;
  nl1 = memchr(buf, '\n', size);
  nl2 = nl1 != NULL ? memchr(nl1 + 1, '\n', size - (size_t) (nl1 + 1 - buf))
                    : NULL;
  if (nl2 != NULL) {
    *nl1 = '\0';
    *nl2 = '\0';
    adr = buf;
    proxy = nl1 + 1;
    reply = (const unsigned char *) nl2 + 1;
  }

  memset(&io, 0, sizeof(io));
  io.reply = reply;
  io.reply_len = size - (size_t) ((const char *) reply - buf);
  ok = socks5_handshake_scripted(opt, adr, proxy, &io);

  /* the reader never runs past the script, the writer never past sent[] */
  if (io.consumed > io.reply_len || io.sent_len > sizeof(io.sent) ||
      memchr(io.msg, '\0', sizeof(io.msg)) == NULL)
    abort();
  /* success means the frames went out whole: greeting, the RFC 1929 one if
     the proxy picked it, then CONNECT by name and nothing after it */
  if (ok && !connect_frame_ends_script(&io))
    abort();
  socks5_sink += (unsigned long) ok + io.consumed + io.sent_len;
  freet(buf);
  return 0;
}
