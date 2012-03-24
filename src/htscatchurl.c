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
#ifndef  _WIN32_WCE
#include <fcntl.h>
#endif
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
HTSEXT_API T_SOC catch_url_init_std(int* port_prox,char* adr_prox) {
  T_SOC soc;
  int try_to_listen_to[]={8080,3128,80,81,82,8081,3129,31337,0,-1};
  int i=0;
  do {
    soc=catch_url_init(&try_to_listen_to[i],adr_prox);
    *port_prox=try_to_listen_to[i];
    i++;
  } while( (soc == INVALID_SOCKET) && (try_to_listen_to[i]>=0));
  return soc;
}


// 1- Init the URL catcher

// catch_url_init(&port,&return_host);
HTSEXT_API T_SOC catch_url_init(int* port,char* adr) {
  T_SOC soc = INVALID_SOCKET;
  char h_loc[256+2];

  if (gethostname(h_loc,256)==0) {    // host name
    SOCaddr server;
    int server_size=sizeof(server);
    t_hostent* hp_loc;
    t_fullhostent buffer;

    // effacer structure
    memset(&server, 0, sizeof(server));
    
    if ( (hp_loc=vxgethostbyname(h_loc, &buffer)) ) {  // notre host      

      // copie adresse
      SOCaddr_copyaddr(server, server_size, hp_loc->h_addr_list[0], hp_loc->h_length);

      if ( (soc = (T_SOC) socket(SOCaddr_sinfamily(server), SOCK_STREAM, 0)) != INVALID_SOCKET) {
        SOCaddr_initport(server, *port);
        if ( bind(soc,(struct sockaddr*) &server,server_size) == 0 ) {
          SOCaddr server2;
          int len;
          len=sizeof(server2);
          // effacer structure
          memset(&server2, 0, sizeof(server2));
          if (getsockname(soc,(struct sockaddr*) &server2,&len) == 0) {
            *port=ntohs(SOCaddr_sinport(server));  // récupérer port
            if (listen(soc,10)>=0) {    // au pif le 10
              SOCaddr_inetntoa(adr, 128, server2, len);
            } else {
#ifdef _WIN32
              closesocket(soc);
#else
              close(soc);
#endif
              soc=INVALID_SOCKET;
            }
            
            
          } else {
#ifdef _WIN32
            closesocket(soc);
#else
            close(soc);
#endif
            soc=INVALID_SOCKET;
          }
          
          
        } else {
#ifdef _WIN32
          closesocket(soc);
#else
          close(soc);
#endif
          soc=INVALID_SOCKET;
        }
      }
    }
  }
  return soc;
}

// 2 - Wait for URL

// catch_url
// returns 0 if error
// url: buffer where URL must be stored - or ip:port in case of failure
// data: 32Kb
HTSEXT_API int catch_url(T_SOC soc,char* url,char* method,char* data) {
  int retour=0;

  // connexion (accept)
  if (soc != INVALID_SOCKET) {
    T_SOC soc2;
    while ( (soc2 = (T_SOC) accept(soc, NULL, NULL)) == INVALID_SOCKET);
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
      int len;
      len=sizeof(server2);
      // effacer structure
      memset(&server2, 0, sizeof(server2));
      if (getpeername(soc,(struct sockaddr*) &server2,&len) == 0) {
        char dot[256+2];
        SOCaddr_inetntoa(dot, 256, server2, sizeof(server2));
        sprintf(url,"%s:%d", dot, htons(SOCaddr_sinport(server2)));  
      }
    }
    /* INFOS */

    // réception
    if (soc != INVALID_SOCKET) {
      char line[1000];
      char protocol[256];
      line[0]=protocol[0]='\0';
      //
      socinput(soc,line,1000);
      if (strnotempty(line)) {
        if (sscanf(line,"%s %s %s",method,url,protocol) == 3) {
          char BIGSTK url_adr[HTS_URLMAXSIZE*2];
          char BIGSTK url_fil[HTS_URLMAXSIZE*2];
          // méthode en majuscule
          int i,r=0;
          url_adr[0]=url_fil[0]='\0';
          //
          for(i=0;i<(int) strlen(method);i++) {
            if ((method[i]>='a') && (method[i]<='z'))
              method[i]-=('a'-'A');
          }
          // adresse du lien
          if (ident_url_absolute(url,url_adr,url_fil)>=0) {
            // Traitement des en-têtes
            char BIGSTK loc[HTS_URLMAXSIZE*2];
            htsblk blkretour;
            memset(&blkretour, 0, sizeof(htsblk));    // effacer
            blkretour.location=loc;    // si non nul, contiendra l'adresse véritable en cas de moved xx
            // Lire en têtes restants
            sprintf(data,"%s %s %s\r\n",method,url_fil,protocol);
            while(strnotempty(line)) {
              socinput(soc,line,1000);
              treathead(NULL,NULL,NULL,&blkretour,line);  // traiter
              strcatbuff(data,line);
              strcatbuff(data,"\r\n");
            }
            // CR/LF final de l'en tête inutile car déja placé via la ligne vide juste au dessus
            //strcatbuff(data,"\r\n");
            if (blkretour.totalsize>0) {
              int len=(int)min(blkretour.totalsize,32000);
              int pos = (int) strlen(data);
              // Copier le reste (post éventuel)
              while((len>0) && ((r=recv(soc,(char*) data+pos,len,0))>0) ) {
                pos+=r;
                len-=r;
                data[pos]='\0';       // terminer par NULL
              }
            }
            // Envoyer page
            sprintf(line,CATCH_RESPONSE);
            send(soc,line,(int)strlen(line),0);
            // OK!
            retour=1;
          }
        }
      }  // sinon erreur
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
void socinput(T_SOC soc,char* s,int max) {
  int c;
  int j=0;
  do {
    unsigned char b;
    if (recv(soc,(char*) &b,1,0)==1) {
      c=b;
      switch(c) {
        case 13: break;  // sauter CR
        case 10: c=-1; break;
        case 9: case 12: break;  // sauter ces caractères
        default: s[j++]=(char) c; break;
      }
    } else
      c=EOF;
  }  while((c!=-1) && (c!=EOF) && (j<(max-1)));
  s[j++]='\0';
}

