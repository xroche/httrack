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
  int try_to_listen_to[] = { 8080, 3128, 80, 81, 82, 8081, 3129, 31337, 0, -1 };
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
  SOCaddr server;

  /* Loopback only. This is a throwaway proxy that the user points a browser
     at so that we can capture a URL, and the request the browser sends
     carries whatever cookies and credentials it holds for that site.
     Binding to the address the machine's hostname resolves to put all of
     that on a port reachable from the network. */
  SOCaddr_initlocal(server);

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
  return soc;
}

// 2 - Wait for URL

/* Upper bound on header lines accepted for one request, so that draining
   an oversized header block always terminates. */
#define CATCH_URL_MAX_HEADER_LINES 512

// catch_url
// returns 0 if error
// url: buffer where URL must be stored - or ip:port in case of failure
//      (CATCH_URL_URL_SIZE bytes)
// method: CATCH_URL_METHOD_SIZE bytes
// data: CATCH_URL_DATA_SIZE bytes
HTSEXT_API int catch_url(T_SOC soc, char *url, char *method, char *data) {
  int retour = 0;

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
        snprintf(url, CATCH_URL_URL_SIZE, "%s:%d", dot,
                 ntohs(SOCaddr_sinport(server2)));
      }
    }
    /* INFOS */

    // réception
    if (soc != INVALID_SOCKET) {
      char line[1000];
      char protocol[256];

      line[0] = protocol[0] = '\0';
      //
      socinput(soc, line, 1000);
      if (strnotempty(line)) {
        /* The widths below have to be literals, so fail the build rather
           than silently overrun if the capacities are ever changed. */
        typedef char catch_url_width_check_[(CATCH_URL_METHOD_SIZE == 32
                                             && CATCH_URL_URL_SIZE == 2048
                                             && sizeof(protocol) == 256)
                                            ? 1 : -1] HTS_UNUSED;
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
            char BIGSTK loc[HTS_URLMAXSIZE * 2];
            htsblk blkretour;
            int headers_too_large = 0;
            int header_lines = 0;

            hts_init_htsblk(&blkretour);
            //memset(&blkretour, 0, sizeof(htsblk));    // effacer
            blkretour.location = loc;   // si non nul, contiendra l'adresse véritable en cas de moved xx
            // Lire en têtes restants
            snprintf(data, CATCH_URL_DATA_SIZE, "%s %s %s\r\n", method, af.fil,
                     protocol);
            while(strnotempty(line)) {
              socinput(soc, line, 1000);
              treathead(NULL, NULL, NULL, &blkretour, line);    // traiter
              /* A client can send more header bytes than data holds. Keep
                 reading to the blank line that ends the block even once we
                 stop storing: leaving the rest of the headers in the socket
                 would have the body read below capture them as the body. */
              if (++header_lines > CATCH_URL_MAX_HEADER_LINES) {
                /* Nothing obliges a client to ever send the blank line. */
                headers_too_large = 1;
                break;
              }
              if (headers_too_large
                  || strlen(data) + strlen(line) + 2 >= CATCH_URL_DATA_SIZE) {
                headers_too_large = 1;
                continue;
              }
              strlcatbuff(data, line, CATCH_URL_DATA_SIZE);
              strlcatbuff(data, "\r\n", CATCH_URL_DATA_SIZE);
            }
            // CR/LF final de l'en tête inutile car déja placé via la ligne vide juste au dessus
            //strcatbuff(data,"\r\n");
            /* Headers we could not store are headers the caller never
               sees, so the capture would be a quiet lie. Reject instead. */
            if (!headers_too_large) {
              if (blkretour.totalsize > 0) {
                int pos = (int) strlen(data);
                /* Keep the body within what is left of data, terminator
                   included: the headers above already consumed part of it. */
                const int room = (int) CATCH_URL_DATA_SIZE - pos - 1;
                int len = (int) min(blkretour.totalsize, 32000);

                if (len > room)
                  len = room;

                // Copier le reste (post éventuel)
                while((len > 0)
                      && ((r = recv(soc, (char *) data + pos, len, 0)) > 0)) {
                  pos += r;
                  len -= r;
                  data[pos] = '\0';     // terminer par NULL
                }
              }
              // Envoyer page
              sprintf(line, CATCH_RESPONSE);
              send(soc, line, (int) strlen(line), 0);
              // OK!
              retour = 1;
            }
          }
        }
      }                         // sinon erreur
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
  return retour;
}

// Lecture de ligne sur socket
void socinput(T_SOC soc, char *s, int max) {
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
        s[j++] = (char) c;
        break;
      }
    } else
      c = EOF;
  } while((c != -1) && (c != EOF) && (j < (max - 1)));
  s[j++] = '\0';
}
