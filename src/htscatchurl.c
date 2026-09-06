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
/* File: URL catch .h                                           */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

// Fichier intercepteur d'URL .c

/* specific definitions */
/* specific definitions */
#include "htsbase.h"
#include "htsnet.h"
#include "htslib.h"
#include "htscore.h"
#include <fcntl.h>
#ifdef _WIN32
#else
#include <arpa/inet.h>
#endif
/* END specific definitions */

/* définitions globales */
#include "htsglobal.h"

/* htslib */
/*#include "htslib.h"*/

/* catch url */
#include "htscatchurl.h"

// URL Link catcher

// 0- Init the URL catcher with standard port

// catch_url_init(&port,&return_host);
HTSEXT_API T_SOC catch_url_init_std(int *port_prox, char *adr_prox) {
  T_SOC soc;
  int try_to_listen_to[] = {8080, 3128, 80, 81, 82, 8081, 3129, 0, -1};
  int i = 0;

  do {
    soc = catch_url_init(&try_to_listen_to[i], adr_prox);
    *port_prox = try_to_listen_to[i];
    i++;
  } while((soc == INVALID_SOCKET) && (try_to_listen_to[i] >= 0));
  return soc;
}

// 1- Init the URL catcher

// catch_url_init(&port,&return_host);
HTSEXT_API T_SOC catch_url_init(int *port, /* 128 bytes */ char *adr) {
  T_SOC soc = INVALID_SOCKET;
  char h_loc[256];

  if (gethostname(h_loc, sizeof(h_loc)) == 0) {   // host name
    SOCaddr server;
    if (hts_dns_resolve_nocache(h_loc, &server) != NULL) {   // notre host
      if ((soc =
           (T_SOC) socket(SOCaddr_sinfamily(server), SOCK_STREAM,
                          0)) != INVALID_SOCKET) {
        SOCaddr_initport(server, *port);
        if (bind(soc, &SOCaddr_sockaddr(server), SOCaddr_size(server)) == 0) {
          SOCaddr server2;
          SOClen len = SOCaddr_capacity(server2);

          if (getsockname(soc, &SOCaddr_sockaddr(server2), &len) == 0) {
            *port = ntohs(SOCaddr_sinport(server));     // récupérer port
            if (listen(soc, 1) >= 0) {
              SOCaddr_inetntoa(adr, 128, server2);
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

/* Accumulate the request headers into "data" (CATCH_URL_DATA_SIZE bytes, on
   entry holding the request line) and feed them to treathead(). A clipped
   header block is not the request the browser sent, so a line or block that
   does not fit fails the capture rather than replay a prefix. Every iteration
   charges "used", which is what ends the loop. */
static catch_url_status catch_url_headers(T_SOC soc, char *data,
                                          htsblk *blkretour) {
  char BIGSTK line[CATCH_URL_LINE_SIZE];
  size_t used = strlen(data); /* invariant: used < CATCH_URL_DATA_SIZE */

  do {
    size_t need;

    /* a line socinput() had to clip is not the header the browser sent, and
       skipping one charges "used" nothing: a peer sending only such lines
       would hold this loop open for free */
    if (socinput(soc, line, (int) sizeof(line)))
      return CATCH_URL_ERR_HEADER;
    treathead(NULL, NULL, NULL, blkretour, line); // traiter
    need = strlen(line) + 2;                      // the CRLF this line carries
    if (need >= CATCH_URL_DATA_SIZE - used)
      return CATCH_URL_ERR_BLOCK;
    strlcatbuff(data, line, CATCH_URL_DATA_SIZE);
    strlcatbuff(data, "\r\n", CATCH_URL_DATA_SIZE);
    used += need;
    /* the empty line ending the block was appended just above, which is why no
       final CRLF is added afterwards */
  } while (strnotempty(line));
  return CATCH_URL_OK;
}

/* Contract in htscatchurl.h. */
const char *catch_url_strerror(catch_url_status status) {
  switch (status) {
  case CATCH_URL_OK:
    return "the request was captured";
  case CATCH_URL_ERR_URL:
    return "the browser asked for a relative URL, not an absolute one";
  case CATCH_URL_ERR_HEADER:
    return "one request header line was over " CATCH_URL_STR(
        CATCH_URL_LINE_MAX) " bytes";
  case CATCH_URL_ERR_BLOCK:
    return "the request headers were over " CATCH_URL_STR(
        CATCH_URL_DATA_SIZE) " bytes in total";
  case CATCH_URL_ERR_REQUEST:
    break;
  }
  return "the browser sent no usable request line";
}

// 2 - Wait for URL

// catch_url
// returns 0 if error
// url: buffer where URL must be stored - or ip:port in case of failure
// data: 32Kb
HTSEXT_API hts_boolean catch_url(T_SOC soc, char *url, char *method,
                                 char *data) {
  return catch_url_capture(soc, url, method, data) == CATCH_URL_OK;
}

/* Contract in htscatchurl.h. */
catch_url_status catch_url_capture(T_SOC soc, char *url, char *method,
                                   char *data) {
  catch_url_status status = CATCH_URL_ERR_REQUEST;

  // connexion (accept)
  if (soc != INVALID_SOCKET) {
    T_SOC soc2;

    while((soc2 = (T_SOC) accept(soc, NULL, NULL)) == INVALID_SOCKET) ;
    /*
       #ifdef _WIN32
       closesocket(soc);
       #else
       close(soc);
       #endif
     */
    soc = soc2;
    /* INFOS */
    {
      SOCaddr server2;
      SOClen len = SOCaddr_capacity(server2);

      if (getpeername(soc, &SOCaddr_sockaddr(server2), &len) == 0) {
        char dot[256 + 2];

        SOCaddr_inetntoa(dot, sizeof(dot), server2);
        sprintf(url, "%s:%d", dot, ntohs(SOCaddr_sinport(server2)));
      }
    }
    /* INFOS */

    // réception
    if (soc != INVALID_SOCKET) {
      char BIGSTK line[CATCH_URL_LINE_SIZE];
      char protocol[256];

      line[0] = protocol[0] = '\0';
      // a clipped request-line names a URL the browser did not ask for
      if (!socinput(soc, line, (int) sizeof(line)) && strnotempty(line)) {
        /* widths bound the caller buffers: method[32], url[HTS_URLMAXSIZE*2],
           protocol[256] */
        if (sscanf(line, "%31s %2047s %255s", method, url, protocol) == 3) {
          lien_adrfil af;

          // méthode en majuscule
          size_t i;
          int r = 0;

          af.adr[0] = af.fil[0] = '\0';
          //
          for(i = 0; method[i] != '\0'; i++) {
            if ((method[i] >= 'a') && (method[i] <= 'z'))
              method[i] -= ('a' - 'A');
          }
          // adresse du lien
          if (ident_url_absolute(url, &af) >= 0) {
            // Traitement des en-têtes
            char BIGSTK loc[HTS_LOCATION_SIZE];
            htsblk blkretour;

            hts_init_htsblk(&blkretour);
            blkretour.location = loc;   // si non nul, contiendra l'adresse véritable en cas de moved xx
            // Lire en têtes restants
            sprintf(data, "%s %s %s\r\n", method, af.fil, protocol);
            status = catch_url_headers(soc, data, &blkretour);
            if (status == CATCH_URL_OK) {
              if (blkretour.totalsize > 0) {
                int pos = (int) strlen(data);
                /* the headers already took part of data[], so bound the body by
                   the room they left rather than by a constant */
                int len = (int) min(blkretour.totalsize,
                                    (LLint) (CATCH_URL_DATA_SIZE - 1 - pos));

                // Copier le reste (post éventuel)
                while ((len > 0) &&
                       ((r = recv(soc, (char *) data + pos, len, 0)) > 0)) {
                  pos += r;
                  len -= r;
                  data[pos] = '\0'; // terminer par NULL
                }
              }
              // Envoyer page
              sprintf(line, CATCH_RESPONSE);
              send(soc, line, (int) strlen(line), 0);
            } else {
              data[0] = '\0'; // no prefix handed back
            }
          } else {
            status = CATCH_URL_ERR_URL;
          }
        }
      } // sinon erreur
    }
  }
  if (soc != INVALID_SOCKET) {
#ifdef _WIN32
    closesocket(soc);
    /*
       WSACleanup();
     */
#else
    close(soc);
#endif
  }
  return status;
}

// Read one line off a socket; HTS_TRUE if it did not fit "s".
hts_boolean socinput(T_SOC soc, char *s, int max) {
  hts_boolean cut = HTS_FALSE;
  int c;
  int j = 0;

  do {
    unsigned char b;

    if (recv(soc, (char *) &b, 1, 0) == 1) {
      c = b;
      switch (c) {
      case 13:
        break;                  // sauter CR
      case 10:
        c = -1;
        break;
      case 9:
      case 12:
        break;                  // sauter ces caractères
      default:
        if (j < max - 1)
          s[j++] = (char) c;
        else
          cut = HTS_TRUE; // keep draining: the tail is not a new line
        break;
      }
    } else
      c = EOF;
  } while ((c != -1) && (c != EOF));
  s[j] = '\0';
  return cut;
}
