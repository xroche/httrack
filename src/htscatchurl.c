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

// Fichier intercepteur d'URL .c

/* specific definitions */
/* specific definitions */
#include "htsbase.h"
#include "htsnet.h"
#include "htsshared.h"
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

/* Read one line off a socket; HTS_TRUE if it did not fit "s". */
static hts_boolean socinput(T_SOC soc, hts_buf s);

// URL Link catcher

/* Contract in httrack-library.h. */
HTSEXT_API T_SOC catch_url_init_std_sized(int *port_prox, char *adr_prox,
                                          size_t adr_size) {
  T_SOC soc;
  int try_to_listen_to[] = {8080, 3128, 80, 81, 82, 8081, 3129, 0, -1};
  int i = 0;

  do {
    soc = catch_url_init(&try_to_listen_to[i], adr_prox, adr_size);
    *port_prox = try_to_listen_to[i];
    i++;
  } while((soc == INVALID_SOCKET) && (try_to_listen_to[i] >= 0));
  return soc;
}

// the 128 bytes the deprecated contract documents
HTSEXT_API T_SOC catch_url_init_std(int *port_prox, char *adr_prox) {
  return catch_url_init_std_sized(port_prox, adr_prox, 128);
}

/* Contract in htslib.h. */
T_SOC catch_url_init(int *port, char *adr, size_t adr_size) {
  SOCaddr server, bound;
  T_SOC soc;

  assertf(adr_size != 0);
  /* Loopback only: the request this proxy captures carries the browser's
     cookies and credentials for the target site. */
  SOCaddr_initloopback(server);
  SOCaddr_initport(server, *port);
  soc = hts_shared_listen(&server, SOCK_STREAM, 1, &bound, NULL);
  if (soc != INVALID_SOCKET) {
    /* only the bound address carries an ephemeral port */
    *port = ntohs(SOCaddr_sinport(bound));
    SOCaddr_inetntoa(adr, adr_size, bound);
  }
  return soc;
}

/* Accumulate the request headers into "data" and feed them to treathead().
   A line or block that does not fit fails. */
static catch_url_status catch_url_headers(T_SOC soc, hts_buf data,
                                          htsblk *blkretour) {
  char BIGSTK line[CATCH_URL_LINE_SIZE];
  size_t used = strlen(data.p); /* invariant: used < data.size */

  do {
    size_t need;

    /* Skipping a clipped line would let a peer hold this loop open for free. */
    if (socinput(soc, HTS_BUF(line)))
      return CATCH_URL_ERR_HEADER;
    treathead(NULL, NULL, NULL, blkretour, line); // traiter
    need = strlen(line) + 2;                      // the CRLF this line carries
    if (need >= data.size - used)
      return CATCH_URL_ERR_BLOCK;
    hts_buf_cat(data, line);
    hts_buf_cat(data, "\r\n");
    used += need;
    /* the empty line ending the block is already appended */
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

/* Contract in httrack-library.h. */
HTSEXT_API hts_boolean catch_url_sized(T_SOC soc, char *url, size_t url_size,
                                       char *method, size_t method_size,
                                       char *data, size_t data_size) {
  return url_size != 0 && method_size != 0 && data_size != 0 &&
         catch_url_capture(soc, hts_buf_at(url, url_size),
                           hts_buf_at(method, method_size),
                           hts_buf_at(data, data_size)) == CATCH_URL_OK;
}

// the sizes the deprecated contract documents
HTSEXT_API hts_boolean catch_url(T_SOC soc, char *url, char *method,
                                 char *data) {
  return catch_url_sized(soc, url, HTS_URLMAXSIZE * 2, method,
                         CATCH_URL_METHOD_SIZE, data, CATCH_URL_DATA_SIZE);
}

/* Contract in htscatchurl.h. */
catch_url_status catch_url_capture(T_SOC soc, hts_buf url, hts_buf method,
                                   hts_buf data) {
  catch_url_status status = CATCH_URL_ERR_REQUEST;

  // connexion (accept)
  if (soc != INVALID_SOCKET) {
    T_SOC soc2;

    while((soc2 = (T_SOC) accept(soc, NULL, NULL)) == INVALID_SOCKET) ;
    socket_set_nosigpipe(soc2);
    soc = soc2;
    /* INFOS */
    {
      SOCaddr server2;
      SOClen len = SOCaddr_capacity(server2);

      if (getpeername(soc, &SOCaddr_sockaddr(server2), &len) == 0) {
        char dot[256 + 2];

        SOCaddr_inetntoa(dot, sizeof(dot), server2);
        hts_buf_printf_clip(url, "%s:%d", dot, ntohs(SOCaddr_sinport(server2)));
      }
    }
    /* INFOS */

    // réception
    if (soc != INVALID_SOCKET) {
      char BIGSTK line[CATCH_URL_LINE_SIZE];
      char BIGSTK wire_url[HTS_URLMAXSIZE * 2];
      char wire_method[CATCH_URL_METHOD_SIZE];
      char protocol[256];

      line[0] = protocol[0] = '\0';
      // a clipped request-line names a URL the browser did not ask for
      if (!socinput(soc, HTS_BUF(line)) && strnotempty(line)) {
        int method_end = 0, url_end = 0;

        // a clipped method or URL names another request: refuse what won't fit
        if (sscanf(line,
                   "%" CATCH_URL_STR(CATCH_URL_METHOD_MAX) "s%n %2047s%n %255s",
                   wire_method, &method_end, wire_url, &url_end,
                   protocol) == 3 &&
            is_realspace(line[method_end]) && is_realspace(line[url_end]) &&
            strlen(wire_method) < method.size && strlen(wire_url) < url.size) {
          lien_adrfil af;

          // méthode en majuscule
          size_t i;
          int r = 0;

          af.adr[0] = af.fil[0] = '\0';
          hts_buf_cpy(method, wire_method);
          hts_buf_cpy(url, wire_url);
          for (i = 0; method.p[i] != '\0'; i++) {
            if ((method.p[i] >= 'a') && (method.p[i] <= 'z'))
              method.p[i] -= ('a' - 'A');
          }
          // adresse du lien
          if (ident_url_absolute(url.p, &af) >= 0) {
            // Traitement des en-têtes
            char BIGSTK loc[HTS_LOCATION_SIZE];
            htsblk blkretour;

            hts_init_htsblk(&blkretour);
            blkretour.location = loc;   // si non nul, contiendra l'adresse véritable en cas de moved xx
            // Lire en têtes restants
            if (!hts_buf_printf(data, "%s %s %s\r\n", method.p, af.fil,
                                protocol))
              status = CATCH_URL_ERR_BLOCK;
            else
              status = catch_url_headers(soc, data, &blkretour);
            if (status == CATCH_URL_OK) {
              if (blkretour.totalsize > 0) {
                int pos = (int) strlen(data.p);
                /* the headers already took part of data[], so bound the body by
                   the room they left rather than by a constant */
                int len = (int) min(blkretour.totalsize,
                                    (LLint) (data.size - 1 - (size_t) pos));

                // Copier le reste (post éventuel)
                while ((len > 0) &&
                       ((r = recv(soc, data.p + pos, len, 0)) > 0)) {
                  pos += r;
                  len -= r;
                  data.p[pos] = '\0'; // terminer par NULL
                }
              }
              // Envoyer page
              sprintf(line, CATCH_RESPONSE);
              hts_send_nosignal(soc, line, strlen(line));
            } else {
              data.p[0] = '\0'; // no prefix handed back
            }
          } else {
            status = CATCH_URL_ERR_URL;
          }
        }
      } // sinon erreur
    }
  }
  if (soc != INVALID_SOCKET) {
    hts_closesocket(soc);
  }
  return status;
}

// Read one line off a socket; HTS_TRUE if it did not fit "s".
static hts_boolean socinput(T_SOC soc, hts_buf s) {
  hts_boolean cut = HTS_FALSE;
  int c;
  size_t j = 0;

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
        if (j + 1 < s.size)
          s.p[j++] = (char) c;
        else
          cut = HTS_TRUE; // keep draining: the tail is not a new line
        break;
      }
    } else
      c = EOF;
  } while ((c != -1) && (c != EOF));
  s.p[j] = '\0';
  return cut;
}
