/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


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

int hts_isStringAscii(const char *s, size_t size) {
  size_t i;
  for(i = 0 ; i < size ; i++) {
    const unsigned char c = (const unsigned char) s[i];
    if (c >= 0x80) {
      return 0;
    }
  }
  return 1;
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
#define IS_ALNUM(C) ( ((C) >= 'A' && (C) <= 'Z') || ((C) >= 'a' && (C) <= 'z') || ((C) >= '0' && (C) <= '9') )
#define CHAR_LOWER(C) ( ((C) >= 'A' && (C) <= 'Z') ? ((C) + 'a' - 'A') : (C) )
  for(id = 0 ; codepages[id].name != NULL ; id++) {
    int i, j;
    /* Compare the two strings, lowercase and alphanum only (ISO88591 == iso-8859-1) */
    const char *a = name, *b = codepages[id].name;
    for(i = 0, j = 0 ; ; i++, j++) {
      /* Skip non-alnum */
      for( ; a[i] != '\0' && !IS_ALNUM(a[i]) ; i++) ;
      for( ; b[j] != '\0' && !IS_ALNUM(b[j]) ; j++) ;
      /* Compare */
      if (CHAR_LOWER(a[i]) != CHAR_LOWER(b[j])) {
        break;
      }
      /* End of string ? (note: a[i] == b[j]) */
      else if (a[i] == '\0') {
        return codepages[id].codepage;
      }
    }
  }
#undef IS_ALNUM
#undef CHAR_LOWER
  /* Not found */
  return 0;
}

static char *strndup(const char *s, size_t size) {
  char *dest = malloc(size + 1);
  if (dest != NULL) {
    memcpy(dest, s, size);
    dest[size] = '\0';
    return dest;
  }
  return NULL;
}

LPWSTR hts_convertStringToUCS2(const char *s, int size, UINT cp, int* pwsize) {
  /* Size in wide chars of the output */
  const int wsize = MultiByteToWideChar(cp, 0, (LPCSTR) s, size, NULL, 0);
  if (wsize > 0) {
    LPSTR uoutput = NULL;
    LPWSTR woutput = malloc((wsize + 1)*sizeof(WCHAR));
    if (woutput != NULL && MultiByteToWideChar(cp, 0, (LPCSTR) s, size, woutput, wsize) == wsize) {
      const int usize = WideCharToMultiByte(CP_UTF8, 0, woutput, wsize, NULL, 0, NULL, FALSE);
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

LPWSTR hts_convertUTF8StringToUCS2(const char *s, int size, int* pwsize) {
  return hts_convertStringToUCS2(s, size, CP_UTF8, pwsize);
}

char *hts_convertUCS2StringToCP(LPWSTR woutput, int wsize, UINT cp) {
  const int usize = WideCharToMultiByte(cp, 0, woutput, wsize, NULL, 0, NULL, FALSE);
  if (usize > 0) {
    char *const uoutput = malloc((usize + 1)*sizeof(char));
    if (uoutput != NULL) {
      if (WideCharToMultiByte(cp, 0, woutput, wsize, uoutput, usize, NULL, FALSE) == usize) {
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
    return strndup(s, size);
  }
  /* Already UTF-8 ? */
  if (cp == CP_UTF8 || hts_isStringAscii(s, size)) {
    return strndup(s, size);
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
    return strndup(s, size);
  }
  /* Already UTF-8 ? */
  if (cp == CP_UTF8 || hts_isStringAscii(s, size)) {
    return strndup(s, size);
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
#include <iconv.h>

char *hts_convertStringToUTF8_(const char *s, size_t size, const char *to, const char *from) {
  /* Empty string ? */
  if (size == 0) {
    return strdup("");
  }
  /* Already on correct charset ? */
  if (strcasecmp(from, to) == 0) {
    return strndup(s, size);
  }
  /* Find codepage */
  else {
    const iconv_t cp = iconv_open(to, from);
    if (cp != (iconv_t) -1) {
      char *inbuf = (char*) s;
      size_t inbytesleft = size;
      size_t outbufCapa = 0;
      char *outbuf = NULL;
      size_t outbytesleft = 0;
      size_t finalSize;

      /* Initial size to around the string size */
      for(outbufCapa = 16 ; outbufCapa < size + 1 ; outbufCapa *= 2) ;
      outbuf = malloc(outbufCapa);

      /* Convert */
      while(outbuf != NULL && inbytesleft != 0) {
        const size_t offset = outbufCapa - outbytesleft;
        char *outbufCurrent = outbuf + offset;
        const size_t ret = iconv(cp, &inbuf, &inbytesleft, &outbufCurrent, &outbytesleft);
        if (ret == (size_t) -1) {
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

  /* Error, charset not found! */
  return NULL;
}

char *hts_convertStringToUTF8(const char *s, size_t size, const char *charset) {
  /* Empty string ? */
  if (size == 0) {
    return strdup("");
  }
  /* Already UTF-8 ? */
  if (strcasecmp(charset, "utf-8") == 0 || strcasecmp(charset, "utf8") == 0 || hts_isStringAscii(s, size)) {
    return strndup(s, size);
  }
  /* Find codepage */
  else {
    return hts_convertStringToUTF8_(s, size, "utf-8", charset);
  }
}

char *hts_convertStringFromUTF8(const char *s, size_t size, const char *charset) {
  /* Empty string ? */
  if (size == 0) {
    return strdup("");
  }
  /* Already UTF-8 ? */
  if (strcasecmp(charset, "utf-8") == 0 || strcasecmp(charset, "utf8") == 0 || hts_isStringAscii(s, size)) {
    return strndup(s, size);
  }
  /* Find codepage */
  else {
    return hts_convertStringToUTF8_(s, size, charset, "utf-8");
  }
}

#endif

char* hts_getCharsetFromContentType(const char *mime) {
  /* text/html; charset=utf-8 */
  const char *const charset = "charset";
  char *pos = strstr(mime, charset);
  if (pos != NULL) {
    /* Skip spaces */
    int eq = 0;
    for(pos += strlen(charset) ; *pos == ' ' || *pos == '=' || *pos == '"' || *pos == '\'' ; pos++) {
      if (*pos == '=') {
        eq = 1;
      }
    }
    if (eq == 1) {
      int len;
      for(len = 0 ; pos[len] == ' ' || pos[len] == ';' || pos[len] == '"' || *pos == '\'' ; pos++) ;
      if (len != 0) {
        char *const s = malloc(len + 1);
        int i;
        for(i = 0 ; i < len ; i++) {
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

char* hts_getCharsetFromMeta(const char *html, size_t size) {
  int i;
  // <META HTTP-EQUIV="CONTENT-TYPE" CONTENT="text/html; charset=utf-8" >
  for(i = 0 ; i < size ; i++) {
    if (html[i] == '<' && strncasecmp(&html[i + 1], "meta", 4) == 0 && is_space(html[i + 5]) ) {
      /* Skip spaces */
      for(i += 5 ; is_space(html[i]) ; i++) ;
      if (strncasecmp(&html[i], "HTTP-EQUIV", 10) == 0 && is_space_or_equal(html[i + 10]) ) {
        for(i += 10 ; is_space_or_equal_or_quote(html[i]) ; i++) ;
        if (strncasecmp(&html[i], "CONTENT-TYPE", 12) == 0) {
          for(i += 12 ; is_space_or_equal_or_quote(html[i]) ; i++) ;
          if (strncasecmp(&html[i], "CONTENT", 7) == 0 && is_space_or_equal(html[i + 7]) ) {
            for(i += 7 ; is_space_or_equal_or_quote(html[i]) ; i++) ;
            /* Skip content-type */
            for( ; i < size && html[i] != ';' && html[i] != '"' && html[i] != '\'' ; i++) ;
            /* Expect charset attribute here */
            if (html[i] == ';') {
              for(i++ ; is_space(html[i]) ; i++) ;
              /* Look for charset */
              if (strncasecmp(&html[i], "charset", 7) == 0 && is_space_or_equal(html[i + 7])) {
                int len;
                for(i += 7 ; is_space_or_equal(html[i]) || html[i] == '\'' ; i++) ;
                /* Charset */
                for(len = 0 ; i + len < size && html[i + len] != '"' && html[i + len] != '\'' && html[i + len] != ' ' ; len++) ;
                /* No error ? */
                if (len != 0 && i < size) {
                  char *const s = malloc(len + 1);
                  int j;
                  for(j = 0 ; j < len ; j++) {
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

