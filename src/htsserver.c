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
/* File: Mini-server                                            */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* specific definitions */
/* specific definitions */

/* Bypass internal definition protection */
#define HTS_INTERNAL_BYTECODE
#include "htsbase.h"
#undef HTS_INTERNAL_BYTECODE

#include "htsnet.h"
#include "htslib.h"
#include "htsio.h"
#include "htscharset.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#ifdef _WIN32
#else
#include <arpa/inet.h>
#endif
#ifndef _WIN32
#include <signal.h>
#endif
/* END specific definitions */

/* définitions globales */
#include "htsglobal.h"

/* htslib */
/*#include "htslib.h"*/

/* HTTrack Website Copier Library */
#include "httrack-library.h"

/* Language files */

/* Bypass internal definition protection */
#define HTS_INTERNAL_BYTECODE
#include "coucal.h"
#undef HTS_INTERNAL_BYTECODE

int NewLangStrSz = 1024;
coucal NewLangStr = NULL;
int NewLangStrKeysSz = 1024;
coucal NewLangStrKeys = NULL;
int NewLangListSz = 1024;
coucal NewLangList = NULL;

/* Language files */

#include "htsserver.h"

const char *gethomedir(void);
int commandRunning = 0;
int commandEndRequested = 0;
int commandEnd = 0;
int commandReturn = 0;
char *commandReturnMsg = NULL;
char *commandReturnCmdl = NULL;
int commandReturnSet = 0;

httrackp *global_opt = NULL;

static void (*pingFun)(void *, smallserver_client_event, const char *) = NULL;
static void* pingFunArg = NULL;

/* Report a client liveness event, if anybody is listening. */
static void client_event(smallserver_client_event ev, const char *window) {
  if (pingFun != NULL) {
    pingFun(pingFunArg, ev, window);
  }
}

/* Extern */
extern void webhttrack_main(char *cmd);
extern void webhttrack_lock(void);
extern void webhttrack_release(void);

/* Content types for the GUI tree and the mirror it serves. Not the engine's
   get_httptype_sized(): it needs an httrackp the server has none of yet, and
   would apply the user's --assume rules to the GUI's own pages. */
static const struct {
  const char *ext;
  const char *type;
} server_mime_types[] = {
    {"html", "text/html"},
    {"htm", "text/html"},
    {"css", "text/css"},
    {"js", "text/javascript"},
    {"txt", "text/plain"},
    {"xml", "application/xml"},
    {"gif", "image/gif"},
    {"png", "image/png"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"webp", "image/webp"},
    {"avif", "image/avif"},
    {"svg", "image/svg+xml"},
    /* x-icon, not the registered vnd.microsoft.icon: what browsers send. */
    {"ico", "image/x-icon"},
    {NULL, NULL},
};

/* Content type of file, NULL if its extension is unlisted. Matched on the last
   segment's whole extension, never on a substring of the path. */
static const char *server_content_type(const char *file) {
  const char *const slash = strrchr(file, '/');
  const char *const dot = strrchr(slash != NULL ? slash : file, '.');
  int i;

  if (dot == NULL) {
    return NULL;
  }
  for (i = 0; server_mime_types[i].ext != NULL; i++) {
    if (strfield2(dot + 1, server_mime_types[i].ext)) {
      return server_mime_types[i].type;
    }
  }
  return NULL;
}

static int is_html(const char *file) {
  const char *const type = server_content_type(file);

  return type != NULL && strcmp(type, "text/html") == 0;
}

static void sig_brpipe(int code) {
  /* ignore */
}

HTS_UNUSED static int check_readinput_t(T_SOC soc, int timeout);
HTS_UNUSED static int recv_bl(T_SOC soc, void *buffer, size_t len, int timeout);
HTS_UNUSED static int linputsoc(T_SOC soc, char *s, int max);
HTS_UNUSED static int check_readinput(htsblk * r);
HTS_UNUSED static int linputsoc_t(T_SOC soc, char *s, int max, int timeout);
HTS_UNUSED static int linput(FILE * fp, char *s, int max);

/* Language files */
HTS_UNUSED static int htslang_load(char *limit_to, size_t limit_size,
                                   const char *apppath);
HTS_UNUSED static void conv_printf(const char *from, char *to);
HTS_UNUSED static void LANG_DELETE(void);
HTS_UNUSED static void LANG_INIT(const char *path);
HTS_UNUSED static int LANG_T(const char *path, int l);
HTS_UNUSED static int QLANG_T(int l);
HTS_UNUSED static const char *LANGSEL(const char *name);
HTS_UNUSED static const char *LANGINTKEY(const char *name);
HTS_UNUSED static int LANG_SEARCH(const char *path, const char *iso);
HTS_UNUSED static int LANG_LIST(const char *path, char *buffer, size_t size);

// URL Link catcher

// 0- Init the URL catcher with standard port

// smallserver_init(&port,&return_host);
T_SOC smallserver_init_std(int *port_prox, char *adr_prox, int defaultPort,
                           const char *bindAddr) {
  T_SOC soc;

  if (defaultPort <= 0) {
    int try_to_listen_to[] =
      { 8080, 8081, 8082, 8083, 8084, 8085, 8086, 8087, 8088, 8089,
      32000, 32001, 32002, 32003, 32004, 32006, 32006, 32007, 32008, 32009,
      42000, 42001, 42002, 42003, 42004, 42006, 42006, 42007, 42008, 42009,
      0, -1
    };
    int i = 0;

    do {
      soc = smallserver_init(&try_to_listen_to[i], adr_prox, bindAddr);
      *port_prox = try_to_listen_to[i];
      i++;
    } while((soc == INVALID_SOCKET) && (try_to_listen_to[i] >= 0));
  } else {
    soc = smallserver_init(&defaultPort, adr_prox, bindAddr);
    *port_prox = defaultPort;
  }
  return soc;
}

// 1- Init the URL catcher

// get hostname. return 1 upon success.
static int gethost(const char *hostname, SOCaddr * server) {
  if (hostname != NULL && *hostname != '\0') {
#if HTS_INET6==0
    /* ipV4 resolver */
    struct hostent *hp = gethostbyname(hostname);

    if (hp != NULL) {
      if (hp->h_length) {
        SOCaddr_copyaddr2(*server, hp->h_addr_list[0], hp->h_length);
        return 1;
      }
    }
#else
    /* ipV6 resolver */
    struct addrinfo *res = NULL;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (getaddrinfo(hostname, NULL, &hints, &res) == 0) {
      if (res) {
        if ((res->ai_addr) && (res->ai_addrlen)) {
          SOCaddr_copyaddr2(*server, res->ai_addr, res->ai_addrlen);
          freeaddrinfo(res);
          return 1;
        }
      }
    }
    if (res) {
      freeaddrinfo(res);
    }
#endif
  }
  return 0;
}

// smallserver_init(&port,&return_host);
T_SOC smallserver_init(int *port, char *adr, const char *bindAddr) {
  T_SOC soc = INVALID_SOCKET;
  char h_loc[256 + 2];
  SOCaddr server;

  commandRunning = commandEnd = commandReturn = commandReturnSet =
    commandEndRequested = 0;
  if (commandReturnMsg)
    free(commandReturnMsg);
  commandReturnMsg = NULL;
  if (commandReturnCmdl)
    free(commandReturnCmdl);
  commandReturnCmdl = NULL;

  /* Loopback unless asked otherwise: the handler trusts its client, and every
     request is unauthenticated. --bind widens it deliberately. */
  SOCaddr_initloopback(server);
  strcpybuff(h_loc, "127.0.0.1");
  if (bindAddr != NULL && *bindAddr != '\0') {
    /* advertise the bound address, else the URL we print is unreachable */
    if (strlen(bindAddr) >= sizeof(h_loc) || !gethost(bindAddr, &server)) {
      return INVALID_SOCKET;
    }
    strcpybuff(h_loc, bindAddr);
  }

  if ((soc = (T_SOC) socket(SOCaddr_sinfamily(server), SOCK_STREAM, 0)) !=
      INVALID_SOCKET) {
    SOCaddr_initport(server, *port);
    if (bind(soc, &SOCaddr_sockaddr(server), SOCaddr_size(server)) == 0) {
      if (listen(soc, 10) >= 0) {
        strcpy(adr, h_loc);
      } else {
#ifdef _WIN32
        closesocket(soc);
#else
        close(soc);
#endif
        soc = INVALID_SOCKET;
      }
    } else {
#ifdef _WIN32
      closesocket(soc);
#else
      close(soc);
#endif
      soc = INVALID_SOCKET;
    }
  }
  return soc;
}

// 2 - Wait for URL

// check if data is available

// smallserver
// returns 0 if error
// url: buffer where URL must be stored - or ip:port in case of failure
// data: 32Kb

typedef struct {
  const char *name;
  int value;
} initIntElt;
typedef struct {
  const char *name;
  const char *value;
} initStrElt;

#define SET_ERROR(err) do { \
  coucal_write(NewLangList, "error", (intptr_t)strdup(err)); \
  error_redirect = "/server/error.html"; \
} while(0)

/* Longest error message shown on the error page; the rest is clipped. */
#define ERROR_MESSAGE_MAX 1024

/* SET_ERROR() with a printf format. Clips: these messages quote posted fields,
   whose length the client picks. */
#define SET_ERRORF(...)                                                        \
  do {                                                                         \
    char errbuf[ERROR_MESSAGE_MAX];                                            \
    slprintfbuff_clip(errbuf, sizeof(errbuf), __VA_ARGS__);                    \
    SET_ERROR(errbuf);                                                         \
  } while (0)

/* Longest "sid" value worth unescaping: the expected one is an md5 hex digest,
   so anything near this is already invalid and is rejected unread. */
#define SID_VALUE_MAX 64

/** Does this Origin name the panel itself?
    Only our own plain-http authority passes: a sandboxed page sends "null", a
    foreign one its own host, and an empty Host cannot be matched at all. */
static hts_boolean origin_is_self(const char *origin, const char *host) {
  const int p = strfield(origin, "http://");
  const char *const authority = origin + p;

  return host[0] != '\0' && p != 0 && strfield2(authority, host) != 0;
}

/** Header value with leading blanks dropped, clipped to fit dst. */
static void copy_header_value(char *dst, size_t size, const char *value) {
  while (*value == ' ' || *value == '\t') {
    value++;
  }
  dst[0] = '\0';
  strlncatbuff(dst, value, size, size - 1);
}

/** Copy query parameter "name"'s alphanumeric value into dst; true when a
    non-empty one fit, and dst is left empty otherwise. Query-string counterpart
    to the POST-body checker below. */
static hts_boolean query_alnum_value(char *dst, size_t size, const char *query,
                                     const char *name) {
  const size_t namelen = strlen(name);
  const char *s = query;

  dst[0] = '\0';
  while (*s != '\0') {
    const char *const amp = strchr(s, '&');

    if (strncmp(s, name, namelen) == 0 && s[namelen] == '=') {
      const char *v = s + namelen + 1;
      size_t n = 0;

      while (*v != '\0' && *v != '&' && n + 1 < size &&
             isalnum((unsigned char) *v)) {
        dst[n++] = *v++;
      }
      dst[n] = '\0';
      /* Truncated, or not alphanumeric to its end, is no value at all: it must
         not reach a caller that trusted the return. */
      if (n > 0 && (*v == '\0' || *v == '&')) {
        return HTS_TRUE;
      }
      dst[0] = '\0';
      return HTS_FALSE;
    }
    if (amp == NULL) {
      break;
    }
    s = amp + 1;
  }
  return HTS_FALSE;
}

/** Does the urlencoded request body present the expected session id?
    True only if at least one "sid" field is present and every occurrence
    matches, so it holds whichever one a later last-write-wins parse keeps.
    Non-destructive: it runs before the body is tokenized in place. */
static hts_boolean body_sid_is_valid(const char *body, const char *expected) {
  const char *s = body;
  hts_boolean seen = HTS_FALSE;

  while (s != NULL && *s != '\0') {
    const char *const amp = strchr(s, '&');
    const char *const eq = strchr(s, '=');

    if (eq != NULL && (amp == NULL || eq < amp) && (size_t) (eq - s) == 3 &&
        strncmp(s, "sid", 3) == 0) {
      const size_t len = amp != NULL ? (size_t) (amp - eq - 1) : strlen(eq + 1);
      hts_boolean match = HTS_FALSE;

      if (len < SID_VALUE_MAX) {
        char raw[SID_VALUE_MAX];
        String value = STRING_EMPTY;

        memcpy(raw, eq + 1, len);
        raw[len] = '\0';
        unescapehttp(raw, &value);
        /* StringBuff is NULL until written, so an empty value lands here. */
        if (StringBuff(value) != NULL &&
            strcmp(StringBuff(value), expected) == 0) {
          match = HTS_TRUE;
        }
        StringFree(value);
      }
      if (!match) {
        return HTS_FALSE;
      }
      seen = HTS_TRUE;
    }
    s = amp != NULL ? amp + 1 : NULL;
  }
  return seen;
}

#define IS_PATH_SEP(c) ((c) == '/' || (c) == '\\')

/** Append src to the NUL-terminated dst of capacity size (NUL included).
    False, leaving dst untouched, if it would not fit: unlike strcatbuff() this
    never aborts, because every piece appended here is client-supplied. */
static hts_boolean path_append(char *dst, size_t size, const char *src) {
  const size_t used = strlen(dst);
  const size_t len = strlen(src);

  /* dst holds at most size-1 bytes, so "size - used" is >= 1 and the untrusted
     len stays alone: "used + len < size" could wrap and pass. */
  if (len >= size - used) {
    return HTS_FALSE;
  }
  memcpy(dst + used, src, len + 1);
  return HTS_TRUE;
}

/** True if path holds no ".." component, either separator counting. Lexical
    only, so "a..b" is a name and no symlink is resolved. */
static hts_boolean hts_path_is_contained(const char *path) {
  const char *s;

  for (s = path; *s != '\0';) {
    while (IS_PATH_SEP(*s)) {
      s++;
    }
    if (s[0] == '.' && s[1] == '.' && (s[2] == '\0' || IS_PATH_SEP(s[2]))) {
      return HTS_FALSE;
    }
    while (*s != '\0' && !IS_PATH_SEP(*s)) {
      s++;
    }
  }
  return HTS_TRUE;
}

/* Append c to dst as an HTML entity, or return HTS_FALSE if it needs none. */
static hts_boolean cat_html_escaped(String *dst, char c) {
  switch (c) {
  case '<':
    StringCat(*dst, "&lt;");
    break;
  case '>':
    StringCat(*dst, "&gt;");
    break;
  case '&':
    StringCat(*dst, "&amp;");
    break;
  case '\'':
    StringCat(*dst, "&#39;");
    break;
  default:
    return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* Append the UTF-8 character at s as a numeric character reference, which names
   a code point and survives any page charset, and return its length in bytes.
   Zero if s does not start a valid character. (hts_readUTF8() decodes the same
   thing, but is not HTSEXT_API, so this binary cannot link it.) */
static size_t cat_html_ncr(String *dst, const char *s) {
  const unsigned char *const p = (const unsigned char *) s;
  unsigned int uc;
  size_t extra, i;
  char tmp[16];

  if (p[0] >= 0xc2 && p[0] <= 0xdf) {
    uc = p[0] & 0x1f;
    extra = 1;
  } else if ((p[0] & 0xf0) == 0xe0) {
    uc = p[0] & 0x0f;
    extra = 2;
  } else if (p[0] >= 0xf0 && p[0] <= 0xf4) {
    uc = p[0] & 0x07;
    extra = 3;
  } else {
    return 0;
  }
  /* A NUL fails this before the next byte is read, so a sequence truncated at
     the end of the string cannot run past it. */
  for (i = 1; i <= extra; i++) {
    if ((p[i] & 0xc0) != 0x80)
      return 0;
    uc = (uc << 6) | (p[i] & 0x3f);
  }
  /* Overlong and surrogate forms name nothing a browser will render. */
  if (uc < 0x80 || (uc >= 0xd800 && uc <= 0xdfff))
    return 0;
  snprintf(tmp, sizeof(tmp), "&#%u;", uc);
  StringCat(*dst, tmp);
  return extra + 1;
}

/* Same, for a double-quoted attribute: the quote included, which
   cat_html_escaped() leaves raw for the single-quoted tooltips. */
static hts_boolean cat_attr_escaped_char(String *dst, char c) {
  if (c == '\"') {
    StringCat(*dst, "&#34;");
    return HTS_TRUE;
  }
  return cat_html_escaped(dst, c);
}

/* Append value escaped for a double-quoted HTML attribute. */
static void cat_attr_escaped(String *dst, const char *value) {
  const char *a;

  for (a = value; *a != '\0'; a++) {
    if (!cat_attr_escaped_char(dst, *a)) {
      StringMemcat(*dst, a, 1);
    }
  }
}

/* Append value escaped for a single-quoted JS literal inside a double-quoted
   HTML attribute. Every escape is a \xNN group, so the only bytes it adds are
   '\', 'x' and hex digits: nothing the attribute decode can expand back into a
   quote, and no lone '\' for a DBCS trail byte to swallow. */
static void cat_js_escaped(String *dst, const char *value) {
  const char *a;

  for (a = value; *a != '\0'; a++) {
    char tmp[8];

    switch (*a) {
    case '\\':
    case '\'':
    case '\"':
    case '&':
    case '<':
    case '>':
      break;
    default:
      if ((unsigned char) *a >= 32) {
        StringMemcat(*dst, a, 1);
        continue;
      }
      break;
    }
    snprintf(tmp, sizeof(tmp), "\\x%02x", (unsigned char) *a);
    StringCat(*dst, tmp);
  }
}

/* Append LEN bytes of the value of a double-quoted command-line argument:
   escaped for HTML, which the browser undoes when it posts the command line
   back, and for the argv splitter, which does not. */
static void cat_cmdline_argn(String *output, const char *value, size_t len) {
  size_t i;

  for (i = 0; i < len; i++) {
    if (value[i] == '\\' || value[i] == '\"') {
      StringCat(*output, "\\");
    }
    if (!cat_html_escaped(output, value[i])) {
      StringMemcat(*output, &value[i], 1);
    }
  }
}

/* see cat_cmdline_argn() */
static void cat_cmdline_arg(String *output, const char *value) {
  cat_cmdline_argn(output, value, strlen(value));
}

/* Append FLAG "<rule>" once per rule in VALUE, the way the engine takes the
   options it accumulates: one flag per rule. Any whitespace separates two
   rules, as it separates two URLs in the wizard's URL box, except beside a ','
   or '=' where it belongs to the rule the parser trims it out of. */
static void cat_cmdline_arglist(String *output, const char *value,
                                const char *flag) {
  /* isspace()'s C-locale set, so a rules box splits the same here and in the
     Android app, whose Java \s covers \v and \f too. */
  static const char *const ws = " \t\r\n\v\f";
  const char *p = value + strspn(value, ws);
  hts_boolean first = HTS_TRUE;

  while (*p != '\0') {
    hts_boolean more = HTS_TRUE;

    if (!first) {
      StringCat(*output, "\n\t");
    }
    StringCat(*output, flag);
    StringCat(*output, " \"");
    first = HTS_FALSE;
    while (more) {
      const size_t len = strcspn(p, ws);
      const char *const next = p + len + strspn(p + len, ws);

      cat_cmdline_argn(output, p, len);
      more = *next != '\0' &&
                     (*next == ',' || *next == '=' ||
                      (len != 0 && (p[len - 1] == ',' || p[len - 1] == '=')))
                 ? HTS_TRUE
                 : HTS_FALSE;
      p = next;
    }
    StringCat(*output, "\"");
  }
}

/* Append one <select> entry, unless its id is the hidden one. Ids reach
   winprofile.ini one lower, so hiding one must not renumber the rest. */
static void cat_list_option(String *output, const char *label, int id,
                            int listDefault, int listHidden) {
  char tag[48];

  if (id == listHidden) {
    return;
  }
  snprintf(tag, sizeof(tag), "<option value=%d%s>", id,
           id == listDefault ? " selected" : "");
  StringCat(*output, tag);
  StringCat(*output, label);
  StringCat(*output, "</option>\r\n");
}

/* step4.html writes these keys as ${ztest:<var>:0:1}; a cleared checkbox is
   empty in session state, so only they need the 0-to-empty inverse here. */
static const char *const ini_checkbox_keys[] = {
    "Near",
    "Test",
    "ParseAll",
    "HTMLFirst",
    "Cache",
    "NoRecatch",
    "Index",
    "WordIndex",
    "Log",
    "RemoveTimeout",
    "RemoveRateout",
    "KeepAlive",
    "NoErrorPages",
    "NoExternalPages",
    "NoPwdInPages",
    "NoQueryStrings",
    "NoPurgeOldFiles",
    "Cookies",
    "ParseJava",
    "HTTP10",
    "TolerantRequests",
    "UpdateHack",
    "URLHack",
    "KeepWww",
    "KeepSlashes",
    "KeepQueryOrder",
    "StoreAllInCache",
    "Sitemap",
    "Warc",
    "WarcCdx",
    "Wacz",
    "Changes",
    "SingleFile",
    "UseHTTPProxyForFTP",
    /* Not a ${ztest:} key, but its HTTP entry is the empty one and
       option10.html already reads 0 as that entry. */
    "ProxyType",
    NULL,
};

/* These hold a ${listid:} id, numbered from 1; winprofile.ini stores the
   0-based combo index WinHTTrack writes instead. A file with no ProfileFormat
   key is WinHTTrack's own, so it is read that way too (#1314). The name says
   list; what membership marks is that shift, and ProxyType's ordinal already
   agrees, so it stays out. */
static const char *const ini_list_keys[] = {
    "CurrentAction", "Build",     "PrimaryScan",     "Travel",  "GlobalTravel",
    "RewriteLinks",  "CheckType", "FollowRobotsTxt", "LogType", NULL,
};

static hts_boolean ini_key_in(const char *const *keys, const char *key) {
  size_t i;

  for (i = 0; keys[i] != NULL; i++) {
    if (strcmp(key, keys[i]) == 0)
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* A stored 0 means "unchecked" for these keys, not the number zero. */
static hts_boolean ini_key_is_checkbox(const char *key) {
  return ini_key_in(ini_checkbox_keys, key);
}

static hts_boolean ini_key_is_list(const char *key) {
  return ini_key_in(ini_list_keys, key);
}

/* A value typed under a cleared checkbox: step4.html's command block reads
   these as <value>_eff, so the flag goes and the stored value stays. */
static const struct {
  const char *value;
  const char *gate;
} ini_gated_values[] = {
    /* clang-format off */
    {"sitemapurl", "sitemap"},
    {"warcfile", "warc"},
    {"warcmaxsize", "warc"},
    {"warccdx", "warc"},
    {"singlefilemax", "singlefile"},
    {NULL, NULL},
    /* clang-format on */
};

/* The session value NAME holds, or "" when it is unset. */
static const char *ini_value_of(const char *name) {
  intptr_t adr = 0;

  return coucal_readptr(NewLangList, name, &adr) && adr != 0
             ? (const char *) adr
             : "";
}

/* Publish each gated value as <value>_eff, empty unless its gate is on. Empty
   is off for ${test:} too, so a flag and its gate cannot disagree. */
static void ini_publish_gated_values(void) {
  size_t i;

  for (i = 0; ini_gated_values[i].value != NULL; i++) {
    const hts_boolean on = *ini_value_of(ini_gated_values[i].gate) != '\0';
    char name[64];

    snprintf(name, sizeof(name), "%s_eff", ini_gated_values[i].value);
    coucal_write(
        NewLangList, name,
        (intptr_t) strdup(on ? ini_value_of(ini_gated_values[i].value) : ""));
  }
}

/* The first LEN bytes of VALUE shifted by DELTA, or -1 to leave it alone. An
   empty, hand-edited or out-of-range id means whatever the reader makes of it.
   So does 0, which already reads as the first entry on both sides. */
static int ini_list_shift(const char *value, size_t len, int delta) {
  char digits[16];
  char *end;
  long id;

  if (len == 0 || len >= sizeof(digits))
    return -1;
  memcpy(digits, value, len);
  digits[len] = '\0';
  id = strtol(digits, &end, 10);
  if (*end != '\0' || id < 0 || id > INT_MAX - 1)
    return -1;
  return (int) id + delta;
}

/* Copy INI to OUT with the ids of ini_list_keys shifted by DELTA, so what
   reaches the file is the 0-based index WinHTTrack reads (#1314). */
static void ini_rebase_lists(const char *ini, int delta, String *out) {
  const char *line;

  StringClear(*out);
  for (line = ini; *line != '\0';) {
    const char *const nl = strchr(line, '\n');
    const size_t len = nl != NULL ? (size_t) (nl - line) + 1 : strlen(line);
    const char *const eq = (const char *) memchr(line, '=', len);
    char key[64];
    char shifted[16];
    size_t klen = 0;
    size_t vlen = 0;
    int id = -1;

    if (eq != NULL) {
      klen = (size_t) (eq - line);
      vlen = len - klen - 1;
      /* Trim the CRLF a textarea posts, which the file keeps. */
      while (vlen != 0 && (eq[vlen] == '\r' || eq[vlen] == '\n'))
        vlen--;
      if (klen < sizeof(key)) {
        memcpy(key, line, klen);
        key[klen] = '\0';
        if (ini_key_is_list(key))
          id = ini_list_shift(eq + 1, vlen, delta);
      }
    }
    if (id >= 0) {
      snprintf(shifted, sizeof(shifted), "%d", id);
      StringMemcat(*out, line, klen + 1);
      StringCat(*out, shifted);
      StringMemcat(*out, eq + 1 + vlen, len - klen - 1 - vlen);
    } else {
      StringMemcat(*out, line, len);
    }
    line += len;
  }
}

/* gmtime accepts the whole int range, where a footer can show four digits and
   the tm_year + 1900 of a year past INT_MAX - 1900 overflows. */
static hts_boolean tm_year_is_printable(const struct tm *tm) {
  return tm->tm_year >= -1900 && tm->tm_year <= 8099 ? HTS_TRUE : HTS_FALSE;
}

int smallserver(T_SOC soc, char *url, char *method, char *data, char *path) {
  int timeout = 30;
  int retour = 0;
  int willexit = 0;
  int buffer_size = 32768;
  char *buffer = (char *) malloc(buffer_size);
  String headers = STRING_EMPTY;
  String output = STRING_EMPTY;
  String tmpbuff = STRING_EMPTY;
  String tmpbuff2 = STRING_EMPTY;
  String fspath = STRING_EMPTY;
  String profile = STRING_EMPTY;
  /* Project directory this server set up; the only root /website/ serves from,
     and deliberately not cleared between requests. */
  String website = STRING_EMPTY;
  char catbuff[CATBUFF_SIZE];

  /* Load strings */
  htslang_init();
  if (!htslang_load(NULL, 0, path)) {
    fprintf(stderr, "unable to find lang.def and/or lang/ strings in %s\n",
            path);
    return 0;
  }
  LANG_T(path, 0);

  /* Init various values */
  {
    char pth[1024];

    /* "cache" beside "Cache": step2.html must not re-default it on every
       render, or a reloaded profile that cleared it flips back. */
    const char *initOn[] = {"Cache",   "cache",   "ka",        "cookies",
                            "logf",    "ftpprox", "parsejava", "testall",
                            "updhack", "urlhack", "index",     NULL};
    const initIntElt initInt[] = {
      {"filter", 4},
      {"travel", 2},
      {"travel2", 1},
      {"travel3", 1},
      /* */
      {"connexion", 4},
      /* */
      {"maxrate", 100000},
      /* */
      {"build", 1},
      /* */
      {"checktype", 2},
      {"robots", 3},

      {NULL, 0}
    };
    initStrElt initStr[] = {{"user", HTS_DEFAULT_USER_AGENT},
                            {"footer", HTS_DEFAULT_FOOTER},
                            {"url2",
                             "+*.png +*.gif +*.jpg +*.jpeg +*.css +*.js "
                             "-ad.doubleclick.net/* -mime:application/foobar"},
                            {NULL, NULL}};
    int i = 0;

    for(i = 0; initInt[i].name; i++) {
      char tmp[32];

      sprintf(tmp, "%d", initInt[i].value);
      coucal_write(NewLangList, initInt[i].name, (intptr_t) strdup(tmp));
    }
    for(i = 0; initOn[i]; i++) {
      coucal_write(NewLangList, initOn[i], (intptr_t) strdup("1"));    /* "on" */
    }
    for(i = 0; initStr[i].name; i++) {
      coucal_write(NewLangList, initStr[i].name,
                    (intptr_t) strdup(initStr[i].value));
    }
    strcpybuff(pth, gethomedir());
    strcatbuff(pth, "/websites");
    coucal_write(NewLangList, "path", (intptr_t) strdup(pth));
  }

  /* Lock */
  webhttrack_lock();

  // connexion (accept)
  while(!willexit && buffer != NULL && soc != INVALID_SOCKET) {
    char line1[1024];
    char line[8192];
    char line2[1024];
    T_SOC soc_c;
    LLint length = 0;
    const char *error_redirect = NULL;
    hts_boolean denied = HTS_FALSE;
    /* The request proved it holds the session id. */
    hts_boolean authed = HTS_FALSE;
    char origin[256];
    char host[256];

    line[0] = '\0';
    buffer[0] = '\0';
    origin[0] = '\0';
    host[0] = '\0';
    StringClear(headers);
    StringClear(output);
    StringClear(tmpbuff);
    StringClear(tmpbuff2);
    StringClear(fspath);
    StringCat(headers, "");
    StringCat(output, "");
    StringCat(tmpbuff, "");
    StringCat(tmpbuff2, "");
    StringCat(fspath, "");

    /* UnLock */
    webhttrack_release();

    /* sigpipe */
#ifndef _WIN32
    signal(SIGPIPE, sig_brpipe);
#endif

    /* Accept */
    while((soc_c = (T_SOC) accept(soc, NULL, NULL)) == INVALID_SOCKET) ;

    /* Ping */
    client_event(SMALLSERVER_CLIENT_REQUEST, NULL);

    /* Lock */
    webhttrack_lock();

    if (linputsoc_t(soc_c, line1, sizeof(line1) - 2, timeout) > 0) {
      int meth = 0;

      if (strfield(line1, "get ")) {
        meth = 1;
      } else if (strfield(line1, "post ")) {
        meth = 2;
      } else if (strfield(line1, "head ")) {    /* yes, we can do that */
        meth = 10;
      } else {
#ifdef _DEBUG
#endif
      }
      if (meth) {
        /* Flush headers */
        length = buffer_size - 2;
        while(linputsoc_t(soc_c, line, sizeof(line) - 2, timeout) > 0) {
          int p;

          if ((p = strfield(line, "Content-length:")) != 0) {
            sscanf(line + p, LLintP, &(length));
          } else if ((p = strfield(line, "Accept-language:")) != 0) {
            char tmp[32];
            char *s = line + p;

            /*int l; */
            while(*s == ' ')
              s++;
            tmp[0] = '\0';
            strncatbuff(tmp, s, 2);
            /*l = LANG_SEARCH(path, tmp); */
          } else if ((p = strfield(line, "Origin:")) != 0) {
            copy_header_value(origin, sizeof(origin), line + p);
          } else if ((p = strfield(line, "Host:")) != 0) {
            copy_header_value(host, sizeof(host), line + p);
          }
        }
        if (meth == 2) {
          int sz = 0;

          if (length > buffer_size - 2) {
            length = buffer_size - 2;
          }
          if (length > 0
              && (sz = recv_bl(soc_c, buffer, (int) length, timeout)) < 0) {
            meth = 0;
          } else {
            buffer[sz] = '\0';
          }
        }
      }

      /* Generated variables */
      if (commandEnd && !commandReturnSet) {
        commandReturnSet = 1;
        if (commandReturn) {
          char tmp[32];

          sprintf(tmp, "%d", commandReturn);
          coucal_write(NewLangList, "commandReturn", (intptr_t) strdup(tmp));
          coucal_write(NewLangList, "commandReturnMsg",
                        (intptr_t) commandReturnMsg);
          coucal_write(NewLangList, "commandReturnCmdl",
                        (intptr_t) commandReturnCmdl);
        } else {
          coucal_write(NewLangList, "commandReturn", (intptr_t) NULL);
          coucal_write(NewLangList, "commandReturnMsg", (intptr_t) NULL);
          coucal_write(NewLangList, "commandReturnCmdl", (intptr_t) NULL);
        }
      }

      /* SID check */
      {
        intptr_t adr = 0;

        if (coucal_readptr(NewLangList, "_sid", &adr)) {
          if (coucal_write
              (NewLangList, "sid", (intptr_t) strdup((char *) adr))) {
          }
        }
      }

      /* CSP stops a mirrored page reading /server/, not posting to it blind:
         a no-cors POST still runs the command. Origin is browser-set and script
         cannot forge it. Absent is allowed, most non-browser clients send none.
       */
      if (meth == 2 && origin[0] != '\0' && !origin_is_self(origin, host)) {
        buffer[0] = '\0';
        meth = 0;
        denied = HTS_TRUE;
      }

      /* Authenticate the body before parsing it: every field it carries is
         written straight into the global key store below, "command" included,
         and that one reaches the engine. Checking afterwards cannot work — the
         damage is already done, and the pre-seeded "sid" above would compare
         equal to itself for a request that simply omits the field. */
      if (meth && buffer[0]) {
        intptr_t expected = 0;

        if (!coucal_readptr(NewLangList, "_sid", &expected) ||
            !body_sid_is_valid(buffer, (const char *) expected)) {
          buffer[0] = '\0';
          meth = 0;
          denied = HTS_TRUE;
        } else {
          authed = HTS_TRUE;
        }
      }

      /* check variables */
      if (meth && buffer[0]) {
        char *s = buffer;
        char *e, *f;

        strlcatbuff(buffer, "&", buffer_size);
        while(s && (e = strchr(s, '=')) && (f = strchr(s, '&'))) {
          const char *ua;
          String sua = STRING_EMPTY;

          *e = *f = '\0';
          ua = e + 1;
          if (strfield2(ua, "on"))      /* hack : "on" == 1 */
            ua = "1";
          unescapehttp(ua, &sua);
          coucal_write(NewLangList, s, (intptr_t) StringAcquire(&sua));
          s = f + 1;
        }
      }

      /* Check variables (internal) */
      if (meth) {
        int doLoad = 0;
        intptr_t adr = 0;

        if (coucal_readptr(NewLangList, "lang", &adr)) {
          int n = 0;

          if (sscanf((char *) adr, "%d", &n) == 1 && n > 0
              && n - 1 != LANG_T(path, -1)) {
            LANG_T(path, n - 1);
            /* make a backup, because the GUI will override it */
            coucal_write(NewLangList, "lang_",
                          (intptr_t) strdup((char *) adr));
          }
        }

        /* Load existing project settings */
        if (coucal_readptr(NewLangList, "loadprojname", &adr)) {
          char *pname = (char *) adr;

          if (*pname) {
            coucal_write(NewLangList, "projname", (intptr_t) strdup(pname));
          }
          coucal_write(NewLangList, "loadprojname", (intptr_t) NULL);
          doLoad = 1;
        } else if (coucal_readptr(NewLangList, "loadprojcateg", &adr)) {
          char *pname = (char *) adr;

          if (*pname) {
            coucal_write(NewLangList, "projcateg", (intptr_t) strdup(pname));
          }
          coucal_write(NewLangList, "loadprojcateg", (intptr_t) NULL);
        }

        /* intial configuration */
        {
          if (!coucal_read(NewLangList, "conf_file_loaded", NULL)) {
            coucal_write(NewLangList, "conf_file_loaded",
                          (intptr_t) strdup("true"));
            doLoad = 2;
          }
        }

        /* path : <path>/<project> */
        if (!commandRunning) {
          intptr_t adrpath = 0, adrprojname = 0;

          if (coucal_readptr(NewLangList, "path", &adrpath)
              && coucal_readptr(NewLangList, "projname", &adrprojname)) {
            StringClear(fspath);
            StringCat(fspath, (char *) adrpath);
            StringCat(fspath, "/");
            StringCat(fspath, (char *) adrprojname);
          }
        }

        /* Load existing project settings */
        if (doLoad) {
          FILE *fp;

          if (doLoad == 1) {
            StringCat(fspath, "/hts-cache/winprofile.ini");
          } else if (doLoad == 2) {
            StringCopy(fspath, gethomedir());
#ifdef _WIN32
            StringCat(fspath, "/httrack.ini");
#else
            StringCat(fspath, "/.httrack.ini");
#endif
          }
          /* fspath is two posted fields: a ".." would read a file outside the
             mirror into the form. */
          if (hts_path_is_contained(StringBuff(fspath))) {
            fp = fopen(StringBuff(fspath), "rb");
          } else {
            fp = NULL;
          }
          if (fp) {
            /* Read file */
            while (!feof(fp) && !ferror(fp)) {
              char *str = line;
              char *pos;

              if (!linput(fp, line, sizeof(line) - 2)) {
                *str = '\0';
              }
              pos = strchr(line, '=');
              if (pos) {
                String escline = STRING_EMPTY;
                char listid[16];

                *pos++ = '\0';
                /* Only a checkbox: elsewhere zero is the user's value, and
                   emptying it silently restores the wizard default (#1177). */
                if (pos[0] == '0' && pos[1] == '\0' &&
                    ini_key_is_checkbox(line))
                  *pos = '\0';
                if (ini_key_is_list(line)) {
                  const int id = ini_list_shift(pos, strlen(pos), 1);

                  if (id >= 0) {
                    snprintf(listid, sizeof(listid), "%d", id);
                    pos = listid;
                  }
                }
                /* A key in the file overwrites the default even when it is
                   empty, and an acquired NULL reads back as absent (#1186). */
                StringClear(escline);
                unescapeini(pos, &escline);
                coucal_write(NewLangList, line,
                              (intptr_t) StringAcquire(&escline));
              }
            }

            fclose(fp);
          }
        }

      }

      /* Execute command */
      {
        intptr_t adr = 0;
        int p = 0;

        if (coucal_readptr(NewLangList, "command", &adr)) {
          if (strcmp((char *) adr, "cancel") == 0) {
            if (commandRunning) {
              if (!commandEndRequested) {
                commandEndRequested = 1;
                hts_request_stop(global_opt, 0);
              } else {
                hts_request_stop(global_opt, 1);        /* note: the force flag does not have anyeffect yet */
                commandEndRequested = 2;        /* will break the loop() callback */
              }
            }
          } else if ((p = strfield((char *) adr, "cancel-file="))) {
            if (commandRunning) {
              hts_cancel_file_push(global_opt, (char *) adr + p);
            }
          } else if (strcmp((char *) adr, "cancel-parsing") == 0) {
            if (commandRunning) {
              hts_cancel_parsing(global_opt);
            }
          } else if ((p = strfield((char *) adr, "pause="))) {
            if (commandRunning) {
              hts_setpause(global_opt, 1);
            }
          } else if ((p = strfield((char *) adr, "unpause"))) {
            if (commandRunning) {
              hts_setpause(global_opt, 0);
            }
          } else if (strcmp((char *) adr, "abort") == 0) {
            if (commandRunning) {
              hts_request_stop(global_opt, 1);
              commandEndRequested = 2;  /* will break the loop() callback */
            }
          } else if ((p = strfield((char *) adr, "add-url="))) {
            if (commandRunning) {
              char *ptraddr[2];

              ptraddr[0] = (char *) adr + p;
              ptraddr[1] = NULL;
              hts_addurl(global_opt, ptraddr);
            }
          } else if ((p = strfield((char *) adr, "httrack"))) {
            if (!commandRunning) {
              intptr_t adrcd = 0;

              if (coucal_readptr(NewLangList, "command_do", &adrcd)) {
                intptr_t adrw = 0;

                if (coucal_readptr(NewLangList, "winprofile", &adrw)) {

                  /* User general profile */
                  intptr_t adruserprofile = 0;

                  if (coucal_readptr
                      (NewLangList, "userprofile", &adruserprofile)
                      && adruserprofile != 0) {
                    int count = (int) strlen((char *) adruserprofile);

                    if (count > 0) {
                      FILE *fp;

                      StringClear(tmpbuff);
                      StringCopy(tmpbuff, gethomedir());
#ifdef _WIN32
                      StringCat(tmpbuff, "/httrack.ini");
#else
                      StringCat(tmpbuff, "/.httrack.ini");
#endif
                      fp = fopen(StringBuff(tmpbuff), "wb");
                      if (fp != NULL) {
                        (void) hts_fwrite_exact((const char *) adruserprofile,
                                                (size_t) count, fp);
                        fclose(fp);
                      }
                    }
                  }

                  /* Profile */
                  StringClear(tmpbuff);
                  StringCat(tmpbuff, StringBuff(fspath));
                  StringCat(tmpbuff, "/hts-cache/");

                  /* Create the structure, unless a ".." component in either
                     posted half would put it outside the named directory. */
                  if (!hts_path_is_contained(StringBuff(fspath))) {
                    SET_ERRORF("Project path escapes its parent directory: %s",
                               StringBuff(fspath));
                  } else if (!structcheck(StringBuff(tmpbuff))) {
                    FILE *fp;

                    StringCopy(website, StringBuff(fspath));
                    StringCat(tmpbuff, "winprofile.ini");
                    fp = fopen(StringBuff(tmpbuff), "wb");
                    if (fp != NULL) {
                      int count;

                      /* The ids leave 0-based, matching how WinHTTrack stores
                         them and this server reads them back (#1314). */
                      ini_rebase_lists((char *) adrw, -1, &profile);
                      count = (int) StringLength(profile);
                      if (hts_fwrite_exact(StringBuff(profile), (size_t) count,
                                           fp)) {

                        /* Wipe the doit.log file, useless here (all options are replicated) and
                           even a bit annoying (duplicate/ghost options)
                           The behaviour is exactly the same as in WinHTTrack
                         */
                        StringClear(tmpbuff);
                        StringCat(tmpbuff, StringBuff(fspath));
                        StringCat(tmpbuff, "/hts-cache/doit.log");
                        remove(StringBuff(tmpbuff));

                        /*
                           RUN THE SERVER
                         */
                        if (strcmp((char *) adrcd, "start") == 0) {
                          /* POST body is in the form's charset, not the
                             UTF-8 argv the engine now assumes (#629). */
                          char *const cmdl = (char *) adr + p;
                          char *cmdlUtf8 = hts_convertStringToUTF8(
                              cmdl, strlen(cmdl), LANGSEL("LANGUAGE_CHARSET"));

                          webhttrack_main(cmdlUtf8 != NULL ? cmdlUtf8 : cmdl);
                          freet(cmdlUtf8);
                        } else {
                          commandRunning = 0;
                          commandEnd = 1;
                        }
                      } else {
                        SET_ERRORF(
                            "Unable to write %d bytes in the the init file %s",
                            count, StringBuff(fspath));
                      }
                      fclose(fp);
                    } else {
                      SET_ERRORF("Unable to create the init file %s",
                                 StringBuff(fspath));
                    }
                  } else {
                    SET_ERRORF("Unable to create the directory structure in %s",
                               StringBuff(fspath));
                  }

                } else {
                  SET_ERROR
                    ("Internal server error: unable to fetch project name or path");
                }
              }
            }
          } else if (strcmp((char *) adr, "quit") == 0) {
            willexit = 1;
          }
          coucal_write(NewLangList, "command", (intptr_t) NULL);
        }
      }

      /* Response */
      if (meth) {
        hts_boolean virtualpath = HTS_FALSE;
        char *pos;
        char *url = strchr(line1, ' ');

        if (url && *++url == '/' && (pos = strchr(url, ' ')) && !(*pos = '\0')) {
          char fsfile[1024];
          const char *file;
          const char *query = "";
          FILE *fp;
          char *qpos;

          /* get the URL */
          fsfile[0] = '\0';
          if (error_redirect == NULL) {
            if ((qpos = strchr(url, '?'))) {
              *qpos = '\0';
              query = qpos + 1;
            }
            if (strcmp(url, "/") == 0) {
              file = "/server/index.html";
              meth = 2;
            } else {
              file = url;
            }
          } else {
            file = error_redirect;
            meth = 2;
          }

          if (strncmp(file, "/website/", 9) == 0) {
            virtualpath = HTS_TRUE;
          }

          /* override */
          if (commandRunning) {
            if (is_html(file)) {
              file = "/server/refresh.html";
            }
          } else if (commandEnd && !virtualpath && !willexit) {
            if (is_html(file)) {
              file = "/server/finished.html";
            }
          }

          /* the override above may have swapped a mirror path for a GUI page */
          virtualpath = strncmp(file, "/website/", 9) == 0;

          if (!virtualpath) {
            if (!path_append(fsfile, sizeof(fsfile), path) ||
                !path_append(fsfile, sizeof(fsfile), "html") ||
                !path_append(fsfile, sizeof(fsfile), file)) {
              fsfile[0] = '\0';
            }
          } else if (StringNotEmpty(website)) {
            /* Never the posted "projpath": a client root reads any file. */
            if (!path_append(fsfile, sizeof(fsfile), StringBuff(website)) ||
                !path_append(fsfile, sizeof(fsfile), "/") ||
                !path_append(fsfile, sizeof(fsfile), file + 9)) {
              fsfile[0] = '\0';
            }
          }

          /* Regular files only: reading a directory or FIFO never ends, and
             "path" may hold "..", so only the untrusted halves are checked. */
          if (fsfile[0] && strstr(file, "..") == NULL && fexist(fsfile) &&
              (fp = fopen(fsfile, "rb"))) {
            char ok[] =
              "HTTP/1.0 200 OK\r\n" "Connection: close\r\n"
              "Server: httrack-small-server\r\n" "Content-type: text/html\r\n"
              "Cache-Control: no-cache, must-revalidate, private\r\n"
              "Pragma: no-cache\r\n";
            char ok_other[] = "HTTP/1.0 200 OK\r\n"
                              "Connection: close\r\n"
                              "Server: httrack small server\r\n"
                              "Content-type: ";

            /* register current page */
            coucal_write(NewLangList, "thisfile", (intptr_t) strdup(file));

            /* Force GET for the last request */
            if (meth == 2 && willexit) {
              meth = 1;
            }

            /* posted data are redirected to get protocol */
            if (meth == 2) {
              char redir[] =
                "HTTP/1.0 302 Redirect\r\n" "Connection: close\r\n"
                "Server: httrack-small-server\r\n";
              intptr_t adr = 0;
              const char *newfile = file;

              if (coucal_readptr(NewLangList, "redirect", &adr) && adr != 0) {
                const char *newadr = (char *) adr;

                if (*newadr) {
                  newfile = newadr;
                }
              }
              StringMemcat(headers, redir, strlen(redir));
              /* client-supplied: a CR/LF here would split the response */
              if (newfile[strcspn(newfile, "\r\n")] == '\0') {
                StringCat(headers, "Location: ");
                StringCat(headers, newfile);
                StringCat(headers, "\r\n");
              }
              coucal_write(NewLangList, "redirect", (intptr_t) NULL);
            } else if (!virtualpath && is_html(file)) {
              /* GUI templates only: ${_sid} in a mirrored page would hand the
                 crawled site the session id that authenticates commands */
              int outputmode = 0;

              ini_publish_gated_values();
              StringMemcat(headers, ok, sizeof(ok) - 1);
              while (!feof(fp) && !ferror(fp)) {
                char *str = line;
                int prevlen = (int) StringLength(output);
                int nocr = 0;

                if (!linput(fp, line, sizeof(line) - 2)) {
                  *str = '\0';
                }
                if (hts_lastchar(str) == '\\') {
                  nocr = 1;
                  hts_striplastchar(str, '\\');
                }
                while(*str) {
                  char *pos;
                  size_t n;

                  if (*str == '$' && *++str == '{' && (pos = strchr(++str, '}'))
                      && (n = (pos - str)) && n < 1024) {
                    char name_[1024 + 2];
                    char *name = name_;
                    const char *langstr = NULL;
                    int p;
                    int format = 0;
                    int listDefault = 0;
                    int listHidden = 0; /* ids start at 1, so 0 hides none */
                    hts_boolean unquoted = HTS_FALSE;
                    /* value comes from the template, not from the settings */
                    hts_boolean literal = HTS_FALSE;
                    /* value is UTF-8, so emit it as character references */
                    hts_boolean needs_ncr = HTS_FALSE;
                    char datebuff[16];

                    name[0] = '\0';
                    strlncatbuff(name, str, sizeof(name_), n);

                    if (strncmp(name, "/*", 2) == 0) {
                      /* comments */
                    } else if ((p = strfield(name, "html:"))) {
                      name += p;
                      format = 1;
                    } else if ((p = strfield(name, "attr:"))) {
                      name += p;
                      format = 7;
                    } else if ((p = strfield(name, "js:"))) {
                      name += p;
                      format = 6;
                    } else if ((p = strfield(name, "unquoted:"))) {
                      name += p;
                      unquoted = HTS_TRUE;
                    } else if ((p = strfield(name, "arg:"))) {
                      name += p;
                      format = 5;
                    } else if ((p = strfield(name, "list:"))) {
                      name += p;
                      format = 2;
                    } else if ((p = strfield(name, "liststr:"))) {
                      name += p;
                      format = -2;
                    } else if ((p = strfield(name, "date:"))) {
                      /* Expanded on each request, so the footer year cannot
                         drift from the calendar the way a stamped one does
                         (#1165). SOURCE_DATE_EPOCH pins it, so a test can ask
                         for another year. */
                      const char *epoch = getenv("SOURCE_DATE_EPOCH");
                      struct tm tmv;
                      hts_boolean ok = HTS_FALSE;

                      name += p;
                      format = 0;
                      langstr = "";
                      if (strcmp(name, "year") == 0) {
                        if (epoch != NULL && *epoch) {
                          char *end;
                          const long long secs = strtoll(epoch, &end, 10);

                          /* UTC, as the variable is defined; one that does not
                             fit time_t would narrow into an unrelated date */
                          if (*end == '\0' && secs >= 0 &&
                              secs == (long long) (time_t) secs)
                            ok = hts_gmtime((time_t) secs, &tmv) &&
                                 tm_year_is_printable(&tmv);
                        }
                        /* an override we cannot use falls back to the clock: a
                           footer reading 1998-1970 is worse than an ignored
                           override */
                        if (!ok)
                          ok = hts_localtime(time(NULL), &tmv) &&
                               tm_year_is_printable(&tmv);
                        if (ok) {
                          /* four digits, as test 185 exempts date: from the
                             escaping it demands of runtime data */
                          snprintf(datebuff, sizeof(datebuff), "%04d",
                                   tmv.tm_year + 1900);
                          langstr = datebuff;
                        }
                      }
                    } else if ((p = strfield(name, "file-exists:"))) {
                      char *pos2;

                      name += p;
                      format = 0;
                      literal = HTS_TRUE;
                      pos2 = strchr(name, ':');
                      langstr = "";
                      if (pos2 != NULL) {
                        *pos2 = '\0';
                        if (strstr(name, "..") == NULL) {
                          if (fexist(fconcat(catbuff, sizeof(catbuff), path, name))) {
                            langstr = pos2 + 1;
                          }
                        }
                      }
                    } else if ((p = strfield(name, "do:"))) {
                      char *pos2;
                      char empty[2] = "";

                      name += p;
                      format = 1;
                      pos2 = strchr(name, ':');
                      langstr = "";
                      if (pos2 != NULL) {
                        *pos2 = '\0';
                        pos2++;
                      } else {
                        pos2 = empty;
                      }
                      if (strcmp(name, "output-mode") == 0) {
                        if (strcmp(pos2, "html") == 0) {
                          outputmode = 1;
                        } else if (strcmp(pos2, "inifile") == 0) {
                          outputmode = 2;
                        } else if (strcmp(pos2, "html-urlescaped") == 0) {
                          outputmode = 3;
                        } else {
                          outputmode = 0;
                        }
                      } else if (strcmp(name, "if-file-exists") == 0) {
                        if (strstr(pos2, "..") == NULL) {
                          if (!fexist(fconcat(catbuff, sizeof(catbuff), path, pos2))) {
                            outputmode = -1;
                          }
                        }
                      } else if (strcmp(name, "if-project-file-exists") == 0) {
                        if (strstr(pos2, "..") == NULL) {
                          if (!fexist
                              (fconcat(catbuff, sizeof(catbuff), StringBuff(fspath), pos2))) {
                            outputmode = -1;
                          }
                        }
                      } else if (strcmp(name, "if-file-do-not-exists") == 0) {
                        if (strstr(pos2, "..") == NULL) {
                          if (fexist(fconcat(catbuff, sizeof(catbuff), path, pos2))) {
                            outputmode = -1;
                          }
                        }
                      } else if (strcmp(name, "if-not-empty") == 0) {
                        intptr_t adr = 0;

                        if (!coucal_readptr(NewLangList, pos2, &adr)
                            || *((char *) adr) == 0) {
                          outputmode = -1;
                        }
                      } else if (strcmp(name, "if-empty") == 0) {
                        intptr_t adr = 0;

                        if (coucal_readptr(NewLangList, pos2, &adr)
                            && *((char *) adr) != 0) {
                          outputmode = -1;
                        }
                      } else if (strcmp(name, "end-if") == 0) {
                        outputmode = 0;
                      } else if (strcmp(name, "loadhash") == 0) {
                        intptr_t adr = 0;

                        if (coucal_readptr(NewLangList, "path", &adr)) {
                          char *rpath = (char *) adr;

                          //find_handle h;
                          /* note: patching stored (inhash) value */
                          hts_striplastchar(rpath, '/');
                          {
                            const char *profiles = hts_getcategories(rpath, 0);
                            const char *categ = hts_getcategories(rpath, 1);

                            coucal_write(NewLangList, "winprofile",
                                          (intptr_t) profiles);
                            coucal_write(NewLangList, "wincateg",
                                          (intptr_t) categ);
                          }
                        }
                      } else if (strcmp(name, "copy") == 0) {
                        if (*pos2) {
                          char *pos3 = strchr(pos2, ':');

                          if (pos3 && *(pos3 + 1)) {
                            intptr_t adr = 0;

                            *pos3++ = '\0';
                            if (coucal_readptr(NewLangList, pos2, &adr)) {
                              coucal_write(NewLangList, pos3,
                                            (intptr_t) strdup((char *) adr));
                              coucal_write(NewLangList, pos2, (intptr_t) NULL);
                            }
                          }
                        }
                      } else if (strcmp(name, "set") == 0) {
                        if (*pos2) {
                          char *pos3 = strchr(pos2, ':');

                          if (pos3) {
                            *pos3++ = '\0';
                            coucal_write(NewLangList, pos2,
                                          (intptr_t) strdup(pos3));
                          } else {
                            coucal_write(NewLangList, pos2, (intptr_t) NULL);
                          }
                        }
                      }
                    }
                    /* arglist:<field>:<flag>
                       one "<flag> "rule"" per non-empty line of the field */
                    else if ((p = strfield(name, "arglist:"))) {
                      char *pos2;

                      langstr = "";
                      literal = HTS_TRUE;
                      name += p;
                      pos2 = strchr(name, ':');
                      if (pos2 != NULL && outputmode != -1) {
                        intptr_t adr = 0;

                        *pos2 = '\0';
                        if (coucal_readptr(NewLangList, name, &adr) &&
                            adr != 0) {
                          cat_cmdline_arglist(&output, (const char *) adr,
                                              pos2 + 1);
                        }
                      }
                    }
                    /*
                       test:<if exist>
                       test:<if ==0>:<if ==1>:<if == 2>..
                       ztest:<if == 0 || !exist>:<if == 1>:<if == 2>..
                     */
                    else if ((p = strfield(name, "test:")) ||
                             (p = strfield(name, "ztest:"))) {
                      intptr_t adr = 0;
                      char *pos2;
                      int ztest = (name[0] == 'z');

                      langstr = "";
                      literal = HTS_TRUE;
                      name += p;
                      pos2 = strchr(name, ':');
                      if (pos2 != NULL) {
                        *pos2 = '\0';
                        if (coucal_readptr(NewLangList, name, &adr) || ztest) {
                          const char *newadr = (char *) adr;

                          if (!newadr)
                            newadr = "";
                          if (*newadr || ztest) {
                            int npos = 0;

                            name = pos2 + 1;
                            format = 4;
                            if (strchr(name, ':') == NULL) {
                              npos = 0; /* first is good if only one : */
                              format = 0;
                            } else {
                              if (sscanf(newadr, "%d", &npos) != 1) {
                                if (strfield(newadr, "on")) {
                                  npos = 1;
                                } else {
                                  npos = 0;     /* first one will be ok */
                                  format = 0;
                                }
                              }
                            }
                            while(*name && *name != '}' && npos >= 0) {
                              int end = 0;
                              char *fpos = strchr(name, ':');
                              int n2;

                              if (fpos == NULL) {
                                fpos = name + strlen(name);
                                end = 1;
                              }
                              n2 = (int) (fpos - name);
                              if (npos == 0) {
                                langstr = name;
                                *fpos = '\0';
                              } else if (end) {
                                npos = 0;
                              }
                              name += n2 + 1;
                              npos--;
                            }
                          }
                        }
                      }
                    } else if ((p = strfield(name, "listid:"))) {
                      char *pos2;

                      name += p;
                      format = 2;
                      pos2 = strchr(name, ':');
                      if (pos2) {
                        char dname[32];
                        int n2 = (int) (pos2 - name);

                        if (n2 > 0 && n2 < sizeof(dname) - 2) {
                          intptr_t adr = 0;

                          dname[0] = '\0';
                          strncatbuff(dname, name, n2);
                          if (coucal_readptr(NewLangList, dname, &adr)) {
                            int n = 0;

                            if (sscanf((char *) adr, "%d", &n) == 1) {
                              listDefault = n;
                            }
                          }
                          name += n2 + 1;
                        }
                      }
                      /* ${listid:<var>:<key>:<id>}: <id> is an entry this front
                         end cannot serve and must not offer. */
                      pos2 = strrchr(name, ':');
                      if (pos2 != NULL && pos2[1] != '\0' &&
                          strspn(pos2 + 1, "0123456789") == strlen(pos2 + 1)) {
                        listHidden = atoi(pos2 + 1);
                        *pos2 = '\0';
                      }
                    } else if ((p = strfield(name, "checked:"))) {
                      name += p;
                      format = 3;
                    }
                    if (langstr == NULL) {
                      if (strfield2(name, "#iso")) {
                        langstr = line2;
                        line2[0] = '\0';
                        LANG_LIST(path, line2, sizeof(line2));
                        assertf(strlen(langstr) < sizeof(line2) - 2);
                        needs_ncr = HTS_TRUE;
                      } else {
                        langstr = LANGSEL(name);
                        if (langstr == NULL || *langstr == '\0') {
                          intptr_t adr = 0;

                          if (coucal_readptr(NewLangList, name, &adr)) {
                            char *newadr = (char *) adr;

                            langstr = newadr;
                          }
                        }
                      }
                    }
                    /* consumed here: it shares nothing with the list and
                       option formats below */
                    if (format == 5 && langstr != NULL && outputmode != -1) {
                      cat_cmdline_arg(&output, langstr);
                      langstr = NULL;
                    }
                    if (langstr && outputmode != -1) {
                      switch (format) {
                      case 0:
                        {
                          const char *a = langstr;

                          while(*a) {
                            /* the ini writer has no inverse for it, so a lone
                               backslash in a settings value must stay one */
                            if (literal && a[0] == '\\' && isxdigit(a[1]) &&
                                isxdigit(a[2])) {
                              int n;
                              char c;

                              if (sscanf(a + 1, "%x", &n) == 1) {
                                c = (char) n;
                                StringMemcat(output, &c, 1);
                              }
                              a += 2;
                            } else if ((unquoted || outputmode == 3) &&
                                       a[0] == '\"') {
                              /* an entity decodes back to a quote, which opens
                                 a quoted run in the argv splitter or ends the
                                 attribute the URL sits in; no URI holds one */
                              StringCat(output, "%22");
                            } else if (outputmode &&
                                       cat_html_escaped(&output, a[0])) {
                              /* appended as an entity */
                            } else if (outputmode == 3 && a[0] == ' ') {
                              StringCat(output, "%20");
                            } else if (outputmode >= 2 &&
                                       ((unsigned char) a[0]) < 32) {
                              char tmp[32];

                              sprintf(tmp, "%%%02x", (unsigned char) a[0]);
                              StringCat(output, tmp);
                            } else if (outputmode == 2 && a[0] == '%') {
                              StringCat(output, "%%");
                            } else if (outputmode == 3 && a[0] == '%') {
                              StringCat(output, "%25");
                            } else {
                              StringMemcat(output, a, 1);
                            }
                            a++;
                          }
                        }
                        break;
                      case 3:
                        if (*langstr) {
                          StringCat(output, "checked");
                        }
                        break;
                      case 6:
                        cat_js_escaped(&output, langstr);
                        break;
                      case 7:
                        cat_attr_escaped(&output, langstr);
                        break;
                      default:
                        if (*langstr) {
                          int id = 1;
                          size_t used;
                          const char *fstr = langstr;

                          StringClear(tmpbuff);
                          if (format == -2) {
                            StringCat(output, "<option value=\"");
                          }
                          while(*fstr) {
                            switch (*fstr) {
                            case 13:
                              break;
                            case 10:
                              if (format == 1) {
                                StringCat(output, StringBuff(tmpbuff));
                                StringCat(output, "<br>\r\n");
                              } else if (format == -2) {
                                StringCat(output, StringBuff(tmpbuff));
                                StringCat(output, "\">");
                                StringCat(output, StringBuff(tmpbuff));
                                StringCat(output, "</option>\r\n");
                                StringCat(output, "<option value=\"");
                              } else {
                                cat_list_option(&output, StringBuff(tmpbuff),
                                                id++, listDefault, listHidden);
                              }
                              StringClear(tmpbuff);
                              break;
                            default:
                              if (needs_ncr &&
                                  (used = cat_html_ncr(&tmpbuff, fstr)) != 0) {
                                /* the loop's own fstr++ takes the last byte */
                                fstr += used - 1;
                                break;
                              }
                              /* format -2 writes its value into the option's
                                 value="" as well, so the quote must go too */
                              if (!(format == -2
                                        ? cat_attr_escaped_char(&tmpbuff, *fstr)
                                        : cat_html_escaped(&tmpbuff, *fstr))) {
                                StringMemcat(tmpbuff, fstr, 1);
                              }
                              break;
                            }
                            fstr++;
                          }
                          if (format == 2) {
                            cat_list_option(&output, StringBuff(tmpbuff), id,
                                            listDefault, listHidden);
                          } else if (format == -2) {
                            StringCat(output, StringBuff(tmpbuff));
                            StringCat(output, "\">");
                            StringCat(output, StringBuff(tmpbuff));
                            StringCat(output, "</option>");
                          } else {
                            StringCat(output, StringBuff(tmpbuff));
                          }
                          StringClear(tmpbuff);
                        }
                      }
                    }
                    str = pos;
                  } else {
                    if (outputmode != -1) {
                      StringMemcat(output, str, 1);
                    }
                  }
                  str++;
                }
                if (!nocr && prevlen != StringLength(output)) {
                  StringCat(output, "\r\n");
                }
              }
#ifdef _DEBUG
              {
                int len = (int) strlen((char *) StringBuff(output));

                assert(len == (int) StringLength(output));
              }
#endif
            } else {
              if (is_html(file)) {
                StringMemcat(headers, ok, sizeof(ok) - 1);
              } else {
                const char *const type = server_content_type(file);

                StringMemcat(headers, ok_other, sizeof(ok_other) - 1);
                StringCat(headers,
                          type != NULL ? type : "application/octet-stream");
                StringCat(headers, "\r\n");
              }
              if (virtualpath) {
                /* No allow-same-origin: an opaque origin keeps script in a
                   crawled page from reading the session id out of /server/ */
                StringCat(headers, "Content-Security-Policy: sandbox "
                                   "allow-scripts allow-forms allow-popups "
                                   "allow-downloads\r\n");
              }
              while(!feof(fp)) {
                int n = (int) fread(line, 1, sizeof(line) - 2, fp);

                if (n <= 0) {
                  break; /* short read: EOF or error, never a retry */
                }
                StringMemcat(output, line, n);
              }
            }
            fclose(fp);
          } else if (strcmp(file, "/ping") == 0) {
            /* A cached heartbeat would never reach us again, and silence is
               what the watchdog reads as a dead window. */
            char error_hdr[] =
                "HTTP/1.0 200 Pong\r\n"
                "Server: httrack small server\r\n"
                "Content-type: text/html\r\n"
                "Cache-Control: no-cache, must-revalidate, private\r\n"
                "Pragma: no-cache\r\n";

            char window[SMALLSERVER_WINDOW_ID_MAX + 1];

            StringCat(headers, error_hdr);
            if (query_alnum_value(window, sizeof(window), query, "w")) {
              char verb[SMALLSERVER_WINDOW_ID_MAX + 1];

              /* Ending a session is a command, so it carries the session id
                 like every other one. A heartbeat can only extend a life, and
                 any local peer or visited page can send one of those. */
              client_event(
                  authed && query_alnum_value(verb, sizeof(verb), query, "e") &&
                          strcmp(verb, "bye") == 0
                      ? SMALLSERVER_CLIENT_LEAVING
                      : SMALLSERVER_CLIENT_PING,
                  window);
            }
          } else {
            char error_hdr[] =
              "HTTP/1.0 404 Not Found\r\n" "Server: httrack small server\r\n"
              "Content-type: text/html\r\n";
            char error[] = "Page not found.\r\n";

            StringCat(headers, error_hdr);
            StringCat(output, error);
          }
        }
      } else if (denied) {
        StringCat(headers, "HTTP/1.0 403 Forbidden\r\n"
                           "Server: httrack small server\r\n"
                           "Content-type: text/html\r\n");
        StringCat(output,
                  "Missing or invalid session id, or foreign origin.\r\n");
      } else {
#ifdef _DEBUG
        char error_hdr[] =
          "HTTP/1.0 500 Server Error\r\n" "Server: httrack small server\r\n"
          "Content-type: text/html\r\n";
        char error[] = "Server error.\r\n";

        StringCat(headers, error_hdr);
        StringCat(output, error);
#endif
      }
      {
        char tmp[256];

        sprintf(tmp, "Content-length: %d\r\n", (int) StringLength(output));
        StringCat(headers, tmp);
      }
      StringCat(headers, "\r\n");
      /* a refusal cleared meth, yet the Content-length above promises a body */
      if ((send(soc_c, StringBuff(headers), (int) StringLength(headers), 0) !=
           StringLength(headers)) ||
          ((meth == 1 || denied) &&
           (send(soc_c, StringBuff(output), (int) StringLength(output), 0) !=
            StringLength(output)))) {
#ifdef _DEBUG
#endif
      }
    } else {
#ifdef _DEBUG
#endif
    }

    /* Shutdown (FIN) and wait until confirmed */
    {
      char c;

#ifdef _WIN32
      shutdown(soc_c, SD_SEND);
#else
      shutdown(soc_c, 1);
#endif
      /* This is necessary as IE sometimes (!) sends an additional CRLF after POST data */
      while(recv(soc_c, ((char *) &c), 1, 0) > 0) ;
    }

#ifdef _WIN32
    closesocket(soc_c);
#else
    close(soc_c);
#endif
  }

  if (soc != INVALID_SOCKET) {
#ifdef _WIN32
    closesocket(soc);
#else
    close(soc);
#endif
  }

  /* Only the UI asking to quit is a clean stop; losing the socket or the buffer
     is what the caller reports as a failure. */
  retour = willexit;

  StringFree(headers);
  StringFree(output);
  StringFree(tmpbuff);
  StringFree(tmpbuff2);
  StringFree(fspath);
  StringFree(profile);
  StringFree(website);

  if (buffer)
    free(buffer);

  if (commandReturnMsg)
    free(commandReturnMsg);
  commandReturnMsg = NULL;
  if (commandReturnCmdl)
    free(commandReturnCmdl);
  commandReturnCmdl = NULL;

  /* Unlock */
  webhttrack_release();

  return retour;
}

/* Language files */

int htslang_init(void) {
  if (NewLangList == NULL) {
    NewLangList = coucal_new(0);
    coucal_set_name(NewLangList, "NewLangList");
    if (NewLangList == NULL) {
      abortLog("Error in lang.h: not enough memory");
    } else {
      coucal_value_is_malloc(NewLangList, 1);
    }
  }
  return 1;
}

int htslang_uninit(void) {
  if (NewLangList != NULL) {
    coucal_delete(&NewLangList);
  }
  return 1;
}

void smallserver_setpinghandler(void (*fun)(void *, smallserver_client_event,
                                            const char *),
                                void *arg) {
  pingFun = fun;
  pingFunArg = arg;
}

int smallserver_setkey(const char *key, const char *value) {
  return coucal_write(NewLangList, key, (intptr_t) strdup(value));
}

int smallserver_setkeyint(const char *key, LLint value) {
  char tmp[256];

  snprintf(tmp, sizeof(tmp), LLintP, value);
  return coucal_write(NewLangList, key, (intptr_t) strdup(tmp));
}
int smallserver_setkeyarr(const char *key, int id, const char *key2, const char *value) {
  char tmp[256];

  snprintf(tmp, sizeof(tmp), "%s%d%s", key, id, key2);
  return coucal_write(NewLangList, tmp, (intptr_t) strdup(value));
}

static int htslang_load(char *limit_to, size_t limit_size, const char *path) {
  const char *hashname;
  char catbuff[CATBUFF_SIZE];

  //
  int selected_lang = LANG_T(path, -1);

  //
  if (!limit_to) {
    LANG_DELETE();
    NewLangStr = coucal_new(0);
    NewLangStrKeys = coucal_new(0);
    coucal_set_name(NewLangStr, "NewLangStr");
    coucal_set_name(NewLangStrKeys, "NewLangStrKeys");
    if ((NewLangStr == NULL) || (NewLangStrKeys == NULL)) {
      abortLog("Error in lang.h: not enough memory");
    } else {
      coucal_value_is_malloc(NewLangStr, 1);
      coucal_value_is_malloc(NewLangStrKeys, 1);
    }
  }

  /* Load master file (list of keys and internal keys) */
  if (!limit_to) {
    const char *mname = "lang.def";
    FILE *fp = fopen(fconcat(catbuff, sizeof(catbuff), path, mname), "rb");

    if (fp) {
      char intkey[8192];
      char key[8192];

      while(!feof(fp)) {
        linput_cpp(fp, intkey, 8000);
        linput_cpp(fp, key, 8000);
        if (strnotempty(intkey) && strnotempty(key)) {
          const char *test = LANGINTKEY(key);

          /* Increment for multiple definitions */
          if (strnotempty(test)) {
            int increment = 0;
            size_t pos = strlen(key);

            do {
              increment++;
              sprintf(key + pos, "%d", increment);
              test = LANGINTKEY(key);
            } while(strnotempty(test));
          }

          if (!strnotempty(test)) { // éviter doublons
            const size_t len = strlen(intkey);
            char *const buff = (char *) malloc(len + 1);

            if (buff) {
              strlcpybuff(buff, intkey, len + 1);
              coucal_add(NewLangStrKeys, key, (intptr_t) buff);
            }
          }
        }                       // if
      }                         // while
      fclose(fp);
    } else {
      return 0;
    }
  }

  /* Language Name? */
  {
    char name[256];

    sprintf(name, "LANGUAGE_%d", selected_lang + 1);
    hashname = LANGINTKEY(name);
  }

  /* Read the key named in limit_to from the selected catalog, not lang.def's
     file name for it. Both come off disk, so every copy clips: the safe_
     helpers abort, and a long line in a catalog must not kill the server. */
  if (limit_to) {
    char wanted[256];

    wanted[0] = '\0';
    strlncatbuff(wanted, limit_to, sizeof(wanted), sizeof(wanted) - 1);
    limit_to[0] = '\0';
    /* Fallback: an empty result ends a caller's loop, so a catalog missing the
       key must not look like the end of the list. */
    if (hashname != NULL)
      strlncatbuff(limit_to, hashname, limit_size, limit_size - 1);
    /* lang.def names a bare basename; a separator in it would leave lang/. */
    if (limit_to[0] != '\0' && strpbrk(hashname, "/\\") == NULL &&
        strstr(hashname, "..") == NULL) {
      char lbasename[1024];
      FILE *fp;

      snprintf(lbasename, sizeof(lbasename), "lang/%s.txt", hashname);
      fp = fopen(fconcat(catbuff, sizeof(catbuff), path, lbasename), "rb");
      if (fp != NULL) {
        char extkey[8192];
        char value[8192];
        hts_boolean found = HTS_FALSE;

        while (!found && !feof(fp)) {
          linput_cpp(fp, extkey, 8000);
          linput_cpp(fp, value, 8000);
          if (strcmp(extkey, wanted) == 0 && value[0] != '\0') {
            limit_to[0] = '\0';
            strlncatbuff(limit_to, value, limit_size, limit_size - 1);
            found = HTS_TRUE;
          }
        }
        fclose(fp);
      }
    }
    return 0;
  }

  /* Error */
  if (!hashname)
    return 0;

  /* Load specific language file */
  {
    int loops;

    // 2nd loop: load undefined strings
    for(loops = 0; loops < 2; loops++) {
      FILE *fp;
      char lbasename[1024];

      {
        char name[256];

        sprintf(name, "LANGUAGE_%d", (loops == 0) ? (selected_lang + 1) : 1);
        hashname = LANGINTKEY(name);
      }
      sprintf(lbasename, "lang/%s.txt", hashname);
      fp = fopen(fconcat(catbuff, sizeof(catbuff), path, lbasename), "rb");
      if (fp) {
        char extkey[8192];
        char value[8192];

        while(!feof(fp)) {
          linput_cpp(fp, extkey, 8000);
          linput_cpp(fp, value, 8000);
          if (strnotempty(extkey) && strnotempty(value)) {
            const char *intkey;

            intkey = LANGINTKEY(extkey);

            if (strnotempty(intkey)) {

              /* Increment for multiple definitions */
              {
                const char *test = LANGSEL(intkey);

                if (strnotempty(test)) {
                  if (loops == 0) {
                    int increment = 0;
                    size_t pos = strlen(extkey);

                    do {
                      increment++;
                      sprintf(extkey + pos, "%d", increment);
                      intkey = LANGINTKEY(extkey);
                      if (strnotempty(intkey))
                        test = LANGSEL(intkey);
                      else
                        test = "";
                    } while(strnotempty(test));
                  } else
                    intkey = "";
                } else {
                  if (loops > 0) {
                  }
                }
              }

              /* Add key */
              if (strnotempty(intkey)) {
                const size_t len = strlen(value);
                char *const buff = (char *) malloc(len + 1);
                if (buff) {
                  conv_printf(value, buff);
                  coucal_add(NewLangStr, intkey, (intptr_t) buff);
                }
              }

            }
          }                     // if
        }                       // while
        fclose(fp);
      } else {
        return 0;
      }
    }
  }

  // Control limit_to
  if (limit_to)
    limit_to[0] = '\0';

  return 1;
}

/* NOTE : also contains the "webhttrack" hack */
static void conv_printf(const char *from, char *to) {
  int i = 0, j = 0, len;

  len = (int) strlen(from);
  while(i < len) {
    switch (from[i]) {
    case '\\':
      i++;
      switch (from[i]) {
      case 'a':
        to[j] = '\a';
        break;
      case 'b':
        to[j] = '\b';
        break;
      case 'f':
        to[j] = '\f';
        break;
      case 'n':
        to[j] = '\n';
        break;
      case 'r':
        to[j] = '\r';
        break;
      case 't':
        to[j] = '\t';
        break;
      case 'v':
        to[j] = '\v';
        break;
      case '\'':
        to[j] = '\'';
        break;
      case '\"':
        to[j] = '\"';
        break;
      case '\\':
        to[j] = '\\';
        break;
      case '?':
        to[j] = '\?';
        break;
      default:
        to[j] = from[i];
        break;
      }
      break;
    default:
      to[j] = from[i];
      break;
    }
    i++;
    j++;
  }
  to[j++] = '\0';
  /* Dirty hack */
  {
    char *a = to;

    while((a = strstr(a, "WinHTTrack"))) {
      a[0] = 'W';
      a[1] = 'e';
      a[2] = 'b';
      a++;
    }
  }
}

static void LANG_DELETE(void) {
  coucal_delete(&NewLangStr);
  coucal_delete(&NewLangStrKeys);
}

// sélection de la langue
static void LANG_INIT(const char *path) { LANG_T(path, 0); }

static int LANG_T(const char *path, int l) {
  if (l >= 0) {
    QLANG_T(l);
    htslang_load(NULL, 0, path);
  }
  return QLANG_T(-1);           // 0=default (english)
}

static int LANG_SEARCH(const char *path, const char *iso) {
  char lang_str[1024];
  int i = 0;
  int curr_lng = LANG_T(path, -1);
  int found = 0;

  do {
    QLANG_T(i);
    strcpybuff(lang_str, "LANGUAGE_ISO");
    htslang_load(lang_str, sizeof(lang_str), path);
    if (strfield(iso, lang_str)) {
      found = i;
    }
    i++;
  } while(strlen(lang_str) > 0);
  QLANG_T(curr_lng);
  return found;
}

static int LANG_LIST(const char *path, char *buffer, size_t buffer_size) {
  char lang_str[1024];
  int i = 0;
  int curr_lng = LANG_T(path, -1);

  buffer[0] = '\0';
  do {
    QLANG_T(i);
    strlcpybuff(lang_str, "LANGUAGE_NAME", sizeof(lang_str));
    htslang_load(lang_str, sizeof(lang_str), path);
    if (strlen(lang_str) > 0) {
      char charset[64];
      char *utf8;

      /* Convert to UTF-8: each catalog names itself in its own charset, and
         the menu carries all of them at once. */
      strlcpybuff(charset, "LANGUAGE_CHARSET", sizeof(charset));
      htslang_load(charset, sizeof(charset), path);
      utf8 = hts_convertStringToUTF8(lang_str, strlen(lang_str), charset);
      if (buffer[0])
        strlcatbuff(buffer, "\n", buffer_size);
      strlcatbuff(buffer, utf8 != NULL ? utf8 : lang_str, buffer_size);
      freet(utf8);
    }
    i++;
  } while(strlen(lang_str) > 0);
  QLANG_T(curr_lng);
  return i;
}

static int QLANG_T(int l) {
  static int lng = 0;

  if (l >= 0) {
    lng = l;
  }
  return lng;                   // 0=default (english)
}

const char* LANGSEL(const char* name) {
  coucal_value value;
  if (NewLangStr != NULL 
      && coucal_read_value(NewLangStr, name, &value) != 0
      && value.ptr != NULL) {
    return (char*) value.ptr;
  } else {
    return "";
  }
}

const char* LANGINTKEY(const char* name) {
  coucal_value value;
  if (NewLangStrKeys != NULL
      && coucal_read_value(NewLangStrKeys, name, &value) != 0
      && value.ptr != NULL) {
    return (char*) value.ptr;
  } else {
    return "";
  }
}

/* *** Various functions *** */

static int recv_bl(T_SOC soc, void *buffer, size_t len, int timeout) {
  if (check_readinput_t(soc, timeout)) {
    int n = 1;
    size_t size = len;
    size_t offs = 0;

    while(n > 0 && size > 0) {
      n = recv(soc, ((char *) buffer) + offs, (int) size, 0);
      if (n > 0) {
        offs += n;
        size -= n;
      }
    }
    return (int) offs;
  }
  return -1;
}

// check if data is available
static int check_readinput(htsblk * r) {
  if (r->soc != INVALID_SOCKET) {
    fd_set fds;                 // poll structures
    struct timeval tv;          // structure for select

    FD_ZERO(&fds);
    FD_SET(r->soc, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    select((int) r->soc + 1, &fds, NULL, NULL, &tv);
    if (FD_ISSET(r->soc, &fds))
      return 1;
    else
      return 0;
  } else
    return 0;
}

/*int strfield(const char* f,const char* s) {
  int r=0;
  while (streql(*f,*s) && ((*f)!=0) && ((*s)!=0)) { f++; s++; r++; }
  if (*s==0)
    return r;
  else
    return 0;
}*/

/* same, except + */
