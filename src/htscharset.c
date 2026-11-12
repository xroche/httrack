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
/* File: Charset conversion functions                           */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htscharset.h"
#ifdef _WIN32
#include <shellapi.h>
#endif
#include "htsbase.h"
#include "punycode.h"
#include "htssafe.h"

#include <stdint.h>
#include <stdarg.h>

int hts_isStringAscii(const char *s, size_t size) {
  size_t i;

  for(i = 0; i < size; i++) {
    const unsigned char c = (const unsigned char) s[i];

    if (c >= 0x80) {
      return 0;
    }
  }
  return 1;
}

#define IS_ALNUM(C) ( ((C) >= 'A' && (C) <= 'Z') || ((C) >= 'a' && (C) <= 'z') || ((C) >= '0' && (C) <= '9') )
#define CHAR_LOWER(C) ( ((C) >= 'A' && (C) <= 'Z') ? ((C) + 'a' - 'A') : (C) )
static int hts_equalsAlphanum(const char *a, const char *b) {
  size_t i, j;
  for(i = 0, j = 0;; i++, j++) {
    /* Skip non-alnum */
    for(; a[i] != '\0' && !IS_ALNUM(a[i]); i++) ;
    for(; b[j] != '\0' && !IS_ALNUM(b[j]); j++) ;
    /* Compare */
    if (CHAR_LOWER(a[i]) != CHAR_LOWER(b[j])) {
      break;
    }
    /* End of string ? (note: a[i] == b[j]) */
    else if (a[i] == '\0') {
      return 1;
    }
  }
  return 0;
}
#undef IS_ALNUM
#undef CHAR_LOWER

/* Copy the memory region [s .. s + size - 1 ] as a \0-terminated string. */
static char *hts_stringMemCopy(const char *s, size_t size) {
  char *dest = malloct(size + 1);

  if (dest != NULL) {
    memcpy(dest, s, size);
    dest[size] = '\0';
    return dest;
  }
  return NULL;
}

#ifdef _WIN32

typedef struct wincodepage_t wincodepage_t;
struct wincodepage_t {
  UINT codepage;
  const char *name;
};

/* See <http://msdn.microsoft.com/en-us/library/windows/desktop/dd317756%28v=vs.85%29.aspx> */
static const wincodepage_t codepages[] = {
  {37, "ibm037"},
  {437, "ibm437"},
  {500, "ibm500"},
  {708, "asmo-708"},
  {720, "dos-720"},
  {737, "ibm737"},
  {775, "ibm775"},
  {850, "ibm850"},
  {852, "ibm852"},
  {855, "ibm855"},
  {857, "ibm857"},
  {858, "ibm00858"},
  {860, "ibm860"},
  {861, "ibm861"},
  {862, "dos-862"},
  {863, "ibm863"},
  {864, "ibm864"},
  {865, "ibm865"},
  {866, "cp866"},
  {869, "ibm869"},
  {870, "ibm870"},
  {874, "windows-874"},
  {875, "cp875"},
  {932, "shift_jis"},
  {936, "gb2312"},
  {949, "ks_c_5601-1987"},
  {950, "big5"},
  {1026, "ibm1026"},
  {1047, "ibm01047"},
  {1140, "ibm01140"},
  {1141, "ibm01141"},
  {1142, "ibm01142"},
  {1143, "ibm01143"},
  {1144, "ibm01144"},
  {1145, "ibm01145"},
  {1146, "ibm01146"},
  {1147, "ibm01147"},
  {1148, "ibm01148"},
  {1149, "ibm01149"},
  {1200, "utf-16"},
  {1201, "unicodefffe"},
  {1250, "windows-1250"},
  {1251, "windows-1251"},
  {1252, "windows-1252"},
  {1253, "windows-1253"},
  {1254, "windows-1254"},
  {1255, "windows-1255"},
  {1256, "windows-1256"},
  {1257, "windows-1257"},
  {1258, "windows-1258"},
  {1361, "johab"},
  {10000, "macintosh"},
  {10001, "x-mac-japanese"},
  {10002, "x-mac-chinesetrad"},
  {10003, "x-mac-korean"},
  {10004, "x-mac-arabic"},
  {10005, "x-mac-hebrew"},
  {10006, "x-mac-greek"},
  {10007, "x-mac-cyrillic"},
  {10008, "x-mac-chinesesimp"},
  {10010, "x-mac-romanian"},
  {10017, "x-mac-ukrainian"},
  {10021, "x-mac-thai"},
  {10029, "x-mac-ce"},
  {10079, "x-mac-icelandic"},
  {10081, "x-mac-turkish"},
  {10082, "x-mac-croatian"},
  {12000, "utf-32"},
  {12001, "utf-32be"},
  {20000, "x-chinese_cns"},
  {20001, "x-cp20001"},
  {20002, "x_chinese-eten"},
  {20003, "x-cp20003"},
  {20004, "x-cp20004"},
  {20005, "x-cp20005"},
  {20105, "x-ia5"},
  {20106, "x-ia5-german"},
  {20107, "x-ia5-swedish"},
  {20108, "x-ia5-norwegian"},
  {20127, "us-ascii"},
  {20261, "x-cp20261"},
  {20269, "x-cp20269"},
  {20273, "ibm273"},
  {20277, "ibm277"},
  {20278, "ibm278"},
  {20280, "ibm280"},
  {20284, "ibm284"},
  {20285, "ibm285"},
  {20290, "ibm290"},
  {20297, "ibm297"},
  {20420, "ibm420"},
  {20423, "ibm423"},
  {20424, "ibm424"},
  {20833, "x-ebcdic-koreanextended"},
  {20838, "ibm-thai"},
  {20866, "koi8-r"},
  {20871, "ibm871"},
  {20880, "ibm880"},
  {20905, "ibm905"},
  {20924, "ibm00924"},
  {20932, "euc-jp"},
  {20936, "x-cp20936"},
  {20949, "x-cp20949"},
  {21025, "cp1025"},
  {21866, "koi8-u"},
  {28591, "iso-8859-1"},
  {28592, "iso-8859-2"},
  {28593, "iso-8859-3"},
  {28594, "iso-8859-4"},
  {28595, "iso-8859-5"},
  {28596, "iso-8859-6"},
  {28597, "iso-8859-7"},
  {28598, "iso-8859-8"},
  {28599, "iso-8859-9"},
  {28603, "iso-8859-13"},
  {28605, "iso-8859-15"},
  {29001, "x-europa"},
  {38598, "iso-8859-8-i"},
  {50220, "iso-2022-jp"},
  {50221, "csiso2022jp"},
  {50222, "iso-2022-jp"},
  {50225, "iso-2022-kr"},
  {50227, "x-cp50227"},
  {50229, "iso-2022-cn"},
  {51932, "euc-jp"},
  {51936, "euc-cn"},
  {51949, "euc-kr"},
  {52936, "hz-gb-2312"},
  {54936, "gb18030"},
  {57002, "x-iscii-de"},
  {57003, "x-iscii-be"},
  {57004, "x-iscii-ta"},
  {57005, "x-iscii-te"},
  {57006, "x-iscii-as"},
  {57007, "x-iscii-or"},
  {57008, "x-iscii-ka"},
  {57009, "x-iscii-ma"},
  {57010, "x-iscii-gu"},
  {57011, "x-iscii-pa"},
  {65000, "utf-7"},
  {65001, "utf-8"},
  {0, NULL}
};

/* Get a Windows codepage, by its name. Return 0 upon error. */
UINT hts_getCodepage(const char *name) {
  int id;

  for(id = 0; codepages[id].name != NULL; id++) {
    /* Compare the two strings, lowercase and alphanum only (ISO88591 == iso-8859-1) */
    if (hts_equalsAlphanum(name, codepages[id].name)) {
      return codepages[id].codepage;
    }
  }

  /* Not found */
  return 0;
}

LPWSTR hts_convertStringToUCS2(const char *s, int size, UINT cp, int *pwsize) {
  /* Size in wide chars of the output */
  const int wsize = MultiByteToWideChar(cp, 0, (LPCSTR) s, size, NULL, 0);

  if (wsize > 0) {
    LPSTR uoutput = NULL;
    LPWSTR woutput = malloct((wsize + 1) * sizeof(WCHAR));

    if (woutput != NULL
        && MultiByteToWideChar(cp, 0, (LPCSTR) s, size, woutput,
                               wsize) == wsize) {
      const int usize =
        WideCharToMultiByte(CP_UTF8, 0, woutput, wsize, NULL, 0, NULL, FALSE);
      if (usize > 0) {
        woutput[wsize] = 0x0;
        if (pwsize != NULL)
          *pwsize = wsize;
        return woutput;
      }
    }
    freet(woutput);
  }
  return NULL;
}

LPWSTR hts_convertUTF8StringToUCS2(const char *s, int size, int *pwsize) {
  return hts_convertStringToUCS2(s, size, CP_UTF8, pwsize);
}

/* WideCharToMultiByte rejects lpUsedDefaultChar, and any non-zero dwFlags, on
   the Unicode, ISO-2022, HZ, GB18030 and ISCII codepages, where a substitution
   stays invisible. */
static hts_boolean cp_reports_default_char(UINT cp) {
  if (cp == 42 /* CP_SYMBOL */ || cp == CP_UTF7 || cp == CP_UTF8 ||
      cp == 52936 || cp == 54936 || (cp >= 50220 && cp <= 50229) ||
      (cp >= 57002 && cp <= 57011)) {
    return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* When plossy is non-NULL, *plossy reports that the codepage lacked a character
   and a substitute was emitted for it. */
static char *hts_convertUCS2StringToCPEx(LPWSTR woutput, int wsize, UINT cp,
                                         hts_boolean *plossy) {
  /* Best-fit maps U+00A5 to CP932's backslash, usedDefault unset. */
  const DWORD flags = cp_reports_default_char(cp) ? WC_NO_BEST_FIT_CHARS : 0;
  /* U+00B5 sizes 1 on CP932, best-fit needs 2: size for the larger pass. */
  const int bsize =
      WideCharToMultiByte(cp, 0, woutput, wsize, NULL, 0, NULL, NULL);
  const int fsize = flags != 0 ? WideCharToMultiByte(cp, flags, woutput, wsize,
                                                     NULL, 0, NULL, NULL)
                               : bsize;
  const int usize = bsize > fsize ? bsize : fsize;

  if (plossy != NULL) {
    *plossy = HTS_FALSE;
  }
  if (usize > 0) {
    char *uoutput = malloct((usize + 1) * sizeof(char));

    if (uoutput != NULL) {
      BOOL usedDefault = FALSE;
      LPBOOL const pUsedDefault =
          plossy != NULL && cp_reports_default_char(cp) ? &usedDefault : NULL;

      const int n = WideCharToMultiByte(cp, flags, woutput, wsize, uoutput,
                                        usize, NULL, pUsedDefault);

      /* usize only bounds the result; the written length is what n reports */
      if (n > 0 && n <= usize) {
        uoutput[n] = '\0';
        if (plossy != NULL && usedDefault) {
          *plossy = HTS_TRUE;
        }
        return uoutput;
      } else {
        freet(uoutput);
      }
    }
  }
  return NULL;
}

char *hts_convertUCS2StringToCP(LPWSTR woutput, int wsize, UINT cp) {
  return hts_convertUCS2StringToCPEx(woutput, wsize, cp, NULL);
}

char *hts_convertUCS2StringToUTF8(LPWSTR woutput, int wsize) {
  return hts_convertUCS2StringToCP(woutput, wsize, CP_UTF8);
}

char *hts_convertStringCPToUTF8(const char *s, size_t size, UINT cp) {
  /* Empty string ? */
  if (size == 0) {
    return hts_stringMemCopy(s, size);
  }
  /* Already UTF-8 ? */
  if (cp == CP_UTF8 || hts_isStringAscii(s, size)) {
    return hts_stringMemCopy(s, size);
  }
  /* Other (valid) charset */
  else if (cp != 0) {
    /* Size in wide chars of the output */
    int wsize;
    LPWSTR woutput = hts_convertStringToUCS2(s, (int) size, cp, &wsize);

    if (woutput != NULL) {
      char *const uoutput = hts_convertUCS2StringToUTF8(woutput, wsize);

      freet(woutput);
      return uoutput;
    }
  }

  /* Error, charset not found! */
  return NULL;
}

static char *hts_convertStringCPFromUTF8Ex(const char *s, size_t size, UINT cp,
                                           hts_boolean *plossy) {
  if (plossy != NULL) {
    *plossy = HTS_FALSE;
  }
  /* Empty string ? */
  if (size == 0) {
    return hts_stringMemCopy(s, size);
  }
  /* Already UTF-8 ? */
  if (cp == CP_UTF8 || hts_isStringAscii(s, size)) {
    return hts_stringMemCopy(s, size);
  }
  /* Other (valid) charset */
  else if (cp != 0) {
    /* Size in wide chars of the output */
    int wsize;
    LPWSTR woutput = hts_convertStringToUCS2(s, (int) size, CP_UTF8, &wsize);

    if (woutput != NULL) {
      char *const uoutput =
          hts_convertUCS2StringToCPEx(woutput, wsize, cp, plossy);

      freet(woutput);
      return uoutput;
    }
  }

  /* Error, charset not found! */
  return NULL;
}

char *hts_convertStringCPFromUTF8(const char *s, size_t size, UINT cp) {
  return hts_convertStringCPFromUTF8Ex(s, size, cp, NULL);
}

HTSEXT_API char *hts_convertStringToUTF8(const char *s, size_t size,
                                         const char *charset) {
  const UINT cp = hts_getCodepage(charset);

  return hts_convertStringCPToUTF8(s, size, cp);
}

char *hts_convertStringFromUTF8(const char *s, size_t size, const char *charset) {
  const UINT cp = hts_getCodepage(charset);

  return hts_convertStringCPFromUTF8(s, size, cp);
}

char *hts_convertStringFromUTF8Strict(const char *s, size_t size,
                                      const char *charset) {
  hts_boolean lossy = HTS_FALSE;
  char *out =
      hts_convertStringCPFromUTF8Ex(s, size, hts_getCodepage(charset), &lossy);

  if (lossy) {
    freet(out);
    return NULL;
  }
  return out;
}

HTSEXT_API char *hts_convertStringSystemToUTF8(const char *s, size_t size) {
  return hts_convertStringCPToUTF8(s, size, GetACP());
}

HTSEXT_API char *hts_convertStringUTF8ToSystem(const char *s, size_t size) {
  return hts_convertStringCPFromUTF8(s, size, GetACP());
}

HTSEXT_API void hts_argv_utf8(int *pargc, char ***pargv) {
  int wargc = 0;
  LPWSTR *const wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
  char **argv;
  int i;

  // On any failure keep the CRT's ANSI argv: lossy, but never half-converted.
  if (wargv == NULL)
    return;
  if (wargc <= 0 ||
      (argv = calloct((size_t) wargc + 1, sizeof(char *))) == NULL) {
    LocalFree(wargv);
    return;
  }
  for (i = 0; i < wargc; i++) {
    const int wsize = (int) wcslen(wargv[i]);

    // hts_convertUCS2StringToUTF8() returns NULL on an empty string
    argv[i] =
        wsize != 0 ? hts_convertUCS2StringToUTF8(wargv[i], wsize) : strdupt("");
    if (argv[i] == NULL) {
      while (i-- != 0)
        freet(argv[i]);
      freet(argv);
      LocalFree(wargv);
      return;
    }
  }
  argv[wargc] = NULL; // callers may rely on argv[argc] == NULL
  LocalFree(wargv);
  *pargc = wargc;
  *pargv = argv; // never freed: argv lives for the process
}

#else

#include <errno.h>

#if ( defined(HTS_USEICONV) && ( HTS_USEICONV == 0 ) )
#define DISABLE_ICONV
#endif

#ifndef DISABLE_ICONV
#include <iconv.h>
#else
#include "htscodepages.h"

/* The tables are keyed "cp1252", but IANA, HTTP headers and <meta charset>
   spell the same codepages "windows-1252" and "IBM850". */
static int hts_equalsCodepageName(const char *table_name, const char *label) {
  static const char *const prefixes[] = {"windows", "ibm", NULL};
  size_t p, i;

#define LOWER(C) (((C) >= 'A' && (C) <= 'Z') ? ((C) + 'a' - 'A') : (C))
  if (hts_equalsAlphanum(table_name, label)) {
    return 1;
  }
  if (LOWER(table_name[0]) != 'c' || LOWER(table_name[1]) != 'p') {
    return 0;
  }
  /* "cp" then whatever followed the prefix; hts_equalsAlphanum drops the
     separator, so "windows-1252" and "windows1252" both reach "cp1252". */
  for (p = 0; prefixes[p] != NULL; p++) {
    for (i = 0; prefixes[p][i] != '\0' && LOWER(label[i]) == prefixes[p][i];
         i++)
      ;
    if (prefixes[p][i] == '\0' &&
        hts_equalsAlphanum(&table_name[2], &label[i])) {
      return 1;
    }
  }
#undef LOWER
  return 0;
}

/* Decode size bytes of a codepage to a NUL-terminated UTF-8 string, or NULL
   when no table names that codepage. */
static char *hts_codepageToUTF8(const char *codepage, const char *s,
                                size_t size) {
  size_t i, j, k, capa;
  char *dest;

  for (i = 0; table_mappings[i].name != NULL &&
              !hts_equalsCodepageName(table_mappings[i].name, codepage);
       i++)
    ;
  if (table_mappings[i].name == NULL) {
    return NULL;
  }

#define MAX_UTF 8
  /* Allocated before the loop: an empty input writes the terminator too. */
  capa = 16;
  dest = malloct(capa);
  if (dest == NULL) {
    return NULL;
  }
  for (j = 0, k = 0; j < size; j++) {
    const unsigned char c = (unsigned char) s[j];
    const hts_UCS4 uc = table_mappings[i].table[c];
    const size_t max = k + MAX_UTF;

    /* Room for the code point and for the terminator past it. */
    if (capa <= max) {
      char *grown;
      /* From a non-zero floor: doubling a zero capacity never progresses. */
      size_t want = capa > 16 ? capa : 16;

      while (want <= max) {
        if (want > (size_t) -1 / 2) {
          freet(dest);
          return NULL;
        }
        want <<= 1;
      }
      grown = realloct(dest, want);
      if (grown == NULL) {
        freet(dest);
        return NULL;
      }
      dest = grown;
      capa = want;
    }
    k += hts_writeUTF8(uc, &dest[k], MAX_UTF);
    assertf(k < capa);
  }
  dest[k] = '\0';
  return dest;
#undef MAX_UTF
}
#endif

static char *hts_convertStringCharset(const char *s, size_t size,
                                      const char *to, const char *from) {
  /* Empty string ? */
  if (size == 0) {
    return strdupt("");
  }
  /* Already on correct charset ? */
  if (hts_equalsAlphanum(from, to)) {
    return hts_stringMemCopy(s, size);
  }
#ifndef DISABLE_ICONV
  /* Find codepage */
  else {
    const iconv_t cp = iconv_open(to, from);

    if (cp != (iconv_t) - 1) {
      char *inbuf = (char*) (uintptr_t) s; /* ugly iconv api, sheesh */
      size_t inbytesleft = size;
      size_t outbufCapa = 0;
      char *outbuf = NULL;
      size_t outbytesleft = 0;
      size_t finalSize;

      /* Initial size to around the string size */
      for(outbufCapa = 16; outbufCapa < size + 1; outbufCapa *= 2) ;
      outbuf = malloct(outbufCapa);
      outbytesleft = outbufCapa;

      /* Convert */
      while(outbuf != NULL && inbytesleft != 0) {
        const size_t offset = outbufCapa - outbytesleft;
        char *outbufCurrent = outbuf + offset;
        const size_t ret =
          iconv(cp, &inbuf, &inbytesleft, &outbufCurrent, &outbytesleft);
        if (ret == (size_t) - 1) {
          if (errno == E2BIG) {
            const size_t used = outbufCapa - outbytesleft;
            char *grown;

            outbufCapa *= 2;
            /* Reached from an exported entry point, which answers NULL rather
               than taking its embedder down. */
            grown = realloct(outbuf, outbufCapa);
            if (grown == NULL) {
              freet(outbuf);
              break;
            }
            outbuf = grown;
            outbytesleft = outbufCapa - used;
          } else {
            freet(outbuf);
            outbuf = NULL;
            break;
          }
        }
      }

      /* Final size ? */
      finalSize = outbufCapa - outbytesleft;

      /* Terminating \0 */
      if (outbuf != NULL && finalSize + 1 >= outbufCapa) {
        char *const grown = realloct(outbuf, finalSize + 1);

        if (grown == NULL) {
          freet(outbuf);
        } else {
          outbuf = grown;
        }
      }
      if (outbuf != NULL)
        outbuf[finalSize] = '\0';

      /* Close codepage */
      iconv_close(cp);

      /* Return result (may be NULL) */
      return outbuf;
    }
  }
#else
  /* Limited codepage decoding support only. */
  if (hts_isCharsetUTF8(to)) {
    return hts_codepageToUTF8(from, s, size);
  }
#endif

  /* Error, charset not found! */
  return NULL;
}

HTSEXT_API char *hts_convertStringToUTF8(const char *s, size_t size,
                                         const char *charset) {
  /* Empty string ? */
  if (size == 0) {
    return strdupt("");
  }
  /* Already UTF-8 ? */
  if (hts_isCharsetUTF8(charset) || hts_isStringAscii(s, size)) {
    return hts_stringMemCopy(s, size);
  }
  /* Find codepage */
  else {
    return hts_convertStringCharset(s, size, "utf-8", charset);
  }
}

char *hts_convertStringFromUTF8(const char *s, size_t size, const char *charset) {
  /* Empty string ? */
  if (size == 0) {
    return strdupt("");
  }
  /* Already UTF-8 ? */
  if (hts_isCharsetUTF8(charset) || hts_isStringAscii(s, size)) {
    return hts_stringMemCopy(s, size);
  }
  /* Find codepage */
  else {
    return hts_convertStringCharset(s, size, charset, "utf-8");
  }
}

char *hts_convertStringFromUTF8Strict(const char *s, size_t size,
                                      const char *charset) {
  /* No transliteration is requested of iconv, so an unrepresentable code point
     already fails the conversion outright. */
  return hts_convertStringFromUTF8(s, size, charset);
}

#endif

#ifdef _WIN32
#define strcasecmp(a,b) stricmp(a,b)
#define strncasecmp(a,b,n) strnicmp(a,b,n)
#endif

/* WHATWG prescan whitespace: SPACE, TAB, CR, LF, FF, but not VT. */
static int is_html_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}

static int is_html_space_or_equal(char c) {
  return is_html_space(c) || c == '=';
}

static int is_html_space_or_equal_or_quote(char c) {
  return is_html_space_or_equal(c) || c == '"' || c == '\'';
}

size_t hts_stringLengthUTF8(const char *s) {
  const unsigned char *const bytes = (const unsigned char *) s;
  size_t i, len;

  for(i = 0, len = 0; bytes[i] != '\0'; i++) {
    const unsigned char c = bytes[i];

    if (HTS_IS_LEADING_UTF8(c)) {       /* ASCII or leading byte */
      len++;
    }
  }
  return len;
}

size_t hts_copyStringUTF8(char *dest, const char *src, size_t size) {
  const unsigned char *const bytes = (const unsigned char *) src;
  size_t i, mark;

  for(i = 0, mark = 0; ( i == 0 || bytes[i + 1] != '\0' ) && i <= size; i++) {
    const unsigned char c = bytes[i];

    dest[i] = c;
    if (HTS_IS_LEADING_UTF8(c)) {
      mark = i;
    }
  }
  dest[mark] = '\0';

  return mark;
}

size_t hts_appendStringUTF8(char *dest, const char *src,  size_t nBytes) {
  const size_t size = strlen(dest);
  return hts_copyStringUTF8(dest + size, src, nBytes);
}

int hts_isCharsetUTF8(const char *charset) {
  return charset != NULL 
    && ( strcasecmp(charset, "utf-8") == 0 
         || strcasecmp(charset, "utf8") == 0 );
}

/* Extract X from a "text/html; charset=X" content= attribute value. */
static char *charset_from_content(const char *s, size_t len) {
  size_t i;

  for (i = 0; i + 7 < len; i++) {
    if ((i == 0 || is_html_space(s[i - 1]) || s[i - 1] == ';') &&
        strncasecmp(&s[i], "charset", 7) == 0 &&
        is_html_space_or_equal(s[i + 7])) {
      size_t j, val;

      for (j = i + 7; j < len && is_html_space_or_equal_or_quote(s[j]); j++)
        ;
      for (val = j; j < len && s[j] != '"' && s[j] != '\'' && s[j] != ';' &&
                    !is_html_space(s[j]);
           j++)
        ;
      if (j != val) {
        return hts_stringMemCopy(&s[val], j - val);
      }
    }
  }
  return NULL;
}

char *hts_getCharsetFromMeta(const char *html, size_t size) {
  size_t i;

  /* HTML5 <meta charset=X> or legacy
     <meta http-equiv="Content-Type" content="text/html; charset=X">,
     attributes in any order; first resolvable tag wins. */
  for (i = 0; i + 6 < size; i++) {
    size_t j;
    const char *equiv = NULL, *content = NULL;
    size_t equiv_len = 0, content_len = 0;

    if (html[i] != '<' || strncasecmp(&html[i + 1], "meta", 4) != 0 ||
        !is_html_space(html[i + 5])) {
      continue;
    }
    /* Attribute scan, strictly bounded by size. */
    for (j = i + 6; j < size && html[j] != '>';) {
      size_t name, name_len, val = 0, val_len = 0;

      if (is_html_space(html[j]) || html[j] == '/') {
        j++;
        continue;
      }
      for (name = j; j < size && html[j] != '=' && html[j] != '>' &&
                     html[j] != '/' && !is_html_space(html[j]);
           j++)
        ;
      name_len = j - name;
      for (; j < size && is_html_space(html[j]); j++)
        ;
      if (j < size && html[j] == '=') {
        for (j++; j < size && is_html_space(html[j]); j++)
          ;
        if (j < size && (html[j] == '"' || html[j] == '\'')) {
          const char quote = html[j++];

          for (val = j; j < size && html[j] != quote; j++)
            ;
          if (j >= size) /* unterminated quote: drop the tag */
            break;
          val_len = j++ - val;
        } else {
          /* unquoted: ends at whitespace or '>' only (HTML5 prescan); '/'
             belongs to the value except as the tail of a self-close */
          for (val = j; j < size && !is_html_space(html[j]) && html[j] != '>';
               j++)
            ;
          val_len = j - val;
          if (val_len != 0 && j < size && html[j] == '>' &&
              html[val + val_len - 1] == '/') {
            val_len--;
          }
        }
      }
      /* first occurrence of each attribute wins, as in browsers */
      if (val_len != 0) {
        if (name_len == 7 && strncasecmp(&html[name], "charset", 7) == 0) {
          return hts_stringMemCopy(&html[val], val_len);
        } else if (equiv == NULL && name_len == 10 &&
                   strncasecmp(&html[name], "http-equiv", 10) == 0) {
          equiv = &html[val];
          equiv_len = val_len;
        } else if (content == NULL && name_len == 7 &&
                   strncasecmp(&html[name], "content", 7) == 0) {
          content = &html[val];
          content_len = val_len;
        }
      }
    }
    if (equiv_len == 12 && strncasecmp(equiv, "content-type", 12) == 0 &&
        content != NULL) {
      char *const s = charset_from_content(content, content_len);

      if (s != NULL) {
        return s;
      }
    }
    i = j;
  }
  return NULL;
}

/* UTF-8 helpers */

/* Length of the strict RFC 3629 sequence that lead starts, or 0. */
static size_t utf8_lead_length(unsigned char lead) {
  if (lead < 0x80)
    return 1;
  if (lead >= 0xc2 && lead <= 0xdf)
    return 2;
  if (lead >= 0xe0 && lead <= 0xef)
    return 3;
  if (lead >= 0xf0 && lead <= 0xf4)
    return 4;
  return 0;
}

/* The one UTF-8 decoder: overlongs, surrogates, > U+10FFFF and truncated
   sequences give 0, else the sequence length. */
static size_t utf8_decode(const unsigned char *s, size_t size, hts_UCS4 *puc) {
  static const hts_UCS4 min[5] = {0, 0, 0x80, 0x800, 0x10000};
  const size_t len = size != 0 ? utf8_lead_length(s[0]) : 0;
  hts_UCS4 uc;
  size_t k;

  if (len == 0 || size < len)
    return 0;
  if (len == 1) {
    *puc = s[0];
    return 1;
  }
  uc = s[0] & (0x7f >> len);
  for (k = 1; k < len; k++) {
    if ((s[k] & 0xc0) != 0x80)
      return 0;
    uc = (uc << 6) | (s[k] & 0x3f);
  }
  if (uc < min[len] || uc > 0x10FFFF || (uc >= 0xD800 && uc <= 0xDFFF))
    return 0;
  *puc = uc;
  return len;
}

/* Encode uc into out[4]; a surrogate or a value above U+10FFFF gives 0. */
static size_t utf8_encode(hts_UCS4 uc, unsigned char *out) {
  if (uc < 0x80) {
    out[0] = (unsigned char) uc;
    return 1;
  } else if (uc < 0x800) {
    out[0] = (unsigned char) (0xc0 | (uc >> 6));
    out[1] = (unsigned char) (0x80 | (uc & 0x3f));
    return 2;
  } else if (uc < 0x10000) {
    if (uc >= 0xD800 && uc <= 0xDFFF)
      return 0;
    out[0] = (unsigned char) (0xe0 | (uc >> 12));
    out[1] = (unsigned char) (0x80 | ((uc >> 6) & 0x3f));
    out[2] = (unsigned char) (0x80 | (uc & 0x3f));
    return 3;
  } else if (uc <= 0x10FFFF) {
    out[0] = (unsigned char) (0xf0 | (uc >> 18));
    out[1] = (unsigned char) (0x80 | ((uc >> 12) & 0x3f));
    out[2] = (unsigned char) (0x80 | ((uc >> 6) & 0x3f));
    out[3] = (unsigned char) (0x80 | (uc & 0x3f));
    return 4;
  }
  return 0;
}

/* IDNA helpers. */
#undef ADD_BYTE
#undef INCREASE_CAPA
#define INCREASE_CAPA()                                                        \
  do {                                                                         \
    capa = capa < 16 ? 16 : (capa << 1);                                       \
    dest = realloct(dest, capa * sizeof(dest[0]));                             \
    assertf(dest != NULL);                                                     \
  } while (0)
#define ADD_BYTE(C) do { \
  if (capa == destSize) { \
    INCREASE_CAPA(); \
  } \
  dest[destSize++] = (C); \
} while(0)
#define FREE_BUFFER() freet(dest)

char *hts_convertStringUTF8ToIDNA(const char *s, size_t size) {
  char *dest = NULL;
  size_t capa = 0, destSize = 0;
  size_t i, startSeg;
  int nonAsciiFound;

  for(i = startSeg = 0, nonAsciiFound = 0 ; i <= size ; i++) {
    const unsigned char c = i < size ? (unsigned char) s[i] : 0;
    /* separator (ending, url segment, scheme, path segment, query string) */
    if (c == 0 || c == '.' || c == ':' || c == '/' || c == '?') {
      /* non-empty segment */
      if (startSeg != i) {
        /* IDNA ? */
        if (nonAsciiFound) {
          const size_t segSize = i - startSeg;
          const unsigned char *segData = (const unsigned char*) &s[startSeg];
          punycode_uint *segInt = NULL;
          size_t j, utfSeq, segOutputSize;

          punycode_uint output_length;
          punycode_status status;

          /* IDNA prefix */
          ADD_BYTE('x');
          ADD_BYTE('n');
          ADD_BYTE('-');
          ADD_BYTE('-');
          
          /* copy utf-8 to integers. note: buffer is sufficient! */
          segInt = (punycode_uint *) malloct(segSize * sizeof(punycode_uint));
          if (segInt == NULL) {
            FREE_BUFFER();
            return NULL;
          }
          for (j = 0, segOutputSize = 0; j < segSize; j += utfSeq) {
            hts_UCS4 uc;

            utfSeq = utf8_decode(&segData[j], segSize - j, &uc);
            if (utfSeq == 0) {
              freet(segInt);
              FREE_BUFFER();
              return NULL;
            }
            segInt[segOutputSize++] = uc;
          }

          /* encode */
          output_length = (punycode_uint) ( capa - destSize );
          while ((status = punycode_encode(
                      (punycode_uint) segOutputSize, segInt, &output_length,
                      &dest[destSize])) == punycode_big_output) {
            INCREASE_CAPA();
            output_length = (punycode_uint) (capa - destSize);
          }

          /* cleanup */
          freet(segInt);

          /* success ? */
          if (status == punycode_success) {
            destSize += output_length;
          } else {
            FREE_BUFFER();
            return NULL;
          }
        }
        /* copy ascii segment otherwise */
        else {
          size_t j;
          for(j = startSeg ; j < i ; j++) {
            const char c = s[j];
            ADD_BYTE(c);
          }
        }
      }

      /* next segment start */
      startSeg = i + 1;
      nonAsciiFound = 0;

      /* add separator (including terminating \0) */
      ADD_BYTE(c);
    }
    /* found non-ascii */
    else if (c >= 0x80) {
      nonAsciiFound = 1;
    }
  }

  return dest;
}

hts_boolean hts_host_to_ascii(char *dst, size_t size, const char *host,
                              size_t len) {
  char *idna = NULL;
  const char *src = host;
  size_t n = len;
  hts_boolean fits;

  if (!hts_isStringAscii(host, len)) {
    idna = hts_convertStringUTF8ToIDNA(host, len);
    if (idna == NULL)
      return HTS_FALSE;
    src = idna;
    n = strlen(idna);
  }
  fits = n < size;
  if (fits) {
    // dst may be host itself
    memmove(dst, src, n);
    dst[n] = '\0';
  }
  freet(idna);
  return fits;
}

int hts_isStringIDNA(const char *s, size_t size) {
  size_t i, startSeg;
  for(i = startSeg = 0 ; i <= size ; i++) {
    const unsigned char c = i < size ? s[i] : 0;
    if (c == 0 || c == '.' || c == ':' || c == '/' || c == '?') {
      const size_t segSize = i - startSeg;
      /* IDNA segment ? */
      if (segSize > 4
          && strncasecmp(&s[startSeg], "xn--", 4) == 0) {
        return 1;
      }
      /* next segment start */
      startSeg = i + 1;
    }
  }
  return 0;
}

char *hts_convertStringIDNAToUTF8(const char *s, size_t size) {
  char *dest = NULL;
  size_t capa = 0, destSize = 0;
  size_t i, startSeg;
  for(i = startSeg = 0 ; i <= size ; i++) {
    const unsigned char c = i < size ? s[i] : 0;
    if (c == 0 || c == '.' || c == ':' || c == '/' || c == '?') {
      const size_t segSize = i - startSeg;
      /* IDNA segment ? */
      if (segSize > 4
          && strncasecmp(&s[startSeg], "xn--", 4) == 0) {
        punycode_status status;
        punycode_uint output_capa;
        punycode_uint output_length;
        punycode_uint *output_dest;

        /* encode. pre-reserve buffer. */
        for(output_capa = 16 ; output_capa < segSize 
          ; output_capa <<= 1) ;
        output_dest =
            (punycode_uint *) malloct(output_capa * sizeof(punycode_uint));
        if (output_dest == NULL) {
          FREE_BUFFER();
          return NULL;
        }
        for (output_length = output_capa;
             (status = punycode_decode((punycode_uint) segSize - 4,
                                       &s[startSeg + 4], &output_length,
                                       output_dest)) == punycode_big_output;) {
          output_capa <<= 1;
          output_dest = (punycode_uint *) realloct(
              output_dest, output_capa * sizeof(punycode_uint));
          assertf(output_dest != NULL);
          output_length = output_capa;
        }

        /* success ? */
        if (status == punycode_success) {
          punycode_uint j;
          for (j = 0; j < output_length; j++) {
            unsigned char seq[4];
            const size_t n = utf8_encode(output_dest[j], seq);
            size_t k;

            if (n == 0) {
              status = punycode_bad_input;
              break;
            }
            for (k = 0; k < n; k++) {
              ADD_BYTE((char) seq[k]);
            }
          }
        }

        /* cleanup */
        freet(output_dest);

        /* error ? */
        if (status != punycode_success) {
          FREE_BUFFER();
          return NULL;
        }
      } else {
        size_t j;
        for(j = startSeg ; j < i ; j++) {
          const char c = s[j];
          ADD_BYTE(c);
        }
      }
      /* next segment start */
      startSeg = i + 1;
      /* add separator (including terminating \0) */
      ADD_BYTE(c);
    }
  }

  return dest;
}

hts_UCS4* hts_convertUTF8StringToUCS4(const char *s, size_t size, size_t *nChars) {
  const unsigned char *const data = (const unsigned char*) s;
  size_t i;
  hts_UCS4 *dest = NULL;
  size_t capa = 0, destSize = 0;

  if (nChars != NULL) {
    *nChars = 0;
  }
  for (i = 0; i < size;) {
    hts_UCS4 uc;
    const size_t n = utf8_decode(&data[i], size - i, &uc);

    /* each byte of a malformed sequence becomes one U+FFFD */
    if (n != 0) {
      i += n;
    } else {
      uc = 0xfffd;
      i++;
    }

    /* Emit char */
    ADD_BYTE(uc);
    if (nChars != NULL) {
      (*nChars)++;
    }
  }
  ADD_BYTE('\0');

  return dest;
}

int hts_isStringUTF8(const char *s, size_t size) {
  const unsigned char *const data = (const unsigned char*) s;
  size_t i, n;
  hts_UCS4 uc;

  for (i = 0; i < size; i += n) {
    n = utf8_decode(&data[i], size - i, &uc);
    if (n == 0) {
      return 0;
    }
  }

  return 1;
}

char *hts_convertUCS4StringToUTF8(const hts_UCS4 *s, size_t nChars) {
  size_t i;
  char *dest = NULL;
  size_t capa = 0, destSize = 0;
  for(i = 0 ; i < nChars ; i++) {
    unsigned char seq[4];
    size_t n = utf8_encode(s[i], seq), k;

    if (n == 0) {
      n = utf8_encode(0xfffd, seq);
    }
    for (k = 0; k < n; k++) {
      ADD_BYTE((char) seq[k]);
    }
  }
  ADD_BYTE('\0');

  return dest;
}

size_t hts_writeUTF8(hts_UCS4 uc, char *dest, size_t size) {
  unsigned char seq[4];
  const size_t n = utf8_encode(uc, seq);

  /* keep room for a NUL the caller may add */
  if (n == 0 || n >= size) {
    return 0;
  }
  memcpy(dest, seq, n);
  return n;
}

size_t hts_readUTF8(const char *src, size_t size, hts_UCS4 *puc) {
  hts_UCS4 uc;
  const size_t n = utf8_decode((const unsigned char *) src, size, &uc);

  if (n != 0 && puc != NULL) {
    *puc = uc;
  }
  return n;
}

size_t hts_getUTF8SequenceLength(const char lead) {
  return utf8_lead_length((unsigned char) lead);
}

void hts_json_string(String *out, const char *s) {
  const unsigned char *p = (const unsigned char *) s;

  StringAddchar(*out, '"');
  while (*p != '\0') {
    const unsigned char c = *p;

    if (c == '"' || c == '\\') {
      StringAddchar(*out, '\\');
      StringAddchar(*out, (char) c);
      p++;
    } else if (c < 0x20 || c == 0x7f) {
      char esc[8];

      snprintf(esc, sizeof(esc), "\\u%04x", (unsigned) c);
      StringCat(*out, esc);
      p++;
    } else if (c < 0x80) {
      StringAddchar(*out, (char) c);
      p++;
    } else {
      /* Emit a sequence verbatim only if it is well-formed UTF-8. */
      hts_UCS4 uc;
      const size_t len = utf8_decode(p, strnlen((const char *) p, 4), &uc);

      if (len != 0) {
        StringMemcat(*out, (const char *) p, len);
        p += len;
      } else {
        StringCat(*out, "\\ufffd");
        p++;
      }
    }
  }
  StringAddchar(*out, '"');
}

#undef ADD_BYTE
#undef INCREASE_CAPA
