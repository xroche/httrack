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

static int st_simplify(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "simplify: needs a path\n");
    return 1;
  }
  fil_simplifie(argv[0]);
  printf("simplified=%s\n", argv[0]);
  return 0;
}

static int st_expandhome(httrackp *opt, int argc, char **argv) {
  String path = STRING_EMPTY;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "expandhome: needs a path\n");
    return 1;
  }
  StringCopy(path, argv[0]);
  expand_home(&path);
  printf("expanded=%s\n", StringBuff(path));
  StringFree(path);
  return 0;
}

static int st_relative(httrackp *opt, int argc, char **argv) {
  char s[HTS_URLMAXSIZE * 2];

  (void) opt;
  if (argc < 2) {
    fprintf(stderr, "relative: needs a link and a current-file path\n");
    return 1;
  }
  if (lienrelatif(s, sizeof(s), argv[0], argv[1]) == 0)
    printf("relative=%s\n", s);
  else
    printf("relative=<ERROR>\n");
  return 0;
}

static int st_resolve(httrackp *opt, int argc, char **argv) {
  lien_adrfil af;
  int r;

  (void) opt;
  if (argc < 3) {
    fprintf(stderr, "resolve: needs a link, an origin address and file\n");
    return 1;
  }
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

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "toport: needs a URL\n");
    return 1;
  }
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

  (void) opt;
  (void) argc;
  (void) argv;
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

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "identurl: needs a URL\n");
    return 1;
  }
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

/* Regression for the one-byte fil[] overflow: a 2047-byte hostless "?"-URL used
   to abort in strncat_safe_ when the missing leading '/' pushed fil to 2048. */
static int st_identabs(httrackp *opt, int argc, char **argv) {
  lien_adrfil af;
  const size_t len =
      sizeof(af.fil) - 1; /* 2047: max URL the top guard admits */
  char *url = malloct(len + 1);

  (void) opt;
  (void) argc;
  (void) argv;
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

/* Default-port strip is scheme-aware (#638), overflow-safe (#614): a scheme's
   own default (any spelling) is dropped, a real port stays; guards #627. */
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
  };

  size_t k;

  (void) opt;
  (void) argc;
  (void) argv;
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

/* get_ext() stops at the '?', so a parameterised URL types by its extension,
   and copies the length it measured rather than the destination size, so a
   long query cannot put a terminator past the end (#1433). */
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

  /* dodging sizeof(void *), which RUNTIME_TIME_CHECK_SIZE takes for a caller
     that passed sizeof() of a pointer */
  static const size_t caps[] = {1, 2, 3, 5, 6, 16, 64};
  /* longer than any capacity below, so a size-bounded copy runs off the end */
  static const char query[] = "?q=00000000001111111111222222222233333333334"
                              "4444444445555555555666666666677777777778888";
  const size_t guard = 64;
  size_t c, k;
  int rc = 0;

  (void) opt;
  (void) argc;
  (void) argv;

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

  /* every capacity against extensions straddling its bound, with and without a
     query behind them: a one-size-fits-all bound shows up as the wrong verdict
     on the small ones */
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
  const char *bodyfile = "st-savename-body.tmp";
  int statuscode = HTTP_OK, status = 0;
  int filpad = 0;
  int nosback = 0;
  int rc = 0;
  int i;

  if (argc < 2) {
    fprintf(stderr, "savename: needs a fil and a content-type\n");
    return 1;
  }
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
    else if (strncmp(a, "cached=", 7) == 0)
      cached = a + 7;
    else if (strncmp(a, "filpad=", 7) == 0)
      filpad = atoi(a + 7);
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
  strcpybuff(afs->af.adr, adr);
  if (filpad > 0) {
    /* '*' stands for filpad bytes: a link longer than argv can carry, argv
       being capped at HTS_CDLMAXSIZE (htscoremain.c) */
    const char *const star = strchr(argv[0], '*');
    char BIGSTK pad[HTS_URLMAXSIZE * 2];

    /* refuse rather than let the harness itself abort in strcatbuff */
    if (star == NULL ||
        (size_t) filpad + strlen(argv[0]) >= sizeof(afs->af.fil)) {
      fprintf(stderr, "savename: filpad needs a '*' in the fil, and room\n");
      return 1;
    }
    memset(pad, 'a', (size_t) filpad);
    pad[filpad] = '\0';
    strncatbuff(afs->af.fil, argv[0], star - argv[0]);
    strcatbuff(afs->af.fil, pad);
    strcatbuff(afs->af.fil, star + 1);
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

      if (p2 == NULL) {
        fprintf(stderr, "savename: prior needs adr|fil|sav\n");
        freet(dup);
        rc = 1;
        goto cleanup;
      }
      *p1 = *p2 = '\0';
      if (!hts_record_link(opt, dup, p1 + 1, p2 + 1, "", "", NULL)) {
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
  if (body != NULL) { /* leading body bytes, read via url_sav */
    char BIGSTK data[1024];
    const size_t n = st_decode_body(body, data, sizeof(data));
    FILE *const fp = fopen(bodyfile, "wb");

    if (fp == NULL || !hts_fwrite_exact(data, n, fp)) {
      fprintf(stderr, "savename: can not write %s\n", bodyfile);
      if (fp != NULL)
        fclose(fp);
      rc = 1;
      goto cleanup;
    }
    fclose(fp);
    strcpybuff(headers.url_sav, bodyfile);
  }

  url_savename(afs, NULL, NULL, NULL, opt, sback, &cache, &hash, 0, 0,
               &headers);
  if (body != NULL)
    (void) UNLINK(bodyfile);
  if (!st_poison_intact("savename: save[]", probe.canary, 0,
                        sizeof(probe.canary)))
    rc = 1;
  else
    printf("savename: %s\n", afs->save);

cleanup:
  st_mirror_wiring_free(opt, &cache, &sback, &hash);
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

  (void) opt;
  (void) argc;
  (void) argv;
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

  (void) opt;
  (void) argc;
  (void) argv;
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

  /* The .test's pads are chosen against this size; printing it reds them when
     it moves rather than letting them quietly stop reaching the end. */
  printf("savename-addtail self-test %s, save %d bytes\n",
         rc == 0 ? "OK" : "FAILED", (int) (HTS_URLMAXSIZE * 2));
  return rc;
}

/* --strip-query: resolver + fil_normalized_filtered, end to end. */
static int st_stripquery(httrackp *opt, int argc, char **argv) {
  char dest[1024], keys[256], ref[1024];
  const char *k;

  (void) opt;
  (void) argc;
  (void) argv;

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
  (void) argc;
  (void) argv;
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

/* --host-alias (#16): rule resolution, and the dedup key it feeds. */
static int st_hostalias(httrackp *opt, int argc, char **argv) {
  char dest[HTS_URLMAXSIZE * 2];
  /* two rules, three aliases of a.com */
  static const char *const rules = "b.com=a.com\nc.com,d.com=a.com";

  (void) argc;
  (void) argv;

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
  /* Under the www. collapse a rule on the bare host covers its www. forms,
     which -%u would otherwise key onto the bare name and away from the
     canonical one. With the collapse off, the host matches as the link
     spells it: --no-www-dedup keeps www.b.com and b.com apart. */
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
  /* a canonical whose trailing slash is trimmed must still settle: the chain
     compares one match against the next, and an untrimmed one never equals it
   */
  assertf(strcmp(hts_host_alias("*=a.com/", "http://b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "http://a.com") == 0);
  /* Credentials belong to the link. The command line refuses a rule carrying
     its own, and the fold drops them, since a caller can set the rules without
     going through that check: kept, they would be prepended to the link's again
     on every re-fold, growing the address until it no longer fits. */
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

  /* Chains resolve to the end of the chain, and the result is a fixpoint:
     url_savename names the same link again from the host it folded last, so a
     one-hop mapping would key that link under an intermediate host. */
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

/* The dedup key is a host and a path built in one buffer: two halves at the
   engine's per-field maximum must fit whole, and either half alone must clip
   rather than run past the end (#1160). */
static int st_hashkey_bounds(httrackp *opt, int argc, char **argv) {
  /* what lien_adrfil.adr / .fil hold, and what a whole key needs */
  enum { field = HTS_URLMAXSIZE * 2, keysize = 2 * field, guard = 64 };

  char *adr = malloct(2 * keysize);
  char *fil = malloct(2 * keysize);
  char *cat = malloct(4 * keysize);
  char *arena = malloct(keysize + guard);
  int hack;

  (void) argc;
  (void) argv;
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

  /* either half alone past the whole key: each copy is bounded on its own */
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

  /* --strip-query builds its match string from the same two halves, and used
     to abort on a maximal pair. Past STRJOKER_MAXLEN no rule can match, so a
     clipped string would answer where the whole one must stay silent. */
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

    /* a matching rule plus a query longer than the normalizer's own scratch:
       the key builder must clip instead of handing it over */
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
  (void) argc;
  (void) argv;
#define SAME(aa, fa, ab, fb) hts_redirect_same_savefile(opt, aa, fa, ab, fb)
  /* scheme and userinfo collapse (the #159 case); a different path does not */
  assertf(SAME("http://foo.com", "/a/b", "https://foo.com", "/a/b"));
  assertf(SAME("http://user@foo.com", "/a", "http://foo.com", "/a"));
  assertf(!SAME("http://foo.com", "/a", "http://foo.com", "/b"));
  /* www stays distinct here; the crawl's dedup layer folds www, not this helper
   */
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

/* A path of exactly "n" bytes, both ends '/' so bauth_prefix() keeps all of
   it: the key it builds is truncated at the last slash. */
static void st_urlbounds_path(char *fil, size_t n) {
  assertf(n >= 2);
  memset(fil, 'a', n);
  fil[0] = fil[n - 1] = '/';
  fil[n] = '\0';
}

/* #1561 stops ident_url_relatif() emitting a fil longer than HTS_URLMAXSIZE,
   but the consumers that append one into an HTS_URLMAXSIZE*2 buffer have to
   refuse it themselves, or the next producer re-arms every one of them. Each
   case here is the last path the buffer holds, and the first it does not. */
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
  /* the wizard builds "http://" + adr + fil, basic auth builds adr + fil */
  const size_t wzfits =
      HTS_URLMAXSIZE * 2 - strlen("http://") - strlen(adr) - 1;
  const size_t bafits = HTS_URLMAXSIZE * 2 - strlen(adr) - 1;
  char BIGSTK fil[HTS_URLMAXSIZE * 2];
  char BIGSTK prefix[HTS_URLMAXSIZE * 2];
  lien_url primary, *liens[1];
  t_cookie *cookie = calloct(sizeof(t_cookie), 1);
  char **filters = NULL;
  int filptr = 0, prio = 0;

  (void) argc;
  (void) argv;
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

  /* An ordinary URL is still keyed, stored and matched. It goes first so the
     refused lookups below actually read the key: against an empty jar
     bauth_check() never touches it, and a missing NULL guard would show. */
  strcpybuff(fil, "/secure/page.html?a=b");
  assertf(bauth_prefix(prefix, adr, fil) == prefix);
  assertf(strcmp(prefix, "www.example.com/secure/") == 0);
  assertf(bauth_add(cookie, adr, fil, "dXNlcjpwYXNz") == 1);
  assertf(bauth_check(cookie, adr, fil) != NULL);

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
  assertf(hts_acceptlink(opt, 0, adr, fil, NULL, NULL, &prio, NULL) == 0);
  filptr = 0; /* the primary link left its own filters */
  st_urlbounds_path(fil, wzfits + 1);
  assertf(hts_acceptlink(opt, 0, adr, fil, NULL, NULL, &prio, NULL) == 1);

  /* #1561 bounds ident_url_relatif(), not ident_url_absolute(), which still
     takes a scheme-less URL of HTS_URLMAXSIZE*2 - 1 bytes and splits off a
     path past the wizard's limit. These bounds are defence in depth, not
     dead code, and this case fails the day that stops being true. */
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

const struct selftest_entry selftests_name[] = {
    {"urlbounds", "",
     "an over-long path is refused by the wizard and auth consumers",
     st_urlbounds},
    {"simplify", "<path>", "collapse ./ and ../ in a path", st_simplify},
    {"expandhome", "<path>", "expand a leading ~/ into $HOME", st_expandhome},
    {"stripquery", "", "--strip-query pattern/key stripping self-test",
     st_stripquery},
    {"urlhack", "", "-%u url-hack sub-flag (www/slash/query) self-test",
     st_urlhack},
    {"hostalias", "", "--host-alias hostname folding self-test", st_hostalias},
    {"hashkey-bounds", "", "dedup key holds a maximal host+path (#1160)",
     st_hashkey_bounds},
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
    {"savename-addstr", "",
     "save-name append clips to the destination size (#1269)",
     st_savename_addstr},
    {"savename-addtail", "",
     "default name and extension arrive whole on a full buffer (#1269)",
     st_savename_addtail},
    {NULL, NULL, NULL, NULL},
};
