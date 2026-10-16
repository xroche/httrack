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

/* What a key stands for: "x" itself, "*.x", "!x", or a mix of them. */
#define PSL_RULE 1
#define PSL_WILDCARD 2
#define PSL_EXCEPTION 4

/* Written only by hts_init() and the self-tests, before any thread reads. */
static coucal psl_rules = NULL;
static char psl_version[64] = "";

/* Add one rule as the list spells it, converting a Unicode name to IDNA. */
static void psl_add(coucal rules, const char *rule, size_t len) {
  intptr_t kind = PSL_RULE, old = 0;
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

static void psl_set(coucal rules, const char *version) {
  coucal_delete(&psl_rules);
  psl_rules = rules;
  psl_version[0] = '\0';
  strlncatbuff(psl_version, version, sizeof(psl_version),
               sizeof(psl_version) - 1);
}

/* Copy the stamp out of a "// VERSION: <stamp>" line, or return HTS_FALSE. */
static hts_boolean psl_version_line(const char *line, char *version,
                                    size_t size) {
  static const char tag[] = "// VERSION: ";
  size_t n;

  if (strncmp(line, tag, sizeof(tag) - 1) != 0)
    return HTS_FALSE;
  line += sizeof(tag) - 1;
  n = strcspn(line, " \t\r\n");
  version[0] = '\0';
  strlncatbuff(version, line, size, n < size ? n : size - 1);
  return HTS_TRUE;
}

hts_boolean hts_psl_load_file(const char *path) {
  char line[1024], version[64] = "";
  coucal rules;
  FILE *fp = FOPEN(path, "rb");

  if (fp == NULL)
    return HTS_FALSE;
  rules = coucal_new(0);
  while (fgets(line, sizeof(line), fp) != NULL) {
    const char *p = line;
    size_t len;

    if (psl_version_line(line, version, sizeof(version)))
      continue;
    /* A line longer than the buffer is no rule, so skip all of it. */
    if (strchr(line, '\n') == NULL && !feof(fp)) {
      int c;

      while ((c = fgetc(fp)) != EOF && c != '\n')
        ;
      continue;
    }
    while (*p == ' ' || *p == '\t')
      p++;
    if (p[0] == '/' && p[1] == '/')
      continue;
    len = strcspn(p, " \t\r\n");
    if (len != 0)
      psl_add(rules, p, len);
  }
  fclose(fp);
  if (coucal_nitems(rules) == 0) {
    coucal_delete(&rules);
    return HTS_FALSE;
  }
  psl_set(rules, version);
  return HTS_TRUE;
}

/* The VERSION stamp of the list file at path, or "" when it has none. */
static void psl_file_version(const char *path, char *version, size_t size) {
  char line[256];
  FILE *fp = FOPEN(path, "rb");
  int i;

  version[0] = '\0';
  if (fp == NULL)
    return;
  /* The stamp sits in the header comment. */
  for (i = 0; i < 32 && fgets(line, sizeof(line), fp) != NULL; i++) {
    if (psl_version_line(line, version, size))
      break;
  }
  fclose(fp);
}

void hts_psl_init(void) {
  coucal rules;
  size_t i;

#ifndef _WIN32
  {
    static const char system_list[] =
        "/usr/share/publicsuffix/public_suffix_list.dat";
    char version[64];

    /* A distro updates its copy, so prefer it when it is the newer one. */
    psl_file_version(system_list, version, sizeof(version));
    if (strcmp(version, hts_psl_builtin_version) > 0 &&
        hts_psl_load_file(system_list))
      return;
  }
#endif
  rules = coucal_new(0);
  for (i = 0; hts_psl_builtin[i] != NULL; i++)
    psl_add(rules, hts_psl_builtin[i], strlen(hts_psl_builtin[i]));
  psl_set(rules, hts_psl_builtin_version);
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
   their count. Return 0 for an empty label or too many labels. */
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
    /* "*.x" covers one label under x, which is the key one level up. */
    if ((kind & PSL_RULE) || (parent & PSL_WILDCARD))
      suffix = k + 1;
    parent = kind;
  }
  /* An exception names a registrable domain, so its suffix is one label less.
   */
  return exception != 0 ? exception - 1 : suffix;
}

const char *hts_psl_registrable_domain(const char *host) {
  const char *labels[128];
  size_t n, suffix;

  if (psl_rules == NULL)
    return NULL;
  n = psl_labels(host, labels, sizeof(labels) / sizeof(labels[0]));
  if (n == 0)
    return NULL;
  if (hts_isStringAscii(host, strlen(host))) {
    suffix = psl_suffix(labels, n);
  } else {
    /* The rules are IDNA, and the conversion keeps every dot. */
    const char *idna_labels[128];
    char *idna = hts_convertStringUTF8ToIDNA(host, strlen(host));

    if (idna == NULL)
      return NULL;
    suffix =
        psl_labels(idna, idna_labels, n) == n ? psl_suffix(idna_labels, n) : n;
    freet(idna);
  }
  return suffix < n ? labels[suffix] : NULL;
}
