/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2015 Xavier Roche and other contributors

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
/* File: basic FTP protocol manager                             */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

// Gestion protocole ftp
// Version .05 (01/2000)

#include "htsftp.h"

#include "htscore.h"
#include "htsthread.h"
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

// ftp mode passif
// #if HTS_INET6==0
#define FTP_PASV 1
// #else
// no passive mode for v6
// #define FTP_PASV 0
// #endif

#define FTP_DEBUG 0
//#define FORK_DEBUG 0

#if USE_BEGINTHREAD

void back_launch_ftp(void *pP) {
  FTPDownloadStruct *pStruct = (FTPDownloadStruct *) pP;

  if (pStruct == NULL)
    return;

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

  /* Delete structure */
  free(pP);

  /* Uninitialize */
  hts_uninit();
  return;
}

// lancer en back
void launch_ftp(FTPDownloadStruct * params) {
  // DOS
#if FTP_DEBUG
  printf("[Launching main ftp thread]\n");
#endif
  hts_newthread(back_launch_ftp, (void *) params);
}

#else
#error No more supported
#endif

// pour l'arrêt du ftp
#ifdef _WIN32
#define _T_SOC_close(soc)  closesocket(soc); soc=INVALID_SOCKET;
#else
#define _T_SOC_close(soc)  close(soc); soc=INVALID_SOCKET;
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

// la véritable fonction une fois lancées les routines thread/fork
int run_launch_ftp(FTPDownloadStruct * pStruct) {
  lien_back *back = pStruct->pBack;
  httrackp *opt = pStruct->pOpt;
  char user[256] = "anonymous";
  char pass[256] = "user@";
  char line_retr[2048];
  int port = 21;

#if FTP_PASV
  int port_pasv = 0;
#endif
  char BIGSTK adr_ip[1024];
  char *adr, *real_adr;
  const char *ftp_filename = "";
  int timeout = 300;            // timeout
  int timeout_onfly = 8;        // attente réponse supplémentaire
  int transfer_list = 0;        // directory
  int rest_understood = 0;      // rest command understood

  //
  T_SOC soc_ctl = INVALID_SOCKET;
  T_SOC soc_servdat = INVALID_SOCKET;
  T_SOC soc_dat = INVALID_SOCKET;
  SOCaddr server_data;

  //
  line_retr[0] = adr_ip[0] = '\0';

  timeout = 300;

  // effacer
  strcpybuff(back->r.msg, "");
  back->r.statuscode = 0;
  back->r.size = 0;

  // récupérer user et pass si présents, et sauter user:id@ dans adr
  real_adr = strchr(back->url_adr, ':');
  if (real_adr)
    real_adr++;
  else
    real_adr = back->url_adr;
  while(*real_adr == '/')
    real_adr++;                 // sauter /
  if ((adr = jump_identification(real_adr)) != real_adr) {      // user
    int i = -1;

    pass[0] = '\0';
    do {
      i++;
      user[i] = real_adr[i];
    } while((real_adr[i] != ':') && (real_adr[i]));
    user[i] = '\0';
    if (real_adr[i] == ':') {   // pass
      int j = -1;

      i++;                      // oui on saute aussi le :
      do {
        j++;
        pass[j] = real_adr[i + j];
      } while(((&real_adr[i + j + 1]) < adr) && (real_adr[i + j]));
      pass[j] = '\0';
    }
  }
  // Calculer RETR <nom>
  {
    char *a;

#if 0
    a = back->url_fil + strlen(back->url_fil) - 1;
    while((a > back->url_fil) && (*a != '/'))
      a--;
    if (*a != '/') {
      a = NULL;
    }
#else
    a = back->url_fil;
#endif
    if (a != NULL && *a != '\0') {
#if 0
      a++;                      // sauter /
#endif
      ftp_filename = a;
      if (strnotempty(a)) {
        char catbuff[CATBUFF_SIZE];
        char *ua = unescape_http(catbuff, sizeof(catbuff), a);
        int len_a = (int) strlen(ua);

        if (len_a > 0 && ua[len_a - 1] == '/') {        /* obviously a directory listing */
          transfer_list = 1;
          snprintf(line_retr, sizeof(line_retr), "LIST -A %s", ua);
        } else if ((strchr(ua, ' '))
                   || (strchr(ua, '\"'))
                   || (strchr(ua, '\''))
          ) {
          snprintf(line_retr, sizeof(line_retr), "RETR \"%s\"", ua);
        } else {                /* Regular one */
          snprintf(line_retr, sizeof(line_retr), "RETR %s", ua);
        }
      } else {
        transfer_list = 1;
        snprintf(line_retr, sizeof(line_retr), "LIST -A");
      }
    } else {
      strcpybuff(back->r.msg, "Unexpected PORT error");
      // back->status=STATUS_FTP_READY;    // fini
      back->r.statuscode = STATUSCODE_INVALID;
    }
  }

#if FTP_DEBUG
  printf("Connecting to %s...\n", adr);
#endif

  // connexion
  {
    SOCaddr server;
    char *a;
    char _adr[256];
    const char *error = "unknown error";

    _adr[0] = '\0';
    //T_SOC soc_ctl;
    // effacer structure
    memset(&server, 0, sizeof(server));

    // port
    a = strchr(adr, ':');       // port
    if (a) {
      sscanf(a + 1, "%d", &port);
      strncatbuff(_adr, adr, (int) (a - adr));
    } else
      strcpybuff(_adr, adr);

    // récupérer adresse résolue
    strcpybuff(back->info, "host name");
    if (hts_dns_resolve2(opt, _adr, &server, &error) == NULL) {
      snprintf(back->r.msg, sizeof(back->r.msg),
               "Unable to get server's address: %s", error);
      // back->status=STATUS_FTP_READY;    // fini
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
      // back->status=STATUS_FTP_READY;    // fini
      back->r.statuscode = STATUSCODE_INVALID;
      _HALT_FTP return 0;
    }

    SOCaddr_initport(server, port);
    // server.sin_port = htons((unsigned short int) port);

    // connexion (bloquante, on est en thread)
    strcpybuff(back->info, "connect");

    if (connect(soc_ctl, &SOCaddr_sockaddr(server), SOCaddr_size(server)) != 0) {
      strcpybuff(back->r.msg, "Unable to connect to the server");
      // back->status=STATUS_FTP_READY;    // fini
      back->r.statuscode = STATUSCODE_INVALID;
      _HALT_FTP return 0;
#ifdef _WIN32
    }
#else
    }
#endif
    _CHECK_HALT_FTP;

    {
      char BIGSTK line[1024];

      // envoi du login

      // --USER--
      get_ftp_line(soc_ctl, line, sizeof(line), timeout);     // en tête
      _CHECK_HALT_FTP;

      if (line[0] == '2') {     // ok, connecté
        strcpybuff(back->info, "login: user");
        snprintf(line, sizeof(line), "USER %s", user);
        send_line(soc_ctl, line);
        get_ftp_line(soc_ctl, line, sizeof(line), timeout);
        _CHECK_HALT_FTP;
        if ((line[0] == '3') || (line[0] == '2')) {
          // --PASS--
          if (line[0] == '3') {
            strcpybuff(back->info, "login: pass");
            snprintf(line, sizeof(line), "PASS %s", pass);
            send_line(soc_ctl, line);
            get_ftp_line(soc_ctl, line, sizeof(line), timeout);
            _CHECK_HALT_FTP;
          }
          if (line[0] == '2') { // ok
            send_line(soc_ctl, "TYPE I");
            get_ftp_line(soc_ctl, line, sizeof(line), timeout);
            _CHECK_HALT_FTP;
            if (line[0] == '2') {
              // ok
            } else {
              strcpybuff(back->r.msg, "TYPE I error");
              // back->status=STATUS_FTP_READY;    // fini
              back->r.statuscode = STATUSCODE_INVALID;
            }
#if 0
            // --CWD--
            char *a;

            a = back->url_fil + strlen(back->url_fil) - 1;
            while((a > back->url_fil) && (*a != '/'))
              a--;
            if (*a == '/') {    // ok repéré
              char BIGSTK target[1024];

              target[0] = '\0';
              strncatbuff(target, back->url_fil, (int) (a - back->url_fil));
              if (strnotempty(target) == 0)
                strcatbuff(target, "/");
              strcpybuff(back->info, "cwd");
              snprintf(line, sizeof(line), "CWD %s", target);
              send_line(soc_ctl, line);
              get_ftp_line(soc_ctl, line, sizeof(line), timeout);
              _CHECK_HALT_FTP;
              if (line[0] == '2') {
                send_line(soc_ctl, "TYPE I");
                get_ftp_line(soc_ctl, line, sizeof(line), timeout);
                _CHECK_HALT_FTP;
                if (line[0] == '2') {
                  // ok..
                } else {
                  strcpybuff(back->r.msg, "TYPE I error");
                  // back->status=STATUS_FTP_READY;    // fini
                  back->r.statuscode = STATUSCODE_INVALID;
                }
              } else {
                snprintf(back->r.msg, sizeof(back->r.msg), "CWD error: %s", linejmp(line));
                // back->status=STATUS_FTP_READY;    // fini
                back->r.statuscode = STATUSCODE_INVALID;
              }                 // sinon on est prêts
            } else {
              strcpybuff(back->r.msg, "Unexpected ftp error");
              // back->status=STATUS_FTP_READY;    // fini
              back->r.statuscode = STATUSCODE_INVALID;
            }
#endif

          } else {
            snprintf(back->r.msg, sizeof(back->r.msg), "Bad password: %s", linejmp(line));
            // back->status=STATUS_FTP_READY;    // fini
            back->r.statuscode = STATUSCODE_INVALID;
          }
        } else {
          snprintf(back->r.msg, sizeof(back->r.msg), "Bad user name: %s", linejmp(line));
          // back->status=STATUS_FTP_READY;    // fini
          back->r.statuscode = STATUSCODE_INVALID;
        }
      } else {
        snprintf(back->r.msg, sizeof(back->r.msg), "Connection refused: %s", linejmp(line));
        // back->status=STATUS_FTP_READY;    // fini
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
          get_ftp_line(soc_ctl, line, sizeof(line), timeout);
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
            snprintf(back->r.msg, sizeof(back->r.msg), "PASV incorrect: %s", linejmp(line));
            // back->status=STATUS_FTP_READY;    // fini
            back->r.statuscode = STATUSCODE_INVALID;
          }                     // sinon on est prêts
        } else {
          /*
           * try epsv (ipv6) *
           */
          strcpybuff(back->info, "pasv");
          snprintf(line, sizeof(line), "EPSV");
          send_line(soc_ctl, line);
          get_ftp_line(soc_ctl, line, sizeof(line), timeout);
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
              snprintf(back->r.msg, sizeof(back->r.msg), "EPSV incorrect: %s", linejmp(line));
              // back->status=STATUS_FTP_READY;    // fini
              back->r.statuscode = STATUSCODE_INVALID;
            }
          } else {
            snprintf(back->r.msg, sizeof(back->r.msg), "PASV/EPSV error: %s", linejmp(line));
            // back->status=STATUS_FTP_READY;    // fini
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
            if (!transfer_list) {
              char catbuff[CATBUFF_SIZE];
              char *ua = unescape_http(catbuff, sizeof(catbuff), ftp_filename);

              if ((strchr(ua, ' '))
                  || (strchr(ua, '\"'))
                  || (strchr(ua, '\''))
                ) {
                snprintf(line, sizeof(line), "SIZE \"%s\"", ua);
              } else {
                snprintf(line, sizeof(line), "SIZE %s", ua);
              }

              // SIZE?
              strcpybuff(back->info, "size");
              send_line(soc_ctl, line);
              get_ftp_line(soc_ctl, line, sizeof(line), timeout);
              _CHECK_HALT_FTP;
              if (line[0] == '2') {     // SIZE compris, ALORS tester REST (sinon pas tester: cf probleme des txt.gz decompresses a la volee)
                char *szstr = strchr(line, ' ');

                if (szstr) {
                  LLint size = 0;

                  szstr++;
                  if (sscanf(szstr, LLintP, &size) == 1) {
                    back->r.totalsize = size;
                  }
                }
                // REST?
                if (fexist(back->url_sav) && (transfer_list == 0)) {
                  strcpybuff(back->info, "rest");
                  snprintf(line, sizeof(line), "REST " LLintP, (LLint) fsize(back->url_sav));
                  send_line(soc_ctl, line);
                  get_ftp_line(soc_ctl, line, sizeof(line), timeout);
                  _CHECK_HALT_FTP;
                  if ((line[0] == '3') || (line[0] == '2')) {   // ok
                    rest_understood = 1;
                  }             // sinon tant pis 
                }
              }                 // sinon tant pis 
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
          int server_size = sizeof(server);
          const char *error = "unknown error";

          // effacer structure
          memset(&server, 0, sizeof(server));

          // infos
          strcpybuff(back->info, "resolv");

          // résoudre
          if (adr_ip[0]) {
            hts_dns_resolve2(opt, adr_ip, &server, &error);
          } else {
            SOCaddr_copy_SOCaddr(server, server_data);
          }

          // infos
          strcpybuff(back->info, "cnxdata");
#if FTP_DEBUG
          printf("Data: Connecting to %s:%d...\n", adr_ip, port_pasv);
#endif
          if (server_size > 0) {
            // socket
            soc_dat = (T_SOC) socket(SOCaddr_sinfamily(server), SOCK_STREAM, 0);
            if (soc_dat != INVALID_SOCKET) {
              // structure: connexion au domaine internet, port 80 (ou autre)
              SOCaddr_initport(server, port_pasv);
              if (connect(soc_dat, &SOCaddr_sockaddr(server), SOCaddr_size(server)) == 0) {
                strcpybuff(back->info, "retr");
                strcpybuff(line, line_retr);
                send_line(soc_ctl, line);
                get_ftp_line(soc_ctl, line, sizeof(line), timeout);
                _CHECK_HALT_FTP;
                if (line[0] == '1') {
                  // OK
                } else {
                  deletesoc(soc_dat);
                  soc_dat = INVALID_SOCKET;
                  //
                  snprintf(back->r.msg, sizeof(back->r.msg), "RETR command errror: %s",
                          linejmp(line));
                  // back->status=STATUS_FTP_READY;    // fini
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
                // back->status=STATUS_FTP_READY;    // fini
                back->r.statuscode = STATUSCODE_INVALID;
              }                 // sinon on est prêts
            } else {
              strcpybuff(back->r.msg, "Unable to create a socket");
              // back->status=STATUS_FTP_READY;    // fini
              back->r.statuscode = STATUSCODE_INVALID;
            }                   // sinon on est prêts
          } else {
            snprintf(back->r.msg, sizeof(back->r.msg),
                     "Unable to resolve IP %s: %s", adr_ip, error);
            // back->status=STATUS_FTP_READY;    // fini
            back->r.statuscode = STATUSCODE_INVALID;
          }                     // sinon on est prêts
        } else {
          snprintf(back->r.msg, sizeof(back->r.msg), "PASV incorrect: %s", linejmp(line));
          // back->status=STATUS_FTP_READY;    // fini
          back->r.statuscode = STATUSCODE_INVALID;
        }                       // sinon on est prêts
#else
        //T_SOC soc_servdat;
        strcpybuff(back->info, "listening");
        if ((soc_servdat = get_datasocket(line, sizeof(line))) != INVALID_SOCKET) {
          _CHECK_HALT_FTP;
          send_line(soc_ctl, line);     // envoi du RETR
          get_ftp_line(soc_ctl, line, sizeof(line), timeout);
          _CHECK_HALT_FTP;
          if (line[0] == '2') { // ok
            strcpybuff(back->info, "retr");
            strcpybuff(line, line_retr);
            send_line(soc_ctl, line);
            get_ftp_line(soc_ctl, line, sizeof(line), timeout);
            _CHECK_HALT_FTP;
            if (line[0] == '1') {
              //T_SOC soc_dat;
              if ((soc_dat = accept(soc_servdat, NULL, NULL)) == INVALID_SOCKET) {
                strcpybuff(back->r.msg, "Unable to accept connection");
                // back->status=STATUS_FTP_READY;    // fini
                back->r.statuscode = STATUSCODE_INVALID;
              }
            } else {
              snprintf(back->r.msg, sizeof(back->r.msg), "RETR command errror: %s", linejmp(line));
              // back->status=STATUS_FTP_READY;    // fini
              back->r.statuscode = STATUSCODE_INVALID;
            }
          } else {
            snprintf(back->r.msg, sizeof(back->r.msg), "PORT command error: %s", linejmp(line));
            // back->status=STATUS_FTP_READY;    // fini
            back->r.statuscode = STATUSCODE_INVALID;
          }
#ifdef _WIN32
          closesocket(soc_servdat);
#else
          close(soc_servdat);
#endif
        } else {
          strcpybuff(back->r.msg, "Unable to listen to a port");
          // back->status=STATUS_FTP_READY;    // fini
          back->r.statuscode = STATUSCODE_INVALID;
        }
#endif

        //
        // Ok, connexion initiée
        //
        if (soc_dat != INVALID_SOCKET) {
          if (rest_understood) {        // REST envoyée et comprise
            file_notify(opt, back->url_adr, back->url_fil, back->url_sav, 0, 1,
                        0);
            back->r.fp = fileappend(&opt->state.strc, back->url_sav);
          } else {
            file_notify(opt, back->url_adr, back->url_fil, back->url_sav, 1, 1,
                        0);
            back->r.fp = filecreate(&opt->state.strc, back->url_sav);
          }
          strcpybuff(back->info, "receiving");
          if (back->r.fp != NULL) {
            char BIGSTK buff[1024];
            int len = 1;
            int read_len = 1024;

            //HTS_TOTAL_RECV_CHECK(read_len);         // Diminuer au besoin si trop de données reçues

            while((len > 0) && (!stop_ftp(back))) {
              // attendre les données
              len = 1;          // pas d'erreur pour le moment
              switch (wait_socket_receive(soc_dat, timeout)) {
              case -1:
                strcpybuff(back->r.msg, "FTP read error");
                // back->status=STATUS_FTP_READY;    // fini
                back->r.statuscode = STATUSCODE_INVALID;
                len = 0;        // fin
                break;
              case 0:
                snprintf(back->r.msg, sizeof(back->r.msg), "Time out (%d)", timeout);
                // back->status=STATUS_FTP_READY;    // fini
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
                    if ((INTsys) fwrite(buff, 1, (INTsys) len, back->r.fp) !=
                        len) {
                      /*
                         int fcheck;
                         if ((fcheck=check_fatal_io_errno())) {
                         opt->state.exit_xh=-1;
                         }
                       */
                      strcpybuff(back->r.msg, "Write error");
                      // back->status=STATUS_FTP_READY;    // fini
                      back->r.statuscode = STATUSCODE_INVALID;
                      len = 0;  // error
                    }
                  } else {
                    strcpybuff(back->r.msg, "Unexpected write error");
                    // back->status=STATUS_FTP_READY;    // fini
                    back->r.statuscode = STATUSCODE_INVALID;
                  }
                } else {        // Erreur ou terminé
                  // back->status=STATUS_FTP_READY;    // fini
                  back->r.statuscode = 0;
                  if (back->r.totalsize > 0
                      && back->r.size != back->r.totalsize) {
                    back->r.statuscode = STATUSCODE_INVALID;
                    strcpybuff(back->r.msg, "FTP file incomplete");
                  }
                }
                read_len = 1024;
                //HTS_TOTAL_RECV_CHECK(read_len);         // Diminuer au besoin si trop de données reçues
              }
            }
            if (back->r.fp) {
              fclose(back->r.fp);
              back->r.fp = NULL;
            }
          } else {
            strcpybuff(back->r.msg, "Unable to write file");
            // back->status=STATUS_FTP_READY;    // fini
            back->r.statuscode = STATUSCODE_INVALID;
          }
#ifdef _WIN32
          closesocket(soc_dat);
#else
          close(soc_dat);
#endif

          // 226 Transfer complete?
          if (back->r.statuscode != -1) {
            if (wait_socket_receive(soc_ctl, timeout_onfly) > 0) {
              // récupérer 226 transfer complete
              get_ftp_line(soc_ctl, line, sizeof(line), timeout);
              if (line[0] == '2') {     // OK
                strcpybuff(back->r.msg, "OK");
                // back->status=STATUS_FTP_READY;    // fini
                back->r.statuscode = HTTP_OK;
              } else {
                snprintf(back->r.msg, sizeof(back->r.msg), "RETR incorrect: %s", linejmp(line));
                // back->status=STATUS_FTP_READY;    // fini
                back->r.statuscode = STATUSCODE_INVALID;
              }
            } else {
              strcpybuff(back->r.msg, "FTP read error");
              // back->status=STATUS_FTP_READY;    // fini
              back->r.statuscode = STATUSCODE_INVALID;
            }
          }

        }

      }

    }

    _CHECK_HALT_FTP;
    strcpybuff(back->info, "quit");
    send_line(soc_ctl, "QUIT"); // bye bye
    get_ftp_line(soc_ctl, NULL, 0, timeout);
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
  // back->status=STATUS_FTP_READY;    // fini
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
  char BIGSTK line[1024];

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
  snprintf(line, sizeof(line), "%s\x0d\x0a", data);
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

int get_ftp_line(T_SOC soc, char *ptrline, size_t line_size, int timeout) {
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
    switch (wait_socket_receive(soc, timeout)) {
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

    //HTS_TOTAL_RECV_CHECK(dummy);     // Diminuer au besoin si trop de données reçues
    switch (recv(soc, &b, 1, 0)) {
      //case 0: break;    // pas encore --> erreur (on attend)!
    case 1:
      HTS_STAT.HTS_TOTAL_RECV += 1;     // compter flux entrant
      if ((b != 10) && (b != 13))
        data[i++] = b;
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
int wait_socket_receive(T_SOC soc, int timeout) {
  // attendre les données
  TStamp ltime = time_local();
  int r;

#if FTP_DEBUG
  printf("\x0dWaiting for data ");
  fflush(stdout);
#endif
  while((!(r = check_socket(soc)))
        && (((int) ((TStamp) (time_local() - ltime))) < timeout)) {
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
    // back->status=STATUS_FTP_READY;    // fini
    back->r.statuscode = STATUSCODE_INVALID;
    return 1;
  }
  return 0;
}
