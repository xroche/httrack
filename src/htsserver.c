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

static void (*pingFun)(void*) = NULL;
static void* pingFunArg = NULL;

/* Extern */
extern void webhttrack_main(char *cmd);
extern void webhttrack_lock(void);
extern void webhttrack_release(void);

static int is_image(const char *file) {
  return strstr(file, ".gif") != NULL
         || strstr(file, ".png") != NULL;
}
static int is_text(const char *file) {
  return ((strstr(file, ".txt") != NULL));
}
static int is_html(const char *file) {
  return ((strstr(file, ".htm") != NULL));
}
static int is_css(const char *file) {
  return ((strstr(file, ".css") != NULL));
}
static int is_js(const char *file) {
  return ((strstr(file, ".js") != NULL));
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
HTS_UNUSED static int htslang_load(char *limit_to, const char *apppath);
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
T_SOC smallserver_init_std(int *port_prox, char *adr_prox, int defaultPort) {
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
      soc = smallserver_init(&try_to_listen_to[i], adr_prox);
      *port_prox = try_to_listen_to[i];
      i++;
    } while((soc == INVALID_SOCKET) && (try_to_listen_to[i] >= 0));
  } else {
    soc = smallserver_init(&defaultPort, adr_prox);
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

static int my_getlocalhost(char *h_loc, size_t size) {
  SOCaddr addr;
  strcpy(h_loc, "localhost");
  if (gethost(h_loc, &addr) == 1) {
    return 0;
  }
  // come on ...
  else {
    strcpy(h_loc, "127.0.0.1");
    return 0;
  }
}

// get local hostname; falls back to "localhost" in case of error 
// always returns 0
static int my_gethostname(char *h_loc, size_t size) {
  h_loc[0] = '\0';
  if (gethostname(h_loc, (int) size) == 0) {    // host name
    SOCaddr addr;
    if (gethost(h_loc, &addr) == 1) {
      return 0;
    } else {
      return my_getlocalhost(h_loc, size);
    }
  } else {
    return my_getlocalhost(h_loc, size);
  }
  return 0;
}

// smallserver_init(&port,&return_host);
T_SOC smallserver_init(int *port, char *adr) {
  T_SOC soc = INVALID_SOCKET;
  char h_loc[256 + 2];

  commandRunning = commandEnd = commandReturn = commandReturnSet =
    commandEndRequested = 0;
  if (commandReturnMsg)
    free(commandReturnMsg);
  commandReturnMsg = NULL;
  if (commandReturnCmdl)
    free(commandReturnCmdl);
  commandReturnCmdl = NULL;

  if (my_gethostname(h_loc, 256) == 0) {   // host name
    SOCaddr server;

    SOCaddr_initany(server);
    if ((soc =
         (T_SOC) socket(SOCaddr_sinfamily(server), SOCK_STREAM,
                        0)) != INVALID_SOCKET) {
      SOCaddr_initport(server, *port);
      if (bind(soc, &SOCaddr_sockaddr(server), SOCaddr_size(server)) == 0) {
        if (listen(soc, 10) >= 0) {
          // SOCaddr_inetntoa(adr, 128, server2);
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
  char catbuff[CATBUFF_SIZE];

  /* Load strings */
  htslang_init();
  if (!htslang_load(NULL, path)) {
    fprintf(stderr, "unable to find lang.def and/or lang/ strings in %s\n",
            path);
    return 0;
  }
  LANG_T(path, 0);

  /* Init various values */
  {
    char pth[1024];

    const char *initOn[] = { "parseall", "Cache", "ka",
      "cookies", "parsejava", "testall", "updhack", "urlhack", "index", NULL
    };
    const initIntElt initInt[] = {
      {"filter", 4},
      {"travel", 2},
      {"travel2", 1},
      {"travel3", 1},
      /* */
      {"connexion", 4},
      /* */
      {"maxrate", 25000},
      /* */
      {"build", 1},
      /* */
      {"checktype", 2},
      {"robots", 3},

      {NULL, 0}
    };
    initStrElt initStr[] = {
      {"user", "Mozilla/4.5 (compatible; HTTrack 3.0x; Windows 98)"},
      {"footer",
       "<!-- Mirrored from %s%s by HTTrack Website Copier/3.x [XR&CO'2014], %s -->"},
      {"url2", "+*.png +*.gif +*.jpg +*.jpeg +*.css +*.js -ad.doubleclick.net/*"},
      {NULL, NULL}
    };
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

    line[0] = '\0';
    buffer[0] = '\0';
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
    if (pingFun != NULL) {
      pingFun(pingFunArg);
    }

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
        // assert(FALSE);
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

      /* check variables */
      if (meth && buffer[0]) {
        char *s = buffer;
        char *e, *f;

        strcatbuff(buffer, "&");
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

      /* Error check */
      {
        intptr_t adr = 0;
        intptr_t adr2 = 0;

        if (coucal_readptr(NewLangList, "sid", &adr)) {
          if (coucal_readptr(NewLangList, "_sid", &adr2)) {
            if (strcmp((char *) adr, (char *) adr2) != 0) {
              meth = 0;
            }
          }
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
          fp = fopen(StringBuff(fspath), "rb");
          if (fp) {
            /* Read file */
            while(!feof(fp)) {
              char *str = line;
              char *pos;

              if (!linput(fp, line, sizeof(line) - 2)) {
                *str = '\0';
              }
              pos = strchr(line, '=');
              if (pos) {
                String escline = STRING_EMPTY;

                *pos++ = '\0';
                if (pos[0] == '0' && pos[1] == '\0')
                  *pos = '\0';  /* 0 => empty */
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
                        (void) ((int)
                                fwrite((void *) adruserprofile, 1, count, fp));
                        fclose(fp);
                      }
                    }
                  }

                  /* Profile */
                  StringClear(tmpbuff);
                  StringCat(tmpbuff, StringBuff(fspath));
                  StringCat(tmpbuff, "/hts-cache/");

                  /* Create minimal directory structure */
                  if (!structcheck(StringBuff(tmpbuff))) {
                    FILE *fp;

                    StringCat(tmpbuff, "winprofile.ini");
                    fp = fopen(StringBuff(tmpbuff), "wb");
                    if (fp != NULL) {
                      int count = (int) strlen((char *) adrw);

                      if ((int) fwrite((void *) adrw, 1, count, fp) == count) {

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
                          webhttrack_main((char *) adr + p);
                        } else {
                          commandRunning = 0;
                          commandEnd = 1;
                        }
                      } else {
                        char tmp[1024];

                        sprintf(tmp,
                                "Unable to write %d bytes in the the init file %s",
                                count, StringBuff(fspath));
                        SET_ERROR(tmp);
                      }
                      fclose(fp);
                    } else {
                      char tmp[1024];

                      sprintf(tmp, "Unable to create the init file %s",
                              StringBuff(fspath));
                      SET_ERROR(tmp);
                    }
                  } else {
                    char tmp[1024];

                    sprintf(tmp,
                            "Unable to create the directory structure in %s",
                            StringBuff(fspath));
                    SET_ERROR(tmp);
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
        int virtualpath = 0;
        char *pos;
        char *url = strchr(line1, ' ');

        if (url && *++url == '/' && (pos = strchr(url, ' ')) && !(*pos = '\0')) {
          char fsfile[1024];
          const char *file;
          FILE *fp;
          char *qpos;

          /* get the URL */
          if (error_redirect == NULL) {
            if ((qpos = strchr(url, '?'))) {
              *qpos = '\0';
            }
            fsfile[0] = '\0';
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
            virtualpath = 1;
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

          if (strlen(path) + strlen(file) + 32 < sizeof(fsfile)) {
            if (strncmp(file, "/website/", 9) != 0) {
              sprintf(fsfile, "%shtml%s", path, file);
            } else {
              intptr_t adr = 0;

              if (coucal_readptr(NewLangList, "projpath", &adr)) {
                sprintf(fsfile, "%s%s", (char *) adr, file + 9);
              }
            }
          }

          if (fsfile[0] && strstr(file, "..") == NULL
              && (fp = fopen(fsfile, "rb"))) {
            char ok[] =
              "HTTP/1.0 200 OK\r\n" "Connection: close\r\n"
              "Server: httrack-small-server\r\n" "Content-type: text/html\r\n"
              "Cache-Control: no-cache, must-revalidate, private\r\n"
              "Pragma: no-cache\r\n";
            char ok_img[] =
              "HTTP/1.0 200 OK\r\n" "Connection: close\r\n"
              "Server: httrack small server\r\n" "Content-type: image/gif\r\n";
            char ok_js[] =
              "HTTP/1.0 200 OK\r\n" "Connection: close\r\n"
              "Server: httrack small server\r\n" "Content-type: text/javascript\r\n";
            char ok_css[] =
              "HTTP/1.0 200 OK\r\n" "Connection: close\r\n"
              "Server: httrack small server\r\n" "Content-type: text/css\r\n";
            char ok_text[] =
              "HTTP/1.0 200 OK\r\n" "Connection: close\r\n"
              "Server: httrack small server\r\n" "Content-type: text/plain\r\n";
            char ok_unknown[] =
              "HTTP/1.0 200 OK\r\n" "Connection: close\r\n"
              "Server: httrack small server\r\n" "Content-type: application/octet-stream\r\n";

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
              {
                char tmp[256];

                if (strlen(file) < sizeof(tmp) - 32) {
                  sprintf(tmp, "Location: %s\r\n", newfile);
                  StringMemcat(headers, tmp, strlen(tmp));
                }
              }
              coucal_write(NewLangList, "redirect", (intptr_t) NULL);
            } else if (is_html(file)) {
              int outputmode = 0;

              StringMemcat(headers, ok, sizeof(ok) - 1);
              while(!feof(fp)) {
                char *str = line;
                int prevlen = (int) StringLength(output);
                int nocr = 0;

                if (!linput(fp, line, sizeof(line) - 2)) {
                  *str = '\0';
                }
                if (*str && str[strlen(str) - 1] == '\\') {
                  nocr = 1;
                  str[strlen(str) - 1] = '\0';
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

                    name[0] = '\0';
                    strncatbuff(name, str, n);

                    if (strncmp(name, "/*", 2) == 0) {
                      /* comments */
                    } else if ((p = strfield(name, "html:"))) {
                      name += p;
                      format = 1;
                    } else if ((p = strfield(name, "list:"))) {
                      name += p;
                      format = 2;
                    } else if ((p = strfield(name, "liststr:"))) {
                      name += p;
                      format = -2;
                    } else if ((p = strfield(name, "file-exists:"))) {
                      char *pos2;

                      name += p;
                      format = 0;
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
                          if (rpath[0]) {
                            if (rpath[strlen(rpath) - 1] == '/') {
                              rpath[strlen(rpath) - 1] = '\0';  /* note: patching stored (inhash) value */
                            }
                          }
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
                    /*
                       test:<if exist>
                       test:<if ==0>:<if ==1>:<if == 2>..
                       ztest:<if == 0 || !exist>:<if == 1>:<if == 2>..
                     */
                    else if ((p = strfield(name, "test:"))
                             || (p = strfield(name, "ztest:"))) {
                      intptr_t adr = 0;
                      char *pos2;
                      int ztest = (name[0] == 'z');

                      langstr = "";
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
                    if (langstr && outputmode != -1) {
                      switch (format) {
                      case 0:
                        {
                          const char *a = langstr;

                          while(*a) {
                            if (a[0] == '\\' && isxdigit(a[1])
                                && isxdigit(a[2])) {
                              int n;
                              char c;

                              if (sscanf(a + 1, "%x", &n) == 1) {
                                c = (char) n;
                                StringMemcat(output, &c, 1);
                              }
                              a += 2;
                            } else if (outputmode && a[0] == '<') {
                              StringCat(output, "&lt;");
                            } else if (outputmode && a[0] == '>') {
                              StringCat(output, "&gt;");
                            } else if (outputmode && a[0] == '&') {
                              StringCat(output, "&amp;");
                            } else if (outputmode && a[0] == '\'') {
                              StringCat(output, "&#39;");
                            } else if (outputmode == 3 && a[0] == ' ') {
                              StringCat(output, "%20");
                            } else if (outputmode >= 2
                                       && ((unsigned char) a[0]) < 32) {
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
                      default:
                        if (*langstr) {
                          int id = 1;
                          const char *fstr = langstr;

                          StringClear(tmpbuff);
                          if (format == 2) {
                            StringCat(output, "<option value=1>");
                          } else if (format == -2) {
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
                                char tmp[32];

                                sprintf(tmp, "%d", ++id);
                                StringCat(output, StringBuff(tmpbuff));
                                StringCat(output, "</option>\r\n");
                                StringCat(output, "<option value=");
                                StringCat(output, tmp);
                                if (listDefault == id) {
                                  StringCat(output, " selected");
                                }
                                StringCat(output, ">");
                              }
                              StringClear(tmpbuff);
                              break;
                            case '<':
                              StringCat(tmpbuff, "&lt;");
                              break;
                            case '>':
                              StringCat(tmpbuff, "&gt;");
                              break;
                            case '&':
                              StringCat(tmpbuff, "&amp;");
                              break;
                            case '\'':
                              StringCat(tmpbuff, "&#39;");
                              break;
                            default:
                              StringMemcat(tmpbuff, fstr, 1);
                              break;
                            }
                            fstr++;
                          }
                          if (format == 2) {
                            StringCat(output, StringBuff(tmpbuff));
                            StringCat(output, "</option>");
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
              if (is_text(file)) {
                StringMemcat(headers, ok_text, sizeof(ok_text) - 1);
              } else if (is_js(file)) {
                StringMemcat(headers, ok_js, sizeof(ok_js) - 1);
              } else if (is_css(file)) {
                StringMemcat(headers, ok_css, sizeof(ok_css) - 1);
              } else if (is_image(file)) {
                StringMemcat(headers, ok_img, sizeof(ok_img) - 1);
              } else {
                StringMemcat(headers, ok_unknown, sizeof(ok_unknown) - 1);
              }
              while(!feof(fp)) {
                int n = (int) fread(line, 1, sizeof(line) - 2, fp);

                if (n > 0) {
                  StringMemcat(output, line, n);
                }
              }
            }
            fclose(fp);
          } else if (strcmp(file, "/ping") == 0
                     || strncmp(file, "/ping?", 6) == 0) {
            char error_hdr[] =
              "HTTP/1.0 200 Pong\r\n" "Server: httrack small server\r\n"
              "Content-type: text/html\r\n";

            StringCat(headers, error_hdr);
          } else {
            char error_hdr[] =
              "HTTP/1.0 404 Not Found\r\n" "Server: httrack small server\r\n"
              "Content-type: text/html\r\n";
            char error[] = "Page not found.\r\n";

            StringCat(headers, error_hdr);
            StringCat(output, error);
            //assert(file == NULL);
          }
        }
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
      if ((send(soc_c, StringBuff(headers), (int) StringLength(headers), 0) !=
           StringLength(headers))
          || ((meth == 1)
              && (send(soc_c, StringBuff(output), (int) StringLength(output), 0)
                  != StringLength(output)))
        ) {
#ifdef _DEBUG
        //assert(FALSE);
#endif
      }
    } else {
#ifdef _DEBUG
      // assert(FALSE);
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

  StringFree(headers);
  StringFree(output);
  StringFree(tmpbuff);
  StringFree(tmpbuff2);
  StringFree(fspath);

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

void smallserver_setpinghandler(void (*fun)(void*), void*arg) {
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

static int htslang_load(char *limit_to, const char *path) {
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

          if (!strnotempty(test)) {     // éviter doublons
            // conv_printf(key,key);
            const size_t len = strlen(intkey);
            char *const buff = (char *) malloc(len + 1);

            if (buff) {
              strcpybuff(buff, intkey);
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

  /* Get only language name */
  if (limit_to) {
    if (hashname)
      strcpybuff(limit_to, hashname);
    else
      strcpybuff(limit_to, "???");
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
                    //err_msg += intkey;
                    //err_msg += " ";
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
static void LANG_INIT(const char *path) {
  //CWinApp* pApp = AfxGetApp();
  //if (pApp) {
  /* pApp->GetProfileInt("Language","IntId",0); */
  LANG_T(path, 0 /*pApp->GetProfileInt("Language","IntId",0) */ );
  //}
}

static int LANG_T(const char *path, int l) {
  if (l >= 0) {
    QLANG_T(l);
    htslang_load(NULL, path);
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
    htslang_load(lang_str, path);
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
    strlcpybuff(lang_str, "LANGUAGE_NAME", buffer_size);
    htslang_load(lang_str, path);
    if (strlen(lang_str) > 0) {
      if (buffer[0])
        strcatbuff(buffer, "\n");
      strlcatbuff(buffer, lang_str, buffer_size);
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
