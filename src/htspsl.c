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

#define HTS_INTERNAL_BYTECODE

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
static void psl_add(coucal rules, const char *rule, size_t len) {
  intptr_t kind = PSL_EXACT, old = 0;
  char *idna = NULL;
  char key[256];

  if (len > 2 && rule[0] == '*' && rule[1] == '.') {
    kind = PSL_WILDCARD;
    rule += 2;
    len -= 2;
  } else if (len > 1 && rule[0] == '!') {
    kind = PSL_EXCEPTION;
    rule++;
    len--;
  }
  if (!hts_isStringAscii(rule, len)) {
    idna = hts_convertStringUTF8ToIDNA(rule, len);
    if (idna == NULL)
      return;
    rule = idna;
    len = strlen(idna);
  }
  if (len > 0 && len < sizeof(key)) {
    memcpy(key, rule, len);
    key[len] = '\0';
    hts_lowcase(key);
    (void) coucal_read(rules, key, &old);
    coucal_write(rules, key, old | kind);
  }
  freet(idna);
}

/* Replace the list in use with rules, freeing the old one. */
static void psl_install(coucal rules, const char *version, size_t vlen) {
  coucal_delete(&psl_rules);
  psl_rules = rules;
  if (vlen >= sizeof(psl_version))
    vlen = sizeof(psl_version) - 1;
  memcpy(psl_version, version, vlen);
  psl_version[vlen] = '\0';
}

/* Add the rule on one line of a list file, or note its VERSION stamp. */
static void psl_add_line(coucal rules, const char *line, size_t len,
                         const char **version, size_t *vlen) {
  static const char tag[] = "// VERSION: ";
  size_t i = 0, n;

  if (len >= sizeof(tag) - 1 && memcmp(line, tag, sizeof(tag) - 1) == 0) {
    *version = line + sizeof(tag) - 1;
    for (*vlen = 0; *vlen < len - (sizeof(tag) - 1); (*vlen)++) {
      const char c = (*version)[*vlen];

      if (c == ' ' || c == '\t' || c == '\r')
        break;
    }
    return;
  }
  while (i < len && (line[i] == ' ' || line[i] == '\t'))
    i++;
  if (len - i >= 2 && line[i] == '/' && line[i + 1] == '/')
    return;
  for (n = 0; i + n < len; n++) {
    const char c = line[i + n];

    if (c == ' ' || c == '\t' || c == '\r')
      break;
  }
  if (n != 0)
    psl_add(rules, line + i, n);
}

hts_boolean hts_psl_load_file(const char *path, size_t min_rules) {
  const LLint size = fsize(path);
  const char *version = "", *p, *end;
  size_t vlen = 0;
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

    psl_add_line(rules, p, len, &version, &vlen);
    p += len + 1;
  }
  if (coucal_nitems(rules) < min_rules || coucal_nitems(rules) == 0) {
    coucal_delete(&rules);
    freet(text);
    return HTS_FALSE;
  }
  psl_install(rules, version, vlen);
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
      psl_add(rules, p, strlen(p));
  }
  psl_install(rules, hts_psl_builtin_version, strlen(hts_psl_builtin_version));
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
