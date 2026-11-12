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

/* ------------------------------------------------------------ */
/* File: cookie jar file writer: durable write, backup, replace */
/* ------------------------------------------------------------ */

#include "htscookiefile.h"

#include <errno.h>
#include <string.h>

#include "htsglobal.h"
#include "htslib.h"
#include "htslines.h"
#include "htscore.h"
#include "htsbundle.h"
#include "htsio.h"
#include "htscookie_selftest.h"
#ifdef _WIN32
#include <io.h> /* _commit */
#endif

/* A cookies.txt line marks HttpOnly by this prefix. */
#define COOKIE_HTTPONLY_PREFIX "#HttpOnly_"

/* Flush fp to the disk and close it. Did every step succeed? */
hts_boolean cookie_close_synced(FILE *fp) {
  // a crash after the rename must not leave an empty file behind it
  hts_boolean ok = fflush(fp) == 0;

#ifdef _WIN32
  ok = ok && _commit(_fileno(fp)) == 0;
#else
  // EINVAL: a device such as /dev/stdout cannot be synced
  ok = ok && (fsync(fileno(fp)) == 0 || errno == EINVAL);
#endif
  // Close even after a failure, so the stream never leaks
  return fclose(fp) == 0 && ok;
}

int cookie_save_fault_selftest = 0;

/* See htsbundle.h. */
void cookie_records_text(const t_cookie *cookie, String *out) {
  char BIGSTK line[8192];
  const char *a = cookie->data;

  while (*a != '\0') {
    char *extra = line;
    const char *rec = line;
    int i;

    a += binput(a, line, COOKIE_MAX_LINE);
    // stop on the seventh TAB, so the cut takes it with the eighth field
    for (i = 0; i < 7 && extra != NULL; i++) {
      extra = strchr(extra, '\t');
      if (extra != NULL && i < 6)
        extra++;
    }
    // the eighth field is ours: the file form marks HttpOnly by prefix
    if (extra != NULL) {
      *extra = '\0';
      StringCat(*out, COOKIE_HTTPONLY_PREFIX);
    }
    // a Netscape jar dots the domain exactly when field 1 is TRUE
    if (rec[0] == '.')
      rec++;
    if (!cookie_is_host_only(line))
      StringCat(*out, ".");
    StringCat(*out, rec);
    StringCat(*out, LF);
  }
}

/* Write the jar to name, or only a header if it is empty. Return 0 on
   success. */
int cookie_write(t_cookie *cookie, const char *name) {
  FILE *fp = hts_fopen_private_new(name, "wb");

  if (fp) {
    String text = STRING_EMPTY;
    hts_boolean ok = HTS_TRUE;

    // Python's MozillaCookieJar refuses a file without this first line
    if (fprintf(fp, "# Netscape HTTP Cookie File" LF
                    "# Written by HTTrack Website Copier" LF) < 0 ||
        (cookie_save_fault_selftest & COOKIE_FAULT_WRITE))
      ok = HTS_FALSE;
    cookie_records_text(cookie, &text);
    if (StringNotEmpty(text) && fwrite(StringBuff(text), 1, StringLength(text),
                                       fp) != StringLength(text))
      ok = HTS_FALSE;
    StringFree(text);
    // the close reports a write the buffer held back
    if (!cookie_close_synced(fp) ||
        (cookie_save_fault_selftest & COOKIE_FAULT_CLOSE))
      ok = HTS_FALSE;
    return ok ? 0 : -1;
  }
  return -1;
}

/* Copy src to dst, which the caller removes on failure. The copy is
   owner-only on Unix. */
static hts_boolean cookie_copy(const char *src, const char *dst) {
  const int fault = cookie_save_fault_selftest;
  char buf[8192];
  FILE *const in = FOPEN(src, "rb");
  FILE *const out = in != NULL ? hts_fopen_private_new(dst, "wb") : NULL;
  hts_boolean ok = out != NULL;
  size_t n;

  while (ok && (n = fread(buf, 1, sizeof(buf), in)) > 0)
    ok =
        fwrite(buf, 1, (fault & COOKIE_FAULT_COPY_WRITE) ? n / 2 : n, out) == n;
  if (in != NULL && (ferror(in) || (fault & COOKIE_FAULT_COPY_READ)))
    ok = HTS_FALSE;
  if (in != NULL)
    fclose(in);
  if (out != NULL &&
      (!cookie_close_synced(out) || (fault & COOKIE_FAULT_COPY_CLOSE)))
    ok = HTS_FALSE;
  return ok;
}

/* Move src over dst in one step. Did it succeed? */
static hts_boolean cookie_move(const char *src, const char *dst) {
#ifdef _WIN32
  LPWSTR wsrc = hts_pathToUCS2(src);
  LPWSTR wdst = hts_pathToUCS2(dst);
  hts_boolean ok = HTS_FALSE;
  int tries;

  // dst is refused while another writer is replacing it, so wait that out
  for (tries = 0; wsrc != NULL && wdst != NULL && tries < 20; tries++) {
    DWORD err;

    if (MoveFileExW(wsrc, wdst,
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) {
      ok = HTS_TRUE;
      break;
    }
    err = GetLastError();
    if (err != ERROR_ACCESS_DENIED && err != ERROR_SHARING_VIOLATION)
      break;
    Sleep(10);
  }
  freet(wsrc);
  freet(wdst);
  if (!ok)
    errno = EACCES; // the log reads errno, not GetLastError()
  return ok;
#else
  return RENAME(src, dst) == 0;
#endif
}

#ifndef _WIN32
/* Flush the directory holding file, so that a rename in it survives a crash.
   A failure is ignored, because some filesystems cannot sync a directory. */
static void cookie_sync_dir(const char *file) {
  char dir[CATBUFF_SIZE];
  char *slash;
  int fd;

  if (!slprintfbuff(dir, sizeof(dir), "%s", file))
    return;
  slash = strrchr(dir, '/');
  if (slash == NULL)
    strcpybuff(dir, ".");
  else
    slash[slash == dir ? 1 : 0] = '\0';
  fd = open(dir, O_RDONLY);
  if (fd != -1) {
    (void) fsync(fd);
    close(fd);
  }
}
#endif

/* Name in dst a temporary beside path that no other run or thread writes. */
hts_boolean cookie_tmpname(char *dst, size_t size, const char *path) {
  static volatile int calls = 0;

  return slprintfbuff(dst, size, "%s.%lu.%u.tmp", path, back_self_pid(),
                      (unsigned int) hts_increment_int(&calls));
}

/* Make bak hold what file holds, leaving file in place. The old bak stays
   until the new one is complete. */
hts_boolean cookie_backup(const char *file, const char *bak) {
  char next[CATBUFF_SIZE];
  hts_boolean ok;
  int err;

  if (!cookie_tmpname(next, sizeof(next), bak))
    return HTS_FALSE;
  (void) UNLINK(next);
#ifndef _WIN32
  if (!(cookie_save_fault_selftest & COOKIE_FAULT_NOLINK) &&
      link(file, next) == 0)
    ok = HTS_TRUE;
  else
#endif
    // Windows, or a filesystem without hard links
    ok = cookie_copy(file, next);
  ok = ok && cookie_move(next, bak);
  err = errno;
  // a failed copy, or a move onto another link to the same file, leaves next
  (void) UNLINK(next);
  errno = err;
  return ok;
}

/* Move tmp over file in one step, so that file holds the old or the new jar
   whatever fails. */
hts_boolean cookie_replace(const char *tmp, const char *file) {
  if (cookie_save_fault_selftest & COOKIE_FAULT_REPLACE)
    return HTS_FALSE;
  if (!cookie_move(tmp, file))
    return HTS_FALSE;
#ifndef _WIN32
  cookie_sync_dir(file);
#endif
  return HTS_TRUE;
}

/* See htsbundle.h. */
hts_boolean cookie_write_replace(const char *file, const char *data,
                                 size_t len) {
  char tmp[CATBUFF_SIZE];
  hts_boolean ok;
  FILE *fp;
  int err;

  if (!cookie_tmpname(tmp, sizeof(tmp), file)) {
    errno = ENAMETOOLONG;
    return HTS_FALSE;
  }
  (void) UNLINK(tmp);
  fp = hts_fopen_private_new(tmp, "wb");
  if (fp == NULL)
    return HTS_FALSE;
  ok = hts_fwrite_exact(data, len, fp);
  ok = cookie_close_synced(fp) && ok;
  ok = ok && cookie_replace(tmp, file);
  err = errno;
  if (!ok)
    (void) UNLINK(tmp);
  errno = err;
  return ok;
}
