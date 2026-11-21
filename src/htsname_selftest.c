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

/* ------------------------------------------------------------ */
/* File: htsname_selftest.c subroutines:                        */
/*       self-tests for URL parsing and save-name building      */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

static int st_refname(httrackp *opt, int argc, char **argv) {
  char name[64];

  url_savename_refname(argv[0], argv[1], name);
  printf("refname=%s\n", name);
  return 0;
}

static int st_simplify(httrackp *opt, int argc, char **argv) {
  fil_simplifie(argv[0]);
  printf("simplified=%s\n", argv[0]);
  return 0;
}

static int st_expandhome(httrackp *opt, int argc, char **argv) {
  String path = STRING_EMPTY;

  StringCopy(path, argv[0]);
  expand_home(&path);
  printf("expanded=%s\n", StringBuff(path));
  StringFree(path);
  return 0;
}

static int st_relative(httrackp *opt, int argc, char **argv) {
  char s[HTS_URLMAXSIZE * 2];

  if (lienrelatif(s, sizeof(s), argv[0], argv[1]) == 0)
    printf("relative=%s\n", s);
  else
    printf("relative=<ERROR>\n");
  return 0;
}

static int st_resolve(httrackp *opt, int argc, char **argv) {
  lien_adrfil af;
  int r;

  r = ident_url_relatif(argv[0], argv[1], argv[2], &af);
  if (r == 0)
    printf("adr=%s fil=%s\n", af.adr, af.fil);
  else
    printf("error=%d\n", r);
  return 0;
}

/* Print the ":port" jump_toport_const() finds in a URL, or "(none)". */
static int st_toport(httrackp *opt, int argc, char **argv) {
  int i;

  for (i = 0; i < argc; i++) {
    const char *const port = jump_toport_const(argv[i]);

    printf("%s\n", port != NULL ? port : "(none)");
  }
  return 0;
}

/* Split a URL into (adr, fil), or print "error" if rejected. A second arg pads
   the URL with that many 'a's to reach lengths a CLI arg can't. */
/* Every ident_url_relatif() arm has to keep fil under HTS_URLMAXSIZE, because
   callers rebuild "http://" + adr + "/" + fil into a buffer sized from that. */
static int st_identrel(httrackp *opt, int argc, char **argv) {
  static const char *const heads[] = {"?", "r", "/", ""};
  const size_t nheads = sizeof(heads) / sizeof(heads[0]);
  char BIGSTK origin[HTS_URLMAXSIZE * 2];
  char BIGSTK lien[HTS_URLMAXSIZE * 2];
  lien_adrfil af;
  size_t k, pad;

  /* origin_fil just under the ceiling its own entry test allows */
  origin[0] = '/';
  memset(origin + 1, 'o', HTS_URLMAXSIZE - 3);
  origin[HTS_URLMAXSIZE - 2] = '\0';
  for (k = 0; k < nheads; k++) {
    const size_t head = strlen(heads[k]);

    for (pad = 0; head + pad + 2 < HTS_URLMAXSIZE; pad += 97) {
      memcpy(lien, heads[k], head);
      memset(lien + head, 'q', pad);
      lien[head + pad] = '\0';
      if (ident_url_relatif(lien, "www.example.com", origin, &af) >= 0) {
        assertf(strlen(af.adr) < HTS_URLMAXSIZE);
        assertf(strlen(af.fil) < HTS_URLMAXSIZE);
      }
    }
  }
  printf("identrel self-test OK\n");
  return 0;
}

static int st_identurl(httrackp *opt, int argc, char **argv) {
  lien_adrfil af;
  char *url;
  size_t len, pad = 0;

  if (argc >= 2)
    pad = (size_t) atoi(argv[1]);
  len = strlen(argv[0]);
  url = malloct(len + pad + 1);
  memcpy(url, argv[0], len);
  memset(url + len, 'a', pad);
  url[len + pad] = '\0';
  if (ident_url_absolute(url, &af) >= 0)
    printf("adr=%s fil=%s\n", af.adr, af.fil);
  else
    printf("error\n");
  freet(url);
  return 0;
}

/* A 2047-byte hostless "?"-URL must not abort once '/' makes fil 2048. */
static int st_identabs(httrackp *opt, int argc, char **argv) {
  lien_adrfil af;
  const size_t len =
      sizeof(af.fil) - 1; /* 2047: max URL the top guard admits */
  char *url = malloct(len + 1);

  url[0] = '?';
  memset(url + 1, 'a', len - 1);
  url[len] = '\0';
  assertf(ident_url_absolute(url, &af) == -1);
  freet(url);
  /* valid URLs still parse, so the guard is not over-rejecting */
  assertf(ident_url_absolute("http://www.example.com/a/b/c.html?x=1", &af) ==
          0);
  assertf(ident_url_absolute("www.foo.com?bar=1", &af) == 0);
  printf("identabs self-test OK\n");
  return 0;
}

/* Only a scheme's own default port is stripped (#638, #614, #627). */
static int st_stripport(httrackp *opt, int argc, char **argv) {
  static const struct {
    const char *in, *out;
  } cases[] = {
      {"http://127.0.0.1:80/x", "http://127.0.0.1/x"},
      {"http://127.0.0.1:080/x", "http://127.0.0.1/x"},
      {"http://127.0.0.1:0080/x", "http://127.0.0.1/x"},
      {"http://127.0.0.1:80", "http://127.0.0.1"},
      {"http://127.0.0.1:0081/x", "http://127.0.0.1:0081/x"},
      {"http://127.0.0.1:81/x", "http://127.0.0.1:81/x"},
      {"http://127.0.0.1:8080/x", "http://127.0.0.1:8080/x"},
      {"http://127.0.0.1:4294967376/x", "http://127.0.0.1:4294967376/x"},
      {"http://127.0.0.1/x", "http://127.0.0.1/x"},
      {"https://127.0.0.1:443/x", "https://127.0.0.1/x"},
      {"https://127.0.0.1:80/x", "https://127.0.0.1:80/x"},
      // Scheme match is case-insensitive: HTTPS' default is 443, so :80 stays.
      {"HTTPS://127.0.0.1:80/x", "HTTPS://127.0.0.1:80/x"},
      {"ftp://127.0.0.1:21/x", "ftp://127.0.0.1/x"},
      {"ftp://127.0.0.1:80/x", "ftp://127.0.0.1:80/x"},
      {"http://127.0.0.1:443/x", "http://127.0.0.1:443/x"},
      // A ":80" past the authority is part of the path, query or fragment.
      {"http://127.0.0.1/x:80", "http://127.0.0.1/x:80"},
      {"http://127.0.0.1/a:80b", "http://127.0.0.1/a:80b"},
      {"http://127.0.0.1/q?u=1:80", "http://127.0.0.1/q?u=1:80"},
      {"http://127.0.0.1?u=1:80", "http://127.0.0.1?u=1:80"},
      {"http://127.0.0.1?u=a@b:80", "http://127.0.0.1?u=a@b"}, /* host is b */
      {"http://127.0.0.1#f:80", "http://127.0.0.1#f:80"},
      {"http://[::1]/x:80", "http://[::1]/x:80"},
      {"https://127.0.0.1/x:443", "https://127.0.0.1/x:443"},
      {"http://127.0.0.1:80/x:80", "http://127.0.0.1/x:80"},
      {"http://127.0.0.1:80?x]", "http://127.0.0.1?x]"},
      {"http://[::1]:80/x", "http://[::1]/x"},
  };

  size_t k;

  for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
    char BIGSTK buff[HTS_URLMAXSIZE * 2];

    strcpybuff(buff, cases[k].in);
    hts_strip_default_port(buff, sizeof(buff));
    assertf(strcmp(buff, cases[k].out) == 0);
  }
  printf("stripport self-test OK\n");
  return 0;
}

/* A poisoned run right behind a destination catches whatever a bounded write
   appends past its end; save[] ends lien_adrfilsave (#1269). */
#define ST_POISON 0x5a
#define ST_SAVENAME_CANARY 128

/* an off-by-one terminator writes a NUL, which a zero poison could not see */
enum { st_poison_is_not_nul = 1 / (ST_POISON != 0) };

/* Does the run from `from` to `size` still hold the poison? */
static hts_boolean st_poison_intact(const char *label, const char *buf,
                                    size_t from, size_t size) {
  size_t i;

  for (i = size; i > from; i--) {
    if (buf[i - 1] != (char) ST_POISON) {
      fprintf(stderr, "%s: wrote %d byte(s) past the end\n", label,
              (int) (i - from));
      return HTS_FALSE;
    }
  }
  return HTS_TRUE;
}

/* One get_ext() into a poisoned arena of `cap` bytes: the extension must come
   out whole and nothing may be written past `cap`. */
static hts_boolean st_getext_case(char *arena, size_t cap, size_t guard,
                                  const char *fil, const char *want) {
  hts_boolean ok = HTS_TRUE;
  const char *ext;
  size_t from;

  memset(arena, ST_POISON, cap + guard);
  ext = get_ext(arena, cap, fil);
  /* every check runs: a wrong answer must not stop the canary from reporting */
  if (strcmp(ext, want) != 0) {
    fprintf(stderr, "getext: [%s] in %d bytes gave [%s], wanted [%s]\n", fil,
            (int) cap, ext, want);
    ok = HTS_FALSE;
  }
  /* the answer is built in the caller's buffer, and only the refusal is not */
  if ((ext == arena) != (*want != '\0')) {
    fprintf(stderr, "getext: [%s] in %d bytes answered from the wrong buffer\n",
            fil, (int) cap);
    ok = HTS_FALSE;
  }
  /* an extension that did not fit leaves the whole arena poisoned; one that
     fitted ends on its own NUL */
  from = ext == arena ? strlen(ext) + 1 : 0;
  if (!st_poison_intact("getext: inside the destination", arena, from, cap))
    ok = HTS_FALSE;
  if (!st_poison_intact("getext: past the destination", arena, cap,
                        cap + guard))
    ok = HTS_FALSE;
  return ok;
}

/* get_ext() reads the extension before '?' and stays in bounds (#1433). */
static int st_getext(httrackp *opt, int argc, char **argv) {
  static const struct {
    const char *fil;
    const char *want;
  } cases[] = {
      {"x.php", "php"},
      {"x.php?id=3", "php"},
      /* the query is not scanned for dots */
      {"x.php?id=3.txt", "php"},
      {"/a/b.c?Q123456789012345678901234567890", "c"},
      {"x.PHP?a", "PHP"}, /* no case folding */
      {"x.php?", "php"},
      {"foo", ""},
      {"x.", ""},
      {"?x.php", ""},
  };

  /* no sizeof(void *), which the size check takes for a pointer argument */
  static const size_t caps[] = {1, 2, 3, 5, 6, 16, 64};
  /* longer than any capacity below, so a size-bounded copy runs off the end */
  static const char query[] = "?q=00000000001111111111222222222233333333334"
                              "4444444445555555555666666666677777777778888";
  const size_t guard = 64;
  size_t c, k;
  int rc = 0;

  /* the semantics, in a destination nothing here comes close to filling */
  {
    char *arena = malloct(256 + guard);

    assertf(arena != NULL);
    for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
      if (!st_getext_case(arena, 256, guard, cases[k].fil, cases[k].want))
        rc = 1;
    }
    freet(arena);
  }

  /* the point of the query half: a parameterised URL types as the bare one */
  {
    char buf[16];

    if (!is_dyntype(get_ext(buf, sizeof(buf), "x.php?id=3")) ||
        !is_dyntype(get_ext(buf, sizeof(buf), "x.php"))) {
      fprintf(stderr, "getext: a parameterised .php is not a dynamic type\n");
      rc = 1;
    }
  }

  /* a one-size-fits-all bound fails on the small capacities */
  for (c = 0; c < sizeof(caps) / sizeof(caps[0]); c++) {
    const size_t cap = caps[c];
    const size_t maxext = cap + guard;
    char *arena = malloct(cap + guard);
    char *fil = malloct(5 + maxext + sizeof(query));
    char *want = malloct(maxext + 1);

    assertf(arena != NULL && fil != NULL && want != NULL);
    for (k = 1; k <= maxext; k++) {
      size_t i;

      /* bytes that vary, or a copy taken from the wrong offset would match */
      memcpy(fil, "/d/f.", 5);
      for (i = 0; i < k; i++)
        fil[5 + i] = (char) ('a' + i % 26);
      fil[5 + k] = '\0';
      memcpy(want, fil + 5, k + 1);
      if (k >= cap) /* k characters plus the NUL do not fit */
        want[0] = '\0';
      if (!st_getext_case(arena, cap, guard, fil, want))
        rc = 1;
      memcpy(fil + 5 + k, query, sizeof(query));
      if (!st_getext_case(arena, cap, guard, fil, want))
        rc = 1;
    }
    freet(arena);
    freet(fil);
    freet(want);
  }

  printf("getext self-test %s\n", rc == 0 ? "OK" : "FAILED");
  return rc;
}

/* Write the bytes a body= style argument decodes to into path. */
static hts_boolean st_savename_write_body(const char *path, const char *arg) {
  char BIGSTK data[1024];
  const size_t n = st_decode_body(arg, data, sizeof(data));
  FILE *const fp = fopen(path, "wb");

  if (fp == NULL || !hts_fwrite_exact(data, n, fp)) {
    fprintf(stderr, "savename: can not write %s\n", path);
    if (fp != NULL)
      fclose(fp);
    return HTS_FALSE;
  }
  fclose(fp);
  return HTS_TRUE;
}

/* Copies src into dst with its '*' expanded to pad bytes of padunit. Is there a
   '*' and room for the result? */
static hts_boolean st_expand_pad(char *dst, size_t size, const char *src,
                                 int pad, const char *padunit) {
  const char *const star = strchr(src, '*');
  const size_t unit = strlen(padunit);
  size_t head, j;

  if (star == NULL || pad < 0 || strlen(src) > size ||
      (size_t) pad > size - strlen(src))
    return HTS_FALSE;
  head = (size_t) (star - src);
  memcpy(dst, src, head);
  for (j = 0; j < (size_t) pad; j++)
    dst[head + j] = padunit[j % unit];
  strlcpybuff(dst + head + pad, star + 1, size - head - pad);
  return HTS_TRUE;
}

static int st_savename(httrackp *opt, int argc, char **argv) {
  struct {
    lien_adrfilsave afs;
    char canary[ST_SAVENAME_CANARY];
  } probe;

  /* the canary is only behind save[] if the struct has no trailing padding */
  enum {
    st_savename_packed =
        1 / (offsetof(lien_adrfilsave, save) + sizeof(probe.afs.save) ==
             sizeof(lien_adrfilsave))
  };

  lien_adrfilsave *const afs = &probe.afs;
  cache_back cache;
  struct_back *sback;
  hash_struct hash;
  lien_back headers;
  const char *adr = "www.example.com";
  const char *cdispo = NULL;
  const char *body = NULL;
  const char *cached = NULL;
  const char *tmpbody = NULL;
  const char *coding = NULL;
  const char *bodyfile = "st-savename-body.tmp";
  const char *tmpbodyfile = "st-savename-tmpfile.tmp";
  int statuscode = HTTP_OK, status = 0;
  int filpad = 0, adrpad = 0;
  const char *padunit = "a"; /* the bytes filpad repeats */
  int nosback = 0;
  int rc = 0;
  int i;

  /* knobs first: hash_init and the prior links depend on them */
  for (i = 2; i < argc; i++) {
    const char *const a = argv[i];

    if (strncmp(a, "adr=", 4) == 0)
      adr = a + 4;
    else if (strncmp(a, "cdispo=", 7) == 0)
      cdispo = a + 7;
    else if (strncmp(a, "statuscode=", 11) == 0)
      statuscode = atoi(a + 11);
    else if (strncmp(a, "status=", 7) == 0)
      status = atoi(a + 7);
    else if (strncmp(a, "strip=", 6) == 0)
      StringCopy(opt->strip_query, a + 6);
    else if (strncmp(a, "urlhack=", 8) == 0)
      opt->urlhack = atoi(a + 8) ? HTS_TRUE : HTS_FALSE;
    else if (strncmp(a, "no-www=", 7) == 0)
      opt->no_www_dedup = atoi(a + 7) ? HTS_TRUE : HTS_FALSE;
    else if (strncmp(a, "no-slash=", 9) == 0)
      opt->no_slash_dedup = atoi(a + 9) ? HTS_TRUE : HTS_FALSE;
    else if (strncmp(a, "no-query=", 9) == 0)
      opt->no_query_dedup = atoi(a + 9) ? HTS_TRUE : HTS_FALSE;
    else if (strncmp(a, "n83=", 4) == 0)
      opt->savename_83 = atoi(a + 4);
    else if (strncmp(a, "type=", 5) == 0)
      opt->savename_type = atoi(a + 5);
    else if (strncmp(a, "body=", 5) == 0)
      body = a + 5;
    else if (strncmp(a, "tmpbody=", 8) == 0) /* the slot's tmpfile */
      tmpbody = a + 8;
    else if (strncmp(a, "coding=", 7) == 0) /* Content-Encoding */
      coding = a + 7;
    else if (strncmp(a, "cached=", 7) == 0)
      cached = a + 7;
    else if (strncmp(a, "filpad=", 7) == 0)
      filpad = atoi(a + 7);
    else if (strncmp(a, "adrpad=", 7) == 0) /* as filpad, for adr= */
      adrpad = atoi(a + 7);
    else if (strncmp(a, "padunit=", 8) == 0 && a[8] != '\0')
      padunit = a + 8;
    else if (strncmp(a, "delayed=", 8) == 0) /* -%N */
      opt->savename_delayed = (hts_savename_delayed) atoi(a + 8);
    else if (strncmp(a, "nosback=", 8) == 0) /* as -#C: no backing at all */
      nosback = atoi(a + 8);
    else if (strncmp(a, "userdef=", 8) == 0) { /* -N, which selects type -1 */
      StringCopy(opt->savename_userdef, a + 8);
      opt->savename_type = -1;
    } else if (strncmp(a, "prior=", 6) != 0) {
      fprintf(stderr, "savename: unknown arg '%s'\n", a);
      return 1;
    }
  }
  /* -N is the only thing that selects type -1, and it always sets the template
     the naming path then walks; without one url_savename reads a NULL. */
  if (opt->savename_type == -1 && StringLength(opt->savename_userdef) == 0) {
    fprintf(stderr, "savename: type=-1 needs a userdef= template\n");
    return 1;
  }
  memset(&probe, 0, sizeof(probe));
  memset(probe.canary, ST_POISON, sizeof(probe.canary));
  if (adrpad > 0) {
    if (!st_expand_pad(afs->af.adr, sizeof(afs->af.adr), adr, adrpad,
                       padunit)) {
      fprintf(stderr, "savename: adrpad needs a '*' in the adr, and room\n");
      return 1;
    }
  } else {
    strcpybuff(afs->af.adr, adr);
  }
  if (filpad > 0) {
    /* '*' stands for filpad bytes: a link longer than argv can carry, argv
       being capped at HTS_CDLMAXSIZE (htscoremain.c) */
    if (!st_expand_pad(afs->af.fil, sizeof(afs->af.fil), argv[0], filpad,
                       padunit)) {
      fprintf(stderr, "savename: filpad needs a '*' in the fil, and room\n");
      return 1;
    }
  } else {
    strcpybuff(afs->af.fil, argv[0]);
  }

  memset(&cache, 0, sizeof(cache));
  if (cached != NULL) { /* cached=<content-type>|<save name> */
    char *dup = strdupt(cached);
    char *const sep = strchr(dup, '|');
    char locbuf[64] = "";
    htsblk cr;

    if (sep == NULL) {
      fprintf(stderr, "savename: cached needs ctype|save\n");
      freet(dup);
      return 1;
    }
    *sep = '\0';
    /* one-entry cache in cwd, reopened read-only; body is PNG magic on
       purpose: only the recorded name (X-Save) may drive the naming */
    StringCopy(opt->path_log, "");
    cache.type = 1;
    cache.log = cache.errlog = stderr;
    cache.hashtable = coucal_new(0);
    cache_init(&cache, opt);
    hts_init_htsblk(&cr);
    cr.statuscode = HTTP_OK;
    strcpybuff(cr.msg, "OK");
    strcpybuff(cr.contenttype, dup);
    cr.location = locbuf;
    cr.adr = strdupt("\x89PNG\r\n\x1a\n");
    cr.size = 8;
    cache_add(opt, &cache, &cr, adr, argv[0], sep + 1, 1, NULL);
    freet(cr.adr);
    st_cache_close(opt, &cache); /* the memset below orphans what it holds */
    memset(&cache, 0, sizeof(cache));
    cache.type = 1;
    cache.log = cache.errlog = stderr;
    cache.hashtable = coucal_new(0);
    cache.ro = 1;
    cache_init(&cache, opt);
    freet(dup);
  } else {
    cache.hashtable = (void *) coucal_new(0);
  }

  st_mirror_wiring(opt, &sback, &hash, nosback ? HTS_FALSE : HTS_TRUE);

  for (i = 2; i < argc; i++) {
    if (strncmp(argv[i], "prior=", 6) == 0) {
      char *dup = strdupt(argv[i] + 6);
      char *const p1 = strchr(dup, '|');
      char *const p2 = p1 != NULL ? strchr(p1 + 1, '|') : NULL;
      /* optional former (pre-redirect) adr|fil */
      char *const p3 = p2 != NULL ? strchr(p2 + 1, '|') : NULL;
      char *const p4 = p3 != NULL ? strchr(p3 + 1, '|') : NULL;

      if (p2 == NULL || (p3 != NULL && p4 == NULL)) {
        fprintf(stderr, "savename: prior needs adr|fil|sav[|adr|fil]\n");
        freet(dup);
        rc = 1;
        goto cleanup;
      }
      *p1 = *p2 = '\0';
      if (p3 != NULL)
        *p3 = *p4 = '\0';
      if (!hts_record_link(opt, dup, p1 + 1, p2 + 1, p3 != NULL ? p3 + 1 : "",
                           p3 != NULL ? p4 + 1 : "", NULL)) {
        freet(dup);
        rc = 1;
        goto cleanup;
      }
      freet(dup);
    }
  }

  memset(&headers, 0, sizeof(headers));
  headers.status = status;
  headers.r.statuscode = statuscode;
  strcpybuff(headers.r.contenttype, argv[1]);
  if (cdispo != NULL)
    strcpybuff(headers.r.cdispo, cdispo);
  strcpybuff(headers.url_fil, afs->af.fil);
  if (coding != NULL) {
    strcpybuff(headers.r.contentencoding, coding);
    headers.r.compressed = 1;
  }
  if (body != NULL) { /* leading body bytes, read via url_sav */
    if (!st_savename_write_body(bodyfile, body)) {
      rc = 1;
      goto cleanup;
    }
    strcpybuff(headers.url_sav, bodyfile);
    /* the sniff reads only files the slot wrote */
    headers.r.is_write = HTS_TRUE;
  }
  if (tmpbody != NULL) {
    if (!st_savename_write_body(tmpbodyfile, tmpbody)) {
      rc = 1;
      goto cleanup;
    }
    strcpybuff(headers.tmpfile_buffer, tmpbodyfile);
    headers.tmpfile = headers.tmpfile_buffer;
  }

  url_savename(afs, NULL, NULL, NULL, opt, sback, &cache, &hash, 0, 0,
               &headers);
  if (body != NULL)
    (void) UNLINK(bodyfile);
  if (tmpbody != NULL)
    (void) UNLINK(tmpbodyfile);
  if (!st_poison_intact("savename: save[]", probe.canary, 0,
                        sizeof(probe.canary)))
    rc = 1;
  else
    printf("savename: %s\n", afs->save);

cleanup:
  st_mirror_wiring_free(opt, &cache, &sback, &hash);
  return rc;
}

/* Case variants of one name each take the next free suffix, and the walk to it
   must not probe every suffix already taken: a page can list thousands. */
static int st_savename_collide(httrackp *opt, int argc, char **argv) {
  /* each letter's case varies, giving 2^11 paths */
  static const char word[] = "abcdefghijk";
  const int count = 2048;
  cache_back cache;
  struct_back *sback = NULL;
  hash_struct hash;
  int pass, k, rc = 0;

  StringCopy(opt->path_html_utf8, "");
  opt->savename_delayed = HTS_SAVENAME_DELAYED_NONE;
  memset(&cache, 0, sizeof(cache));
  cache.hashtable = coucal_new(0);
  cache.cached_tests = coucal_new(0);
  coucal_value_is_malloc(cache.cached_tests, 1);
  st_mirror_wiring(opt, &sback, &hash, HTS_FALSE);

  /* the second pass names the same links with the host's case changed */
  for (pass = 0; pass < 2 && rc == 0; pass++) {
    for (k = 0; k < count && rc == 0; k++) {
      lien_adrfilsave afs;
      char want[64];
      size_t j;

      memset(&afs, 0, sizeof(afs));
      strcpybuff(afs.af.adr, pass == 0 ? "example.com" : "EXAMPLE.com");
      strcpybuff(afs.af.fil, "/");
      strcatbuff(afs.af.fil, word);
      for (j = 0; j < sizeof(word) - 1; j++) {
        if ((k >> j) & 1)
          afs.af.fil[1 + j] = (char) toupper(afs.af.fil[1 + j]);
      }
      strcatbuff(afs.af.fil, ".html");
      url_savename(&afs, NULL, NULL, NULL, opt, sback, &cache, &hash, 0, 0,
                   NULL);
      snprintf(want, sizeof(want), "example.com%.*s", (int) sizeof(word),
               afs.af.fil);
      if (k != 0)
        snprintf(want + strlen(want), sizeof(want) - strlen(want), "-%d",
                 k + 1);
      strcatbuff(want, ".html");
      if (strcmp(afs.save, want) != 0) {
        fprintf(stderr, "savename-collide: %s%s named %s, not %s\n", afs.af.adr,
                afs.af.fil, afs.save, want);
        rc = 1;
      } else if (pass == 0 && !hts_record_link(opt, afs.af.adr, afs.af.fil,
                                               afs.save, "", "", NULL)) {
        rc = 1;
      }
    }
  }
  /* each name costs a few probes; probing every taken suffix costs count^2/2 */
  if (rc == 0 && hash.savename_probes > (size_t) count * 8) {
    fprintf(stderr, "savename-collide: %d names took %lu probes\n", count,
            (unsigned long) hash.savename_probes);
    rc = 1;
  }
  if (rc == 0)
    printf("savename-collide: %d names, %lu probes\n", count,
           (unsigned long) hash.savename_probes);
  coucal_delete(&cache.cached_tests);
  st_mirror_wiring_free(opt, &cache, &sback, &hash);
  return rc;
}

/* Replays url_savename() calls and checks that each name is the one a walk
   name by name gives, whatever jumps earlier calls recorded. */
static int st_savename_sequence(httrackp *opt, int argc, char **argv) {
  /* A step "host path=name" names a link, "!host path name" records one as is
     and "@N" switches the 8.3 mode. FLAGS hold the 8.3 mode | urlhack << 2. */
  static const struct {
    int flags;
    const char *const steps[10];
  } cases[] = {
      /* a -0 suffix walks from 1 */
      {0,
       {"example.com /A-0.html=example.com/A-0.html",
        "example.com /a-0.html=example.com/a-1.html",
        "example.com /a-0.HTML=example.com/a-2.HTML",
        "EXAMPLE.com /a-0.html=example.com/a-1.html"}},
      /* the same in ISO9660 names */
      {2,
       {"example.com /A-0.html=EXAMPLE_COM/A_0.HTM",
        "example.com /a-0.html=EXAMPLE_COM/A_1.HTM",
        "example.com /a-0.HTML=EXAMPLE_COM/A_2.HTM",
        "EXAMPLE.com /a-0.html=EXAMPLE_COM/A_1.HTM"}},
      /* links sharing a fingerprint with distinct names block jumps */
      {0,
       {"!Example.COM /a--3.htm example.com/a-10-3.gif",
        "!Example.COM /a-10.gif example.com/a--3.htm",
        "EXAMPLE.com /a--3.htm=example.com/a--4.htm",
        "EXAMPLE.com /A--3.htm=example.com/A--5.htm",
        "example.com /a--3.htm=example.com/a--4.htm"}},
      /* a naming-mode switch keeps its own jump table */
      {0,
       {"x /ABCDEFGH.htm=x/ABCDEFGH.htm", "x /AbCdefgh.htm=x/AbCdefgh-2.htm",
        "x /ABcdefgh.htm=x/ABcdefgh-3.htm", "@1",
        "x /abcdefgh.htm=X/ABCDEF-2.HTM"}},
      /* a link recorded again keeps its first fingerprint owner */
      {0,
       {"EXAMPLE.com /A-01.HTML=example.com/A-01.HTML",
        "EXAMPLE.com /A-01.html=example.com/A-2.html",
        "!EXAMPLE.com /A-01.html example.com/a-01_10.php",
        "EXAMPLE.com /a-01.html=example.com/a-3.html",
        "example.com /A-01.html=example.com/A-2.html"}},
      /* a walk reaching INT_MAX records no jump */
      {2,
       {"example.com /a-2147483646.HTML=EXAMPLE_COM/A_2147483646.HTM",
        "Example.COM /a-2147483646.htm=EXAMPLE_COM/A_2147483647.HTM",
        "Example.COM /A-2147483646.HTML=EXAMPLE_COM/A_2147483647_2.HTM",
        "EXAMPLE.com /a-2147483646=EXAMPLE_COM/A_2147483647_3.HTM",
        "EXAMPLE.com /A-2147483646.HTML=EXAMPLE_COM/A_2147483647_2.HTM"}},
      /* an irregular walk restarts name by name */
      {0,
       {"example.com /a-2147483640.html=example.com/a-2147483640.html",
        "example.com /A-2147483640.html=example.com/A-2147483641.html",
        "example.com /a-2147483640.HTML=example.com/a-2147483642.HTML",
        "example.com /a-2147483640.htMl=example.com/a-2147483643.htMl",
        "example.com /a-2147483640.htmL=example.com/a-2147483644.htmL",
        "example.com /A-2147483640.htmL=example.com/A-2147483645.htmL",
        "example.com /a-2147483640.HTml=example.com/a-2147483646.HTml",
        "example.com /a-2147483640.HTMl=example.com/a-2147483647.HTMl",
        "EXAMPLE.com /a-2147483640.HTML=example.com/a-2147483642.HTML"}},
      /* a jump never passes the link's own name */
      {0,
       {"!example.com /a-00.HTML example.com/a-9.HTML",
        "example.com /A-00.HTML=example.com/A-00.HTML",
        "EXAMPLE.com /a-00.HTML=example.com/a-1.HTML"}},
      /* only the very name the walk passed counts as passed */
      {4,
       {"www.example.com /abCDEf-9.gif=www.example.com/abCDEf-9.gif",
        "!www.example.com /a-.gif example.com/ABCDEf-9.gif",
        "EXAMPLE.com /abcdef-9.gif=example.com/abcdef-10.gif",
        "www.example.com /AbcDef-9.gif=www.example.com/AbcDef-10.gif",
        "www.example.com /abcdef-9.gif=www.example.com/abcdef-11.gif"}},
      /* the passed name must be this link's */
      {0,
       {"EXAMPLE.com /a-2.HTML=example.com/a-2.HTML",
        "Example.COM /a-2.html=example.com/a-3.html",
        "!EXAMPLE.com /A-2.html example.com/a-3.HTML",
        "EXAMPLE.com /a-2.html=example.com/a-4.html"}},
      /* a forced high suffix does not swallow the link's name */
      {0,
       {"!example.com /c example.com/c-27.html",
        "EXAMPLE.com /C=example.com/C.html",
        "example.com /c=example.com/c-27.html"}},
      /* a stored 9-digit suffix cuts no further back than the name start */
      {1, {"!X /foo X/X-999999999", "!x /bar X/FOO.HTM", "x /foo=X/FOO-2.HTM"}},
  };

  const hts_savename_83 mode = opt->savename_83;
  const hts_boolean urlhack = opt->urlhack;
  size_t c, s;
  int rc = 0;

  StringCopy(opt->path_html_utf8, "");
  opt->savename_delayed = HTS_SAVENAME_DELAYED_NONE;
  for (c = 0; c < sizeof(cases) / sizeof(cases[0]) && rc == 0; c++) {
    cache_back cache;
    struct_back *sback = NULL;
    hash_struct hash;

    opt->savename_83 = (hts_savename_83) (cases[c].flags & 3);
    opt->urlhack = (cases[c].flags & 4) != 0;
    memset(&cache, 0, sizeof(cache));
    cache.hashtable = coucal_new(0);
    cache.cached_tests = coucal_new(0);
    coucal_value_is_malloc(cache.cached_tests, 1);
    st_mirror_wiring(opt, &sback, &hash, HTS_FALSE);
    assertf(
        hts_record_link(opt, "example.com", "/", "/dev/null", "", "", NULL));
    for (s = 0; s < 10 && cases[c].steps[s] != NULL && rc == 0; s++) {
      const char *const step = cases[c].steps[s];
      char adr[64], fil[64], name[64];
      lien_adrfilsave afs;
      lien_adrfil former;

      if (step[0] == '@') {
        opt->savename_83 = (hts_savename_83) (step[1] - '0');
        continue;
      }
      if (step[0] == '!') {
        assertf(sscanf(step + 1, "%63s %63s %63s", adr, fil, name) == 3);
        assertf(hts_record_link(opt, adr, fil, name, "", "", NULL));
        continue;
      }
      assertf(sscanf(step, "%63s %63[^=]=%63s", adr, fil, name) == 3);
      memset(&afs, 0, sizeof(afs));
      memset(&former, 0, sizeof(former));
      strcpybuff(afs.af.adr, adr);
      strcpybuff(afs.af.fil, fil);
      url_savename(&afs, &former, "example.com", "/", opt, sback, &cache, &hash,
                   1, 0, NULL);
      if (strcmp(afs.save, name) != 0) {
        fprintf(stderr,
                "savename-sequence: case %u step %u: %s%s named %s, not %s\n",
                (unsigned) c, (unsigned) s, adr, fil, afs.save, name);
        rc = 1;
      } else if (hash_read(&hash, afs.save, NULL, HASH_STRUCT_FILENAME) < 0) {
        assertf(hts_record_link(opt, adr, fil, afs.save, "", "", NULL));
      }
    }
    coucal_delete(&cache.cached_tests);
    st_mirror_wiring_free(opt, &cache, &sback, &hash);
  }
  /* queued self-tests share this opt */
  opt->savename_83 = mode;
  opt->urlhack = urlhack;
  if (rc == 0)
    printf("savename-sequence: %u cases\n", (unsigned) c);
  return rc;
}

/* url_savename_addstr() takes attacker-controlled link text and must clip to
   the destination size it is handed, at every capacity (#1269). */
static int st_savename_addstr(httrackp *opt, int argc, char **argv) {
  static const struct {
    size_t dsize;
    const char *seed;
    const char *add;
    const char *want;
  } cases[] = {
      {8, "", "a\\b", "a/b"},
      {8, "ab/", "cd", "ab/cd"},
      {8, "", "abcdefg", "abcdefg"}, /* exact fit, NUL on the last byte */
      {8, "", "abcdefgh", "abcdefg"},
      {8, "", "abcdefghijklmnop", "abcdefg"},
      {8, "ab", "\\\\\\\\\\\\\\\\", "ab/////"}, /* clip inside a '\' run */
      {8, "abcdefg", "xy", "abcdefg"},
      {1, "", "abc", ""},
      {2, "", "a\\c", "a"},
      {4, "abcdefghij", "xy", "abcdefghij"}, /* seed past dsize: left alone */
      {0, "abc", "xy", "abc"},
  };

  /* last entry is the naming path's own destination, sizeof(afs->save) */
  static const size_t bigcaps[] = {64, 255, HTS_URLMAXSIZE, HTS_URLMAXSIZE * 2};
  char buf[64];
  size_t k;
  int rc = 0;

  for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
    const size_t seedlen = strlen(cases[k].seed);
    /* the appender may write up to dsize, and never past a longer seed */
    const size_t guard =
        cases[k].dsize > seedlen + 1 ? cases[k].dsize : seedlen + 1;
    char label[64];

    snprintf(label, sizeof(label), "savename-addstr: '%s' at dsize %d",
             cases[k].add, (int) cases[k].dsize);
    memset(buf, ST_POISON, sizeof(buf));
    memcpy(buf, cases[k].seed, seedlen + 1);
    url_savename_addstr(buf, cases[k].dsize, cases[k].add);
    if (strcmp(buf, cases[k].want) != 0) {
      fprintf(stderr, "savename-addstr: '%s' + '%s' (%d) -> '%s' want '%s'\n",
              cases[k].seed, cases[k].add, (int) cases[k].dsize, buf,
              cases[k].want);
      rc = 1;
    }
    if (!st_poison_intact(label, buf, guard, sizeof(buf)))
      rc = 1;
  }

  /* Oversized buffers, so an unclipped append lands on poison rather than
     smashing the stack. */
  for (k = 0; k < sizeof(bigcaps) / sizeof(bigcaps[0]); k++) {
    const size_t dsize = bigcaps[k];
    char BIGSTK src[HTS_URLMAXSIZE * 4];
    char BIGSTK big[sizeof(src) + ST_SAVENAME_CANARY];
    char label[64];

    snprintf(label, sizeof(label), "savename-addstr: long link at dsize %d",
             (int) dsize);
    memset(big, ST_POISON, sizeof(big));
    big[0] = '\0';
    memset(src, 'a', sizeof(src) - 1);
    src[sizeof(src) - 1] = '\0';
    url_savename_addstr(big, dsize, src);
    if (strlen(big) != dsize - 1) {
      fprintf(stderr, "savename-addstr: dsize %d filled %d byte(s)\n",
              (int) dsize, (int) strlen(big));
      rc = 1;
    }
    if (!st_poison_intact(label, big, dsize, sizeof(big)))
      rc = 1;
  }

  printf("savename-addstr self-test %s\n", rc == 0 ? "OK" : "FAILED");
  return rc;
}

/* What gives way to the arriving tail is the middle, never the tail (#1269). */
static int st_savename_addtail(httrackp *opt, int argc, char **argv) {
  static const struct {
    size_t dsize;
    const char *seed;
    const char *sep;
    const char *add;
    const char *want;
  } cases[] = {
      {16, "/a/", "", "index.html", "/a/index.html"}, /* room to spare */
      {16, "/aaaaa/", "", "index.html", "/aaaaindex.html"},
      {8, "/ab/", ".", "gz", "/ab/.gz"},  /* exact fit, no byte given up */
      {8, "/abc/", ".", "gz", "/abc.gz"}, /* one byte of middle gives way */
      {8, "abcdefg", "", "xy", "abcdexy"},
      {8, "abcdefg", ".", "html", "ab.html"}, /* the tail arrives whole */
      {8, "", ".", "html", ".html"},
      {6, "abc", ".", "html", ".html"}, /* the middle can give way entirely */
      {5, "abc", ".", "html", "abc"},   /* tail alone fills dsize: no-op */
      {0, "abc", "", "x", "abc"},
      /* the cut lands inside a 3-byte character and backs off it */
      {8, "a\342\202\254b", ".", "html", "a.html"},
      {10, "a\342\202\254b", ".", "html",
       "a\342\202\254.html"}, /* cut on a lead byte */
      {8, "\360\237\230\200x", ".", "gz",
       "\360\237\230\200.gz"},                    /* 4-byte kept */
      {7, "\360\237\230\200x", ".", "gz", ".gz"}, /* 4-byte cut away whole */
  };

  char buf[64];
  size_t k;
  int rc = 0;

  for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
    const size_t seedlen = strlen(cases[k].seed);
    const size_t guard =
        cases[k].dsize > seedlen + 1 ? cases[k].dsize : seedlen + 1;
    char label[64];

    snprintf(label, sizeof(label), "savename-addtail: '%s%s' at dsize %d",
             cases[k].sep, cases[k].add, (int) cases[k].dsize);
    memset(buf, ST_POISON, sizeof(buf));
    memcpy(buf, cases[k].seed, seedlen + 1);
    url_savename_addtail(buf, cases[k].dsize, cases[k].sep, cases[k].add);
    if (strcmp(buf, cases[k].want) != 0) {
      fprintf(stderr,
              "savename-addtail: '%s' + '%s%s' (%d) -> '%s' want '%s'\n",
              cases[k].seed, cases[k].sep, cases[k].add, (int) cases[k].dsize,
              buf, cases[k].want);
      rc = 1;
    }
    if (!st_poison_intact(label, buf, guard, sizeof(buf)))
      rc = 1;
  }

  /* A link filling the whole buffer still gets its extension. */
  {
    char BIGSTK big[HTS_URLMAXSIZE * 2 + ST_SAVENAME_CANARY];
    const size_t dsize = HTS_URLMAXSIZE * 2;

    memset(big, ST_POISON, sizeof(big));
    memset(big, 'a', dsize - 1);
    big[dsize - 1] = '\0';
    url_savename_addtail(big, dsize, ".", "html");
    if (strlen(big) != dsize - 1 ||
        strcmp(big + dsize - 1 - strlen(".html"), ".html") != 0) {
      fprintf(stderr,
              "savename-addtail: full buffer kept %d byte(s), no .html\n",
              (int) strlen(big));
      rc = 1;
    }
    if (!st_poison_intact("savename-addtail: full buffer", big, dsize,
                          sizeof(big)))
      rc = 1;
  }

  /* The .test's pads depend on this size, so it reds when the size moves. */
  printf("savename-addtail self-test %s, save %d bytes\n",
         rc == 0 ? "OK" : "FAILED", (int) (HTS_URLMAXSIZE * 2));
  return rc;
}

/* --strip-query: resolver + fil_normalized_filtered, end to end. */
static int st_stripquery(httrackp *opt, int argc, char **argv) {
  char dest[1024], keys[256], ref[1024];
  const char *k;

  /* empty rules == plain fil_normalized */
  assertf(hts_query_strip_keys(NULL, "h.com", "/p?a=1", keys, sizeof(keys)) ==
          NULL);
  assertf(hts_query_strip_keys("", "h.com", "/p?a=1", keys, sizeof(keys)) ==
          NULL);
  assertf(strcmp(fil_normalized_filtered("/p?b=2&a=1", dest, NULL),
                 fil_normalized("/p?b=2&a=1", ref)) == 0);

  /* bare form (*=keys): strip the key everywhere, keep+sort the rest */
  k = hts_query_strip_keys("sid", "any.com", "/p?b=2&sid=x&a=1", keys,
                           sizeof(keys));
  assertf(k != NULL && strcmp(k, "sid") == 0);
  assertf(strcmp(fil_normalized_filtered("/p?b=2&sid=x&a=1", dest, k),
                 "/p?a=1&b=2") == 0);

  /* reordered variant + an extra stripped key == the clean URL */
  assertf(strcmp(fil_normalized_filtered("/p?sid=y&a=1&b=2", dest, "sid"),
                 fil_normalized("/p?a=1&b=2", ref)) == 0);

  /* host pattern matches only that host, incl. its www-normalized forms */
  assertf(hts_query_strip_keys("ex.com/*=utm", "other.com", "/p?utm=1", keys,
                               sizeof(keys)) == NULL);
  assertf(hts_query_strip_keys("ex.com/*=utm", "ex.com", "/p?utm=1", keys,
                               sizeof(keys)) != NULL);
  assertf(hts_query_strip_keys("ex.com/*=utm", "www.ex.com", "/p?utm=1", keys,
                               sizeof(keys)) != NULL);
  assertf(hts_query_strip_keys("ex.com/*=utm", "http://www-3.ex.com",
                               "/p?utm=1", keys, sizeof(keys)) != NULL);

  /* last match wins, wholesale: host rule overrides global, no union */
  k = hts_query_strip_keys("*=sid\nex.com/*=utm", "ex.com",
                           "/p?sid=1&utm=2&a=3", keys, sizeof(keys));
  assertf(k != NULL && strcmp(k, "utm") == 0);
  assertf(strcmp(fil_normalized_filtered("/p?sid=1&utm=2&a=3", dest, k),
                 "/p?a=3&sid=1") == 0);
  k = hts_query_strip_keys("*=sid\nex.com/*=utm", "z.com", "/p?sid=1&a=3", keys,
                           sizeof(keys));
  assertf(k != NULL && strcmp(k, "sid") == 0);

  /* whole-key match, not prefix: "utm" must not strip utm_source */
  assertf(strcmp(fil_normalized_filtered("/p?utm_source=x&a=1", dest, "utm"),
                 "/p?a=1&utm_source=x") == 0);

  /* "*" drops every param; a fully-stripped single-arg query loses its '?' */
  assertf(strcmp(fil_normalized_filtered("/p?a=1&b=2", dest, "*"), "/p") == 0);
  assertf(strcmp(fil_normalized_filtered("/p?utm=1", dest, "utm"), "/p") == 0);

  /* an empty field is not a param, so a query holding only one keeps its '?' */
  assertf(strcmp(fil_normalized_filtered("/d//p?", dest, "a"), "/d/p?") == 0);
  /* the kept query must re-normalize to itself, or two spellings of one URL
     stop deduping */
  assertf(strcmp(fil_normalized_filtered("/p?a=1&&b=2", dest, "a"),
                 "/p?&b=2") == 0);
  assertf(strcmp(fil_normalized_filtered("/p?b=2&", dest, "z"), "/p?&b=2") ==
          0);

  /* degenerate forms a=, b, c== (key 'c'); strip c keeps a= and b */
  assertf(strcmp(fil_normalized_filtered("/p?a=&b&c==", dest, "c"),
                 "/p?a=&b") == 0);
  /* short key must not strip a longer one: 'c' must not touch 'cc' */
  assertf(strcmp(fil_normalized_filtered("/p?cc=1&c=2", dest, "c"),
                 "/p?cc=1") == 0);

  /* repeated key: every occurrence is stripped, not just the first */
  assertf(
      strcmp(fil_normalized_filtered("/p?foo=42&bar=13&foo=43", dest, "foo"),
             "/p?bar=13") == 0);
  /* repeated key mixing missing/empty values */
  assertf(
      strcmp(fil_normalized_filtered("/p?foo&bar=13&foo=42&foo=", dest, "foo"),
             "/p?bar=13") == 0);
  /* repeated key kept (no match): all occurrences retained, then sorted */
  assertf(strcmp(fil_normalized_filtered("/p?foo=42&bar=13&foo=43", dest, "z"),
                 "/p?bar=13&foo=42&foo=43") == 0);

  /* value containing '=': the key is only the part before the first '='. Strip
     'foo' drops "foo=42=17" whole; the '=' in the value is not a delimiter. */
  assertf(strcmp(fil_normalized_filtered("/p?foo=42=17&bar=", dest, "foo"),
                 "/p?bar=") == 0);
  /* keeping it preserves the embedded '=' verbatim */
  assertf(strcmp(fil_normalized_filtered("/p?foo=42=17&bar=", dest, "bar"),
                 "/p?foo=42=17") == 0);
  /* a value segment is not a key: stripping "42" must not touch foo=42=17 */
  assertf(strcmp(fil_normalized_filtered("/p?foo=42=17", dest, "42"),
                 "/p?foo=42=17") == 0);

  /* Idempotency: the read path re-normalizes an already-normalized fil, so the
     result must be a fixpoint or dedup misses (catches a dropped empty/trailing
     arg like "?&&", "a&"). */
  {
    static const char *const qs[] = {"/p?a=&b&c==",
                                     "/p?a&&b",
                                     "/p?&a",
                                     "/p?a&",
                                     "/p?",
                                     "/p?=v",
                                     "/p?&&",
                                     "/p?b=2&a=1",
                                     "/p?utm=x&",
                                     "/p?&utm=x",
                                     "/p?foo=42&bar=13&foo=43",
                                     "/p?foo&bar=13&foo=42&foo=",
                                     "/p?foo=42=17&bar="};
    static const char *const strips[] = {NULL, "z", "utm", "*", "a", "foo"};
    char once[1024], twice[1024];
    size_t i, j;

    for (i = 0; i < sizeof(qs) / sizeof(qs[0]); i++) {
      for (j = 0; j < sizeof(strips) / sizeof(strips[0]); j++) {
        fil_normalized_filtered(qs[i], once, strips[j]);
        fil_normalized_filtered(once, twice, strips[j]);
        assertf(strcmp(once, twice) == 0);
      }
    }
  }

  printf("strip-query self-test OK\n");
  return 0;
}

/* -%u url-hack split (#271): each sub-flag must toggle independently. */
static int st_urlhack(httrackp *opt, int argc, char **argv) {
#define EQ(aa, fa, ab, fb) hash_url_equals(opt, aa, fa, ab, fb)
  /* urlhack on, no opt-outs: www, // and query order all collapse */
  opt->urlhack = HTS_TRUE;
  opt->no_www_dedup = opt->no_slash_dedup = opt->no_query_dedup = HTS_FALSE;
  assertf(EQ("www.foo.com", "/a", "foo.com", "/a"));
  assertf(EQ("foo.com", "/a//b", "foo.com", "/a/b"));
  assertf(EQ("foo.com", "/p?b=2&a=1", "foo.com", "/p?a=1&b=2"));

  /* keep-www-prefix: host off; // and query still collapse */
  opt->no_www_dedup = HTS_TRUE;
  assertf(!EQ("www.foo.com", "/a", "foo.com", "/a"));
  assertf(EQ("foo.com", "/a//b", "foo.com", "/a/b"));
  assertf(EQ("foo.com", "/p?b=2&a=1", "foo.com", "/p?a=1&b=2"));
  opt->no_www_dedup = HTS_FALSE;

  /* keep-double-slashes: // significant; www, query order still collapse */
  opt->no_slash_dedup = HTS_TRUE;
  assertf(!EQ("foo.com", "/a//b", "foo.com", "/a/b"));
  assertf(EQ("www.foo.com", "/a", "foo.com", "/a"));
  assertf(EQ("foo.com", "/p?b=2&a=1", "foo.com", "/p?a=1&b=2"));
  opt->no_slash_dedup = HTS_FALSE;

  /* keep-query-order: query order significant; www and // still collapse */
  opt->no_query_dedup = HTS_TRUE;
  assertf(!EQ("foo.com", "/p?b=2&a=1", "foo.com", "/p?a=1&b=2"));
  assertf(EQ("www.foo.com", "/a", "foo.com", "/a"));
  assertf(EQ("foo.com", "/a//b", "foo.com", "/a/b"));
  opt->no_query_dedup = HTS_FALSE;

  /* all opt-outs == urlhack off entirely */
  opt->no_www_dedup = opt->no_slash_dedup = opt->no_query_dedup = HTS_TRUE;
  assertf(!EQ("www.foo.com", "/a", "foo.com", "/a"));
  assertf(!EQ("foo.com", "/a//b", "foo.com", "/a/b"));
  assertf(!EQ("foo.com", "/p?b=2&a=1", "foo.com", "/p?a=1&b=2"));
  opt->urlhack = HTS_FALSE;
  opt->no_www_dedup = opt->no_slash_dedup = opt->no_query_dedup = HTS_FALSE;
  assertf(!EQ("www.foo.com", "/a", "foo.com", "/a"));
  assertf(!EQ("foo.com", "/a//b", "foo.com", "/a/b"));
#undef EQ
  printf("urlhack self-test OK\n");
  return 0;
}

/* --host-alias (#16) resolves rules and feeds the dedup key. */
static int st_hostalias(httrackp *opt, int argc, char **argv) {
  char dest[HTS_URLMAXSIZE * 2];
  /* two rules, three aliases of a.com */
  static const char *const rules = "b.com=a.com\nc.com,d.com=a.com";

  /* nothing declared, or nothing matching: the host is left alone */
  assertf(hts_host_alias(NULL, "b.com", HTS_TRUE, dest, sizeof(dest)) == NULL);
  assertf(hts_host_alias("", "b.com", HTS_TRUE, dest, sizeof(dest)) == NULL);
  assertf(hts_host_alias(rules, "other.com", HTS_TRUE, dest, sizeof(dest)) ==
          NULL);
  /* a rule mapping a host to itself is a no-op, not a rewrite */
  assertf(hts_host_alias("a.com=a.com", "a.com", HTS_TRUE, dest,
                         sizeof(dest)) == NULL);

  /* exact, comma-list and glob aliases */
  assertf(strcmp(hts_host_alias(rules, "b.com", HTS_TRUE, dest, sizeof(dest)),
                 "a.com") == 0);
  assertf(strcmp(hts_host_alias(rules, "d.com", HTS_TRUE, dest, sizeof(dest)),
                 "a.com") == 0);
  assertf(strcmp(hts_host_alias("*.cdn.b.com=a.com", "img.cdn.b.com", HTS_TRUE,
                                dest, sizeof(dest)),
                 "a.com") == 0);
  /* hosts compare case-insensitively, like the filters */
  assertf(strcmp(hts_host_alias(rules, "B.CoM", HTS_TRUE, dest, sizeof(dest)),
                 "a.com") == 0);
  /* the port is part of the matched host */
  assertf(strcmp(hts_host_alias("b.com:8080=a.com:80", "b.com:8080", HTS_TRUE,
                                dest, sizeof(dest)),
                 "a.com:80") == 0);
  assertf(hts_host_alias("b.com:8080=a.com", "b.com", HTS_TRUE, dest,
                         sizeof(dest)) == NULL);
  /* last match wins */
  assertf(strcmp(hts_host_alias("b.com=a.com\nb.com=z.com", "b.com", HTS_TRUE,
                                dest, sizeof(dest)),
                 "z.com") == 0);
  /* A bare-host rule covers its www. forms unless --no-www-dedup is set. */
  assertf(
      strcmp(hts_host_alias(rules, "www.b.com", HTS_TRUE, dest, sizeof(dest)),
             "a.com") == 0);
  assertf(strcmp(hts_host_alias(rules, "www-42.b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "a.com") == 0);
  assertf(hts_host_alias(rules, "www.b.com", HTS_FALSE, dest, sizeof(dest)) ==
          NULL);
  assertf(strcmp(hts_host_alias(rules, "b.com", HTS_FALSE, dest, sizeof(dest)),
                 "a.com") == 0);
  /* Under the collapse a rule may spell either form of its alias and both names
     fold, so naming www.b.com cannot split the pair -%u had merged. */
  assertf(strcmp(hts_host_alias("www.b.com=a.com", "www.b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "a.com") == 0);
  assertf(strcmp(hts_host_alias("www.b.com=a.com", "b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "a.com") == 0);
  /* with the collapse off the two names are distinct, and so are the rules */
  assertf(strcmp(hts_host_alias("www.b.com=a.com", "www.b.com", HTS_FALSE, dest,
                                sizeof(dest)),
                 "a.com") == 0);
  assertf(hts_host_alias("www.b.com=a.com", "b.com", HTS_FALSE, dest,
                         sizeof(dest)) == NULL);

  /* scheme and credentials survive, only the host is replaced */
  assertf(strcmp(hts_host_alias(rules, "https://b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "https://a.com") == 0);
  assertf(strcmp(hts_host_alias(rules, "http://user:pw@b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "http://user:pw@a.com") == 0);

  assertf(
      strcmp(hts_host_alias(rules, "ftp://b.com", HTS_TRUE, dest, sizeof(dest)),
             "ftp://a.com") == 0);
  assertf(
      strcmp(hts_host_alias("b.com:8080=a.com:80", "https://user:pw@b.com:8080",
                            HTS_TRUE, dest, sizeof(dest)),
             "https://user:pw@a.com:80") == 0);
  /* An alias-side scheme narrows the match to it; a canonical-side scheme
     replaces the link's, for a host that speaks one scheme only. */
  assertf(strcmp(hts_host_alias("b.com=https://a.com", "http://b.com", HTS_TRUE,
                                dest, sizeof(dest)),
                 "https://a.com") == 0);
  assertf(strcmp(hts_host_alias("https://www.foo.com=ftp://ftp.foo.com",
                                "https://www.foo.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "ftp://ftp.foo.com") == 0);
  assertf(hts_host_alias("https://www.foo.com=ftp://ftp.foo.com",
                         "http://www.foo.com", HTS_TRUE, dest,
                         sizeof(dest)) == NULL);
  /* a rule without a scheme still matches whichever scheme the link uses */
  assertf(
      strcmp(hts_host_alias(rules, "ftp://b.com", HTS_TRUE, dest, sizeof(dest)),
             "ftp://a.com") == 0);
  /* credentials are the link's, whichever side names the scheme */
  assertf(strcmp(hts_host_alias("b.com=ftp://a.com", "http://user:pw@b.com",
                                HTS_TRUE, dest, sizeof(dest)),
                 "ftp://user:pw@a.com") == 0);
  /* A plain-http address carries no scheme, so http:// has to be spelled out
     for the rule to reach it: naming the default scheme must not be dead. */
  assertf(strcmp(hts_host_alias("http://b.com=a.com", "b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "a.com") == 0);
  assertf(hts_host_alias("http://b.com=a.com", "https://b.com", HTS_TRUE, dest,
                         sizeof(dest)) == NULL);
  /* a scheme named mid-chain stays in effect: the hop after it names none, and
     dropping back to the link's would undo the reachability the rule bought */
  assertf(strcmp(hts_host_alias("a.com=https://b.com\nb.com=c.com",
                                "http://a.com", HTS_TRUE, dest, sizeof(dest)),
                 "https://c.com") == 0);
  /* the spaces a user leaves around a token are not part of the host */
  assertf(hts_host_alias_rule_ok("b.com = a.com"));
  assertf(strcmp(hts_host_alias("b.com = a.com", "http://b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "http://a.com") == 0);
  assertf(strcmp(hts_host_alias("b.com\t=\ta.com", "http://b.com", HTS_TRUE,
                                dest, sizeof(dest)),
                 "http://a.com") == 0);
  /* a pattern naming no scheme is matched against the bare host, not the URL */
  assertf(hts_host_alias("http*=x.com", "http://b.com", HTS_TRUE, dest,
                         sizeof(dest)) == NULL);
  /* the "//host" form names a host, and a glob may stand where a scheme goes */
  assertf(strcmp(hts_host_alias("//b.com=a.com", "http://b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "http://a.com") == 0);
  assertf(hts_host_alias_rule_ok("*://b.com=a.com"));
  assertf(strcmp(hts_host_alias("*://b.com=a.com", "ftp://b.com", HTS_TRUE,
                                dest, sizeof(dest)),
                 "ftp://a.com") == 0);
  /* an IPv6 literal is a host; a scheme with none behind it is not, nor is the
     engine's own pseudo-host */
  assertf(hts_host_alias_rule_ok("b.com=[::1]"));
  assertf(hts_host_alias_rule_ok("b.com=[::1]:8080"));
  assertf(!hts_host_alias_rule_ok("b.com=https://"));
  assertf(!hts_host_alias_rule_ok("b.com=///"));
  assertf(!hts_host_alias_rule_ok("b.com=primary"));
  assertf(hts_host_alias("b.com=///", "http://b.com", HTS_TRUE, dest,
                         sizeof(dest)) == NULL);
  /* a run of trailing slashes is stripped by both the check and the fold */
  assertf(hts_host_alias_rule_ok("b.com//=a.com//"));
  /* a canonical with a trailing slash must still end the chain */
  assertf(strcmp(hts_host_alias("*=a.com/", "http://b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "http://a.com") == 0);
  /* Rule credentials are refused or dropped, because each re-fold would
     prepend them to the link's again. */
  assertf(!hts_host_alias_rule_ok("a.com=user:pw@a.com"));
  assertf(strcmp(hts_host_alias("*=u:p@b.com", "z.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "b.com") == 0);
  assertf(hts_host_alias("*=u:p@b.com", "b.com", HTS_TRUE, dest,
                         sizeof(dest)) == NULL);
  assertf(strcmp(hts_host_alias("*=u:p@b.com", "http://user:pw@z.com", HTS_TRUE,
                                dest, sizeof(dest)),
                 "http://user:pw@b.com") == 0);
  /* a trailing slash is not a path */
  assertf(strcmp(hts_host_alias("b.com/=https://a.com/", "http://b.com",
                                HTS_TRUE, dest, sizeof(dest)),
                 "https://a.com") == 0);
  /* every scheme of one host is one site to the scope test */
  assertf(hts_host_same_alias(rules, "ftp://b.com", "https://a.com", HTS_TRUE));

  /* engine pseudo-hosts survive a catch-all rule */
  assertf(hts_host_alias("*=a.com", "primary", HTS_TRUE, dest, sizeof(dest)) ==
          NULL);
  assertf(hts_host_alias("*=a.com", "file://", HTS_TRUE, dest, sizeof(dest)) ==
          NULL);
  assertf(hts_host_alias("*=a.com", "", HTS_TRUE, dest, sizeof(dest)) == NULL);
  /* control: the catch-all does rewrite a real host */
  assertf(
      strcmp(hts_host_alias("*=a.com", "b.com", HTS_TRUE, dest, sizeof(dest)),
             "a.com") == 0);

  /* A chain resolves to its end, because url_savename re-keys from there. */
  {
    static const char *const chain = "a.com=b.com\nb.com=c.com";
    /* the www form of a canonical is a chain that does not look like one */
    static const char *const wwwchain = "b.com=www.c.com\nc.com=d.com";
    char again[HTS_URLMAXSIZE * 2], looping[HTS_URLMAXSIZE * 2];

    assertf(strcmp(hts_host_alias(chain, "a.com", HTS_TRUE, dest, sizeof(dest)),
                   "c.com") == 0);
    assertf(hts_host_alias(chain, dest, HTS_TRUE, again, sizeof(again)) ==
            NULL);
    assertf(
        strcmp(hts_host_alias(wwwchain, "b.com", HTS_TRUE, dest, sizeof(dest)),
               "d.com") == 0);
    assertf(hts_host_alias(wwwchain, dest, HTS_TRUE, again, sizeof(again)) ==
            NULL);
    assertf(hts_host_alias_looping(chain, HTS_TRUE, looping, sizeof(looping)) ==
            NULL);
    assertf(hts_host_alias_looping(rules, HTS_TRUE, looping, sizeof(looping)) ==
            NULL);

    /* rules pointing in a circle name no canonical host: leave them alone */
    {
      static const char *const loop = "a.com=b.com\nb.com=a.com";

      assertf(hts_host_alias(loop, "a.com", HTS_TRUE, dest, sizeof(dest)) ==
              NULL);
      assertf(hts_host_alias(loop, "b.com", HTS_TRUE, dest, sizeof(dest)) ==
              NULL);
      assertf(strcmp(hts_host_alias_looping(loop, HTS_TRUE, looping,
                                            sizeof(looping)),
                     "b.com") == 0);
    }
  }

  /* alias lists tolerate the spaces a user writes after the commas */
  assertf(strcmp(hts_host_alias("b.com, c.com =a.com", "c.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "a.com") == 0);

  /* a canonical host that would not fit leaves the URL alone: never a
     truncated (and therefore wrong) hostname */
  {
    char small[5]; /* "a.com" needs 6 with its terminator */

    assertf(hts_host_alias(rules, "b.com", HTS_TRUE, small, sizeof(small)) ==
            NULL);
    assertf(hts_host_alias(rules, "https://b.com", HTS_TRUE, small,
                           sizeof(small)) == NULL);
    /* control: one more byte and it fits */
    assertf(strcmp(hts_host_alias(rules, "b.com", HTS_TRUE, dest, 6),
                   "a.com") == 0);
  }

  /* malformed rules are ignored... */
  assertf(hts_host_alias("b.com", "b.com", HTS_TRUE, dest, sizeof(dest)) ==
          NULL);
  assertf(hts_host_alias("=a.com", "b.com", HTS_TRUE, dest, sizeof(dest)) ==
          NULL);
  assertf(hts_host_alias("b.com=", "b.com", HTS_TRUE, dest, sizeof(dest)) ==
          NULL);
  /* ...and the command line refuses them up front */
  assertf(hts_host_alias_rule_ok("b.com,c.com=a.com"));
  assertf(hts_host_alias_rule_ok("*.b.com=a.com:8080"));
  assertf(!hts_host_alias_rule_ok(NULL));
  assertf(!hts_host_alias_rule_ok("b.com"));
  assertf(!hts_host_alias_rule_ok("=a.com"));
  assertf(!hts_host_alias_rule_ok("b.com="));
  assertf(!hts_host_alias_rule_ok("b.com=a.com,z.com"));
  assertf(!hts_host_alias_rule_ok("b.com=*.a.com"));
  assertf(!hts_host_alias_rule_ok("b.com=a com"));
  assertf(!hts_host_alias_rule_ok("b.com=a.com#x"));
  /* a scheme is part of an address, a path is not */
  assertf(hts_host_alias_rule_ok("https://www.foo.com=ftp://ftp.foo.com"));
  assertf(hts_host_alias_rule_ok("https://www.foo.com/=ftp://ftp.foo.com/"));
  assertf(
      !hts_host_alias_rule_ok("https://www.foo.com/=ftp://ftp.foo.com/pub/"));
  assertf(!hts_host_alias_rule_ok("https://www.foo.com/a=ftp://ftp.foo.com"));
  assertf(!hts_host_alias_rule_ok("a.com,b.com/deep=c.com"));
  assertf(!hts_host_alias_rule_ok("b.com=http://a.com/x"));
  /* a control byte, or a scheme the fold cannot parse, lands in the host */
  assertf(!hts_host_alias_rule_ok("b.com=a.com\nc.com"));
  assertf(!hts_host_alias_rule_ok("b.com\nc.com=a.com"));
  assertf(!hts_host_alias_rule_ok("b.com=a.com\n"));
  assertf(!hts_host_alias_rule_ok("\nb.com=a.com"));
  assertf(!hts_host_alias_rule_ok("b.com=a.com\r"));
  assertf(!hts_host_alias_rule_ok("b.com=a.com\v"));
  assertf(!hts_host_alias_rule_ok("b.com=a.com\f"));
  assertf(!hts_host_alias_rule_ok("b.com=a\x7f.com"));
  assertf(!hts_host_alias_rule_ok("b\v.com=a.com"));
  /* a high byte is a host in some local charset, not a control byte */
  assertf(hts_host_alias_rule_ok("caf\xe9.example.com=h\xf4tel.example.com"));
  /* an alias may lead with '-': why case 'C' carries no dash guard (#1179) */
  assertf(hts_host_alias_rule_ok("-legacy.example.com=example.com"));
  assertf(!hts_host_alias_rule_ok("b.com=x://a.com"));
  assertf(!hts_host_alias_rule_ok("b.com=a.com://c.com"));
  assertf(hts_host_alias_rule_ok("b.com=ftp://a.com"));
  assertf(hts_host_alias_rule_ok("*://b.com=a.com"));

  /* the same-address test the wizard uses to decide scope */
  assertf(hts_host_same_alias(rules, "b.com", "a.com", HTS_TRUE));
  assertf(
      hts_host_same_alias(rules, "https://b.com", "http://a.com", HTS_TRUE));
  assertf(hts_host_same_alias(rules, "b.com", "d.com",
                              HTS_TRUE)); /* two aliases of one */
  assertf(!hts_host_same_alias(rules, "b.com", "other.com", HTS_TRUE));
  assertf(!hts_host_same_alias(rules, "other.com", "elsewhere.com", HTS_TRUE));
  /* no rules: the caller's own exact compare decides, not this one */
  assertf(!hts_host_same_alias(NULL, "b.com", "b.com", HTS_TRUE));

  StringCopy(opt->host_alias, rules);

  /* the in-place fold every link goes through before it is probed and fetched
   */
  {
    lien_adrfil af;

    memset(&af, 0, sizeof(af));
    strcpybuff(af.adr, "https://b.com");
    strcpybuff(af.fil, "/x");
    assertf(strcmp(hts_host_alias_fold(opt, &af), "https://a.com") == 0);
    assertf(strcmp(af.adr, "https://a.com") == 0); /* rewritten in place */
    assertf(strcmp(af.fil, "/x") == 0);            /* the path is not touched */
    /* idempotent: the delayed-type path names the same link again */
    assertf(strcmp(hts_host_alias_fold(opt, &af), "https://a.com") == 0);
    strcpybuff(af.adr, "other.com");
    assertf(strcmp(hts_host_alias_fold(opt, &af), "other.com") == 0);
  }

  /* The dedup key itself: aliases collapse whatever the url hacks say, so
     --host-alias never silently no-ops under -%u0. */
#define EQ(aa, fa, ab, fb) hash_url_equals(opt, aa, fa, ab, fb)
  opt->urlhack = HTS_TRUE;
  opt->no_www_dedup = opt->no_slash_dedup = opt->no_query_dedup = HTS_FALSE;
  assertf(EQ("b.com", "/x", "a.com", "/x"));
  assertf(EQ("b.com", "/x", "d.com", "/x"));
  assertf(EQ("www.b.com", "/x", "a.com", "/x"));
  assertf(!EQ("b.com", "/x", "a.com", "/y"));
  assertf(!EQ("other.com", "/x", "a.com", "/x"));
  /* with the url hacks off the alias still fires, but it stops covering the
     www. forms: -%u0 asked for those to stay distinct */
  opt->urlhack = HTS_FALSE;
  assertf(EQ("b.com", "/x", "a.com", "/x"));
  assertf(!EQ("www.b.com", "/x", "a.com", "/x"));
  assertf(!EQ("other.com", "/x", "a.com", "/x"));
  opt->urlhack = HTS_TRUE;
  opt->no_www_dedup = HTS_TRUE;
  assertf(EQ("b.com", "/x", "a.com", "/x"));
  assertf(!EQ("www.b.com", "/x", "a.com", "/x"));
  opt->no_www_dedup = HTS_FALSE;
#undef EQ
  StringCopy(opt->host_alias, "");

  printf("host-alias self-test OK\n");
  return 0;
}

/* The dedup key and compare before the no-rules fast paths, frozen. */
static const char *legacy_hash_url_key(hash_struct *hash, const char *adr,
                                       const char *fil, char *dst,
                                       size_t dstsize) {
  char BIGSTK aliasbuf[HTS_URLMAXSIZE * 2];
  const char *const alias = hts_host_alias(
      hash->host_alias, adr, hash->norm_host, aliasbuf, sizeof(aliasbuf));
  const char *const adr_canon = alias != NULL ? alias : adr;
  const char *const adr_norm = hash->norm_host
                                   ? jump_normalized_const(adr_canon)
                                   : jump_identification_const(adr_canon);

  assertf(adr_norm != NULL);
  (void) strclipbuff(dst, dstsize, adr_norm);
  assertf(fil != NULL);
  {
    char BIGSTK keybuf[HTS_URLMAXSIZE];
    const char *const keys = hts_query_strip_keys(hash->strip_query, adr_canon,
                                                  fil, keybuf, sizeof(keybuf));
    const size_t used = strlen(dst);
    char *const tail = &dst[used];
    const size_t avail = dstsize - used;

    if ((hash->norm_slash || hash->norm_query || keys != NULL) &&
        strlen(fil) < avail && strlen(fil) < HTS_URLMAXSIZE * 2) {
      fil_normalized_filtered_ex(fil, tail, keys, hash->norm_slash,
                                 hash->norm_query);
    } else {
      (void) strclipbuff(tail, avail, fil);
    }
  }
  return dst;
}

static int legacy_key_adrfil_equals(hash_struct *hash, const char *a_adr,
                                    const char *a_fil, const char *b_adr,
                                    const char *b_fil) {
  char BIGSTK a_aliasbuf[HTS_URLMAXSIZE * 2], b_aliasbuf[HTS_URLMAXSIZE * 2];
  const char *a_canon, *b_canon;
  const char *ja;
  const char *jb;

  a_canon = hts_host_alias(hash->host_alias, a_adr, hash->norm_host, a_aliasbuf,
                           sizeof(a_aliasbuf));
  b_canon = hts_host_alias(hash->host_alias, b_adr, hash->norm_host, b_aliasbuf,
                           sizeof(b_aliasbuf));
  if (a_canon == NULL)
    a_canon = a_adr;
  if (b_canon == NULL)
    b_canon = b_adr;
  ja = hash->norm_host ? jump_normalized_const(a_canon)
                       : jump_identification_const(a_canon);
  jb = hash->norm_host ? jump_normalized_const(b_canon)
                       : jump_identification_const(b_canon);
  if (strcasecmp(ja, jb) != 0) {
    return 0;
  }
  {
    char BIGSTK ka[HTS_URLMAXSIZE], kb[HTS_URLMAXSIZE];
    const char *const keysa =
        hts_query_strip_keys(hash->strip_query, a_canon, a_fil, ka, sizeof(ka));
    const char *const keysb =
        hts_query_strip_keys(hash->strip_query, b_canon, b_fil, kb, sizeof(kb));

    if (hash->norm_slash || hash->norm_query || keysa != NULL ||
        keysb != NULL) {
      fil_normalized_filtered_ex(a_fil, hash->normfil, keysa, hash->norm_slash,
                                 hash->norm_query);
      fil_normalized_filtered_ex(b_fil, hash->normfil2, keysb, hash->norm_slash,
                                 hash->norm_query);
      return strcmp(hash->normfil, hash->normfil2) == 0;
    } else {
      return strcmp(a_fil, b_fil) == 0;
    }
  }
}

/* Stand-ins in the corpus for the long host, path and query path. */
static const char hashdiff_long[] = "(long)";
static const char hashdiff_longq[] = "(long query)";

/* The URL corpus, its key buffers and the running counts. */
typedef struct hashdiff_run {
  const char **adr, **fil;
  size_t n, nf;
  char *ko, *kn;
  unsigned long setups, cases, failures, equal, rewritten;
} hashdiff_run;

/* Check every key, and a sample of pairs, of RUN under one HASH setup. */
static void hashdiff_setup(hashdiff_run *run, hash_struct *hash,
                           const char *label) {
  static const size_t sizes[] = {1, 8, 40, HTS_URLMAXSIZE * 4};
  const size_t nf = run->nf;
  size_t i, j, k;

  for (i = 0; i < run->n; i++) {
    for (k = 0; k < sizeof(sizes) / sizeof(sizes[0]); k++) {
      memset(run->ko, '#', HTS_URLMAXSIZE * 4);
      memset(run->kn, '#', HTS_URLMAXSIZE * 4);
      legacy_hash_url_key(hash, run->adr[i], run->fil[i], run->ko, sizes[k]);
      hash_url_key(hash, run->adr[i], run->fil[i], run->kn, sizes[k]);
      run->cases++;
      if (memcmp(run->ko, run->kn, HTS_URLMAXSIZE * 4) != 0) {
        run->failures++;
        printf("hashdiff: key differs: %s size=%u %.40s %.40s\n", label,
               (unsigned) sizes[k], run->adr[i], run->fil[i]);
      }
    }
  }
  /* half the pairs sharing a host or a path, plus a sample of the rest */
  for (i = 0; i < run->n; i++) {
    for (j = 0; j < run->n; j++) {
      const char *const a = run->adr[i], *const af = run->fil[i];
      const char *const b = run->adr[j], *const bf = run->fil[j];
      int eo, en;

      if (i / nf != j / nf && i % nf != j % nf ? (i + j) % 23 != 0
                                               : (i + j + run->setups) % 2 != 0)
        continue;
      eo = legacy_key_adrfil_equals(hash, a, af, b, bf);
      en = hash_url_equals_in(hash, a, af, b, bf);
      run->cases++;
      if (eo != 0) {
        run->equal++;
        /* equal only once normalized: the compare's slow path */
        if (strcmp(a, b) != 0 || strcmp(af, bf) != 0)
          run->rewritten++;
      }
      if ((eo != 0) != (en != 0)) {
        run->failures++;
        printf("hashdiff: equals differs: %s %.40s%.40s vs %.40s%.40s: "
               "old %d new %d\n",
               label, a, af, b, bf, eo, en);
      }
    }
  }
  run->setups++;
}

/* The dedup key and compare agree with their frozen copies on a generated URL
   set, under every urlhack sub-flag and rule set. */
static int st_hashdiff(httrackp *opt, int argc, char **argv) {
  static const char *const rules_strip[] = {"", "sid", "a.com/*=sid\n*=utm",
                                            "*.com/x*=a,sid"};
  static const char *const rules_alias[] = {"", "*.b.com,b.com=https://a.com",
                                            "c.com=www.a.com\nd.com=c.com"};
  static const char *const hosts[] = {"a.com",        "A.COM",
                                      "www.a.com",    "WWW.a.com",
                                      "http://a.com", "https://www.a.com",
                                      "u:p@a.com",    "b.com",
                                      "www.b.com",    "x.b.com",
                                      "c.com",        "d.com",
                                      "a.com:8080",   "",
                                      "primary",      hashdiff_long};
  static const char *const fils[] = {
      "/",           "/x",           "//x",    "/x//y",        "/x?b=1&a=2",
      "/x?a=2&b=1",  "/x?sid=1&a=2", "/x?a=2", "/x?a=2&sid=9", "/x?utm=1",
      "/x?",         "/x?&&",        "x",      "/X",           "/x//y?b&a",
      hashdiff_long, hashdiff_longq};

  enum {
    nh = sizeof(hosts) / sizeof(hosts[0]),
    nf = sizeof(fils) / sizeof(fils[0])
  };

  char *longhost = malloct(HTS_URLMAXSIZE * 2);
  char *longfil = malloct(HTS_URLMAXSIZE * 2);
  char *longqfil = malloct(HTS_URLMAXSIZE * 2);
  const char *adr[nh * nf], *fil[nh * nf];
  hashdiff_run run;
  size_t rs, ra, i, j;
  int flags;

  memset(&run, 0, sizeof(run));
  run.adr = adr;
  run.fil = fil;
  run.n = nh * nf;
  run.nf = nf;
  run.ko = malloct(HTS_URLMAXSIZE * 4);
  run.kn = malloct(HTS_URLMAXSIZE * 4);
  assertf(longhost != NULL && longfil != NULL && longqfil != NULL &&
          run.ko != NULL && run.kn != NULL);
  /* one byte short of the fields the engine stores them in */
  memset(longhost, 'h', HTS_URLMAXSIZE * 2 - 1);
  longhost[HTS_URLMAXSIZE * 2 - 1] = '\0';
  memset(longfil, 'f', HTS_URLMAXSIZE * 2 - 1);
  longfil[0] = '/';
  longfil[HTS_URLMAXSIZE * 2 - 1] = '\0';
  strlcpybuff(longqfil, longfil, HTS_URLMAXSIZE * 2);
  memcpy(longqfil + HTS_URLMAXSIZE * 2 - 16, "?sid=1&b=2&a=3", 14);
  for (i = 0; i < nh; i++) {
    for (j = 0; j < nf; j++) {
      adr[i * nf + j] = hosts[i] == hashdiff_long ? longhost : hosts[i];
      fil[i * nf + j] = fils[j] == hashdiff_long    ? longfil
                        : fils[j] == hashdiff_longq ? longqfil
                                                    : fils[j];
    }
  }

  opt->urlhack = HTS_TRUE;
  for (rs = 0; rs < sizeof(rules_strip) / sizeof(rules_strip[0]); rs++) {
    for (ra = 0; ra < sizeof(rules_alias) / sizeof(rules_alias[0]); ra++) {
      StringCopy(opt->strip_query, rules_strip[rs]);
      StringCopy(opt->host_alias, rules_alias[ra]);
      for (flags = 0; flags < 8; flags++) {
        hash_struct hash;
        char label[64];

        opt->no_www_dedup = (flags & 1) != 0 ? HTS_TRUE : HTS_FALSE;
        opt->no_slash_dedup = (flags & 2) != 0 ? HTS_TRUE : HTS_FALSE;
        opt->no_query_dedup = (flags & 4) != 0 ? HTS_TRUE : HTS_FALSE;
        snprintf(label, sizeof(label), "strip=%u alias=%u flags=%d",
                 (unsigned) rs, (unsigned) ra, flags);
        hash_init(opt, &hash, HTS_TRUE);
        hashdiff_setup(&run, &hash, label);
        hash_free(&hash);
      }
    }
  }
  StringCopy(opt->strip_query, "");
  StringCopy(opt->host_alias, "");
  freet(longhost);
  freet(longfil);
  freet(longqfil);
  freet(run.ko);
  freet(run.kn);
  printf(
      "hashdiff: %lu cases, %lu failures, %lu equal, %lu of them rewritten\n",
      run.cases, run.failures, run.equal, run.rewritten);
  if (run.failures != 0)
    return 1;
  printf("hashdiff: OK\n");
  return 0;
}

/* Build the key for ADR/FIL into ARENA, whose KEYSIZE bytes are followed by
   GUARD poisoned ones, and report whether those are still untouched. */
static hts_boolean st_hashkey(httrackp *opt, const char *adr, const char *fil,
                              char *arena, size_t keysize, size_t guard) {
  hash_struct hash;
  size_t i;

  /* poisoned with '#', not 0, or the stray NUL of an off-by-one would read as
     untouched */
  memset(arena, '#', keysize + guard);
  hash_init(opt, &hash, opt->urlhack);
  hash_url_key(&hash, adr, fil, arena, keysize);
  hash_free(&hash);
  for (i = keysize; i < keysize + guard; i++) {
    if (arena[i] != '#')
      return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* Two maximal halves fit the dedup key, and either alone must clip (#1160). */
static int st_hashkey_bounds(httrackp *opt, int argc, char **argv) {
  /* what lien_adrfil.adr / .fil hold, and what a whole key needs */
  enum { field = HTS_URLMAXSIZE * 2, keysize = 2 * field, guard = 64 };

  char *adr = malloct(2 * keysize);
  char *fil = malloct(2 * keysize);
  char *cat = malloct(4 * keysize);
  char *arena = malloct(keysize + guard);
  int hack;

  assertf(adr != NULL && fil != NULL && cat != NULL && arena != NULL);
  opt->no_www_dedup = opt->no_slash_dedup = opt->no_query_dedup = HTS_FALSE;
  /* the crawler's own key buffer, or every maximal URL would collide */
  assertf(sizeof(((hash_struct *) NULL)->normfil) >= (size_t) keysize);

  /* a host and a path one byte short of their own buffers: nothing here
     normalizes away, so the key is the plain concatenation */
  memset(adr, 'a', field - 1);
  adr[field - 1] = '\0';
  fil[0] = '/';
  memset(fil + 1, 'b', field - 2);
  fil[field - 1] = '\0';
  snprintf(cat, 4 * keysize, "%s%s", adr, fil);
  for (hack = 0; hack < 2; hack++) {
    opt->urlhack = hack != 0 ? HTS_TRUE : HTS_FALSE;
    assertf(st_hashkey(opt, adr, fil, arena, keysize, guard));
    assertf(strcmp(arena, cat) == 0);
  }

  /* each half alone past the whole key must be bounded on its own */
  for (hack = 0; hack < 4; hack++) {
    const hts_boolean longadr = (hack & 1) != 0 ? HTS_TRUE : HTS_FALSE;

    memset(adr, 'a', longadr ? keysize + 32 : 8);
    adr[longadr ? keysize + 32 : 8] = '\0';
    fil[0] = '/';
    memset(fil + 1, 'b', longadr ? 8 : keysize + 32);
    fil[(longadr ? 8 : keysize + 32) + 1] = '\0';
    snprintf(cat, 4 * keysize, "%s%s", adr, fil);
    opt->urlhack = (hack & 2) != 0 ? HTS_TRUE : HTS_FALSE;
    assertf(st_hashkey(opt, adr, fil, arena, keysize, guard));
    /* clipped and terminated, and a prefix of the pair rather than garbage */
    assertf(strlen(arena) == keysize - 1);
    assertf(strncmp(arena, cat, strlen(arena)) == 0);
  }

  /* --strip-query must not abort on a maximal pair. No rule matches past
     STRJOKER_MAXLEN, so the string must not be clipped to match. */
  {
    const char *const rules = "*=other\n*bbz=sid";
    char keys[64];
    const char *k;

    /* control: the rules do match a short URL */
    k = hts_query_strip_keys(rules, "h.com", "/bbz", keys, sizeof(keys));
    assertf(k != NULL && strcmp(k, "sid") == 0);
    memset(adr, 'a', field - 1);
    adr[field - 1] = '\0';
    fil[0] = '/';
    memset(fil + 1, 'b', field - 3);
    fil[field - 2] = 'z';
    fil[field - 1] = '\0';
    assertf(hts_query_strip_keys(rules, adr, fil, keys, sizeof(keys)) == NULL);
    opt->urlhack = HTS_TRUE; /* and the key builder survives the same pair */
    StringCopy(opt->strip_query, rules);
    assertf(st_hashkey(opt, adr, fil, arena, keysize, guard));

    /* the key builder must clip a query past the normalizer's scratch */
    snprintf(fil, 2 * keysize, "/bbz?");
    memset(&fil[5], 'q', field + 64); /* past the normalizer, inside the key */
    fil[5 + field + 64] = '\0';
    assertf(st_hashkey(opt, "h.com", fil, arena, keysize, guard));
    StringCopy(opt->strip_query, "");
  }

  freet(adr);
  freet(fil);
  freet(cat);
  freet(arena);
  printf("hashkey-bounds self-test OK\n");
  return 0;
}

/* #159: hts_redirect_same_savefile decides whether a redirect is a same-file
 * alias. */
static int st_redirect_samefile(httrackp *opt, int argc, char **argv) {
#define SAME(aa, fa, ab, fb) hts_redirect_same_savefile(opt, aa, fa, ab, fb)
  /* scheme and userinfo collapse (the #159 case); a different path does not */
  assertf(SAME("http://foo.com", "/a/b", "https://foo.com", "/a/b"));
  assertf(SAME("http://user@foo.com", "/a", "http://foo.com", "/a"));
  assertf(!SAME("http://foo.com", "/a", "http://foo.com", "/b"));
  /* www stays distinct here because the crawl's dedup layer folds it */
  opt->urlhack = HTS_TRUE;
  opt->no_www_dedup = opt->no_slash_dedup = opt->no_query_dedup = HTS_FALSE;
  assertf(!SAME("http://www.foo.com", "/a", "http://foo.com", "/a"));
  /* slash/query fold only when the dedup flag is on */
  assertf(SAME("https://foo.com", "/a//b", "http://foo.com", "/a/b"));
  assertf(
      SAME("https://foo.com", "/p?b=2&a=1", "http://foo.com", "/p?a=1&b=2"));
  opt->no_slash_dedup = opt->no_query_dedup = HTS_TRUE;
  assertf(!SAME("https://foo.com", "/a//b", "http://foo.com", "/a/b"));
  assertf(
      !SAME("https://foo.com", "/p?b=2&a=1", "http://foo.com", "/p?a=1&b=2"));
  /* but a pure scheme alias still collapses regardless of dedup opt-outs */
  assertf(SAME("http://foo.com", "/a/b", "https://foo.com", "/a/b"));
  opt->no_slash_dedup = opt->no_query_dedup = HTS_FALSE;
#undef SAME
  printf("redirect-samefile self-test OK\n");
  return 0;
}

/* Builds an n-byte path ending in '/', so bauth_prefix() keeps all of it. */
static void st_urlbounds_path(char *fil, size_t n) {
  assertf(n >= 2);
  memset(fil, 'a', n);
  fil[0] = fil[n - 1] = '/';
  fil[n] = '\0';
}

/* Each consumer of an HTS_URLMAXSIZE*2 buffer must refuse an oversized fil
   itself (#1561). The cases sit on that boundary. */
static int st_urlbounds(httrackp *opt, int argc, char **argv) {
  const htsfilters savedfilters = opt->filters;
  const hts_wizard savedwizard = opt->wizard;
  const int savedgetmode = opt->getmode;
  const hts_robots savedrobots = opt->robots;
  lien_url **savedliens = opt->liens;
  FILE *savedlog = opt->log;
  /* lien_url holds non-const strings, so the primary link owns its own */
  char adrbuf[] = "www.example.com", filroot[] = "/", savname[] = "index.html";
  const char *const adr = adrbuf;
  /* the wizard builds "http://" + adr + fil, basic auth builds "http://" + adr
   * + fil */
  const size_t wzfits =
      HTS_URLMAXSIZE * 2 - strlen("http://") - strlen(adr) - 1;
  const size_t bafits =
      HTS_URLMAXSIZE * 2 - strlen("http://") - strlen(adr) - 1;
  char BIGSTK fil[HTS_URLMAXSIZE * 2];
  char BIGSTK prefix[HTS_URLMAXSIZE * 2];
  lien_url primary, *liens[1];
  t_cookie *cookie = calloct(sizeof(t_cookie), 1);
  char **filters = NULL;
  int filptr = 0, prio = 0;

  assertf(cookie != NULL);
  opt->log = NULL; /* the paths below are 2 KB of noise */
  assertf(filters_init(&filters, opt->maxfilter, 0) != 0);
  opt->filters.filters = &filters;
  opt->filters.filptr = &filptr;

  /* hts_testlinksize(): -1 is "forbidden" to its one caller, back_checksize */
  st_urlbounds_path(fil, wzfits);
  assertf(hts_testlinksize(opt, adr, fil, 1) == 0);
  st_urlbounds_path(fil, wzfits + 1);
  assertf(hts_testlinksize(opt, adr, fil, 1) == -1);

  /* This goes first, because bauth_check() skips the key on an empty jar and
     the refused lookups below must read it. */
  strcpybuff(fil, "/secure/page.html?a=b");
  assertf(bauth_prefix(prefix, adr, fil) == prefix);
  assertf(strcmp(prefix, "http://www.example.com/secure/") == 0);
  assertf(bauth_add(cookie, adr, fil, "dXNlcjpwYXNz") == 1);
  assertf(bauth_check(cookie, adr, fil) != NULL);

  /* A login learned on https is not offered to plain http, and vice versa. */
  assertf(bauth_add(cookie, "https://www.example.com", "/s/p.html",
                    "aHR0cHM=") == 1);
  assertf(bauth_check(cookie, "https://www.example.com", "/s/q.html") != NULL);
  assertf(bauth_check(cookie, "http://www.example.com", "/s/q.html") == NULL);
  assertf(bauth_check(cookie, "www.example.com", "/s/q.html") == NULL);
  /* a host that reads as "https:" must not reach the https entry */
  assertf(bauth_check(cookie, "http://https:", "//www.example.com/s/q.html") ==
          NULL);
  /* an http login follows a redirect to https, and a later https login is
     still stored beside it */
  assertf(bauth_add(cookie, "old.example.com", "/d/p.html", "aHR0cA==") == 1);
  assertf(bauth_check(cookie, "https://old.example.com", "/d/q.html") != NULL);
  assertf(bauth_add(cookie, "https://old.example.com", "/d/p.html",
                    "aHR0cHM=") == 1);
  assertf(strcmp(bauth_check(cookie, "https://old.example.com", "/d/q.html"),
                 "aHR0cHM=") == 0);
  assertf(strcmp(bauth_check(cookie, "old.example.com", "/d/q.html"),
                 "aHR0cA==") == 0);
  assertf(bauth_check(cookie, "https://other.example.com", "/s/q.html") ==
          NULL);

  /* bauth_prefix(): the key it keeps fills its buffer to the last byte, and
     #1561's guard then refuses to store a key wider than bauth_chain */
  st_urlbounds_path(fil, bafits);
  assertf(bauth_prefix(prefix, adr, fil) == prefix);
  assertf(strlen(prefix) == HTS_URLMAXSIZE * 2 - 1);
  assertf(bauth_add(cookie, adr, fil, "dXNlcjpwYXNz") == 0);
  /* one byte more: no key at all, and both callers survive the NULL */
  st_urlbounds_path(fil, bafits + 1);
  assertf(bauth_prefix(prefix, adr, fil) == NULL);
  assertf(bauth_check(cookie, adr, fil) == NULL);
  assertf(bauth_add(cookie, adr, fil, "dXNlcjpwYXNz") == 0);

  /* hts_acceptlink_()'s wizard arm builds the same pair. A primary link (ptr
     0) resolves without asking the front end, so the verdict is the guard's:
     0 authorized, 1 forbidden. */
  memset(&primary, 0, sizeof(primary));
  primary.adr = adrbuf;
  primary.fil = filroot;
  primary.sav = savname;
  liens[0] = &primary;
  opt->liens = liens;
  opt->wizard = HTS_WIZARD_ASK;
  opt->getmode |= HTS_GETMODE_NONHTML;
  opt->robots = HTS_ROBOTS_NEVER;
  st_urlbounds_path(fil, wzfits);
  assertf(hts_acceptlink(opt, 0, adr, fil, NULL, NULL, HTS_FALSE, &prio,
                         NULL) == 0);
  filptr = 0; /* the primary link left its own filters */
  st_urlbounds_path(fil, wzfits + 1);
  assertf(hts_acceptlink(opt, 0, adr, fil, NULL, NULL, HTS_FALSE, &prio,
                         NULL) == 1);

  /* This fails once ident_url_absolute() stops yielding an oversized path. */
  {
    lien_adrfil af;
    char BIGSTK url[HTS_URLMAXSIZE * 2];

    memset(url, 'a', sizeof(url) - 1);
    memcpy(url, "h.test/", 7);
    url[sizeof(url) - 1] = '\0';
    assertf(ident_url_absolute(url, &af) >= 0);
    assertf(strcmp(af.adr, "h.test") == 0);
    assertf(hts_testlinksize(opt, af.adr, af.fil, 1) == -1);
  }

  opt->log = savedlog;
  opt->liens = savedliens;
  opt->wizard = savedwizard;
  opt->getmode = savedgetmode;
  opt->robots = savedrobots;
  bauth_free(cookie);
  freet(cookie);
  freet(filters[0]);
  freet(filters);
  opt->filters = savedfilters;
  printf("urlbounds self-test OK (wizard %d/%d, auth %d/%d)\n", (int) wzfits,
         (int) wzfits + 1, (int) bafits, (int) bafits + 1);
  return 0;
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

enum { RS_ROOM = 64, RS_GUARD = 8 };

/* Does escaping in, in a size-byte buffer poisoned past its end, give want? */
static hts_boolean rs_check(const char *in, size_t size, const char *want) {
  char buf[RS_ROOM + RS_GUARD];
  size_t i;
  hts_boolean ok;

  memset(buf, '#', sizeof(buf));
  strlcpybuff(buf, in, sizeof(buf));
  url_savename_escape_reserved(buf, size);
  ok = strcmp(buf, want) == 0 ? HTS_TRUE : HTS_FALSE;
  for (i = size > strlen(in) ? size : strlen(in) + 1; i < sizeof(buf); i++) {
    if (buf[i] != '#')
      ok = HTS_FALSE;
  }
  if (!ok)
    printf("reserved: '%s' size %lu gave '%s', want '%s'\n", in,
           (unsigned long) size, buf, want);
  return ok;
}

static int st_reserved(httrackp *opt, int argc, char **argv) {
  /* clang-format off */
  static const char *const names[] = {
    "prn", "con", "aux", "nul",
    "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9",
    "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
    "conin$", "conout$", "clock$",
    "emmxxxx0", "xmsxxxx0", "setverxx",
    "hts-cache", "hts-tmp"
  };
  /* clang-format on */

  /* A size of 0 means room to spare. */
  static const struct {
    size_t size;
    const char *in, *want;
  } rows[] = {
      {0, "D/CoN", "D/CoN_"},
      {0, "d/x/con", "d/x/con_"},
      {0, "d/con/d", "d/con_/d"},
      {0, "d/con/", "d/con_/"},
      {0, "a//con", "a//con_"},
      {0, "con", "con_"},
      {0, "con/d", "con_/d"},
      {0, "aux.example.com", "aux.example.com"},
      {0, "con.", "con_."},
      {0, "con ", "con_ "},
      {0, "con .x/d", "con .x/d"},
      {0, "d/con.txt", "d/con_.txt"},
      {0, "d/com1.tar.gz", "d/com1_.tar.gz"},
      {0, "d/con.", "d/con_."},
      {0, "d/con..", "d/con_.."},
      {0, "d/con ", "d/con_ "},
      {0, "d/con . ", "d/con_ . "},
      {0, "d/con x", "d/con x"},
      {0, "d/con .x", "d/con .x"},
      {0, "d/cons", "d/cons"},
      {0, "d/com10", "d/com10"},
      {0, "d/com0", "d/com0"},
      {0, "d/com\xc2\xb9", "d/com\xc2\xb9"},
      {0, "d/xcon", "d/xcon"},
      {0, "d/lpt", "d/lpt"},
      {0, "d/auxx", "d/auxx"},
      {0, "d/conin", "d/conin"},
      {0, "d/conin$", "d/conin$_"},
      {0, "d/hts-cachex", "d/hts-cachex"},
      {0, "d\\con", "d\\con"},
      {0, "nul/con.x/aux", "nul_/con_.x/aux_"},
      /* A full buffer overwrites the name's last byte instead. */
      {6, "d/con", "d/co_"},
      {7, "d/con", "d/con_"},
      {4, "con", "co_"},
      {8, "aux/con", "au_/co_"},
      {9, "aux/con", "aux_/co_"},
      {9, "aux/conx", "au_/conx"},
      {9, "nul/con.", "nu_/co_."},
      {10, "d/con.txt", "d/co_.txt"},
      {10, "hts-cache", "hts-cach_"},
      {7, "d/lpt1", "d/lpt_"},
      {10, "d/con   x", "d/con   x"},
      {9, "d/con . ", "d/co_ . "},
  };

  char in[RS_ROOM], want[RS_ROOM];
  unsigned long cases = 0, failures = 0;
  size_t i;

  (void) opt;
  (void) argc;
  (void) argv;
  for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
    snprintf(in, sizeof(in), "d/%s", names[i]);
    snprintf(want, sizeof(want), "d/%s_", names[i]);
    failures += rs_check(in, RS_ROOM, want) ? 0 : 1;
    strcpybuff(want, in);
    *hts_lastcharptr(want) = '_';
    failures += rs_check(in, strlen(in) + 1, want) ? 0 : 1;
    cases += 2;
  }
  for (i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
    failures += rs_check(rows[i].in, rows[i].size != 0 ? rows[i].size : RS_ROOM,
                         rows[i].want)
                    ? 0
                    : 1;
    cases++;
  }
  /* The caller may pass size 0, and nothing is written then. */
  failures += rs_check("con", 0, "con") ? 0 : 1;
  cases++;
  printf("reserved: %lu cases, %lu failures\n", cases, failures);
  if (failures != 0)
    return 1;
  printf("reserved: OK\n");
  return 0;
}

const struct selftest_entry selftests_name[] = {
    {"urlbounds", "",
     "an over-long path is refused by the wizard and auth consumers",
     st_urlbounds},
    {"simplify", "<path>", "collapse ./ and ../ in a path", st_simplify},
    {"refname", "<adr> <fil>", "cache ref file name of a URL", st_refname},
    {"expandhome", "<path>", "expand a leading ~/ into $HOME", st_expandhome},
    {"stripquery", "", "--strip-query pattern/key stripping self-test",
     st_stripquery},
    {"urlhack", "", "-%u url-hack sub-flag (www/slash/query) self-test",
     st_urlhack},
    {"hostalias", "", "--host-alias hostname folding self-test", st_hostalias},
    {"hashkey-bounds", "", "dedup key holds a maximal host+path (#1160)",
     st_hashkey_bounds},
    {"hashdiff", "", "dedup key and compare match their frozen copies",
     st_hashdiff},
    {"redirect-samefile", "", "same-file redirect detection self-test (#159)",
     st_redirect_samefile},
    {"relative", "<link> <curr-file>", "relative link between two paths",
     st_relative},
    {"resolve", "<link> <adr> <fil>", "resolve a link against an origin",
     st_resolve},
    {"identurl", "<url>", "split an absolute URL into (adr, fil)", st_identurl},
    {"identrel", "", "every relative-URL arm bounds the path it builds",
     st_identrel},
    {"toport", "<url>...", "port separator found in a URL authority",
     st_toport},
    {"identabs", "", "ident_url_absolute one-byte fil[] overflow self-test",
     st_identabs},
    {"stripport", "", "default :80 port strip preserves host (#627)",
     st_stripport},
    {"getext", "",
     "extension parsing stops at the query and inside the buffer (#1433)",
     st_getext},
    {"savename", "<fil> <content-type> [key=value ...]",
     "local save-name for a URL", st_savename},
    {"savename-collide", "",
     "case variants of one name rename without probing every suffix",
     st_savename_collide},
    {"savename-sequence", "",
     "save-names of url_savename() call sequences match a name-by-name walk",
     st_savename_sequence},
    {"savename-addstr", "",
     "save-name append clips to the destination size (#1269)",
     st_savename_addstr},
    {"savename-addtail", "",
     "default name and extension arrive whole on a full buffer (#1269)",
     st_savename_addtail},
    {"reserved", "", "DOS device and engine directory names are escaped",
     st_reserved},
    {NULL, NULL, NULL, NULL},
};
