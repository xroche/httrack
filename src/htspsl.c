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
/* File: htspsl.c subroutines:                                  */
/*       Public Suffix List lookups                             */
/* ------------------------------------------------------------ */

#include "htspsl.h"

#include "htscore.h"
#include "htscharset.h"
#include "coucal.h"

/* Kinds of rule for one key "x", which a key can combine. */
#define PSL_EXACT 1     /* "x" */
#define PSL_WILDCARD 2  /* "*.x" */
#define PSL_EXCEPTION 4 /* "!x" */

/* Written only by hts_init() and the self-tests, before any thread reads. */
static coucal psl_rules = NULL;
static char psl_version[64] = "";

/* Add one rule as the list spells it, converting a Unicode name to IDNA. */
static void psl_add(coucal rules, hts_span rule) {
  intptr_t kind = PSL_EXACT, old = 0;
  char key[256];

  if (rule.len > 2 && hts_span_starts_with(rule, hts_span_of("*."))) {
    kind = PSL_WILDCARD;
    rule = hts_span_skip(rule, 2);
  } else if (rule.len > 1 && rule.p[0] == '!') {
    kind = PSL_EXCEPTION;
    rule = hts_span_skip(rule, 1);
  }
  if (hts_host_to_ascii(HTS_SIZED(key), rule.p, rule.len) && key[0] != '\0') {
    hts_lowcase(key);
    (void) coucal_read(rules, key, &old);
    coucal_write(rules, key, old | kind);
  }
}

/* Replace the list in use with rules, freeing the old one. */
static void psl_install(coucal rules, hts_span version) {
  coucal_delete(&psl_rules);
  psl_rules = rules;
  (void) hts_span_copy(version, HTS_SIZED(psl_version));
}

/* Return the leading bytes of S up to its first blank or CR. */
static hts_span psl_word(hts_span s) {
  size_t n = 0;

  while (n < s.len && s.p[n] != ' ' && s.p[n] != '\t' && s.p[n] != '\r')
    n++;
  return hts_span_at(s.p, n);
}

/* Add the rule on one line of a list file, or note its VERSION stamp. */
static void psl_add_line(coucal rules, hts_span line, hts_span *version) {
  static const char tag[] = "// VERSION: ";
  hts_span rule;

  if (hts_span_starts_with(line, hts_span_at(tag, sizeof(tag) - 1))) {
    *version = psl_word(hts_span_skip(line, sizeof(tag) - 1));
    return;
  }
  line = hts_span_trim(line, " \t", "");
  if (hts_span_starts_with(line, hts_span_of("//")))
    return;
  rule = psl_word(line);
  if (rule.len != 0)
    psl_add(rules, rule);
}

hts_boolean hts_psl_load_file(const char *path, size_t min_rules) {
  const LLint size = fsize(path);
  hts_span version = hts_span_of("");
  const char *p, *end;
  char *text;
  coucal rules;
  FILE *fp;

  /* The real list is a few hundred KB. */
  if (size <= 0 || size > 16 * 1024 * 1024)
    return HTS_FALSE;
  fp = FOPEN(path, "rb");
  if (fp == NULL)
    return HTS_FALSE;
  text = malloct((size_t) size + 1);
  if (fread(text, 1, (size_t) size, fp) != (size_t) size) {
    fclose(fp);
    freet(text);
    return HTS_FALSE;
  }
  fclose(fp);
  text[size] = '\0';
  rules = coucal_new(0);
  for (p = text, end = text + size; p < end;) {
    const char *const eol = memchr(p, '\n', (size_t) (end - p));
    const size_t len = (size_t) ((eol != NULL ? eol : end) - p);

    psl_add_line(rules, hts_span_at(p, len), &version);
    p += len + 1;
  }
  if (coucal_nitems(rules) < min_rules || coucal_nitems(rules) == 0) {
    coucal_delete(&rules);
    freet(text);
    return HTS_FALSE;
  }
  psl_install(rules, version);
  freet(text);
  return HTS_TRUE;
}

/* A system list replaces the built-in one only when it is about as large. */
#define PSL_MIN_SYSTEM_RULES 1000

void hts_psl_init_from(const char *path) {
  coucal rules;
  size_t i;

  if (path != NULL && hts_psl_load_file(path, PSL_MIN_SYSTEM_RULES))
    return;
  rules = coucal_new(0);
  for (i = 0; hts_psl_builtin[i] != NULL; i++) {
    const char *p = hts_psl_builtin[i];
    const char *const end = p + hts_psl_builtin_size[i] - 1;

    for (; p < end; p += strlen(p) + 1)
      psl_add(rules, hts_span_of(p));
  }
  psl_install(rules, hts_span_of(hts_psl_builtin_version));
}

void hts_psl_init(void) {
#ifdef HTS_PSL_FILE
  hts_psl_init_from(HTS_PSL_FILE);
#else
  hts_psl_init_from(NULL);
#endif
}

void hts_psl_uninit(void) {
  coucal_delete(&psl_rules);
  psl_version[0] = '\0';
}

hts_boolean hts_psl_loaded(void) { return psl_rules != NULL; }

const char *hts_psl_version(void) { return psl_version; }

static intptr_t psl_kind(const char *key) {
  intptr_t kind = 0;

  return coucal_read(psl_rules, key, &kind) ? kind : 0;
}

/* Fill labels[k] with where the last k + 1 labels of host start, and return
   their count. Return 0 for an empty label or more than max labels. */
static size_t psl_labels(const char *host, const char **labels, size_t max) {
  size_t n = 0;
  const char *p;

  if (host[0] == '\0')
    return 0;
  for (p = host + strlen(host);; p--) {
    if (p == host || p[-1] == '.') {
      if (*p == '.' || *p == '\0' || n == max)
        return 0;
      labels[n++] = p;
      if (p == host)
        return n;
    }
  }
}

/* How many of the n labels form the public suffix? */
static size_t psl_suffix(const char *const *labels, size_t n) {
  size_t k, suffix = 1, exception = 0;
  intptr_t parent = 0;

  for (k = 0; k < n; k++) {
    const intptr_t kind = psl_kind(labels[k]);

    if (kind & PSL_EXCEPTION)
      exception = k + 1;
    /* "*.x" makes x and y.x suffixes, as browsers read it. */
    if ((kind & (PSL_EXACT | PSL_WILDCARD)) || (parent & PSL_WILDCARD))
      suffix = k + 1;
    parent = kind;
  }
  /* An exception is a registrable domain, so drop its first label. */
  return exception != 0 ? exception - 1 : suffix;
}

const char *hts_psl_registrable_domain(const char *host) {
  const char *labels[128], *idna_labels[128];
  const char *const *keys = labels;
  char *idna = NULL;
  size_t n, suffix;

  if (psl_rules == NULL)
    return NULL;
  n = psl_labels(host, labels, sizeof(labels) / sizeof(labels[0]));
  if (n == 0)
    return NULL;
  /* The rules are IDNA, and the conversion keeps every dot. */
  if (!hts_isStringAscii(host, strlen(host))) {
    idna = hts_convertStringUTF8ToIDNA(host, strlen(host));
    if (idna == NULL || psl_labels(idna, idna_labels, n) != n) {
      freet(idna);
      return NULL;
    }
    keys = idna_labels;
  }
  suffix = psl_suffix(keys, n);
  freet(idna);
  return suffix < n ? labels[suffix] : NULL;
}
