/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2017 Xavier Roche and other contributors

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

Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: Charset conversion functions                           */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htscharset.h"
#include "htsbase.h"
#include "punycode.h"
#include "htssafe.h"

#ifdef _WIN32
#include <stddef.h>
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#elif (defined(SOLARIS) || defined(sun) || defined(HAVE_INTTYPES_H) \
  || defined(BSD) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD_kernel__))
#include <inttypes.h>
#else
#include <stdint.h>
#endif
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
  char *dest = malloc(size + 1);

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
    LPWSTR woutput = malloc((wsize + 1) * sizeof(WCHAR));

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
    if (woutput != NULL)
      free(woutput);
  }
  return NULL;
}

LPWSTR hts_convertUTF8StringToUCS2(const char *s, int size, int *pwsize) {
  return hts_convertStringToUCS2(s, size, CP_UTF8, pwsize);
}

char *hts_convertUCS2StringToCP(LPWSTR woutput, int wsize, UINT cp) {
  const int usize =
    WideCharToMultiByte(cp, 0, woutput, wsize, NULL, 0, NULL, FALSE);
  if (usize > 0) {
    char *const uoutput = malloc((usize + 1) * sizeof(char));

    if (uoutput != NULL) {
      if (WideCharToMultiByte
          (cp, 0, woutput, wsize, uoutput, usize, NULL, FALSE) == usize) {
        uoutput[usize] = '\0';
        return uoutput;
      } else {
        free(uoutput);
      }
    }
  }
  return NULL;
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

      free(woutput);
      return uoutput;
    }
  }

  /* Error, charset not found! */
  return NULL;
}

char *hts_convertStringCPFromUTF8(const char *s, size_t size, UINT cp) {
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
      char *const uoutput = hts_convertUCS2StringToCP(woutput, wsize, cp);

      free(woutput);
      return uoutput;
    }
  }

  /* Error, charset not found! */
  return NULL;
}

char *hts_convertStringToUTF8(const char *s, size_t size, const char *charset) {
  const UINT cp = hts_getCodepage(charset);

  return hts_convertStringCPToUTF8(s, size, cp);
}

char *hts_convertStringFromUTF8(const char *s, size_t size, const char *charset) {
  const UINT cp = hts_getCodepage(charset);

  return hts_convertStringCPFromUTF8(s, size, cp);
}

char *hts_convertStringSystemToUTF8(const char *s, size_t size) {
  return hts_convertStringCPToUTF8(s, size, GetACP());
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

/* decode from a codepage to UTF-8 */
static char* hts_codepageToUTF8(const char *codepage, const char *s) {
  /* find the given codepage */
  size_t i;
  for(i = 0 ; table_mappings[i].name != NULL
      && !hts_equalsAlphanum(table_mappings[i].name, codepage) ; i++) ;

  /* found ; decode */
  if (table_mappings[i].name != NULL) {
    size_t j, k;
    char *dest = NULL;
    size_t capa = 0;
#define MAX_UTF 8
    for(j = 0, k = 0 ; s[j] != '\0' ; j++) {
      const unsigned char c = (unsigned char) s[j];
      const hts_UCS4 uc = table_mappings[i].table[c];
      const size_t max = k + MAX_UTF;
      if (capa < max) {
        for(capa = 16 ; capa < max ; capa <<= 1) ;
        dest = realloc(dest, capa);
        if (dest == NULL) {
          return NULL;
        }
      }
      if (dest != NULL) {
        const size_t len = hts_writeUTF8(uc, &dest[k], MAX_UTF);
        k += len;
        assertf(k < capa);
      }
    }
    dest[k] = '\0';
    return dest;
#undef MAX_UTF
  }
  return NULL;
}
#endif

static char *hts_convertStringCharset(const char *s, size_t size,
                                      const char *to, const char *from) {
  /* Empty string ? */
  if (size == 0) {
    return strdup("");
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
      outbuf = malloc(outbufCapa);
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

            outbufCapa *= 2;
            outbuf = realloc(outbuf, outbufCapa);
            if (outbuf == NULL) {
              break;
            }
            outbytesleft = outbufCapa - used;
          } else {
            free(outbuf);
            outbuf = NULL;
            break;
          }
        }
      }

      /* Final size ? */
      finalSize = outbufCapa - outbytesleft;

      /* Terminating \0 */
      if (outbuf != NULL && finalSize + 1 >= outbufCapa) {
        outbuf = realloc(outbuf, finalSize + 1);
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
    return hts_codepageToUTF8(from, s);
  }
#endif

  /* Error, charset not found! */
  return NULL;
}

char *hts_convertStringToUTF8(const char *s, size_t size, const char *charset) {
  /* Empty string ? */
  if (size == 0) {
    return strdup("");
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
    return strdup("");
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

#endif

HTS_STATIC char *hts_getCharsetFromContentType(const char *mime) {
  /* text/html; charset=utf-8 */
  const char *const charset = "charset";
  char *pos = strstr(mime, charset);

  if (pos != NULL) {
    /* Skip spaces */
    int eq = 0;

    for(pos += strlen(charset);
        *pos == ' ' || *pos == '=' || *pos == '"' || *pos == '\''; pos++) {
      if (*pos == '=') {
        eq = 1;
      }
    }
    if (eq == 1) {
      int len;

      for(len = 0;
          pos[len] == ' ' || pos[len] == ';' || pos[len] == '"' || *pos == '\'';
          pos++) ;
      if (len != 0) {
        char *const s = malloc(len + 1);
        int i;

        for(i = 0; i < len; i++) {
          s[i] = pos[i];
        }
        s[len] = '\0';
        return s;
      }
    }
  }
  return NULL;
}

#ifdef _WIN32
#define strcasecmp(a,b) stricmp(a,b)
#define strncasecmp(a,b,n) strnicmp(a,b,n)
#endif

static int is_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int is_space_or_equal(char c) {
  return is_space(c) || c == '=';
}

static int is_space_or_equal_or_quote(char c) {
  return is_space_or_equal(c) || c == '"' || c == '\'';
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

char *hts_getCharsetFromMeta(const char *html, size_t size) {
  int i;

  /* <META HTTP-EQUIV="CONTENT-TYPE" CONTENT="text/html; charset=utf-8" > */
  for(i = 0; i < size; i++) {
    if (html[i] == '<' && strncasecmp(&html[i + 1], "meta", 4) == 0
        && is_space(html[i + 5])) {
      /* Skip spaces */
      for(i += 5; is_space(html[i]); i++) ;
      if (strncasecmp(&html[i], "HTTP-EQUIV", 10) == 0
          && is_space_or_equal(html[i + 10])) {
        for(i += 10; is_space_or_equal_or_quote(html[i]); i++) ;
        if (strncasecmp(&html[i], "CONTENT-TYPE", 12) == 0) {
          for(i += 12; is_space_or_equal_or_quote(html[i]); i++) ;
          if (strncasecmp(&html[i], "CONTENT", 7) == 0
              && is_space_or_equal(html[i + 7])) {
            for(i += 7; is_space_or_equal_or_quote(html[i]); i++) ;
            /* Skip content-type */
            for(;
                i < size && html[i] != ';' && html[i] != '"' && html[i] != '\'';
                i++) ;
            /* Expect charset attribute here */
            if (html[i] == ';') {
              for(i++; is_space(html[i]); i++) ;
              /* Look for charset */
              if (strncasecmp(&html[i], "charset", 7) == 0
                  && is_space_or_equal(html[i + 7])) {
                int len;

                for(i += 7; is_space_or_equal(html[i]) || html[i] == '\'';
                    i++) ;
                /* Charset */
                for(len = 0;
                    i + len < size && html[i + len] != '"'
                    && html[i + len] != '\'' && html[i + len] != ' '; len++) ;
                /* No error ? */
                if (len != 0 && i < size) {
                  char *const s = malloc(len + 1);
                  int j;

                  for(j = 0; j < len; j++) {
                    s[j] = html[i + j];
                  }
                  s[len] = '\0';
                  return s;
                }
              }
            }
          }
        }
      }
    }
  }
  return NULL;
}

/* UTF-8 helpers */

/* Number of leading zeros. Returns a value between 0 and 8. */
static unsigned int nlz8(unsigned char x) {
  unsigned int b = 0;

  if (x & 0xf0) {
    x >>= 4;
  } else {
    b += 4;
  }

  if (x & 0x0c) {
    x >>= 2;
  } else {
    b += 2;
  }

  if (! (x & 0x02) ) {
    b++;
  }

  return b;
}

/*
  Emit the Unicode character 'UC' (internal)
  See <http://en.wikipedia.org/wiki/UTF-8>
  7	  U+0000		U+007F		1	0xxxxxxx
  11	U+0080		U+07FF		2	110xxxxx
  16	U+0800		U+FFFF		3	1110xxxx
  21	U+10000		U+1FFFFF	4	11110xxx
  26	U+200000	U+3FFFFFF	5	111110xx
  31	U+4000000	U+7FFFFFFF	6	1111110x
*/
#define ADD_FIRST_SEQ(UC, LEN, EMITTER) do {                            \
  /* first octet */                                                     \
  const unsigned char lead =                                            \
    /* leading bits: LEN "1" bits */                                    \
    ~ ( ( 1 << (unsigned) ( 8 - LEN ) ) - 1 )                           \
    /* encoded bits */                                                  \
    | ( (UC) >> (unsigned) ( ( LEN - 1 ) * 6 ) );                       \
  EMITTER(lead);                                                        \
  } while(0)

#define ADD_NEXT_SEQ(UC, SHIFT, EMITTER) do {                           \
  /* further bytes are encoding 6 bits */                               \
  const unsigned char next =                                            \
    0x80 | ( ( (UC) >> SHIFT ) & 0x3f );                                \
  EMITTER(next);                                                        \
  } while(0)

/* UC is a constant. EMITTER is a macro function taking an unsigned int. */
#define EMIT_UNICODE(UC, EMITTER) do {          \
    if ((UC) < 0x80) {                          \
      EMITTER(((unsigned char) (UC)));          \
    } else if ((UC) < 0x0800) {                 \
      ADD_FIRST_SEQ(UC, 2, EMITTER);            \
      ADD_NEXT_SEQ(UC, 0, EMITTER);             \
    } else if ((UC) < 0x10000) {                \
      ADD_FIRST_SEQ(UC, 3, EMITTER);            \
      ADD_NEXT_SEQ(UC, 6, EMITTER);             \
      ADD_NEXT_SEQ(UC, 0, EMITTER);             \
    } else if ((UC) < 0x200000) {               \
      ADD_FIRST_SEQ(UC, 4, EMITTER);            \
      ADD_NEXT_SEQ(UC, 12, EMITTER);            \
      ADD_NEXT_SEQ(UC, 6, EMITTER);             \
      ADD_NEXT_SEQ(UC, 0, EMITTER);             \
    } else if ((UC) < 0x4000000) {              \
      ADD_FIRST_SEQ(UC, 5, EMITTER);            \
      ADD_NEXT_SEQ(UC, 18, EMITTER);            \
      ADD_NEXT_SEQ(UC, 12, EMITTER);            \
      ADD_NEXT_SEQ(UC, 6, EMITTER);             \
      ADD_NEXT_SEQ(UC, 0, EMITTER);             \
    } else {                                    \
      ADD_FIRST_SEQ(UC, 6, EMITTER);            \
      ADD_NEXT_SEQ(UC, 24, EMITTER);            \
      ADD_NEXT_SEQ(UC, 18, EMITTER);            \
      ADD_NEXT_SEQ(UC, 12, EMITTER);            \
      ADD_NEXT_SEQ(UC, 6, EMITTER);             \
      ADD_NEXT_SEQ(UC, 0, EMITTER);             \
    }                                           \
  } while(0)

#undef READ_LOOP
#define READ_LOOP(C, READER, EMITTER, CLEARED)  \
  do {                                          \
    unsigned int uc_ =                          \
      (C) & ( (1 << (CLEARED - 1)) - 1 );       \
    int i_;                                     \
    /* loop should be unrolled by compiler */   \
    for(i_ = 0 ; i_ < 7 - CLEARED ; i_++) {     \
      const int c_ = (READER);                  \
      /* continuation byte 10xxxxxx */          \
      if (c_ != -1 && ( c_ >> 6 ) == 0x2) {     \
        uc_ <<= 6;                              \
        uc_ |= (c_ & 0x3f);                     \
      } else {                                  \
        uc_ = (unsigned int) -1;                \
        break;                                  \
      }                                         \
    }                                           \
    EMITTER(((int) uc_));                       \
  } while(0)

/* READER is a macro returning an int (-1 for error).
   EMITTER is a macro function taking an int (-1 for error). */
#define READ_UNICODE(READER, EMITTER) do {      \
    const unsigned int f_ =                     \
      (unsigned int) (READER);                  \
    /* 1..8 */                                  \
    const unsigned int c_ =                     \
      nlz8((unsigned char)~f_);                 \
    /* ascii */                                 \
    switch(c_) {                                \
    case 0:                                     \
      EMITTER(((int)f_));                       \
      break;                                    \
      /* 110xxxxx 10xxxxxx */                   \
    case 2:                                     \
      READ_LOOP(f_, READER, EMITTER, 6);        \
      break;                                    \
    case 3:                                     \
      READ_LOOP(f_, READER, EMITTER, 5);        \
      break;                                    \
    case 4:                                     \
      READ_LOOP(f_, READER, EMITTER, 4);        \
      break;                                    \
    case 5:                                     \
      READ_LOOP(f_, READER, EMITTER, 3);        \
      break;                                    \
    case 6:                                     \
      READ_LOOP(f_, READER, EMITTER, 2);        \
      break;                                    \
      /* unexpected continuation/bogus */       \
    default:                                    \
      EMITTER(-1);                              \
      break;                                    \
    }                                           \
  } while(0)

/* Sample. */
#if 0

int main(int argc, char **argv) {
  int i;
  int hex = 0;
  
#define READ_INT(DEST)                                  \
  ( ( !hex && sscanf(argv[i], "%d", &(DEST)) == 1)      \
    || (hex && sscanf(argv[i], "%x", &(DEST)) == 1 ) )
  
  for(i = 1 ; i < argc ; i++) {
    unsigned int uc, from, to;
    
    if (strcmp(argv[i], "--hex") == 0) {
      hex = 1;
    }
    else if (strcmp(argv[i], "--decimal") == 0) {
      hex = 0;
    }
    else if (strcmp(argv[i], "--decode") == 0) {
#define RD fgetc_unlocked(stdin)
#define WR(C) do {                              \
        if (C != -1) {                          \
          printf("%04x\n", C);                  \
        } else if (!feof(stdin)) {              \
          fprintf(stderr, "read error\n");      \
          exit(EXIT_FAILURE);                   \
        }                                       \
      } while(0)
      while(!feof(stdin)) {
        READ_UNICODE(RD, WR);
      }
#undef RD
#undef WR
    }
    else if (strcmp(argv[i], "-range") == 0
             && i + 2 < argc
             && (++i, 1)
             && READ_INT(from)
             && (++i, 1)
             && READ_INT(to)
             ) {
      unsigned int i;
      for(i = from ; i < to ; i++) {
#define EM(C) fputc_unlocked(C, stdout)
        EMIT_UNICODE(i, EM);
#undef EM
      }
    }
    else if (READ_INT(uc)) {
#define EM(C) fputc_unlocked(C, stdout)
      EMIT_UNICODE(uc, EM);
#undef EM
    }
    else {
      return EXIT_FAILURE;
    }
  }
  
  return EXIT_SUCCESS;
}

#endif

/* IDNA helpers. */
#undef ADD_BYTE
#undef INCREASE_CAPA
#define INCREASE_CAPA() do { \
  capa = capa < 16 ? 16 : ( capa << 1 ); \
  dest = realloc(dest, capa*sizeof(dest[0])); \
  if (dest == NULL) { \
    return NULL; \
  } \
} while(0)
#define ADD_BYTE(C) do { \
  if (capa == destSize) { \
    INCREASE_CAPA(); \
  } \
  dest[destSize++] = (C); \
} while(0)
#define FREE_BUFFER() do { \
  if (dest != NULL) { \
    free(dest); \
    dest = NULL; \
  } \
} while(0)

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
          segInt = (punycode_uint*) malloc(segSize*sizeof(punycode_uint));
          if (segInt == NULL) {
            FREE_BUFFER();
            return NULL;
          }
          for(j = 0, segOutputSize = 0, utfSeq = (size_t) -1
            ; j <= segSize ; j++) {
            const unsigned char c = j < segSize ? segData[j] : 0;

            /* character start (ascii, or utf-8 leading sequence) */
            if (HTS_IS_LEADING_UTF8(c)) {
              /* commit sequence ? */
              if (utfSeq != (size_t) -1) {

                /* Reader: can read bytes up to j */
#define RD ( utfSeq < j ? segData[utfSeq++] : -1 )

                /* Writer: upon error, return FFFD (replacement character) */
#define WR(C) do { \
  if ((C) != -1) { \
    /* copy character */ \
    assertf(segOutputSize < segSize); \
    segInt[segOutputSize++] = (C); \
  } \
  /* In case of error, abort. */ \
  else { \
    FREE_BUFFER(); \
    return NULL; \
  } \
} while(0)

                /* Read/Write Unicode character. */
                READ_UNICODE(RD, WR);
#undef RD
#undef WR

                /* not anymore in sequence */
                utfSeq = (size_t) -1;
              }

              /* ascii ? */
              if (c < 0x80) {
                assertf(segOutputSize < segSize);
                segInt[segOutputSize] = c;
                if (c != 0) {
                  segOutputSize++;
                }
              }
              /* new UTF8 sequence */
              else {
                utfSeq = j;
              }
            }
          }

          /* encode */
          output_length = (punycode_uint) ( capa - destSize );
          while((status = punycode_encode((punycode_uint) segOutputSize,
            segInt, NULL, &output_length, &dest[destSize]))
            == punycode_big_output) {
              INCREASE_CAPA();
              output_length = (punycode_uint) ( capa - destSize );
          }

          /* cleanup */
          free(segInt);

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
          (punycode_uint*) malloc(output_capa*sizeof(punycode_uint));
        if (output_dest == NULL) {
          FREE_BUFFER();
          return NULL;
        }
        for(output_length = output_capa 
          ; (status = punycode_decode((punycode_uint) segSize - 4,
            &s[startSeg + 4], &output_length, output_dest, NULL))
            == punycode_big_output 
          ; ) {
          output_capa <<= 1;
          output_dest =
            (punycode_uint*) realloc(output_dest,
                                     output_capa*sizeof(punycode_uint));
          if (output_dest == NULL) {
            FREE_BUFFER();
            return NULL;
          }
          output_length = output_capa;
        }

        /* success ? */
        if (status == punycode_success) {
          punycode_uint j;
          for(j = 0 ; j < output_length ; j++) {
            const punycode_uint uc = output_dest[j];
            if (uc < 0x80) {
              ADD_BYTE((char) uc);
            } else {
              /* emiter (byte per byte) */
#define EM(C) do { \
  if (C != -1) {   \
    ADD_BYTE(C);   \
  } else {         \
    FREE_BUFFER(); \
    return NULL;   \
  }                \
} while(0)
              /* Emit codepoint */
              EMIT_UNICODE(uc, EM);
#undef EM
            }
          }
        }

        /* cleanup */
        free(output_dest);

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
  for(i = 0 ; i < size ; ) {
    hts_UCS4 uc;

    /* Reader: can read bytes up to j */
#define RD ( i < size ? data[i++] : -1 )

    /* Writer: upon error, return FFFD (replacement character) */
#define WR(C) uc = (C) != -1 ? (hts_UCS4) (C) : (hts_UCS4) 0xfffd

    /* Read Unicode character. */
    READ_UNICODE(RD, WR);
#undef RD
#undef WR

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
  size_t i;

  for(i = 0 ; i < size ; ) {
    /* Reader: can read bytes up to j */
#define RD ( i < size ? data[i++] : -1 )

    /* Writer: upon error, return FFFD (replacement character) */
#define WR(C) if ((C) == -1) { return 0; }

    /* Read Unicode character. */
    READ_UNICODE(RD, WR);
#undef RD
#undef WR
  }

  return 1;
}

char *hts_convertUCS4StringToUTF8(const hts_UCS4 *s, size_t nChars) {
  size_t i;
  char *dest = NULL;
  size_t capa = 0, destSize = 0;
  for(i = 0 ; i < nChars ; i++) {
    const hts_UCS4 uc = s[i];
    /* emitter (byte per byte) */
#define EM(C) do { \
  if (C != -1) {   \
    ADD_BYTE(C);   \
  } else {         \
    FREE_BUFFER(); \
    return NULL;   \
  }                \
} while(0)
    EMIT_UNICODE(uc, EM);
#undef EM
  }
  ADD_BYTE('\0');

  return dest;
}

size_t hts_writeUTF8(hts_UCS4 uc, char *dest, size_t size) {
  size_t offs = 0;
#define EM(C) do {       \
  if (offs + 1 < size) { \
    dest[offs++] = C;    \
  } else {               \
    return 0;            \
  }                      \
} while(0)
  EMIT_UNICODE(uc, EM);
#undef EM
  return offs; 
}

size_t hts_readUTF8(const char *src, size_t size, hts_UCS4 *puc) {
  const unsigned char *const data = (const unsigned char*) src;
  size_t i = 0;
  int uc = -1;

  /* Reader: can read bytes up to j */
#define RD ( i < size ? data[i++] : -1 )

  /* Writer: upon error, return FFFD (replacement character) */
#define WR(C) uc = (C)

  /* Read Unicode character. */
  READ_UNICODE(RD, WR);
#undef RD
#undef WR

  /* Return */
  if (uc != -1) {
    if (puc != NULL) {
      *puc = (hts_UCS4) uc;
    }
    return i;
  }

  return 0;
}

size_t hts_getUTF8SequenceLength(const char lead) {
  const unsigned char f = (unsigned char) lead;
  const unsigned int c = nlz8(~f);
  switch(c) {
  case 0:
    /* ASCII */
    return 1;
    break;
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
    /* UTF-8 */
    return c;
    break;
  default:
    /* ERROR */
    return 0;
    break;
  }
}

size_t hts_stringLengthUCS4(const hts_UCS4 *s) {
  size_t i;
  for(i = 0 ; s[i] != 0 ; i++) ;
  return i;
}

#undef ADD_BYTE
#undef INCREASE_CAPA
