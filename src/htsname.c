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

/* ------------------------------------------------------------ */
/* File: httrack.c subroutines:                                 */
/*       savename routine (compute output filename)             */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htscore.h"
#include "htsname.h"
#include "md5.h"
#include "htsmd5.h"
#include "htstools.h"
#include "htscharset.h"
#include "htsencoding.h"
#include "htsescape.h"
#include "htssniff.h"
#include "htscodec.h"
#include "htszlib.h"
#include "htsmirror.h"
#include <limits.h>

#define MAX_EXT_LEN 12 /* longest tail kept across a cut, sans the dot */
#define DELAYED_ID_MAX_DIGITS 8 /* a delayedId in hex */

/* Clipped to the whole buffer: the length shortener downstream still has to
   see this name's extension to reserve it (#852). */
#define ADD_STANDARD_PATH                                                      \
  { /* add path */                                                             \
    char BIGSTK buff[HTS_URLMAXSIZE * 4];                                      \
    size_t len;                                                                \
    strcpybuff(buff, afs->save);                                               \
    strncatbuff(buff, start_pos, nom_pos - start_pos);                         \
    len = strlen(buff);                                                        \
    /* too long: shorten the longest directories, never drop one */            \
    if (len >= sizeof(afs->save))                                              \
      save_trim_dirs(buff, len, len - (sizeof(afs->save) - 1));                \
    afs->save[0] = '\0';                                                       \
    url_savename_addstr(afs->save, sizeof(afs->save), buff);                   \
  }

#define ADD_STANDARD_NAME(shortname)                                           \
  { /* add name */                                                             \
    char BIGSTK buff[HTS_URLMAXSIZE * 2];                                      \
    char BIGSTK tail[HTS_URLMAXSIZE * 2];                                      \
    size_t len, need;                                                          \
    standard_name(buff, sizeof(buff), dot_pos, nom_pos, fil_complete,          \
                  (shortname));                                                \
    strcpybuff(tail, buff + save_tail_offset(buff));                           \
    len = strlen(afs->save);                                                   \
    need = save_name_keep(buff) + strlen(tail) + 1;                            \
    /* the name head a cut keeps and the tail must fit: shorten the dirs */    \
    if (len + need > sizeof(afs->save))                                        \
      save_trim_dirs(afs->save, len, len + need - sizeof(afs->save));          \
    if (tail[0] != '\0')                                                       \
      buff[strlen(buff) - strlen(tail)] = '\0';                                \
    else /* sized: a long extension is dropped before the name */              \
      standard_name(buff, sizeof(afs->save) - strlen(afs->save), dot_pos,      \
                    nom_pos, fil_complete, (shortname));                       \
    url_savename_addstr(afs->save, sizeof(afs->save), buff);                   \
    save_append_tail(afs->save, sizeof(afs->save), tail);                      \
  }

/* Avoid stupid DOS system folders/file such as 'nul' */
/* Based on linux/fs/umsdos/mangle.c */
/* Hand-grouped rows; clang-format reflows the table into a blob. */
/* clang-format off */
static const char *hts_tbdev[] = {
  "prn", "con", "aux", "nul",
  "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9",
  "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
  "conin$", "conout$", "clock$",
  "emmxxxx0", "xmsxxxx0", "setverxx",
  ""
};
/* clang-format on */

/* A URL naming an engine directory would destroy the cache (#774). */
static const char *hts_tbreserved[] = {"hts-cache", "hts-tmp", ""};

/* True once the component holds only what cleanEndingSpaceOrDot() strips. */
static hts_boolean strippedToComponentEnd(const char *s) {
  while (*s == ' ' || *s == '.')
    s++;
  return *s == '\0' || *s == '/' ? HTS_TRUE : HTS_FALSE;
}

/* Return the length of the name in names that the component at c is, or 0. */
static size_t reservedNameAt(const char *save, const char *c,
                             const char *const *names) {
  int i;

  for (i = 0; names[i][0] != '\0'; i++) {
    const size_t len = (size_t) strfield(c, names[i]);

    if (len == 0)
      continue;
    if (c == save) {
      /* The first component usually holds the hostname (#842), so only a
         trailing run ends it, or aux.example.com would be renamed. */
      if (strippedToComponentEnd(c + len))
        return len;
    } else {
      switch ((int) c[len]) {
      case '\0':
      case '/':
      case '.':
        return len;
      case ' ':
        /* cleanEndingSpaceOrDot() runs after us and hands the name back */
        if (strippedToComponentEnd(c + len))
          return len;
        break;
      }
    }
  }
  return 0;
}

/* Replace foo/<reserved>/bar by foo/<reserved>_/bar, matching a whole path
   component only (case-insensitively: the filesystem may be too). */
static void escapeReservedNames(char *save, size_t size,
                                const char *const *names) {
  char *c = save;

  if (size == 0)
    return;
  for (;;) {
    const size_t len = reservedNameAt(save, c, names);

    if (len != 0) {
      const size_t total = strlen(save);

      /* a full buffer loses the name's last byte, never the path's tail */
      if (total < size - 1) {
        memmove(c + len + 1, c + len, total - (size_t) (c - save) - len + 1);
        c[len] = '_';
      } else {
        c[len - 1] = '_';
      }
    }
    c = strchr(c, '/');
    if (c == NULL)
      break;
    c++;
  }
}

void url_savename_escape_reserved(char *save, size_t size) {
  escapeReservedNames(save, size, hts_tbreserved);
  escapeReservedNames(save, size, hts_tbdev);
}

/* Strip all // */
static void cleanDoubleSlash(char *s) {
  int i, j;

  for(i = 0, j = 0; s[i] != '\0'; i++) {
    if (s[i] == '/' && i != 0 && s[i - 1] == '/') {
      continue;
    }
    if (i != j) {
      s[j] = s[i];
    }
    j++;
  }
  // terminating \0
  if (i != j) {
    s[j] = s[i];
  }
}

/* Drop every leading '/': the name is joined to the output directory, so one
   left behind makes it absolute. */
static void stripLeadingSlashes(char *s) {
  const char *start = s;

  while (*start == '/')
    start++;
  if (start != s)
    memmove(s, start, strlen(start) + 1);
}

/* Strip all ending . or ' ' (windows-forbidden) */
static void cleanEndingSpaceOrDot(char *s) {
  int i, j, lastWriteEnd;

  for(i = 0, j = 0, lastWriteEnd = 0; i == 0 || s[i - 1] != '\0'; i++) {
    if (s[i] == '/' || s[i] == '\0') {
      // Last write was not good, revert
      if (j != lastWriteEnd) {
        j = lastWriteEnd;
      }
    }
     
    if (i != j) {
      s[j] = s[i];
    }
    j++;

    // Commit good candidate for terminating character
    if (s[i] != ' ' && s[i] != '.') {
      lastWriteEnd = j;
    }
  }
}

/* Wire Content-Type vs URL extension. A patchable wire type beats a generic
   ext, HTS_UNKNOWN_MIME keeps a specific non-HTML ext (#267), and a declared
   disagreement is CONTESTED and sniffed below. */
typedef enum wire_verdict {
  WIRE_KEEPS_EXT,
  WIRE_WINS,
  WIRE_CONTESTED
} wire_verdict;

static wire_verdict wire_ext_verdict(httrackp *opt, const char *wiremime,
                                     const char *file, char *urlmime,
                                     size_t urlmime_size) {
  if (may_unknown2(opt, wiremime, file))
    return WIRE_KEEPS_EXT; /* type kept verbatim (keep-list / bogus-multiple) */
  urlmime[0] = '\0';
  /* type implied by the URL extension, only when confidently known (flag 0) */
  if (!get_httptype_sized(opt, urlmime, urlmime_size, file, 0))
    return WIRE_WINS; /* URL ext implies no known type */
  if (strfield2(wiremime, urlmime))
    return WIRE_KEEPS_EXT; /* agreement (no .htm->.html churn) */
  if (!is_hypertext_mime(opt, urlmime, file) &&
      strfield2(wiremime, HTS_UNKNOWN_MIME))
    return WIRE_KEEPS_EXT; /* no declared type */
  return WIRE_CONTESTED;
}

/* Optional evidence for a contested wire-vs-ext verdict. */
typedef struct sniff_src {
  struct_back *sback;       /* live backing (looked up by adr/fil) */
  const lien_back *headers; /* snapshot: r.adr, else the url_sav file */
  const char *adr, *fil;
  const char *prev_save; /* previous run's save name (cache X-Save) */
} sniff_src;

size_t hts_read_file_head(const char *path, void *buf, size_t len) {
  char catbuff[CATBUFF_SIZE];
  FILE *const fp = FOPEN(fconv(catbuff, sizeof(catbuff), path), "rb");
  size_t n = 0;

  if (fp != NULL) {
    n = fread(buf, 1, len, fp);
    fclose(fp);
  }
  return n;
}

/* Body head of one slot: memory, else the file it writes (url_sav, or tmpfile
   for a compressed stream); inflated so the sniff sees the final body.
 */
static size_t sniff_slot_head(const lien_back *slot, void *buf, size_t len) {
  const htsblk *const r = &slot->r;
  size_t n = 0;

  if (r->adr != NULL && r->size > 0) {
    n = (size_t) r->size < len ? (size_t) r->size : len;
    memcpy(buf, r->adr, n);
  } else if (r->is_write) {
    /* tmpfile of an uncompressed slot is a backup of an earlier copy */
    const char *const path =
        r->compressed && slot->tmpfile != NULL ? slot->tmpfile : slot->url_sav;

    if (r->out != NULL)
      fflush(r->out);
    if (path[0] != '\0')
      n = hts_read_file_head(path, buf, len);
  }
  if (n > 0 && r->compressed) {
    unsigned char raw[HTS_SNIFF_LEN];

    if (n > sizeof(raw))
      n = sizeof(raw);
    memcpy(raw, buf, n);
    n = hts_codec_head(hts_codec_parse(r->contentencoding), raw, n, buf, len);
  }
  return n;
}

/* Up to len leading body bytes; 0 when unavailable, and always in
   non-delayed mode (its HEAD-probe first run couldn't sniff either). */
static size_t sniff_body_head(httrackp *opt, const sniff_src *src, void *buf,
                              size_t len) {
  size_t n = 0;

  if (src == NULL || opt->savename_delayed == HTS_SAVENAME_DELAYED_NONE)
    return 0;
  /* live backing slot: a snapshot (back_copy_static) loses r.adr/r.out */
  if (src->sback != NULL && src->adr != NULL && src->fil != NULL) {
    const int b = back_index(opt, src->sback, src->adr, src->fil, NULL);

    if (b >= 0)
      n = sniff_slot_head(&src->sback->lnk[b], buf, len);
  }
  if (n == 0 && src->headers != NULL)
    n = sniff_slot_head(src->headers, buf, len);
  return n;
}

/* Contested verdicts: magic proving the URL ext keeps it, else wire wins. */
static int wire_patches_ext(httrackp *opt, const sniff_src *src,
                            const char *wiremime, const char *file) {
  char urlmime[256];

  switch (wire_ext_verdict(opt, wiremime, file, urlmime, sizeof(urlmime))) {
  case WIRE_KEEPS_EXT:
    return 0;
  case WIRE_WINS:
    return 1;
  case WIRE_CONTESTED:
    break;
  }
  if (src != NULL) {
    if (hts_sniff_mime_known(urlmime)) {
      unsigned char head[HTS_SNIFF_LEN];
      const size_t n = sniff_body_head(opt, src, head, sizeof(head));

      if (n > 0)
        return hts_sniff_mime_consistent(head, n, urlmime) ? 0 : 1;
    }
    /* no bytes: reproduce the previous run's verdict (cached X-Save name) */
    if (src->prev_save != NULL && src->prev_save[0] != '\0') {
      char prevmime[256];

      prevmime[0] = '\0';
      if (get_httptype_sized(opt, prevmime, sizeof(prevmime), src->prev_save,
                             0) &&
          strfield2(prevmime, urlmime))
        return 0;
    }
  }
  return 1;
}

int hts_ext_sniff_wanted(httrackp *opt, const char *wiremime,
                         const char *file) {
  char urlmime[256];

  return wiremime != NULL && strnotempty(wiremime) &&
         wire_ext_verdict(opt, wiremime, file, urlmime, sizeof(urlmime)) ==
             WIRE_CONTESTED &&
         hts_sniff_mime_known(urlmime);
}

/* Wire-metadata name change: a Content-Disposition filename wins (returns 2),
   else the declared type's ext when wire_patches_ext() allows (returns 1),
   else 0. ext receives the new extension or replacement filename. */
static int resolve_extension(httrackp *opt, const sniff_src *src,
                             const char *cdispo, const char *contenttype,
                             const char *fil, char *ext, size_t ext_size) {
  if (strnotempty(cdispo)) {
    strlcpybuff(ext, cdispo, ext_size);
    return 2;
  }
  if (wire_patches_ext(opt, src, contenttype, fil) &&
      give_mimext(ext, ext_size, contenttype))
    return 1;
  return 0;
}

/* Largest cut of s at or below len that does not split a UTF-8 character. */
static size_t utf8_cut(const char *s, size_t len) {
  while (len > 0 && ((unsigned char) s[len] & 0xC0) == 0x80)
    len--;
  return len;
}

void url_savename_addtail(char *d, size_t dsize, const char *sep,
                          const char *s) {
  const size_t slen = strlen(sep) + strlen(s);

  if (slen + 1 > dsize) /* the tail alone does not fit: leave d alone */
    return;
  if (strlen(d) + slen + 1 > dsize)
    d[utf8_cut(d, dsize - slen - 1)] = '\0';
  strlcatbuff(d, sep, dsize);
  strlcatbuff(d, s, dsize);
}

/* Append at most n characters of s, clipped to dsize. HTS_FALSE if clipped. */
static hts_boolean tmpl_catn(char *d, size_t dsize, const char *s, size_t n) {
  size_t i = strlen(d);

  for (; *s != '\0' && n != 0 && i + 1 < dsize; n--)
    d[i++] = *s++;
  d[i] = '\0';
  return *s == '\0' || n == 0 ? HTS_TRUE : HTS_FALSE;
}

static void tmpl_catc(char *d, size_t dsize, char c) {
  char s[2];

  s[0] = c;
  s[1] = '\0';
  tmpl_catn(d, dsize, s, 1);
}

/* These appends clip a savename_type -1 name, never abort (#1295). Template
   parts go in through tmpltail and save_append_tail() in one run, so ".html"
   keeps its dot. */
#define TMPL_FLUSH()                                                           \
  do {                                                                         \
    if (tmpltail[0] != '\0') {                                                 \
      save_append_tail(afs->save, sizeof(afs->save), tmpltail);                \
      tmpltail[0] = '\0';                                                      \
    }                                                                          \
  } while (0)
#define TMPL_CATN(S, N)                                                        \
  do {                                                                         \
    TMPL_FLUSH();                                                              \
    if (!tmpl_catn(afs->save, sizeof(afs->save), (S), (size_t) (N)))           \
      tmpl_clipped = HTS_TRUE;                                                 \
  } while (0)
#define TMPL_CAT(S) TMPL_CATN((S), (size_t) -1)
#define TMPL_TAILN(S, N)                                                       \
  tmpl_catn(tmpltail, sizeof(tmpltail), (S), (size_t) (N))
#define TMPL_TAIL(S) TMPL_TAILN((S), (size_t) -1)
#define TMPL_TAILC(C) tmpl_catc(tmpltail, sizeof(tmpltail), (C))

hts_boolean url_query_value(const char *url, const char *name, char *dst,
                            size_t size) {
  const char *const query = strchr(url, '?');
  hts_query_field f;
  size_t len;

  dst[0] = '\0';
  if (query == NULL || !hts_query_find(query + 1, name, &f)) {
    return HTS_FALSE;
  }
  /* crawled query text: clip it */
  len = f.vallen < size - 1 ? f.vallen : size - 1;
  memcpy(dst, f.val, len);
  dst[len] = '\0';
  return HTS_TRUE;
}

/* Drop excess bytes off the longest directory segments of save[0..dir_len). */
static void save_trim_dirs(char *save, size_t dir_len, size_t excess) {
  while (excess > 0) {
    size_t i, start = 0, best = 0, best_len = 0, cut;

    for (i = 0; i <= dir_len; i++) {
      if (i == dir_len || save[i] == '/') {
        if (i - start > best_len) {
          best = start;
          best_len = i - start;
        }
        start = i + 1;
      }
    }
    if (best_len <= 1)
      break;
    cut = utf8_cut(save + best, best_len > excess ? best_len - excess : 1);
    if (cut == 0)
      cut = 1;
    while (cut < best_len && ((unsigned char) save[best + cut] & 0xC0) == 0x80)
      cut++;
    if (cut >= best_len)
      break;
    memmove(save + best + cut, save + best + best_len,
            strlen(save + best + best_len) + 1);
    dir_len -= best_len - cut;
    excess -= excess < best_len - cut ? excess : best_len - cut;
  }
}

/* What a cut keeps of a name: a dotfile's ".stem", else one character. */
static size_t save_name_keep(const char *name) {
  size_t n = 0, i;

  if (name[0] == '.')
    while (n <= MAX_EXT_LEN && name[n + 1] != '\0' && name[n + 1] != '.')
      n++;
  if (name[n] != '\0')
    n++;
  for (i = 0; i < 3 && ((unsigned char) name[n] & 0xC0) == 0x80; i++)
    n++;
  return n;
}

/* Append tail to save (capacity size), cutting the name down to its head. */
static void save_append_tail(char *save, size_t size, const char *tail) {
  const size_t len = strlen(save);
  size_t room;

  assertf(strlen(tail) < size);
  room = size - 1 - strlen(tail);
  if (len > room) {
    const char *const slash = strrchr(save, '/');
    const size_t dir_len = slash != NULL ? (size_t) (slash - save) + 1 : 0;
    const size_t keep = save_name_keep(save + dir_len);

    if (dir_len + keep > room) {
      /* the directory alone fills save[]: shorten its longest segments */
      save[dir_len + keep] = '\0';
      save_trim_dirs(save, dir_len, dir_len + keep - room);
    }
  }
  url_savename_addtail(save, size, "", tail);
}

/* Append the part of name a cut keeps, when a clip left no name byte. */
static void save_append_name_head(char *save, size_t size, const char *name) {
  char head[MAX_EXT_LEN + 8];

  head[0] = '\0';
  strncatbuff(head, name, save_name_keep(name));
  save_append_tail(save, size, head);
}

/* Give save a file name when a clip left it none, or only a dotfile's dot. */
static void save_keep_clipped_name(char *save, size_t size, const char *name) {
  char *const slash = strrchr(save, '/');
  char *const last = slash != NULL ? slash + 1 : save;

  if (strcmp(last, ".") == 0)
    *last = '\0';
  if (*last == '\0')
    save_append_name_head(save, size, name);
}

/* Return the byte offset of each character of s, then strlen(s), and store
   their count in *nChars. A malformed byte is one character. NULL if out of
   memory. */
static size_t *utf8_char_offsets(const char *s, size_t *nChars) {
  const size_t size = strlen(s);
  size_t *const pos = malloct((size + 1) * sizeof(size_t));
  size_t i, n;

  if (pos == NULL)
    return NULL;
  for (i = 0, n = 0; i < size; n++) {
    const size_t len = hts_readUTF8(s + i, size - i, NULL);

    pos[n] = i;
    i += len != 0 ? len : 1;
  }
  pos[n] = size;
  *nChars = n;
  return pos;
}

/* Move character k of s to byte *out and advance *out. Callers keep k
   increasing, so *out <= pos[k] and nothing unread is overwritten. */
static void utf8_keep_char(char *s, const size_t *pos, size_t k, size_t *out) {
  const size_t len = pos[k + 1] - pos[k];

  memmove(s + *out, s + pos[k], len);
  *out += len;
}

/* Where the tail a cut keeps starts: ".<hexid>.delayed" or a short ext. */
static size_t save_tail_offset(const char *save) {
  const size_t len = strlen(save);
  const char *const slash = strrchr(save, '/');
  const size_t last = slash != NULL ? (size_t) (slash - save) + 1 : 0;
  size_t p;

  if (IS_DELAYED_EXT(save)) {
    const size_t dot = len - strlen("." DELAYED_EXT);

    for (p = dot; p > last && dot - p < DELAYED_ID_MAX_DIGITS &&
                  ((save[p - 1] >= '0' && save[p - 1] <= '9') ||
                   (save[p - 1] >= 'a' && save[p - 1] <= 'f'));
         p--)
      ;
    /* a wholly-hex base, e.g. a hashed #133 name, is not a tag */
    return p > last && p < dot && save[p - 1] == '.' ? p - 1 : dot;
  }
  for (p = len; p > last && save[p - 1] != '.'; p--)
    ;
  /* #852: keep a short extension, like the ".html" the page is browsed by */
  return p > last + 1 && hts_stringLengthUTF8(save + p) <= MAX_EXT_LEN ? p - 1
                                                                       : len;
}

/* The last dot of the name part, or NULL when the name carries no extension. */
static char *name_last_dot(char *path) {
  char *lastDot = NULL;

  for (; *path != '\0'; path++) {
    if (*path == '.') {
      lastDot = path;
    } else if (*path == '/' || *path == '\\') {
      lastDot = NULL;
    }
  }
  return lastDot;
}

/* Drop the delayed-type placeholder tail when the mirror already holds the file
   the shortened name points at (#1926). HTS_TRUE when the tail was dropped. */
static hts_boolean url_savename_undelay(httrackp *opt, char *save,
                                        const size_t size,
                                        /* "<id>.delayed", no leading dot */
                                        const char *delayed_tail) {
  char BIGSTK candidate[HTS_URLMAXSIZE * 2];
  char catbuff[CATBUFF_SIZE];
  const size_t len = strlen(save);
  size_t cut = 0; /* prefix to keep, 0 until a tail matches */

  /* the exact tail, so a link whose own name ends in ".<hex>" keeps it */
  if (delayed_tail != NULL && strnotempty(delayed_tail)) {
    const size_t n = strlen(delayed_tail) + 1;

    if (len > n && save[len - n] == '.' &&
        strcmp(save + len - n + 1, delayed_tail) == 0)
      cut = len - n;
  }
  if (cut == 0) {
    if (!IS_DELAYED_EXT(save) || len <= strlen("." DELAYED_EXT))
      return HTS_FALSE;
    cut = len - strlen("." DELAYED_EXT);
  }
  /* the copy below trusts this size, so refuse rather than clip */
  if (cut + 1 > sizeof(candidate))
    return HTS_FALSE;
  strclipbuff(candidate, cut + 1, save);
  /* an undelayed run names an extensionless link by type, so its copy says
     nothing about this link's name */
  if (name_last_dot(candidate) == NULL)
    return HTS_FALSE;
  /* back_add()'s on-disk resume skips HTML, its links having moved */
  if (ishtml(opt, candidate) == 1)
    return HTS_FALSE;
  if (fsize_utf8(fconv(catbuff, sizeof(catbuff), candidate)) <= 0)
    return HTS_FALSE;
  hts_log_print(opt, LOG_DEBUG,
                "engine: save-name: keeping the previous mirror's name %s "
                "instead of the placeholder %s",
                candidate, save);
  strlcpybuff(save, candidate, size);
  return HTS_TRUE;
}

/* Replace SAVE (SIZE bytes), a name another link holds, by the next one to try:
   index.html -> index-2.html -> index-3.html. Jump to suffix SKIP when it is
   further. Clear *REGULAR when the new name breaks the rule that the walk
   from any of its names is the same suffix sequence. Return the suffix. */
static int savename_collision_next(char *save, size_t size, const httrackp *opt,
                                   int max_char, int skip,
                                   hts_boolean *regular) {
  char BIGSTK tempo[HTS_URLMAXSIZE * 2];
  char suffix[16];
  char *a = hts_lastcharptr(save);
  size_t stem;
  int n = 2;
  const char collisionSeparator =
      ((opt->savename_83 != HTS_SAVENAME_83_ISO9660) ? '-' : '_');

  tempo[0] = '\0';
  while ((a > save) && (*a != '.') && (*a != '\\') && (*a != '/'))
    a--;
  if (*a == '.')
    strncatbuff(tempo, save, a - save);
  else
    strcatbuff(tempo, save);

  // bump an existing -N suffix (index-2.html -> index-3.html)
  stem = hts_rtrimlen(tempo, "0123456789");
  if (stem != 0 && tempo[stem - 1] == collisionSeparator) {
    LLint v = n;

    /* a suffix too large to increment stays part of the name */
    if (tempo[stem] == '\0' ||
        hts_parse_llint(tempo + stem, NULL, 0, INT_MAX - 1, &v)) {
      n = (int) v + 1;
      tempo[stem - 1] = '\0';
    }
  }
  if (skip > n)
    n = skip;
  /* the next walk could not bump this suffix */
  if (n == INT_MAX)
    *regular = HTS_FALSE;
  // then fit the name to 8.3 if asked
  if (opt->savename_83) {
    int max;
    char *b = hts_lastcharptr(tempo);

    while ((b > tempo) && (*b != '/'))
      b--;
    if (*b == '/')
      b++;
    max = max_char - 1 - nombre_digit(n);
    /* a cut reaching the directory changes the name being numbered */
    if (max <= 0) {
      *regular = HTS_FALSE;
      max = 0; // a stored suffix of 1e7 or more would cut before b
    }
    if ((int) strlen(b) > max)
      *(b + max) = '\0'; // leave room for the suffix
  }
  // add the suffix, then the extension
  snprintf(suffix, sizeof(suffix), "%c%d", collisionSeparator, n);
  strcatbuff(tempo, suffix);
  if (*a == '.')
    strcatbuff(tempo, a);

  strlcpybuff(save, tempo, size);
  return n;
}

/* Is link I the one url_savename() is naming? */
static hts_boolean savename_same_link(const httrackp *opt, int i,
                                      const char *normadr,
                                      const char *normfil) {
  return strfield2(heap(i)->adr, normadr) != 0 &&
         strcmp(heap(i)->fil, normfil) == 0;
}

/* Walk the collision names from SAVE until one is free or already this link's,
   which then lands in SAVE. With JUMP, skip names an earlier walk found taken.
   Set *SAME when the walk ended on this link's name, *LAST to the last suffix
   tried. Return the number of steps. */
static int savename_walk(char *save, size_t size, const httrackp *opt,
                         hash_struct *hash, const char *normadr,
                         const char *normfil, int max_char, char kind,
                         hts_boolean jump, int *last, hts_boolean *same,
                         hts_boolean *regular) {
  int steps = 0;

  *same = HTS_FALSE;
  for (;;) {
    int i;

    hash->savename_probes++;
    i = hash_read(hash, save, NULL, HASH_STRUCT_FILENAME);
    if (i < 0)
      break;
    if (savename_same_link(opt, i, normadr, normfil)) {
      /* keep its name, whatever its case */
      if (strcmp(heap(i)->sav, save) != 0)
        strlcpybuff(save, heap(i)->sav, size);
      *same = HTS_TRUE;
      break;
    }
    *last = savename_collision_next(
        save, size, opt, max_char,
        jump ? hash_savename_next(hash, kind, save) : 0, regular);
    steps++;
  }
  return steps;
}

/* Is NAME on the collision walk from FIRST, before suffix LAST? */
static hts_boolean savename_walk_skipped(const httrackp *opt, int max_char,
                                         const char *first, int last,
                                         const char *name) {
  char BIGSTK node[HTS_URLMAXSIZE * 2];
  hts_boolean regular = HTS_TRUE;
  int k;

  /* the suffix NAME carries, if it is a collision name at all */
  strcpybuff(node, name);
  k = savename_collision_next(node, sizeof(node), opt, max_char, 0, &regular) -
      1;
  if (k < 1 || k >= last)
    return HTS_FALSE;
  /* the walk reaches suffix K unless it starts above it */
  strcpybuff(node, first);
  return savename_collision_next(node, sizeof(node), opt, max_char, k,
                                 &regular) == k &&
         hts_strcasecmp(node, name) == 0;
}

/* Sets ext from the user's --assume type for fil. Did it find one? */
static hts_boolean user_mime_ext(httrackp *opt, const char *fil, char *ext,
                                 size_t size) {
  char BIGSTK mime[1024];

  mime[0] = ext[0] = '\0';
  get_userhttptype(opt, mime, sizeof(mime), fil);
  return strnotempty(mime) && give_mimext(ext, size, mime) ? HTS_TRUE
                                                           : HTS_FALSE;
}

/* Appends the host directory, without "www." in 8-3 mode. */
static void add_host_dir(lien_adrfilsave *const afs, const httrackp *opt,
                         const char *host, int max_char) {
  if (!opt->savename_83) {
    /* no crawled host is longer, and the type's directories still fit */
    strlncatbuff(afs->save, host, sizeof(afs->save), HTS_URLMAXSIZE);
  } else if (strlen(host) > 4 && strfield(host, "www.")) {
    hts_appendStringUTF8(afs->save, sizeof(afs->save), host + 4, max_char);
  } else {
    hts_appendStringUTF8(afs->save, sizeof(afs->save), host, max_char);
  }
}

// Build the local save name (save) from adr/fil; renames on collision
// (e.g. INDEX.HTML vs index.html).
int url_savename(lien_adrfilsave *const afs,
                 lien_adrfil *const former,
                 const char *referer_adr, const char *referer_fil, 
                 httrackp * opt, struct_back * sback, cache_back * cache,
                 hash_struct * hash, int ptr, int numero_passe,
                 const lien_back * headers) {
  char catbuff[CATBUFF_SIZE];
  const int is_redirect = headers != NULL && HTTP_IS_REDIRECT(headers->r.statuscode);
  const char *mime_type = headers != NULL && !is_redirect ? headers->r.contenttype : NULL;
  /*const char* mime_type = ( headers && HTTP_IS_OK(headers->r.statuscode) ) ?
   * headers->r.contenttype : NULL; */

  /* */
  char BIGSTK fil[HTS_URLMAXSIZE * 2];       /* ="" */

  const char *const adr_complete = hts_host_alias_fold(opt, &afs->af);
  const char *const fil_complete = afs->af.fil;

  /*char BIGSTK normadr_[HTS_URLMAXSIZE*2]; */
  char BIGSTK normadr_[HTS_URLMAXSIZE * 2], normfil_[HTS_URLMAXSIZE * 2];
  enum { PROTOCOL_HTTP, PROTOCOL_HTTPS, PROTOCOL_FTP, PROTOCOL_FILE,
      PROTOCOL_UNKNOWN };
  static const char *protocol_str[] =
    { "http", "https", "ftp", "file", "unknown" };
  int protocol = PROTOCOL_HTTP;
  const char *const adr = jump_identification_const(adr_complete);
  // copy of fil, used for lookups (see urlhack)
  const char *normadr = adr;
  const char *normfil = fil_complete;
  /* query keys to strip for this URL (NULL = none); decoupled from urlhack */
  char BIGSTK stripkeys[HTS_URLMAXSIZE];
  const char *const strip_rules = hts_strip_query_rules(opt);
  const char *const strip =
      strip_rules != NULL ? hts_query_strip_keys(strip_rules, adr, fil_complete,
                                                 stripkeys, sizeof(stripkeys))
                          : NULL;
  const char *const print_adr = jump_protocol_const(adr);
  const char *start_pos = NULL, *nom_pos = NULL, *dot_pos = NULL;     // Position nom et point

  // pour changement d'extension ou de nom (content-disposition)
  int ext_chg = 0, ext_chg_delayed = 0;
  int is_html = 0;
  char ext[256];
  int max_char = 0;

  //CLEAR
  fil[0] = ext[0] = '\0';
  afs->save[0] = '\0';

  /* 8-3 ? */
  switch (opt->savename_83) {
  case HTS_SAVENAME_83_DOS: // 8-3
    max_char = 8;
    break;
  case HTS_SAVENAME_83_ISO9660: // Level 2 File names may be up to 31
                                // characters.
    max_char = 31;
    break;
  default:
    max_char = 8;
    break;
  }

  // normalize the URL:
  // www.foo.com -> foo.com
  // www-42.foo.com -> foo.com
  // foo.com/bar//foobar -> foo.com/bar/foobar
  if (opt->urlhack) {
    // dedup-lookup key; honor the per-feature negatives like htshash.c so
    // distinct URLs keep distinct savenames (else keep normadr = adr)
    if (!opt->no_www_dedup)
      normadr = adr_normalized_sized(adr, normadr_, sizeof(normadr_));
    normfil =
        fil_normalized_filtered_ex(fil_complete, normfil_, strip,
                                   !opt->no_slash_dedup, !opt->no_query_dedup);
  } else {
    if (link_has_authority(adr_complete)) {     // https or other protocols : in "http/" subfolder
      char *pos = strchr(adr_complete, ':');

      /* savename_same_link reads the scheme, so drop it only on overflow */
      if (pos != NULL && (size_t) (pos - adr_complete) < sizeof(normadr_) - 3 &&
          strlen(normadr) <
              sizeof(normadr_) - 3 - (size_t) (pos - adr_complete)) {
        normadr_[0] = '\0';
        strncatbuff(normadr_, adr_complete, (int) (pos - adr_complete));
        strcatbuff(normadr_, "://");
        strcatbuff(normadr_, normadr);
        normadr = normadr_;
      }
    }
    // strip applies even with urlhack off, but // and query-sort do not, to
    // match the key the URL was stored with
    if (strip != NULL)
      normfil = fil_normalized_filtered_ex(fil_complete, normfil_, strip, 0, 0);
  }

  // à afficher sans ftp://
  if (strfield(adr_complete, "https:")) {
    protocol = PROTOCOL_HTTPS;
  } else if (strfield(adr_complete, "ftp:")) {
    protocol = PROTOCOL_FTP;
  } else if (strfield(adr_complete, "file:")) {
    protocol = PROTOCOL_FILE;
  } else {
    protocol = PROTOCOL_HTTP;
  }

  // court-circuit pour lien primaire
  if (strnotempty(adr) == 0) {
    if (strcmp(fil_complete, "primary") == 0) {
      strcatbuff(afs->save, "primary.html");
      return 0;
    }
  }

  /* Declare adr (IDNA-decoded if necessary) */
  /* clang-format off: an edit realigns all backslashes, churning the macro. */
  /* clang-format off */
#define DECLARE_ADR(FINAL_ADR) \
  char *idna_adr =\
    /* http or https */\
    (\
    protocol == PROTOCOL_HTTP\
    || protocol == PROTOCOL_HTTPS \
    )\
    /* and contains IDNA */\
    && hts_isStringIDNA(print_adr, strlen(print_adr))\
    ? hts_convertStringIDNAToUTF8(print_adr, strlen(print_adr))\
    : NULL;\
  /* Punycode can quadruple the host, and a clipped one names another host,\
     so an over-long decode keeps the ASCII form. The bound is the pre-IDNA\
     one and not the save buffer, which still has a path to append. */\
  const char *const FINAL_ADR = \
    (idna_adr != NULL && strlen(idna_adr) < HTS_URLMAXSIZE) \
    ? idna_adr : ( protocol == PROTOCOL_FILE ? "file" : print_adr )
  /* clang-format on */

  /* Release adr */
#define RELEASE_ADR()                                                          \
  do {                                                                         \
    freet(idna_adr);                                                           \
  } while (0)

  // vérifier que le nom n'a pas déja été calculé (si oui le renvoyer tel que)
  // vérifier que le nom n'est pas déja pris...
  // NOTE: si on cherche /toto/ et que /toto est trouvé on le prend (et réciproquqment) ** // **
  if (opt->liens != NULL) {
    int i;

    i = hash_read(hash, normadr, normfil, HASH_STRUCT_ADR_PATH);     // recherche table 1 (adr+fil)
    if (i >= 0) {               // ok, trouvé
      strcpybuff(afs->save, heap(i)->sav);
      return 0;
    }
    i = hash_read(hash, normadr, normfil, HASH_STRUCT_ORIGINAL_ADR_PATH);     // recherche table 2 (former->adr+former->fil)
    if (i >= 0) {               // ok, trouvé
      // copier location moved!
      strcpybuff(afs->af.adr, heap(i)->adr);
      strcpybuff(afs->af.fil, heap(i)->fil);
      // et save
      strcpybuff(afs->save, heap(i)->sav);  // copier (formé à partir du nouveau lien!)
      return 0;
    }
    // look up the other form, with or without the trailing /, in former
    {
      char BIGSTK fil_complete_patche[HTS_URLMAXSIZE * 2];

      strcpybuff(fil_complete_patche, normfil);
      // a full-length path has no room for the /
      if (!hts_striplastchar(fil_complete_patche, '/') &&
          strlen(fil_complete_patche) < sizeof(fil_complete_patche) - 1)
        strcatbuff(fil_complete_patche, "/");
      i = hash_read(hash, normadr, fil_complete_patche,
                    HASH_STRUCT_ORIGINAL_ADR_PATH); // table 2: former adr+fil
      if (i >= 0) {
        // écraser fil et adr (pas former->fil?????)
        strcpybuff(afs->af.adr, heap(i)->adr);
        strcpybuff(afs->af.fil, heap(i)->fil);
        // écrire save
        strcpybuff(afs->save, heap(i)->sav);
        return 0;
      }
    }
  }
  // vérifier la non présence de paramètres dans le nom de fichier
  // si il y en a, les supprimer (ex: truc.cgi?subj=aspirateur)
  // néanmoins, gardé pour vérifier la non duplication (voir après)
  {
    char *a;

    a = strchr(fil_complete, '?');
    if (a != NULL) {
      strncatbuff(fil, fil_complete, a - fil_complete);
    } else {
      strcpybuff(fil, fil_complete);
    }
  }

  // decode remaining % (normally not necessary; already done in htsparse.c)
  // this will NOT decode buggy %xx (ie. not UTF-8) ones
  if (hts_unescapeUrl(fil, catbuff, sizeof(catbuff)) == 0) {
    strcpybuff(fil, catbuff);
  } else {
    hts_log_print(opt, LOG_WARNING,
      "could not URL-decode string '%s'", fil);
  }

  /* replace shtml to html.. */
  /* HARD delays every type, except one the user pinned with --assume: honor it
     immediately (ishtml() consults the user type), no delayed name (#56) */
  if (opt->savename_delayed == HTS_SAVENAME_DELAYED_HARD &&
      !is_userknowntype(opt, fil))
    is_html = -1;               /* ALWAYS delay type */
  else
    is_html = ishtml(opt, fil);
  switch (is_html) {            /* .html,.shtml,.. */
  case 1:
    if ((strfield2(get_ext(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), fil), "html") == 0)
        && (strfield2(get_ext(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), fil), "htm") == 0)
      ) {
      strcpybuff(ext, "html");
      ext_chg = 1;
    }
    break;
  case 0:
    if (!strnotempty(ext)) {
      if (is_userknowntype(opt, fil) && // mime known by user
          user_mime_ext(opt, fil, ext, sizeof(ext))) {
        ext_chg = 1;
      }
    }
    break;
  }

  // si option check_type activée
  if (is_html < 0 && opt->check_type && !ext_chg) {
    if (protocol != PROTOCOL_FILE
        && protocol != PROTOCOL_FTP
      ) {
      // tester type avec requète HEAD si on ne connait pas le type du fichier
      if (!((opt->check_type == 1) &&
            (hts_lastchar(fil) == '/'))) // slash doit être html?
        if (opt->savename_delayed == HTS_SAVENAME_DELAYED_HARD ||
            ishtml(opt, fil) < 0) { // unsure whether it's html or a file
          // lire dans le cache
          char BIGSTK previous_save[HTS_URLMAXSIZE * 2];
          htsblk r;

          previous_save[0] = '\0';
          r = cache_read_including_broken(opt, cache, adr, fil, previous_save,
                                          sizeof(previous_save), NULL,
                                          0); // test only

          if (r.statuscode != -1) { // cache entry read OK
            hts_log_print(opt, LOG_DEBUG, "Testing link type (from cache) %s%s",
                          adr_nologin(opt, adr_complete), fil_complete);
            if (!HTTP_IS_REDIRECT(r.statuscode)) {
              const sniff_src src = {sback, NULL, adr, fil, previous_save};

              ext_chg = resolve_extension(opt, &src, r.cdispo, r.contenttype,
                                          fil, ext, sizeof(ext));
            }
          } else if (opt->savename_delayed != HTS_SAVENAME_DELAYED_HARD &&
                     is_userknowntype(opt, fil)) { /* PATCH BY BRIAN SCHRÖDER.
                              Lookup mimetype not only by extension,
                              but also by filename */
            /* "foo.cgi => text/html" saves foo.cgi as ".html" */
            if (user_mime_ext(opt, fil, ext, sizeof(ext))) {
              ext_chg = 1;
            }
          }
          // with savename_delayed, the name is temporary and slightly invalid.
          // When stopping, back_add() will fail later.
          else if (opt->savename_delayed != HTS_SAVENAME_DELAYED_NONE &&
                   !hts_load_acquire_int(&opt->state.stop)) {
            // Check if the file is ready in backing.
            if (headers != NULL && back_status(headers) >= 0 && !is_redirect) {
              const sniff_src src = {sback, headers, adr, fil, NULL};

              ext_chg = resolve_extension(opt, &src, headers->r.cdispo,
                                          headers->r.contenttype,
                                          headers->url_fil, ext, sizeof(ext));
            } else if (mime_type != NULL) {
              ext[0] = '\0';
              if (*mime_type) {
                give_mimext(ext, sizeof(ext), mime_type);
              }
              if (strnotempty(ext)) {
                char mime_from_file[128];

                mime_from_file[0] = 0;
                get_httptype_sized(opt, mime_from_file, sizeof(mime_from_file),
                                   fil, 1);
                if (!strnotempty(mime_from_file) ||
                    hts_strcasecmp(mime_type, mime_from_file) !=
                        0) { /* different mime for this type */
                  /* type change not forbidden (or no extension at all) */
                  if (!may_unknown2(opt, mime_type, fil)) {
                    ext_chg = 1;
                  }
                } else {
                  ext_chg = 0;
                }
              }
            } else {
              /* Avoid collisions (no collisionning detection) */
              sprintf(ext, "%x.%s", opt->state.delayedId++, DELAYED_EXT);
              ext_chg = 1;
              ext_chg_delayed = 1;      /* due to naming system */
            }
          }
          // not in the cache: probe the type with a request
          // no backing to probe with: -#C names offline (#1393)
          else if (sback != NULL) {
            //
            int hihp = opt->state._hts_in_html_parsing;
            int has_been_moved = 0;
            lien_adrfil current;

            /* Wait for an available test slot, honoring the connection limits
             */
            if (!hts_wait_available_socket(sback, opt, cache, ptr))
              return -1;

            /* Rock'in */
            current.adr[0] = current.fil[0] = '\0';
            opt->state._hts_in_html_parsing = 2;        // test
            hts_log_print(opt, LOG_DEBUG, "Testing link type %s%s",
                          adr_nologin(opt, adr_complete), fil_complete);
            strcpybuff(current.adr, adr_complete);
            strcpybuff(current.fil, fil_complete);
            // ajouter dans le backing le fichier en mode test
            // savename: rien car en mode test
            if (hts_is_exiting(opt)) {
              /* an exit names the link untested rather than probe it */
            } else if (back_add(sback, opt, cache, current.adr, current.fil,
                                BACK_ADD_TEST, referer_adr, referer_fil, 1,
                                HTS_FALSE, NULL) != -1) {
              int b;

              b = back_index(opt, sback, current.adr, current.fil, BACK_ADD_TEST);
              if (b >= 0) {
                lien_back *const back = sback->lnk;
                int stop_looping = 0;
                int petits_tours = 0;
                int get_test_request = 0;       // en cas de bouclage sur soi même avec HEAD, tester avec GET.. parfois c'est la cause des problèmes

                do {
                  // temps à attendre, et remplir autant que l'on peut le cache (backing)
                  if (back_status(&back[b]) > 0) {
                    back_wait(sback, opt, cache, 0);
                  }
                  if (ptr >= 0) {
                    back_fillmax(sback, opt, cache, ptr, numero_passe);
                  }
                  (void) hts_poll_abort_request(opt);
                  if (!hts_loop_tick(sback, opt, b, ptr)) {
                    return -1;
                  } else if (opt->state._hts_cancel || !back_checkmirror(opt) ||
                             hts_is_exiting(opt)) { // cancel parsing, or exit
                    back_delete(opt, cache, sback, b);  // cancel test
                    stop_looping = 1;
                  }
                  // traitement des 304,303..
                  if (back_status(&back[b]) <= 0) {
                    if (HTTP_IS_REDIRECT(back[b].r.statuscode)) {       // agh moved.. un tit tour de plus
                      if ((petits_tours < 5) && former != NULL) { // on va pas tourner en rond non plus!
                        if (strnotempty(back[b].r.location)) {    // location existe!
                          char BIGSTK mov_url[HTS_URLMAXSIZE * 2];
                          lien_adrfil moved;
                          mov_url[0] = moved.adr[0] = moved.fil[0] = '\0';
                          //
                          strcpybuff(mov_url, back[b].r.location);      // copier URL
                          if (ident_url_relatif
                              (mov_url, current.adr, current.fil, &moved) >= 0) {
                            // si non bouclage sur soi même, ou si test avec GET non testé
                            if ((strcmp(moved.adr, current.adr))
                                || (strcmp(moved.fil, current.fil))
                                || (get_test_request == 0)) {
                              // bouclage?
                              if ((!strcmp(moved.adr, current.adr))
                                  && (!strcmp(moved.fil, current.fil)))
                                get_test_request = 1;   // faire requète avec GET

                              // recopier former->adr/fil?
                              if (former != NULL) {
                                if (strnotempty(former->adr) == 0) {     // Pas déja noté
                                  strcpybuff(former->adr, current.adr);
                                  strcpybuff(former->fil, current.fil);
                                }
                              }
                              // check explicit forbidden - don't follow 3xx in this case
                              {
                                int set_prio_to = 0;

                                if (hts_acceptlink(opt, ptr, moved.adr,
                                                   moved.fil, NULL, NULL,
                                                   HTS_FALSE, &set_prio_to,
                                                   NULL) == 1) { /* forbidden */
                                  has_been_moved = 1;
                                  back_maydelete(opt, cache, sback, b); // ok
                                  strcpybuff(current.adr, moved.adr);
                                  strcpybuff(current.fil, moved.fil);
                                  mov_url[0] = '\0';
                                  stop_looping = 1;
                                }
                              }

                              // ftp: stop!
                              if (strfield(mov_url, "ftp://")
                                ) {     // ftp, ok on arrête
                                has_been_moved = 1;
                                back_maydelete(opt, cache, sback, b);   // ok
                                strcpybuff(current.adr, moved.adr);
                                strcpybuff(current.fil, moved.fil);
                                stop_looping = 1;
                              } else if (*mov_url) {
                                const char *methode;

                                if (!get_test_request)
                                  methode = BACK_ADD_TEST;      // tester avec HEAD
                                else {
                                  methode = BACK_ADD_TEST2;     // tester avec GET
                                  hts_log_print(opt, LOG_WARNING,
                                                "Loop with HEAD request "
                                                "(during prefetch) at %s%s",
                                                adr_nologin(opt, current.adr),
                                                current.fil);
                                }
                                if (!hts_wait_available_socket(sback, opt,
                                                               cache, ptr))
                                  return -1;
                                if (back_add(sback, opt, cache, moved.adr,
                                             moved.fil, methode, referer_adr,
                                             referer_fil, 1, HTS_FALSE,
                                             NULL) != -1) { // OK
                                  hts_log_print(
                                      opt, LOG_DEBUG,
                                      "(during prefetch) %s (%d) to link %s at "
                                      "%s%s",
                                      back[b].r.msg, back[b].r.statuscode,
                                      url_nologin(opt, back[b].r.location),
                                      adr_nologin(opt, current.adr),
                                      current.fil);

                                  // libérer emplacement backing actuel et attendre le prochain
                                  back_maydelete(opt, cache, sback, b);
                                  strcpybuff(current.adr, moved.adr);
                                  strcpybuff(current.fil, moved.fil);
                                  b =
                                    back_index(opt, sback, current.adr, current.fil,
                                               methode);
                                  if (!get_test_request)
                                    has_been_moved = 1; // sinon ne pas forcer has_been_moved car non déplacé
                                  petits_tours++;
                                  //
                                } else { // sinon on fait rien et on s'en va..
                                         // (ftp etc)
                                  hts_log_print(opt, LOG_DEBUG,
                                                "Warning: Savename redirect "
                                                "backing error at %s%s",
                                                adr_nologin(opt, moved.adr),
                                                moved.fil);
                                }
                              }
                            } else {
                              hts_log_print(
                                  opt, LOG_WARNING,
                                  "Unable to test %s%s (loop to same filename)",
                                  adr_nologin(opt, adr_complete), fil_complete);
                            }

                          }
                        }
                      } else {  // arrêter les frais
                        hts_log_print(
                            opt, LOG_WARNING, "Unable to test %s%s (loop)",
                            adr_nologin(opt, adr_complete), fil_complete);
                      }
                    }           // ok, leaving
                  }
                } while (!stop_looping && back_status(&back[b]) > 0 &&
                         back_status(&back[b]) < 1000);

                // Si non déplacé, forcer type?
                if (!has_been_moved) {
                  if (back[b].r.statuscode != -10) {    // erreur
                    if (strnotempty(back[b].r.contenttype) == 0)
                      strcpybuff(back[b].r.contenttype,
                                 HTS_UNKNOWN_MIME); // no declared type
                    // Finalement on, renvoie un erreur, pour ne toucher à rien dans le code
                    // libérer emplacement backing
                  }

                  // no error: change the type?
                  ext_chg = resolve_extension(
                      opt, NULL, back[b].r.cdispo, back[b].r.contenttype,
                      back[b].url_fil, ext, sizeof(ext));
                }
                // FIN Si non déplacé, forcer type?

                // libérer emplacement backing
                back_maydelete(opt, cache, sback, b);

                // --- --- ---
                // oops, a été déplacé.. on recalcule en récursif (osons!)
                if (has_been_moved) {
                  // copier adr, fil (optionnel, mais sinon marche pas pour le rip)
                  strcpybuff(afs->af.adr, current.adr);
                  strcpybuff(afs->af.fil, current.fil);
                  // copier adr, fil

                  return url_savename(afs, NULL,
                                      referer_adr, referer_fil, opt, 
                                      sback, cache, hash, ptr,
                                      numero_passe, NULL);
                }
                // --- --- ---

              }

            } else {
              printf("PANIC! : Savename Crash adding error, unexpected error "
                     "found.. [%d]\n",
                     __LINE__);
              hts_log_print(opt, LOG_ERROR,
                            "Unexpected savename backing error at %s%s",
                            adr_nologin(opt, adr), fil_complete);
            }
            // restaurer
            opt->state._hts_in_html_parsing = hihp;
          } // caché?
        }
    }
  }

  // - - - DEBUT NOMMAGE - - -

  // Donner nom par défaut?
  if (hts_lastchar(fil) == '/') {
    if (!strfield(adr_complete, "ftp://")
      ) {
      save_append_tail(fil, sizeof(fil), DEFAULT_HTML);
    } else {
      if (!opt->proxy.active)
        save_append_tail(fil, sizeof(fil), DEFAULT_FTP);
      else // through a proxy, assume html
        save_append_tail(fil, sizeof(fil), DEFAULT_HTML);
    }
  }
  // Change the extension? e.g. php3 saved as html, cgi as html or gif/xbm
  // depending on the resolved type.
  if (ext_chg && !opt->no_type_change) {
    char *a = hts_lastcharptr(fil);

    if ((opt->debug > 1) && (opt->log != NULL)) {
      if (ext_chg == 1)
        hts_log_print(opt, LOG_DEBUG, "Changing link extension %s%s to .%s",
                      adr_nologin(opt, adr_complete), fil_complete, ext);
      else
        hts_log_print(opt, LOG_DEBUG, "Changing link name %s%s to %s",
                      adr_nologin(opt, adr_complete), fil_complete, ext);
    }
    if (ext_chg == 1) {
      // Cut the old extension only when it is empty (a bare trailing dot), the
      // new one, or a recognized one; an unknown trailing ".token" (e.g.
      // /article-1.884291, #115) is part of the name, not an extension.
      const char *const old_ext = get_ext(catbuff, sizeof(catbuff), fil);
      const int known_ext = !*old_ext || strfield2(old_ext, ext) ||
                            is_knowntype(opt, fil) || is_dyntype(old_ext) ||
                            ishtml_ext(old_ext) != -1;
      char dot_ext[sizeof(ext) + 1];

      while((a > fil) && (*a != '.') && (*a != '/'))
        a--;
      /* the placeholder is temporary, so it keeps the name an undelayed run
         would have picked */
      if (*a == '.' && known_ext && !ext_chg_delayed)
        *a = '\0'; // cut
      dot_ext[0] = '.';
      strlcpybuff(dot_ext + 1, ext, sizeof(dot_ext) - 1);
      save_append_tail(fil, sizeof(fil), dot_ext);
    } else {
      while((a > fil) && (*a != '/'))
        a--;
      if (*a == '/')
        a++;
      *a = '\0';
      save_append_tail(fil, sizeof(fil), ext);
    }
  }
  // find the last / and the last .
  {
    const char *a = hts_lastcharptr(fil);

    // passer structures
    start_pos = fil;
    while((a > fil) && (*a != '/') && (*a != '\\')) {
      if (*a == '.')            // point? noter position
        if (!dot_pos)
          dot_pos = a;
      a--;
    }
    if ((*a == '/') || (*a == '\\'))
      a++;
    nom_pos = a;
  }

  // un nom de fichier est généré
  // s'il existe déja, alors on le mofifie légèrement

  // ajouter nom du site éventuellement en premier
  if (opt->savename_type == -1) {       // utiliser savename_userdef! (%h%p/%n%q.%t)
    const char *a = StringBuff(opt->savename_userdef);
    char BIGSTK tmpltail[HTS_URLMAXSIZE * 2];
    hts_boolean tmpl_clipped = HTS_FALSE;
    char tok;

    // build the name
    tmpltail[0] = '\0';
    /* parse to the end, or the extension the appends protect never arrives */
    while (*a != '\0') {
      if (*a == '%') {
        int short_ver = 0;

        a++;
        if (*a == 's') { // '%s...' selects the short (8.3) form
          short_ver = 1;
          a++;
        }
        if (*a == '\0') /* a '%' or '%s' ending the template */
          break;
        switch (tok = *a++) {
        case '[':              // %[param:prefix_if_not_empty:suffix_if_not_empty:empty_replacement:notfound_replacement]
          if (strchr(a, ']')) {
            int pos = 0;
            char name[5][256];
            char value[256];
            char *c = name[0];
            const char *d;

            for(pos = 0; pos < 5; pos++) {
              name[pos][0] = '\0';
            }
            pos = 0;
            // The + 2 cap is kept so existing names still fit.
            while(*a != '\0' && *a != ']') {
              if (*a == ':') { // next token; past the fifth they are dropped
                c = pos + 1 < 5 ? name[++pos] : NULL;
              } else if (c != NULL &&
                         (size_t) (c - name[pos]) + 2 < sizeof(name[pos])) {
                *c++ = *a;
                *c = '\0';
              }
              a++;
            }
            if (*a == ']') {
              a++;
            }
            if (url_query_value(fil_complete, name[0], value, sizeof(value))) {
              TMPL_TAIL(name[1]); /* prefix */
              d = value[0] != '\0'
                      ? unescape_http(catbuff, sizeof(catbuff), value)
                      : NULL;
              if (d && *d) {
                TMPL_CAT(d); /* value */
              } else {
                TMPL_TAIL(name[3]); /* empty replacement if any */
              }
              TMPL_TAIL(name[2]); /* suffix */
            } else {
              TMPL_TAIL(name[4]); /* not found replacement if any */
            }
          }
          break;
        case '%':
          TMPL_TAILC('%');
          break;
        case 'n': // name without extension
        case 'N': // name with extension
          tmpl_clipped = HTS_FALSE;
          if (dot_pos) {
            if (!short_ver)
              TMPL_CATN(nom_pos, (int) (dot_pos - nom_pos));
            else
              TMPL_CATN(nom_pos, min((int) (dot_pos - nom_pos), 8));
          } else {
            if (!short_ver)
              TMPL_CAT(nom_pos);
            else
              TMPL_CATN(nom_pos, 8);
          }
          if (tmpl_clipped)
            save_keep_clipped_name(afs->save, sizeof(afs->save), nom_pos);
          if (tok == 'n')
            break;
          TMPL_TAILC('.');
          HTS_FALLTHROUGH;
        case 't': // extension
          if (dot_pos) {
            if (!short_ver)
              TMPL_TAIL(dot_pos + 1);
            else
              TMPL_TAILN(dot_pos + 1, 3);
          } else {
            if (!short_ver)
              TMPL_TAIL(&DEFAULT_EXT[1]); // skip the leading dot
            else
              TMPL_TAIL(&DEFAULT_EXT_SHORT[1]); // skip the leading dot
          }
          break;
        case 'p': // path without trailing /
          if (nom_pos !=
              fil + 1) { // skip when the path is empty (e.g. /index.html)
            if (!short_ver) {
              TMPL_CATN(fil, (int) (nom_pos - fil) - 1);
            } else {
              char BIGSTK pth[HTS_URLMAXSIZE * 2], n83[HTS_URLMAXSIZE * 2];

              pth[0] = n83[0] = '\0';
              strncatbuff(pth, fil, (int) (nom_pos - fil) - 1);
              long_to_83(opt->savename_83, n83, sizeof(n83), pth);
              TMPL_CAT(n83);
            }
          }
          break;
        case 'h':              // host (IDNA decoded if suitable)
          // IDNA / RFC 3492 (Punycode) handling for HTTP(s)
          {
            DECLARE_ADR(final_adr);

            /* Copy address (8.3 mode does not shorten a host name) */
            TMPL_CAT(final_adr);

            /* release */
            RELEASE_ADR();
          }
          break;
        case 'H': // host, raw (old mode)
          if (protocol == PROTOCOL_FILE) {
            if (!short_ver)
              TMPL_CAT("localhost");
            else
              TMPL_CAT("local");
          } else {
            if (!short_ver)
              TMPL_CAT(print_adr);
            else
              TMPL_CATN(print_adr, 8);
          }
          break;
        case 'M': /* host/address?query MD5 (128-bits) */
        {
          char digest[32 + 2];
          /* sized for both sources, and clipped so a longer one cannot abort */
          char BIGSTK buff[sizeof(afs->af.adr) + sizeof(afs->af.fil)];

          digest[0] = buff[0] = '\0';
          tmpl_catn(buff, sizeof(buff), adr, (size_t) -1);
          tmpl_catn(buff, sizeof(buff), fil_complete, (size_t) -1);
          domd5mem(buff, strlen(buff), digest, 1);
          TMPL_TAIL(digest);
        } break;
        case 'Q':
        case 'q': /* query MD5 (128-bits/16-bits)
                      GENERATED ONLY IF query string exists! */
        {
          char md5[32 + 2];

          TMPL_TAILN(url_md5(md5, sizeof(md5), fil_complete),
                     (tok == 'Q') ? 32 : 4);
        } break;
        case 'r':
        case 'R':              // protocol
          TMPL_TAIL(protocol_str[protocol]);
          break;

          /* Patch by Juan Fco Rodriguez to get the full query string */
        case 'k':
          {
            char *d = strchr(fil_complete, '?');

            if (d != NULL) {
              TMPL_CAT(d);
            }
          }
          break;

        }
      } else
        TMPL_TAILC(*a++);
    }
    TMPL_FLUSH();
    /* %n of a dotfile like ".y" is empty: keep a file name (#2249) */
    if (afs->save[0] == '\0' || hts_lastchar(afs->save) == '/') {
      tmpl_clipped = HTS_FALSE;
      TMPL_CAT(nom_pos);
      if (tmpl_clipped)
        save_keep_clipped_name(afs->save, sizeof(afs->save), nom_pos);
    }
    //
    // predefined types
    //

  }
  //
  // Structure originale
  else if (opt->savename_type % 100 == 0) {
    if (opt->savename_type != 100) {
      // an odd thousands digit drops the host directory
      if (((opt->savename_type / 1000) % 2) == 0) {
        DECLARE_ADR(final_adr);

        add_host_dir(afs, opt, final_adr, max_char);
        if (*fil != '/')
          strcatbuff(afs->save, "/");
        RELEASE_ADR();
      }
    }

    hts_lowcase(afs->save);

    /* add name */
    ADD_STANDARD_PATH;
    ADD_STANDARD_NAME(0);

  }
  //
  // Structure html/image
  else {
    // an odd thousands digit drops the top directory
    if (((opt->savename_type / 1000) % 2) == 0) {
      // an odd hundreds digit names it after the host rather than "web"
      if ((opt->savename_type / 100) % 2) {
        DECLARE_ADR(final_adr);

        add_host_dir(afs, opt, final_adr, max_char);
        strcatbuff(afs->save, "/");
        RELEASE_ADR();
      } else {
        strcatbuff(afs->save, "web/");       // répertoire général
      }
    }
    // si un html à coup sûr
    if ((ext_chg != 0) ? (ishtml_ext(ext) == 1) : (ishtml(opt, fil) == 1)) {
      if (opt->savename_type % 100 == 2) {      // html/
        strcatbuff(afs->save, "html/");
      }
    } else {
      if ((opt->savename_type % 100 == 1) || (opt->savename_type % 100 == 2)) { // html & images
        strcatbuff(afs->save, "images/");
      }
    }

    switch (opt->savename_type % 100) {
    case 4:
    case 5:{                   // séparer par types
      const char *a = hts_lastcharptr(fil);

      // passer structures
      while ((a > fil) && (*a != '/') && (*a != '\\'))
        a--;
      if ((*a == '/') || (*a == '\\'))
        a++;

      // html?
      if ((ext_chg != 0) ? (ishtml_ext(ext) == 1) : (ishtml(opt, fil) == 1)) {
        if (opt->savename_type % 100 == 5)
          strcatbuff(afs->save, "html/");
      } else {
        const char *a = hts_lastcharptr(fil);

        while ((a > fil) && (*a != '/') && (*a != '.'))
          a--;
        if (*a != '.')
          strcatbuff(afs->save, "other");
        else /* an extension off the wire, clipped to leave room for the "/" */
          url_savename_addstr(afs->save, sizeof(afs->save) - 1, a + 1);
        strcatbuff(afs->save, "/");
      }
        /*strcatbuff(save,a); */
        /* add name */
        ADD_STANDARD_NAME(0);
      }
      break;
    case 99:{                  // 'codé' .. c'est un gadget
        size_t i;
        size_t j;
        const char *a;
        char C[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
        int L;

        // pseudo-CRC sur fil et adr pour initialiser générateur aléatoire..
        unsigned int s = 0;

        L = (int) strlen(C);
        for(i = 0; fil_complete[i] != '\0'; i++) {
          s += (unsigned int) fil_complete[i];
        }
        for(i = 0; adr_complete[i] != '\0'; i++) {
          s += (unsigned int) adr_complete[i];
        }
        srand(s);

        j = strlen(afs->save);
        for(i = 0; i < 8; i++) {
          char c = C[(rand() % L)];

          afs->save[i + j] = c;
        }
        afs->save[i + j] = '\0';
        // append the extension
        a = hts_lastcharptr(fil);
        while((a > fil) && (*a != '/') && (*a != '.'))
          a--;
        if (*a == '.') {
          save_append_tail(afs->save, sizeof(afs->save), a);
        }
      }
      break;
    default:{                  // noms sans les noms des répertoires
      // ne garder que le nom, pas la structure
      /* add name */
      ADD_STANDARD_NAME(0);
      }
      break;
    }

    hts_lowcase(afs->save);

    if (hts_lastchar(afs->save) == '/')
      save_append_tail(afs->save, sizeof(afs->save), DEFAULT_HTML);
  }

  // Without an extension, force .html (not used for delayed types)
  // so /a/b and /a/b/index.html cannot clash.
  if (opt->savename_type != -1 &&
      opt->savename_delayed != HTS_SAVENAME_DELAYED_HARD) {
    char *a = hts_lastcharptr(afs->save);

    while((a > afs->save) && (*a != '.') && (*a != '/'))
      a--;
    if (*a != '.') {
      save_append_tail(afs->save, sizeof(afs->save), ".html");
      hts_log_print(opt, LOG_DEBUG, "Default HTML type set for %s%s => %s",
                    adr_nologin(opt, adr_complete), fil_complete, afs->save);
    }
  }
  // éviter les / au début (cause: N100)
  if (afs->save[0] == '/') {
    char BIGSTK tempo[HTS_URLMAXSIZE * 2];

    strcpybuff(tempo, afs->save + 1);
    strcpybuff(afs->save, tempo);
  }

  /* Cleanup reserved or forbidden characters. */
  {
    size_t i;
    for(i = 0 ; afs->save[i] != '\0' ; i++) {
      unsigned char c = (unsigned char) afs->save[i];
      if (c < 32       // control
          || c == 127  // unwise
          || c == '~'  // unix unwise
          || c == '\\' // windows separator
          || c == ':'  // windows forbidden
          || c == '*'  // windows forbidden
          || c == '?'  // windows forbidden
          || c == '\"' // windows forbidden
          || c == '<'  // windows forbidden
          || c == '>'  // windows forbidden
          || c == '|'  // windows forbidden
          //|| c == '@' // ?
          || (opt->savename_83 == HTS_SAVENAME_83_ISO9660 // CDROM
              && (c == '-' || c == '=' || c == '+'))) {
        afs->save[i] = '_';
      }
    }
  }

  // éliminer les // (comme ftp://)
  cleanDoubleSlash(afs->save);

  /* Before path_html is prepended, so the user's own output directory keeps
     its name. */
  url_savename_escape_reserved(afs->save, sizeof(afs->save));

  /* Strip ending . or ' ' forbidden on windoz */
  cleanEndingSpaceOrDot(afs->save);
  /* Blanking a dots-only component leaves '//' behind. */
  cleanDoubleSlash(afs->save);

  // conversion 8-3 .. y compris pour les répertoires
  if (opt->savename_83) {
    char BIGSTK n83[HTS_URLMAXSIZE * 2];

    long_to_83(opt->savename_83, n83, sizeof(n83), afs->save);
    strcpybuff(afs->save, n83);
  }
  // enforce stricter ISO9660 compliance (bug reported by Steffo Carlsson)
  // Level 1 File names are restricted to 8 characters with a 3 character extension, 
  // upper case letters, numbers and underscore; maximum depth of directories is 8.
  // This will be our "DOS mode"
  // L2: 31 characters
  // A-Z,0-9,_
  if (opt->savename_83 > 0) {
    char *a, *last;

    for (last = hts_lastcharptr(afs->save);
         last != afs->save && *last != '/' && *last != '\\' && *last != '.';
         last--)
      ;
    if (*last != '.') {
      last = NULL;
    }
    for(a = afs->save; *a != '\0'; a++) {
      if (*a >= 'a' && *a <= 'z') {
        *a -= 'a' - 'A';
      } else if (*a == '.') {
        if (a != last) {
          *a = '_';
        }
      } else
        if (!
            ((*a >= 'A' && *a <= 'Z') || (*a >= '0' && *a <= '9') || *a == '_'
             || *a == '/' || *a == '\\')) {
        *a = '_';
      }
    }
  }

  /* ensure that there is no ../ (potential vulnerability) */
  fil_simplifie(afs->save);

  /* convert name to UTF-8 ? Note: already done while parsing. */

  /* cleanEndingSpaceOrDot() can restore a leading slash. Strip it so the
     reserved-name and 8.3 passes above see the same name. */
  stripLeadingSlashes(afs->save);

  /* callback */
  {
    /* The hook's name joins the output directory unsanitized, so refuse an
       absolute or ".." name, because an absolute one has no "../" to drop. */
    char BIGSTK engine_save[HTS_URLMAXSIZE * 2];

    strcpybuff(engine_save, afs->save);
    RUN_CALLBACK5(opt, savename, adr_complete, fil_complete, referer_adr,
                  referer_fil, afs->save);
    if (!hts_path_is_relative_contained(afs->save)) {
      hts_log_print(opt, LOG_WARNING,
                    "engine: save-name: the hook returned \"%s\", which leaves "
                    "the mirror, so the engine keeps \"%s\"",
                    afs->save, engine_save);
      strcpybuff(afs->save, engine_save);
    }
  }

  hts_log_print(opt, LOG_DEBUG, "engine: save-name: local name: %s%s -> %s",
                adr_nologin(opt, adr), fil, afs->save);

  /* Ensure that the MANDATORY "temporary" extension is set */
  if (ext_chg_delayed && !IS_DELAYED_EXT(afs->save))
    save_append_tail(afs->save, sizeof(afs->save), "." DELAYED_EXT);
  // Cap the save path: the final parent+name is copied into a fixed buffer that
  // aborts() on overflow (htssafe.h), so clamp every ceiling to fit it.
#define HTS_SAVE_BUFSIZE (HTS_URLMAXSIZE * 2) /* sizeof(afs->save) */
#define HTS_PATH_TAIL_RESERVE 64 /* collision suffix + ".delayed" + NUL */
#ifdef _WIN32
  // MAX_PATH minus 8.3 headroom (MSDN) minus the ".delayed" marker; raising it
  // needs the engine to "\\?\"-prefix its paths, which is separate work.
#define HTS_MAX_PATH_LEN (260 - 12 - 12)
#define MAX_SEG_LEN 48
#else
  // #133: use the platform's own PATH_MAX/NAME_MAX (Linux/Android 4096, macOS
  // 1024) rather than the far smaller Windows MAX_PATH.
#ifdef PATH_MAX
#define HTS_PATH_MAX_ PATH_MAX
#else
#define HTS_PATH_MAX_ 1024
#endif
#ifdef NAME_MAX
#define HTS_NAME_MAX_ NAME_MAX
#else
#define HTS_NAME_MAX_ 255
#endif
#define HTS_MAX_PATH_LEN                                                       \
  ((HTS_PATH_MAX_ - HTS_PATH_TAIL_RESERVE) <                                   \
           (HTS_SAVE_BUFSIZE - HTS_PATH_TAIL_RESERVE)                          \
       ? (HTS_PATH_MAX_ - HTS_PATH_TAIL_RESERVE)                               \
       : (HTS_SAVE_BUFSIZE - HTS_PATH_TAIL_RESERVE))
#define MAX_SEG_LEN (HTS_NAME_MAX_ > 64 ? HTS_NAME_MAX_ - 16 : HTS_NAME_MAX_)
#endif
#define MIN_LAST_SEG_RESERVE 12
#define MAX_LAST_SEG_RESERVE 24
  if (hts_stringLengthUTF8(afs->save) +
      hts_stringLengthUTF8(StringBuff(opt->path_html_utf8)) >=
      HTS_MAX_PATH_LEN) {
    // Cut by character but keep the original bytes, so the name cannot grow.
    size_t nChars;
    size_t *charByte = utf8_char_offsets(afs->save, &nChars);
    if (charByte != NULL) {
      const size_t parentLen =
        hts_stringLengthUTF8(StringBuff(opt->path_html_utf8));
      // parent path length is not insane (otherwise, ignore and pick 200 as
      // suffix length)
      const size_t maxLen =
        parentLen <
        HTS_MAX_PATH_LEN - HTS_MAX_PATH_LEN / 4
        ? HTS_MAX_PATH_LEN - parentLen : HTS_MAX_PATH_LEN;
      size_t i, j, lastSeg, lastSegSize, dirSize;
      size_t outByte = 0;
      // byte where the kept tail starts, read before the cut moves bytes
      const size_t markByte = save_tail_offset(afs->save);

      // pick up last segment
      for (i = 0, lastSeg = 0; i < nChars; i++) {
        if (afs->save[charByte[i]] == '/') {
          lastSeg = i + 1;
        }
      }
      lastSegSize = nChars - lastSeg;
      if (lastSegSize > MAX_LAST_SEG_RESERVE) {
        lastSegSize = MAX_LAST_SEG_RESERVE;
      }
      else if (lastSegSize < MIN_LAST_SEG_RESERVE) {
        lastSegSize = MIN_LAST_SEG_RESERVE;
      }

      // add as much pathes as we can (i and j count characters)
      for(i = 0, j = 0, dirSize = 0
        ; i + 1 < lastSeg && j + lastSegSize < maxLen; i++) {
          // reset segment counting
          if (afs->save[charByte[i]] == '/') {
            dirSize = 0;
          }

          // copy if not too long
          if (dirSize < MAX_SEG_LEN) {
            utf8_keep_char(afs->save, charByte, i, &outByte);
            j++;
            dirSize++;
          }
      }

      // last segment, after its separator if the name has a directory part
      if (lastSeg > 0) {
        utf8_keep_char(afs->save, charByte, lastSeg - 1, &outByte);
        j++;
      }
#define MAX_UTF8_SEQ_CHARS 4
      {
        // #623: the ".delayed" placeholder marker sits at the tail; cutting
        // through it drops IS_DELAYED_EXT, so the file is never renamed to its
        // final name. Reserve the trailing ".<id>.delayed" across the cut.
        size_t markStart;
        // #852: clamp the name like any directory segment; it was bounded by
        // the whole path alone, so one component could run to maxLen.
        size_t tailLen, headLen;

        // the same tail start as a character index
        for (markStart = nChars;
             markStart > 0 && charByte[markStart - 1] >= markByte; markStart--)
          ;
        tailLen = nChars - markStart;
        headLen = tailLen < MAX_SEG_LEN ? MAX_SEG_LEN - tailLen : 0;

        // head, bounded so the reserved tail still fits, then the tail itself
        for (i = lastSeg; i < markStart && i - lastSeg < headLen &&
                          j < maxLen && tailLen < maxLen - j;
             i++, j++)
          utf8_keep_char(afs->save, charByte, i, &outByte);
        for (i = markStart; i < nChars && j < maxLen; i++, j++)
          utf8_keep_char(afs->save, charByte, i, &outByte);
      }
      afs->save[outByte] = '\0';
      freet(charByte);

      // log in debug
      hts_log_print(opt, LOG_DEBUG, "Too long filename shortened: %s%s => %s",
                    adr_nologin(opt, adr_complete), fil_complete, afs->save);
    } else {
      hts_log_print(opt, LOG_ERROR, "Out of memory shortening: %s", afs->save);
    }

    // Re-check again ending space or dot after cut (see bug #5)
    cleanEndingSpaceOrDot(afs->save);
  }
  // Parent+name must fit a fixed byte buffer, so hard-cut on a codepoint
  // boundary (#133).
  {
    const size_t parentBytes = strlen(StringBuff(opt->path_html_utf8));
    const size_t cap = HTS_SAVE_BUFSIZE - HTS_PATH_TAIL_RESERVE;
    // Shrink only the name, because an empty name would collide across URLs.
    if (parentBytes < cap) {
      const size_t budget = cap - parentBytes;
      if (strlen(afs->save) > budget) {
        const size_t off = save_tail_offset(afs->save);

        /* cut the name, keep its tail */
        if (strlen(afs->save + off) < budget) {
          char BIGSTK tail[HTS_URLMAXSIZE * 2];

          strcpybuff(tail, afs->save + off);
          afs->save[off] = '\0';
          save_append_tail(afs->save, budget + 1, tail);
        } else {
          afs->save[utf8_cut(afs->save, budget)] = '\0';
        }
        cleanEndingSpaceOrDot(afs->save);
      }
    }
  }
  /* The cuts above can blank a directory too. */
  cleanDoubleSlash(afs->save);
#undef MAX_UTF8_SEQ_CHARS
#undef MIN_LAST_SEG_RESERVE
#undef MAX_LAST_SEG_RESERVE
#undef MAX_SEG_LEN
#undef HTS_MAX_PATH_LEN
#undef HTS_PATH_TAIL_RESERVE
#undef HTS_SAVE_BUFSIZE
#ifndef _WIN32
#undef HTS_PATH_MAX_
#undef HTS_NAME_MAX_
#endif

  // chemin primaire éventuel A METTRE AVANT
  if (strnotempty(StringBuff(opt->path_html_utf8))) {
    char BIGSTK tempo[HTS_URLMAXSIZE * 2];

    strcpybuff(tempo, StringBuff(opt->path_html_utf8));
    strcatbuff(tempo, afs->save);
    strcpybuff(afs->save, tempo);
  }
  // Before the collision search, so a name already taken this run still moves.
  if (ext_chg_delayed)
    url_savename_undelay(opt, afs->save, sizeof(afs->save), ext);

  // rename on collision with another link's name
  if (opt->liens != NULL) {
    const int i = hash_read(hash, afs->save, NULL, HASH_STRUCT_FILENAME);

    if (i >= 0 && savename_same_link(opt, i, normadr, normfil)) {
      /* the same link: keep its name, whatever its case */
      if (strcmp(heap(i)->sav, afs->save) != 0)
        strcpybuff(afs->save, heap(i)->sav);
    } else if (i >= 0) {
      /* the naming mode, which decides each name the walk tries */
      const char kind = (char) ('0' + opt->savename_83);
      /* A jump must not pass a name this link already holds. */
      const int owner = hash_savename_owner(hash, normadr, normfil);
      char BIGSTK first[HTS_URLMAXSIZE * 2];
      /* -2 means several names may be this link's, so walk name by name. */
      hts_boolean jump = owner != -2 ? HTS_TRUE : HTS_FALSE;
      hts_boolean regular = HTS_TRUE;
      hts_boolean same;
      int last = 0, steps, s;

      strcpybuff(first, afs->save);
      steps =
          savename_walk(afs->save, sizeof(afs->save), opt, hash, normadr,
                        normfil, max_char, kind, jump, &last, &same, &regular);
      if (owner >= 0 && !same) {
        const char *const name = heap(owner)->sav;
        const int j = hash_read(hash, name, NULL, HASH_STRUCT_FILENAME);

        if (!regular) {
          /* no way to tell what the jumps passed: walk name by name */
          strcpybuff(afs->save, first);
          jump = HTS_FALSE;
          regular = HTS_TRUE;
          steps = savename_walk(afs->save, sizeof(afs->save), opt, hash,
                                normadr, normfil, max_char, kind, jump, &last,
                                &same, &regular);
        } else if (j >= 0 && savename_same_link(opt, j, normadr, normfil) &&
                   savename_walk_skipped(opt, max_char, first, last, name)) {
          /* a walk name by name would have stopped there */
          strcpybuff(afs->save, heap(j)->sav);
        }
      }
      /* every name this walk passed leads to LAST: let later walks jump */
      if (regular) {
        char BIGSTK node[HTS_URLMAXSIZE * 2];

        strcpybuff(node, first);
        for (s = 0; s < steps; s++) {
          const int skip = jump ? hash_savename_next(hash, kind, node) : 0;

          hash_savename_next_set(hash, kind, node, last);
          (void) savename_collision_next(node, sizeof(node), opt, max_char,
                                         skip, &regular);
        }
      }
    }
  }

  return 0;
}

/* Build the md5-based name into b (capacity bsize). */
void standard_name(char *b, size_t bsize, const char *dot_pos,
                   const char *nom_pos, const char *fil, int short_ver) {
  char md5[32 + 2];
  htsbuff bb;
  size_t room, md5_len, ext_len, ext_tail, name_len;

  assertf(bsize != 0);
  bb = htsbuff_ptr(b, bsize);
  room = bsize - 1;
  md5_len = strnlen(url_md5(md5, sizeof(md5), fil), 4);
  ext_len = dot_pos == NULL
                ? 0
                : (short_ver ? strnlen(dot_pos + 1, 3) : strlen(dot_pos + 1));
  /* the "." plus the extension, or 0 once the extension is dropped */
  ext_tail = dot_pos != NULL ? 1 + ext_len : 0;
  if (dot_pos != NULL)
    name_len = short_ver ? (size_t) min(dot_pos - nom_pos, 8)
                         : (size_t) (dot_pos - nom_pos);
  else
    name_len = short_ver ? strnlen(nom_pos, 8) : strlen(nom_pos);
  /* too long: drop the extension, then the md5, but keep one name byte */
  if (md5_len + ext_tail > room || name_len > room - (md5_len + ext_tail)) {
    const size_t min_name = name_len > 0 ? 1 : 0;

    if (ext_tail > room - min(room, min_name + md5_len))
      ext_tail = 0;
    if (md5_len + ext_tail > room - min(room, min_name))
      md5_len = 0;
    name_len = min(name_len, room - min(room, md5_len + ext_tail));
  }
  if (name_len + md5_len + ext_tail == 0 && dot_pos != NULL) {
    /* a bare ".ext" name: keep its bytes, never a new dotfile */
    htsbuff_catn(&bb, dot_pos + 1, room);
    return;
  }
  htsbuff_catn(&bb, nom_pos, name_len);
  /* MD5 - 16 bits */
  htsbuff_catn(&bb, md5, md5_len);
  if (ext_tail != 0) {
    htsbuff_catc(&bb, '.');
    htsbuff_catn(&bb, dot_pos + 1, ext_len);
  }
}

char *url_md5(char *digest, size_t size, const char *fil) {
  const char *const query = strchr(fil, '?');
  char hex[33];

  assertf(size != 0);
  digest[0] = '\0';
  if (query != NULL) {
    domd5mem(query + 1, strlen(query + 1), hex, 1);
    (void) strclipbuff(digest, size, hex);
  }
  return digest;
}

void url_savename_addstr(char *d, size_t dsize, const char *s) {
  size_t i = strlen(d);

  if (i + 1 >= dsize) /* nothing fits, and d already carries its NUL */
    return;
  while (*s != '\0' && i + 1 < dsize) {
    d[i++] = *s == '\\' ? '/' : *s;
    s++;
  }
  d[i] = '\0';
}

hts_boolean url_savename_refname(const char *adr, const char *fil,
                                 char *filename, size_t size) {
  unsigned char bindigest[16];
  char hex[33];
  struct MD5Context ctx;

  hts_md5_init(&ctx);
  MD5Update(&ctx, (const unsigned char *) adr, (int) strlen(adr));
  MD5Update(&ctx, (const unsigned char *) ",", 1);
  MD5Update(&ctx, (const unsigned char *) fil, (int) strlen(fil));
  MD5Final(bindigest, &ctx);
  hts_md5_hex(bindigest, hex);
  if (snprintf(filename, size, CACHE_REFNAME "/%s.ref", hex) < (int) size)
    return HTS_TRUE;
  filename[0] = '\0';
  return HTS_FALSE;
}

/* note: return a local filename */
char *url_savename_refname_fullpath(httrackp * opt, const char *adr,
                                    const char *fil) {
  char digest_filename[64];
  const hts_boolean fits =
      url_savename_refname(adr, fil, digest_filename, sizeof(digest_filename));

  assertf(fits);
  return hts_logpath(opt, digest_filename);
}

/* Remove any refname, and say if one was removed. */
hts_boolean url_savename_refname_remove(httrackp *opt, const char *adr,
                                        const char *fil) {
  char *filename = url_savename_refname_fullpath(opt, adr, fil);

  if (UNLINK(filename) != 0)
    return HTS_FALSE;
  if (opt->refs_written != NULL)
    (void) coucal_remove((coucal) opt->refs_written, filename);
  return HTS_TRUE;
}

void url_savename_refname_written(httrackp *opt, const char *filename) {
  if (opt->refs_written != NULL)
    coucal_add((coucal) opt->refs_written, filename, 1);
}

hts_boolean url_savename_refname_may_exist(httrackp *opt,
                                           const char *filename) {
  return opt->refs_written == NULL ||
         coucal_exists((coucal) opt->refs_written, filename);
}

void url_savename_refnames_start(httrackp *opt) {
  url_savename_refnames_free(opt);
  /* Only a certain absence counts: any other failure probes every ref. */
  if (hts_path_absent(hts_logpath(opt, CACHE_REFNAME)))
    opt->refs_written = coucal_new(0);
}

void url_savename_refnames_free(httrackp *opt) {
  if (opt->refs_written != NULL) {
    coucal table = (coucal) opt->refs_written;

    coucal_delete(&table);
    opt->refs_written = NULL;
  }
}
