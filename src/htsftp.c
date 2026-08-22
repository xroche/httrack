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
/* File: basic FTP protocol manager                             */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

// Gestion protocole ftp
// Version .05 (01/2000)

#include "htsftp.h"

#include "htscore.h"
#include "htsio.h"
#include "htsthread.h"

#include <limits.h>

#ifdef _WIN32
#else
//inet_ntoa
#include <arpa/inet.h>
#endif

#ifdef _WIN32
#ifndef __cplusplus
// DOS
#include <process.h>            /* _beginthread, _endthread */
#endif
#endif

/* Passive FTP; overridable so CI can compile the active branch (#1091). */
#ifndef FTP_PASV
#define FTP_PASV 1
#endif

#define FTP_DEBUG 0

#if USE_BEGINTHREAD

/* Live FTP workers. Each writes through its backlog slot and reads opt for its
   whole run, so neither may be freed while the list is not empty (#1051).
   Registration happens at spawn, not at thread entry: a worker that has not
   been scheduled yet must already hold the list against a teardown. */
static FTPDownloadStruct *ftp_workers = NULL;
static htsmutex ftp_workers_mutex = HTSMUTEX_INIT;

static void ftp_worker_register(FTPDownloadStruct *worker) {
  hts_mutexlock(&ftp_workers_mutex);
  worker->pNext = ftp_workers;
  ftp_workers = worker;
  hts_mutexrelease(&ftp_workers_mutex);
}

static void ftp_worker_unregister(FTPDownloadStruct *worker) {
  FTPDownloadStruct **prev;

  hts_mutexlock(&ftp_workers_mutex);
  for (prev = &ftp_workers; *prev != NULL; prev = &(*prev)->pNext) {
    if (*prev == worker) {
      *prev = worker->pNext;
      break;
    }
  }
  hts_mutexrelease(&ftp_workers_mutex);
}

void ftp_stop_workers(void) {
  int wait;

  do {
    FTPDownloadStruct *worker;

    hts_mutexlock(&ftp_workers_mutex);
    /* Idempotent, and re-raised each pass so a worker registered meanwhile is
       not missed. A registered worker's slot is still live by construction:
       back_delete_all() drains before back_free(). */
    for (worker = ftp_workers; worker != NULL; worker = worker->pNext)
      worker->pBack->stop_ftp = HTS_TRUE;
    wait = ftp_workers != NULL;
    hts_mutexrelease(&ftp_workers_mutex);
    if (wait)
      Sleep(100);
  } while (wait);
}

void back_launch_ftp(void *pP) {
  FTPDownloadStruct *pStruct = (FTPDownloadStruct *) pP;

  if (pStruct == NULL) {
#if FTP_DEBUG
    printf("[ftp error: no args]\n");
#endif
    return;
  }

  /* Initialize */
  hts_init();

  // lancer ftp
#if FTP_DEBUG
  printf("[Launching main ftp routine]\n");
#endif
  run_launch_ftp(pStruct);
  // prêt
  pStruct->pBack->status = STATUS_FTP_READY;

  /* Uninitialize */
  hts_uninit();

  /* The status store above was this worker's last read of the slot and of opt;
     deregistering lets the engine free both. */
  ftp_worker_unregister(pStruct);

  /* Delete structure */
  free(pP);
  return;
}

// lancer en back
void launch_ftp(FTPDownloadStruct * params) {
  // DOS
#if FTP_DEBUG
  printf("[Launching main ftp thread]\n");
#endif
  ftp_worker_register(params);
  if (hts_newthread(back_launch_ftp, (void *) params) != 0) {
    /* Nobody would ever reap a slot left in STATUS_FTP_TRANSFER, and
       ftp_stop_workers() would never return. */
    ftp_worker_unregister(params);
    strcpybuff(params->pBack->r.msg, "Unable to launch FTP thread");
    params->pBack->r.statuscode = STATUSCODE_INVALID;
    params->pBack->status = STATUS_FTP_READY;
    free(params);
  }
}

#else
#error No more supported
#endif

// pour l'arrêt du ftp
#ifdef _WIN32
#define _T_SOC_close(soc)  do { closesocket(soc); soc=INVALID_SOCKET; } while(0)
#else
#define _T_SOC_close(soc)  do { close(soc); soc=INVALID_SOCKET; } while(0)
#endif
#define _HALT_FTP { \
  if ( soc_ctl     != INVALID_SOCKET ) _T_SOC_close(soc_ctl); \
  if ( soc_servdat != INVALID_SOCKET ) _T_SOC_close(soc_servdat); \
  if ( soc_dat     != INVALID_SOCKET ) _T_SOC_close(soc_dat); \
}
#define _CHECK_HALT_FTP \
  if (stop_ftp(back)) { \
  _HALT_FTP \
  return 0; \
  }

/* Split a hostile-URL "user[:pass]@" prefix (see htsftp.h). */
hts_boolean ftp_split_userpass(const char *src, const char *end, char *user,
                               size_t user_size, char *pass, size_t pass_size) {
  size_t len = 0, user_len, pass_len;
  const char *colon;

  assertf(user_size > 0 && pass_size > 0); /* the size-1 math underflows on 0 */
  assertf(end > src);                      /* end is one past the '@' */

  user[0] = pass[0] = '\0'; // fail safe for a caller that ignores the result
  while (len < (size_t) (end - src) - 1 && src[len] != '\0')
    len++;
  colon = memchr(src, ':', len);
  user_len = colon != NULL ? (size_t) (colon - src) : len;
  pass_len = colon != NULL ? len - user_len - 1 : 0;
  /* a clipped name is another account's, so refuse rather than log in as it */
  if (user_len >= user_size || pass_len >= pass_size)
    return HTS_FALSE;
  memcpy(user, src, user_len);
  user[user_len] = '\0';
  if (pass_len != 0)
    memcpy(pass, colon + 1, pass_len);
  pass[pass_len] = '\0';
  return HTS_TRUE;
}

/* Build "<verb> <path>" (see htsftp.h). */
hts_boolean ftp_command(char *line, size_t line_size, const char *verb,
                        const char *path) {
  int n;

  /* A leading '-' would reach a server that shells out to ls as a flag. */
  if (path[0] == '-' || strchr(path, ' ') != NULL ||
      strchr(path, '\"') != NULL || strchr(path, '\'') != NULL)
    n = snprintf(line, line_size, "%s \"%s\"", verb, path);
  else
    n = snprintf(line, line_size, "%s %s", verb, path);
  if (n < 0 || (size_t) n >= line_size) {
    line[0] = '\0'; // fail safe for a caller that ignores the result
    return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* MDTM reply "213 YYYYMMDDHHMMSS[.frac]" (RFC 3659, UTC) into tm_time. */
static hts_boolean ftp_parse_mdtm(const char *line, struct tm *tm_time) {
  int year, mon, mday, hour, min, sec;

  if (sscanf(line, "213 %4d%2d%2d%2d%2d%2d", &year, &mon, &mday, &hour, &min,
             &sec) != 6)
    return HTS_FALSE;
  if (year < 1900 || mon < 1 || mon > 12 || mday < 1 || mday > 31 || hour < 0 ||
      hour > 23 || min < 0 || min > 59 || sec < 0 || sec > 60)
    return HTS_FALSE;
  memset(tm_time, 0, sizeof(*tm_time));
  tm_time->tm_year = year - 1900;
  tm_time->tm_mon = mon - 1;
  tm_time->tm_mday = mday;
  tm_time->tm_hour = hour;
  tm_time->tm_min = min;
  tm_time->tm_sec = sec;
  tm_time->tm_isdst = 0;
  return HTS_TRUE;
}

/* Whether the local copy can still be a prefix of the remote file. FTP has no
   conditional fetch, so an unprovable answer has to be no (#823). */
static hts_boolean ftp_may_resume(httrackp *opt, const lien_back *back,
                                  time_t remote_mtime) {
  const time_t local_mtime = get_filetime(back->url_sav);

  if (back->r.totalsize <= back->range_req_size) {
    hts_log_print(opt, LOG_DEBUG,
                  "FTP: not resuming %s%s, the local copy is not shorter",
                  back->url_adr, back->url_fil);
    return HTS_FALSE;
  }
  /* Equality, not ordering: a copy this code did not stamp carries a local
     clock time newer than any MDTM, which "not newer" would wave through. */
  if (remote_mtime == (time_t) -1 || local_mtime == (time_t) -1 ||
      remote_mtime != local_mtime) {
    hts_log_print(opt, LOG_DEBUG,
                  "FTP: not resuming %s%s, the copy is not dated as the remote",
                  back->url_adr, back->url_fil);
    return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* Clip a wait to what is left of --max-time: back_wait() can't abort this slot
   the way it aborts an HTTP one. */
static int ftp_wait_left(const httrackp *opt, int timeout) {
  if (opt != NULL && opt->maxtime > 0) {
    const TStamp left =
        (TStamp) opt->maxtime - (time_local() - HTS_STAT.stat_timestart);

    if (left < (TStamp) timeout)
      timeout = left > 0 ? (int) left : 0;
  }
  return timeout;
}

/* Connect honoring the stop flag, --timeout and --max-time: a blocking
   connect() to a black hole sits for the kernel's own two minutes (#1059). */
static hts_boolean ftp_connect(lien_back *back, T_SOC soc, SOCaddr *server,
                               int timeout, const httrackp *opt) {
  const TStamp started = time_local();

  timeout = ftp_wait_left(opt, timeout);
  if (!socket_set_nonblocking(soc, HTS_TRUE))
    return HTS_FALSE;
  if (connect(soc, &SOCaddr_sockaddr(*server), SOCaddr_size(*server)) != 0) {
#ifdef _WIN32
    if (WSAGetLastError() != WSAEWOULDBLOCK)
#else
    if (errno != EINPROGRESS && errno != EINTR)
#endif
      return HTS_FALSE;
    /* state.stop counts here, unlike in the waits below: a mirror the user
       stopped has nothing to finish on a connection never established. */
    while (check_socket_connect(soc) == 0) {
      if (back->stop_ftp || (opt != NULL && opt->state.stop) ||
          (int) (time_local() - started) >= timeout)
        return HTS_FALSE;
      Sleep(100);
    }
    if (connect_socket_error(soc) != 0)
      return HTS_FALSE;
  }
  /* the rest of the session reads and writes this socket blocking */
  return socket_set_nonblocking(soc, HTS_FALSE);
}

#if !FTP_PASV
/* The active-mode counterpart: an unbounded accept() waits for a data
   connection the server may never make (#1074). */
static T_SOC ftp_accept(lien_back *back, T_SOC soc, int timeout,
                        const httrackp *opt) {
  const TStamp started = time_local();
  T_SOC soc_dat;

  timeout = ftp_wait_left(opt, timeout);
  if (!socket_set_nonblocking(soc, HTS_TRUE))
    return INVALID_SOCKET;
  /* state.stop counts as in ftp_connect: nothing is established yet */
  while (check_socket(soc) == 0) {
    if (back->stop_ftp || (opt != NULL && opt->state.stop) ||
        (int) (time_local() - started) >= timeout)
      return INVALID_SOCKET;
    Sleep(100);
  }
  soc_dat = accept(soc, NULL, NULL);
  /* BSD hands the listener's non-blocking flag down to the accepted socket,
     which the transfer below reads blocking. */
  if (soc_dat != INVALID_SOCKET &&
      !socket_set_nonblocking(soc_dat, HTS_FALSE)) {
    deletesoc(soc_dat);
    return INVALID_SOCKET;
  }
  return soc_dat;
}
#endif

/* Resolve, likewise: the shared resolver bounds itself by opt->timeout alone
   and takes no stop flag. */
static SOCaddr *ftp_dns_resolve(httrackp *opt, lien_back *back,
                                const char *host, SOCaddr *addr, int timeout,
                                const char **error) {
  const int left = ftp_wait_left(opt, timeout);

  SOCaddr_clear(*addr);
  if (left <= 0) {
    if (error != NULL)
      *error = "mirror deadline reached";
    return NULL;
  }
  if (hts_dns_resolve_all_bounded(opt, host, addr, 1, left, &back->stop_ftp,
                                  error) > 0)
    return SOCaddr_is_valid(*addr) ? addr : NULL;
  return NULL;
}

// la véritable fonction une fois lancées les routines thread/fork
int run_launch_ftp(FTPDownloadStruct * pStruct) {
  lien_back *back = pStruct->pBack;
  httrackp *opt = pStruct->pOpt;
  char user[256] = "anonymous";
  char pass[256] = "user@";
  char line_retr[FTP_LINE_SIZE];
  int port = 21;

#if FTP_PASV
  int port_pasv = 0;
#endif
  char BIGSTK adr_ip[1024];
  char *adr, *real_adr;
  char BIGSTK ftp_path[CATBUFF_SIZE]; // the decoded URL path, screened once
  /* opt->timeout == 0 means unlimited; use INT_MAX so 0 doesn't make the read
     return instantly. */
  const int timeout = opt->timeout > 0 ? opt->timeout : INT_MAX;
  int timeout_onfly = 8;        // attente réponse supplémentaire
  int transfer_list = 0;        // directory
  int rest_understood = 0;      // rest command understood

  //
  T_SOC soc_ctl = INVALID_SOCKET;
  T_SOC soc_servdat = INVALID_SOCKET;
  T_SOC soc_dat = INVALID_SOCKET;
  SOCaddr server_data;

  //
  line_retr[0] = adr_ip[0] = ftp_path[0] = '\0';

  // effacer
  strcpybuff(back->r.msg, "");
  back->r.statuscode = 0;
  back->r.size = 0;
  back->r.lastmodified[0] = '\0'; // a retry must not stamp the previous MDTM

  // récupérer user et pass si présents, et sauter user:id@ dans adr
  real_adr = strchr(back->url_adr, ':');
  if (real_adr)
    real_adr++;
  else
    real_adr = back->url_adr;
  while(*real_adr == '/')
    real_adr++;                 // sauter /
  if ((adr = jump_identification(real_adr)) != real_adr) {      // user
    if (!ftp_split_userpass_buf(real_adr, adr, user, pass)) {
      strcpybuff(back->r.msg, "FTP user name or password too long");
      back->r.statuscode = STATUSCODE_INVALID;
      _HALT_FTP return 0;
    }
  }
  // Calculer RETR <nom>
  {
    char *a;

    a = back->url_fil;
    if (a != NULL && *a != '\0') {
      if (strnotempty(a)) {
        const size_t len_a =
            strlen(unescape_http(ftp_path, sizeof(ftp_path), a));
        hts_boolean fits;

        if (len_a > 0 &&
            ftp_path[len_a - 1] == '/') { /* obviously a directory listing */
          transfer_list = 1;
          fits = ftp_command_line(line_retr, "LIST -A", ftp_path);
        } else {
          fits = ftp_command_line(line_retr, "RETR", ftp_path);
        }
        if (!fits) {
          strcpybuff(back->r.msg, "FTP path too long");
          back->r.statuscode = STATUSCODE_INVALID;
          _HALT_FTP return 0;
        }
      } else {
        transfer_list = 1;
        snprintf(line_retr, sizeof(line_retr), "LIST -A");
      }
    } else {
      strcpybuff(back->r.msg, "Unexpected PORT error");
      back->r.statuscode = STATUSCODE_INVALID;
    }
  }

  // fail here, or send_line() drops the command and the reply read times out.
  // Every command below is a literal verb plus one of these three.
  if (!hts_is_control_free(user) || !hts_is_control_free(pass) ||
      !hts_is_control_free(ftp_path)) {
    strcpybuff(back->r.msg, "Invalid control character in FTP URL");
    back->r.statuscode = STATUSCODE_INVALID;
    _HALT_FTP return 0;
  }

#if FTP_DEBUG
  printf("Connecting to %s...\n", adr);
#endif

  // connexion
  {
    SOCaddr server;
    char *a;
    char _adr[256];
    size_t adr_len;
    const char *error = "unknown error";

    _adr[0] = '\0';
    //T_SOC soc_ctl;
    // effacer structure
    memset(&server, 0, sizeof(server));

    // port
    a = strchr(adr, ':');       // port
    if (a) {
      // folding a nonsense port into 1..65535 fetches one the link never named;
      // an empty "host:" just means the default (#614)
      if (a[1] != '\0' && !hts_parse_url_port(a + 1, &port)) {
        htsblk_failf(&back->r, "Invalid port: %s", a + 1);
        back->r.statuscode = STATUSCODE_INVALID; // permanent, unlike a DNS miss
        _HALT_FTP return 0;
      }
      adr_len = (size_t) (a - adr);
    } else
      adr_len = strlen(adr);
    // no resolvable name is this long, and clipping would query another host
    if (adr_len >= sizeof(_adr)) {
      htsblk_failf(&back->r, "Host name too long");
      back->r.statuscode = STATUSCODE_INVALID;
      _HALT_FTP return 0;
    }
    strncatbuff(_adr, adr, (int) adr_len);

    // récupérer adresse résolue
    strcpybuff(back->info, "host name");
    if (ftp_dns_resolve(opt, back, _adr, &server, timeout, &error) == NULL) {
      htsblk_failf(&back->r, "Unable to get server's address: %s", error);
      back->r.statuscode = STATUSCODE_NON_FATAL;
      _HALT_FTP return 0;
    }
    _CHECK_HALT_FTP;

    // copie adresse pour cnx data
    SOCaddr_copy_SOCaddr(server_data, server);

    // créer ("attachement") une socket (point d'accès) internet,en flot
    soc_ctl = (T_SOC) socket(SOCaddr_sinfamily(server), SOCK_STREAM, 0);
    if (soc_ctl == INVALID_SOCKET) {
      strcpybuff(back->r.msg, "Unable to create a socket");
      back->r.statuscode = STATUSCODE_INVALID;
      _HALT_FTP return 0;
    }

    SOCaddr_initport(server, port);

    strcpybuff(back->info, "connect");
    hts_log_print(opt, LOG_DEBUG, "FTP: connecting to %s:%d", _adr, port);

    if (!ftp_connect(back, soc_ctl, &server, timeout, opt)) {
      strcpybuff(back->r.msg, "Unable to connect to the server");
      back->r.statuscode = STATUSCODE_INVALID;
      _HALT_FTP return 0;
    }
    _CHECK_HALT_FTP;

    {
      char BIGSTK line[FTP_LINE_SIZE];

      /* line_retr is copied here verbatim; a narrower line[] would clip it. */
      HTS_COMPILE_ASSERT(sizeof(line) == sizeof(line_retr));

      // envoi du login

      // --USER--
      get_ftp_line(back, soc_ctl, line, sizeof(line), timeout, opt); // en tête
      _CHECK_HALT_FTP;

      if (line[0] == '2') {     // ok, connecté
        strcpybuff(back->info, "login: user");
        snprintf(line, sizeof(line), "USER %s", user);
        send_line(soc_ctl, line);
        get_ftp_line(back, soc_ctl, line, sizeof(line), timeout, opt);
        _CHECK_HALT_FTP;
        if ((line[0] == '3') || (line[0] == '2')) {
          // --PASS--
          if (line[0] == '3') {
            strcpybuff(back->info, "login: pass");
            snprintf(line, sizeof(line), "PASS %s", pass);
            send_line(soc_ctl, line);
            get_ftp_line(back, soc_ctl, line, sizeof(line), timeout, opt);
            _CHECK_HALT_FTP;
          }
          if (line[0] == '2') { // ok
            send_line(soc_ctl, "TYPE I");
            get_ftp_line(back, soc_ctl, line, sizeof(line), timeout, opt);
            _CHECK_HALT_FTP;
            if (line[0] == '2') {
              // ok
            } else {
              strcpybuff(back->r.msg, "TYPE I error");
              back->r.statuscode = STATUSCODE_INVALID;
            }

          } else {
            htsblk_failf(&back->r, "Bad password: %s", linejmp(line));
            back->r.statuscode = STATUSCODE_INVALID;
          }
        } else {
          htsblk_failf(&back->r, "Bad user name: %s", linejmp(line));
          back->r.statuscode = STATUSCODE_INVALID;
        }
      } else {
        htsblk_failf(&back->r, "Connection refused: %s", linejmp(line));
        back->r.statuscode = STATUSCODE_INVALID;
      }

      // ok, si on est prêts on écoute sur un port et on demande la sauce
      if (back->r.statuscode != -1) {

        //
        // Pré-REST
        //
#if FTP_PASV
        if (SOCaddr_getproto(server) == '1') {
          strcpybuff(back->info, "pasv");
          snprintf(line, sizeof(line), "PASV");
          send_line(soc_ctl, line);
          get_ftp_line(back, soc_ctl, line, sizeof(line), timeout, opt);
        } else {                /* ipv6 */
          line[0] = '\0';
        }
        _CHECK_HALT_FTP;
        if (line[0] == '2') {
          char *a, *b, *c;

          a = strchr(line, '(');        // exemple: 227 Entering Passive Mode (123,45,67,89,177,27)
          if (a) {

            // -- analyse de l'adresse IP et du port --
            a++;
            b = strchr(a, ',');
            if (b)
              b = strchr(b + 1, ',');
            if (b)
              b = strchr(b + 1, ',');
            if (b)
              b = strchr(b + 1, ',');
            c = a;
            while((c = strchr(c, ',')))
              *c = '.';         // remplacer , par .
            if (b)
              *b = '\0';
            //
            strcpybuff(adr_ip, a);      // copier adresse ip
            //
            if (b) {
              a = b + 1;        // début du port
              b = strchr(a, '.');
              if (b) {
                int n1, n2;

                //
                *b = '\0';
                b++;
                c = strchr(b, ')');
                if (c) {
                  *c = '\0';
                  if ((sscanf(a, "%d", &n1) == 1) && (sscanf(b, "%d", &n2) == 1)
                      && (strlen(adr_ip) <= 16)) {
                    port_pasv = n2 + (n1 << 8);
                  }
                } else {
                  deletesoc(soc_dat);
                  soc_dat = INVALID_SOCKET;
                }               // sinon on est prêts
              }
            }
            // -- fin analyse de l'adresse IP et du port --
          } else {
            htsblk_failf(&back->r, "PASV incorrect: %s", linejmp(line));
            back->r.statuscode = STATUSCODE_INVALID;
          }                     // sinon on est prêts
        } else {
          /*
           * try epsv (ipv6) *
           */
          strcpybuff(back->info, "pasv");
          snprintf(line, sizeof(line), "EPSV");
          send_line(soc_ctl, line);
          get_ftp_line(back, soc_ctl, line, sizeof(line), timeout, opt);
          _CHECK_HALT_FTP;
          if (line[0] == '2') { /* got it */
            char *a;

            a = strchr(line, '(');      // exemple: 229 Entering Extended Passive Mode (|||6446|)
            if ((a != NULL)
                && (*a == '(')
                && (*(a + 1))
                && (*(a + 1) == *(a + 2)) && (*(a + 1) == *(a + 3))
                && (isdigit(*(a + 4)))
                && (*(a + 5))
              ) {
              unsigned int n1 = 0;

              if (sscanf(a + 4, "%d", &n1) == 1) {
                if ((n1 < 65535) && (n1 > 0)) {
                  port_pasv = n1;
                }
              }
            } else {
              htsblk_failf(&back->r, "EPSV incorrect: %s", linejmp(line));
              back->r.statuscode = STATUSCODE_INVALID;
            }
          } else {
            htsblk_failf(&back->r, "PASV/EPSV error: %s", linejmp(line));
            back->r.statuscode = STATUSCODE_INVALID;
          }                     // sinon on est prêts
        }
#else
        // rien à faire avant
#endif

#if FTP_PASV
        if (port_pasv) {
#endif
          // SIZE
          if (back->r.statuscode != -1) {
            // a clipped probe would size and date a different file
            if (!transfer_list && ftp_command_line(line, "SIZE", ftp_path)) {
              // SIZE?
              strcpybuff(back->info, "size");
              send_line(soc_ctl, line);
              get_ftp_line(back, soc_ctl, line, sizeof(line), timeout, opt);
              _CHECK_HALT_FTP;
              if (line[0] == '2') {     // SIZE compris, ALORS tester REST (sinon pas tester: cf probleme des txt.gz decompresses a la volee)
                char *szstr = strchr(line, ' ');
                time_t remote_mtime = (time_t) -1;
                struct tm remote_tm;

                if (szstr) {
                  LLint size = 0;

                  szstr++;
                  if (sscanf(szstr, LLintP, &size) == 1) {
                    back->r.totalsize = size;
                  }
                }

                // MDTM?
                if (ftp_command_line(line, "MDTM", ftp_path)) {
                  strcpybuff(back->info, "mdtm");
                  send_line(soc_ctl, line);
                  get_ftp_line(back, soc_ctl, line, sizeof(line), timeout, opt);
                  _CHECK_HALT_FTP;
                  if (ftp_parse_mdtm(line, &remote_tm)) {
                    char date[256];

                    time_rfc822(date, &remote_tm);
                    /* Stamp the mirror as the HTTP path does, so a later pass
                       compares server-clock times instead of crossing clocks.
                     */
                    back->r.lastmodified[0] = '\0';
                    strlncatbuff(back->r.lastmodified, date,
                                 sizeof(back->r.lastmodified),
                                 sizeof(back->r.lastmodified) - 1);
                    remote_mtime = timegm(&remote_tm);
                  }
                }

                /* Only over a copy back_add() judged partial: on --update every
                   mirrored file exists, and resuming a complete one splices the
                   old body into the new (#798). */
                if (back->range_req_size > 0 && (transfer_list == 0) &&
                    ftp_may_resume(opt, back, remote_mtime)) {
                  strcpybuff(back->info, "rest");
                  snprintf(line, sizeof(line), "REST " LLintP,
                           (LLint) back->range_req_size);
                  send_line(soc_ctl, line);
                  get_ftp_line(back, soc_ctl, line, sizeof(line), timeout, opt);
                  _CHECK_HALT_FTP;
                  if ((line[0] == '3') || (line[0] == '2')) {   // ok
                    rest_understood = 1;
                  } // else never mind
                }
              } // sinon tant pis
            }
          }
#if FTP_PASV
        }
#endif

        //
        // Post-REST
        //
#if FTP_PASV
        // Ok, se connecter
        if (port_pasv) {
          SOCaddr server;
          hts_boolean resolved;
          const char *error = "unknown error";

          // effacer structure
          memset(&server, 0, sizeof(server));

          // infos
          strcpybuff(back->info, "resolv");

          // résoudre
          if (adr_ip[0]) {
            resolved = ftp_dns_resolve(opt, back, adr_ip, &server, timeout,
                                       &error) != NULL;
          } else {
            SOCaddr_copy_SOCaddr(server, server_data);
            resolved = HTS_TRUE;
          }

          // infos
          strcpybuff(back->info, "cnxdata");
#if FTP_DEBUG
          printf("Data: Connecting to %s:%d...\n", adr_ip, port_pasv);
#endif
          if (resolved) {
            // socket
            soc_dat = (T_SOC) socket(SOCaddr_sinfamily(server), SOCK_STREAM, 0);
            if (soc_dat != INVALID_SOCKET) {
              // structure: connexion au domaine internet, port 80 (ou autre)
              SOCaddr_initport(server, port_pasv);
              if (ftp_connect(back, soc_dat, &server, timeout, opt)) {
                strcpybuff(back->info, "retr");
                // clip, never abort: this line is built from a crawled URL
                line[0] = '\0';
                strlncatbuff(line, line_retr, sizeof(line), sizeof(line) - 1);
                send_line(soc_ctl, line);
                get_ftp_line(back, soc_ctl, line, sizeof(line), timeout, opt);
                _CHECK_HALT_FTP;
                if (line[0] == '1') {
                  // OK
                } else {
                  deletesoc(soc_dat);
                  soc_dat = INVALID_SOCKET;
                  //
                  htsblk_failf(&back->r, "RETR command error: %s",
                               linejmp(line));
                  back->r.statuscode = STATUSCODE_INVALID;
                }               // sinon on est prêts
              } else {
#if FTP_DEBUG
                printf("Data: unable to connect\n");
#endif
                deletesoc(soc_dat);
                soc_dat = INVALID_SOCKET;
                //
                strcpybuff(back->r.msg, "Unable to connect");
                back->r.statuscode = STATUSCODE_INVALID;
              } // sinon on est prêts
            } else {
              strcpybuff(back->r.msg, "Unable to create a socket");
              back->r.statuscode = STATUSCODE_INVALID;
            }                   // sinon on est prêts
          } else {
            htsblk_failf(&back->r, "Unable to resolve IP %s: %s", adr_ip,
                         error);
            back->r.statuscode = STATUSCODE_INVALID;
          } // sinon on est prêts
        } else {
          htsblk_failf(&back->r, "PASV incorrect: %s", linejmp(line));
          back->r.statuscode = STATUSCODE_INVALID;
        }                       // sinon on est prêts
#else
        //T_SOC soc_servdat;
        strcpybuff(back->info, "listening");
        if ((soc_servdat = get_datasocket(line, sizeof(line))) != INVALID_SOCKET) {
          _CHECK_HALT_FTP;
          send_line(soc_ctl, line);     // envoi du RETR
          get_ftp_line(back, soc_ctl, line, sizeof(line), timeout, opt);
          _CHECK_HALT_FTP;
          if (line[0] == '2') { // ok
            strcpybuff(back->info, "retr");
            // clip, never abort: this line is built from a crawled URL
            line[0] = '\0';
            strlncatbuff(line, line_retr, sizeof(line), sizeof(line) - 1);
            send_line(soc_ctl, line);
            get_ftp_line(back, soc_ctl, line, sizeof(line), timeout, opt);
            _CHECK_HALT_FTP;
            if (line[0] == '1') {
              if ((soc_dat = ftp_accept(back, soc_servdat, timeout, opt)) ==
                  INVALID_SOCKET) {
                strcpybuff(back->r.msg, "Unable to accept connection");
                back->r.statuscode = STATUSCODE_INVALID;
              }
            } else {
              htsblk_failf(&back->r, "RETR command error: %s", linejmp(line));
              back->r.statuscode = STATUSCODE_INVALID;
            }
          } else {
            htsblk_failf(&back->r, "PORT command error: %s", linejmp(line));
            back->r.statuscode = STATUSCODE_INVALID;
          }
          /* invalidate, or the _CHECK_HALT_FTP below re-closes a stale fd */
          deletesoc(soc_servdat);
          soc_servdat = INVALID_SOCKET;
        } else {
          strcpybuff(back->r.msg, "Unable to listen to a port");
          back->r.statuscode = STATUSCODE_INVALID;
        }
#endif

        //
        // Ok, connexion initiée
        //
        if (soc_dat != INVALID_SOCKET) {
          if (rest_understood) { // REST sent and understood
            file_notify(opt, back->url_adr, back->url_fil, back->url_sav, 0, 1,
                        0);
            /* The bytes already on disk count too, or the completeness check
               below rejects every resumed transfer (#798). */
            back->r.size = back->range_req_size;
            back->r.fp = fileappend(&opt->state.strc, back->url_sav);
          } else {
            file_notify(opt, back->url_adr, back->url_fil, back->url_sav, 1, 1,
                        0);
            /* Every failure exit below would else leave the mirror truncated
               (#771); the resume branch appends and needs no backup. */
            back_refetch_backup(opt, back);
            back->r.fp = filecreate(&opt->state.strc, back->url_sav);
          }
          strcpybuff(back->info, "receiving");
          if (back->r.fp != NULL) {
            char BIGSTK buff[1024];
            int len = 1;
            int read_len = 1024;

            while((len > 0) && (!stop_ftp(back))) {
              // attendre les données
              len = 1;          // pas d'erreur pour le moment
              switch (wait_socket_receive(back, soc_dat, timeout, opt)) {
              case -1:
                strcpybuff(back->r.msg, "FTP read error");
                back->r.statuscode = STATUSCODE_INVALID;
                len = 0;        // fin
                break;
              case 0:
                /* stop_ftp() names the real reason when it is the stop that
                   cut the wait short. */
                if (!stop_ftp(back))
                  htsblk_failf(&back->r, "Time out (%d)", timeout);
                back->r.statuscode = STATUSCODE_INVALID;
                len = 0;        // fin
                break;
              }

              // réception
              if (len) {
                len = recv(soc_dat, buff, read_len, 0);
                if (len > 0) {
                  back->r.size += len;
                  HTS_STAT.HTS_TOTAL_RECV += len;
                  if (back->r.fp) {
                    if (!hts_fwrite_exact(buff, (size_t) len, back->r.fp)) {
                      /*
                         int fcheck;
                         if ((fcheck=check_fatal_io_errno())) {
                         opt->state.exit_xh=-1;
                         }
                       */
                      strcpybuff(back->r.msg, "Write error");
                      back->r.statuscode = STATUSCODE_INVALID;
                      len = 0;  // error
                    }
                  } else {
                    strcpybuff(back->r.msg, "Unexpected write error");
                    back->r.statuscode = STATUSCODE_INVALID;
                  }
                } else { // Erreur ou terminé
                  back->r.statuscode = 0;
                  if (back->r.totalsize > 0
                      && back->r.size != back->r.totalsize) {
                    back->r.statuscode = STATUSCODE_INVALID;
                    strcpybuff(back->r.msg, "FTP file incomplete");
                  }
                }
                read_len = 1024;
              }
            }
            if (back->r.fp) {
              fclose(back->r.fp);
              back->r.fp = NULL;
            }
            /* back_flush_output() stamps only r.is_write transfers, which FTP
               never is; a partial needs the remote's date too (#823). */
            if (strnotempty(back->r.lastmodified))
              set_filetime_rfc822(back->url_sav, back->r.lastmodified);
          } else {
            strcpybuff(back->r.msg, "Unable to write file");
            back->r.statuscode = STATUSCODE_INVALID;
          }
          /* invalidate, or the _CHECK_HALT_FTP below re-closes a stale fd */
          deletesoc(soc_dat);
          soc_dat = INVALID_SOCKET;

          // 226 Transfer complete?
          if (back->r.statuscode != -1) {
            if (wait_socket_receive(back, soc_ctl, timeout_onfly, opt) > 0) {
              // récupérer 226 transfer complete
              get_ftp_line(back, soc_ctl, line, sizeof(line), timeout, opt);
              if (line[0] == '2') {     // OK
                strcpybuff(back->r.msg, "OK");
                back->r.statuscode = HTTP_OK;
              } else {
                htsblk_failf(&back->r, "RETR incorrect: %s", linejmp(line));
                back->r.statuscode = STATUSCODE_INVALID;
              }
            } else {
              strcpybuff(back->r.msg, "FTP read error");
              back->r.statuscode = STATUSCODE_INVALID;
            }
          }

        }

      }

    }

    _CHECK_HALT_FTP;
    strcpybuff(back->info, "quit");
    send_line(soc_ctl, "QUIT"); // bye bye
    /* A stopped mirror skips the second --timeout window this courtesy reply
       costs on a silent server; its text is discarded anyway (#1096). */
    if (!opt->state.stop)
      get_ftp_line(back, soc_ctl, NULL, 0, timeout, opt);
#ifdef _WIN32
    closesocket(soc_ctl);
#else
    close(soc_ctl);
#endif
  }

  if (back->r.statuscode != -1) {
    back->r.statuscode = HTTP_OK;
    strcpybuff(back->r.msg, "OK");
  }
  return 0;
}

// ouverture d'un port
T_SOC get_datasocket(char *to_send, size_t to_send_size) {
  T_SOC soc = INVALID_SOCKET;
  char h_loc[256 + 2];

  to_send[0] = '\0';
  if (gethostname(h_loc, 256) == 0) {   // host name
    SOCaddr server;

    if (hts_dns_resolve_nocache(h_loc, &server) != NULL) {   // notre host
      if ((soc =
           (T_SOC) socket(SOCaddr_sinfamily(server), SOCK_STREAM,
                          0)) != INVALID_SOCKET) {

        if (bind(soc, &SOCaddr_sockaddr(server), SOCaddr_size(server)) == 0) {
          SOCaddr server2;
          SOClen len = SOCaddr_capacity(server2);

          if (getsockname(soc, &SOCaddr_sockaddr(server2), &len) == 0) {
            // *port=ntohs(server.sin_port);  // récupérer port
            if (listen(soc, 1) >= 0) {
#if HTS_INET6==0
              unsigned short int a, n1, n2;

              // calculer port
              a = SOCaddr_sinport(server2);
              n1 = (a & 0xff);
              n2 = ((a >> 8) & 0xff);
              {
                char dots[256 + 2];
                char dot[256 + 2];
                char *a;

                SOCaddr_inetntoa(dot, 256, server2);
                //
                dots[0] = '\0';
                strncatbuff(dots, dot, 128);
                while((a = strchr(dots, '.')))
                  *a = ',';     // virgules!
                while((a = strchr(dots, ':')))
                  *a = ',';     // virgules!
                snprintf(to_send, to_send_size, "PORT %s,%d,%d", dots, n1, n2);
              }
#else
              /*
                 EPRT |1|132.235.1.2|6275|
                 EPRT |2|1080::8:800:200C:417A|5282|
               */
              {
                char dot[256 + 2];

                SOCaddr_inetntoa(dot, 256, server2);
                snprintf(to_send, to_send_size, "EPRT |%c|%s|%d|",
                         SOCaddr_getproto(server2), dot,
                         SOCaddr_sinport(server2));
              }
#endif

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
  }

  return soc;
}

#if FTP_DEBUG
FILE *dd = NULL;
#endif

// routines de réception/émission
// 0 = ERROR
int send_line(T_SOC soc, const char *data) {
  char BIGSTK line[FTP_LINE_SIZE + 2]; // room for the CRLF of a maximal command
  int n;

  // backstop: the driver fails earlier, but no injected byte reaches the wire
  if (!hts_is_control_free(data))
    return 0;

  if (_DEBUG_HEAD) {
    if (ioinfo) {
      fprintf(ioinfo, "---> %s\x0d\x0a", data);
      fflush(ioinfo);
    }
  }
#if FTP_DEBUG
  if (dd == NULL)
    dd = fopen("toto.txt", "w");
  fprintf(dd, "---> %s\x0d\x0a", data);
  fflush(dd);
  printf("---> %s", data);
  fflush(stdout);
#endif
  // an unterminated command would blend into whatever the server reads next
  n = snprintf(line, sizeof(line), "%s\x0d\x0a", data);
  if (n < 0 || n >= (int) sizeof(line))
    return 0;
  if (check_socket_connect(soc) != 1) {
#if FTP_DEBUG
    printf("!SOC WRITE ERROR\n");
#endif
    return 0;                   // erreur, plus connecté!
  }
#if FTP_DEBUG
  {
    int r = (send(soc, line, strlen(line), 0) == (int) strlen(line));

    printf("%s\x0d\x0a", data);
    fflush(stdout);
    return r;
  }
#else
  return (send(soc, line, (int) strlen(line), 0) == (int) strlen(line));
#endif
}

int get_ftp_line(lien_back *back, T_SOC soc, char *ptrline, size_t line_size,
                 int timeout, const httrackp *opt) {
  char BIGSTK data[1024];
  int i, ok, multiline;

#if FTP_DEBUG
  if (dd == NULL)
    dd = fopen("toto.txt", "w");
#endif

  data[0] = '\0';
  i = ok = multiline = 0;
  data[3] = '\0';
  do {
    char b;

    // vérifier données
    switch (wait_socket_receive(back, soc, timeout, opt)) {
    case -1:                   // erreur de lecture
      if (ptrline)
        snprintf(ptrline, line_size, "500 *read error");
      return 0;
      break;
    case 0:
      if (ptrline)
        snprintf(ptrline, line_size, "500 *read timeout (%d)", timeout);
      return 0;
      break;
    }

    switch (recv(soc, &b, 1, 0)) {
    case 1:
      HTS_STAT.HTS_TOTAL_RECV += 1;     // compter flux entrant
      if ((b != 10) && (b != 13) && (i < (int) sizeof(data) - 1))
        data[i++] = b; // truncate hostile over-long reply lines
      break;
    default:
      if (ptrline)
        snprintf(ptrline, line_size, "500 *read error");
      return 0;                 // error
      break;
    }
    if (((b == 13) || (b == 10)) && (i > 0)) {  // CR/LF
      if ((data[3] == '-')
          || ((multiline) && (!isdigit((unsigned char) data[0])))
        ) {
        data[3] = '\0';
        i = 0;
        multiline = 1;
      } else
        ok = 1;                 // sortir
    }
  } while(!ok);
  data[i++] = '\0';

  if (_DEBUG_HEAD) {
    if (ioinfo) {
      fprintf(ioinfo, "<--- %s\x0d\x0a", data);
      fflush(ioinfo);
    }
  }
#if FTP_DEBUG
  fprintf(dd, "<--- %s\n", data);
  fflush(dd);
  printf("<--- %s\n", data);
#endif
  if (ptrline)
    snprintf(ptrline, line_size, "%s", data);
  return (strnotempty(data));
}

// sauter NNN
char *linejmp(char *line) {
  if (strlen(line) > 4)
    return line + 4;
  else
    return line;
}

// test socket:
// 0 : no data
// 1 : data detected
// -1: error
int check_socket(T_SOC soc) {
  fd_set fds, fds_e;            // poll structures
  struct timeval tv;            // structure for select

  FD_ZERO(&fds);
  FD_ZERO(&fds_e);
  // socket read 
  FD_SET(soc, &fds);
  // socket error
  FD_SET(soc, &fds_e);
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  // poll!     
  select((int) soc + 1, &fds, NULL, &fds_e, &tv);
  if (FD_ISSET(soc, &fds_e)) {  // error detected
    return -1;
  } else if (FD_ISSET(soc, &fds)) {
    return 1;
  }
  return 0;
}

// check if connected
int check_socket_connect(T_SOC soc) {
  fd_set fds, fds_e;            // poll structures
  struct timeval tv;            // structure for select

  FD_ZERO(&fds);
  FD_ZERO(&fds_e);
  // socket write 
  FD_SET(soc, &fds);
  // socket error
  FD_SET(soc, &fds_e);
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  // poll!     
  select((int) soc + 1, NULL, &fds, &fds_e, &tv);
  if (FD_ISSET(soc, &fds_e)) {  // error detected
    return -1;
  } else if (FD_ISSET(soc, &fds)) {
    return 1;
  }
  return 0;
}

// attendre des données
int wait_socket_receive(lien_back *back, T_SOC soc, int timeout,
                        const httrackp *opt) {
  TStamp ltime = time_local();
  int r;

  timeout = ftp_wait_left(opt, timeout);

#if FTP_DEBUG
  printf("\x0dWaiting for data ");
  fflush(stdout);
#endif
  /* A stop raised by the engine ends the wait on the next 100ms tick instead of
     after the full timeout, so teardown does not sit on a silent server. */
  while ((!(r = check_socket(soc))) && (back == NULL || !back->stop_ftp) &&
         (((int) ((TStamp) (time_local() - ltime))) < timeout)) {
    Sleep(100);
#if FTP_DEBUG
    printf(".");
    fflush(stdout);
#endif
  }
#if FTP_DEBUG
  printf("\x0dreturn: %d\x0d", r);
  fflush(stdout);
#endif
  return r;
}

// cancel reçu?
int stop_ftp(lien_back * back) {
  if (back->stop_ftp) {
    strcpybuff(back->r.msg, "Cancelled by User");
    back->r.statuscode = STATUSCODE_INVALID;
    return 1;
  }
  return 0;
}
