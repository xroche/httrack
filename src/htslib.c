/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
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
/* File: Subroutines                                            */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

// Fichier librairie .c

#include "htslib.h"
#include "htsbauth.h"

/* specific definitions */
#include "htsbase.h"
#include "htsnet.h"
#include "htsbauth.h"
#include "htsthread.h"
#include "htsnostatic.h"
#include "htswrap.h"
#include <stdio.h>
#if HTS_WIN
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/timeb.h>
#include <fcntl.h>
// pour utimbuf
#if HTS_WIN
#include <sys/utime.h>
#else
#if HTS_PLATFORM!=3
#include <utime.h>
#else
#include <utime.h>
#endif
#endif
/* END specific definitions */



// Débuggage de contrôle
#if HTS_DEBUG_CLOSESOCK
#define _HTS_WIDE 1
#endif
#if HTS_WIDE_DEBUG
#define _HTS_WIDE 1
#endif
#if _HTS_WIDE
FILE* DEBUG_fp=NULL;
#define DEBUG_W(A)  { if (DEBUG_fp==NULL) DEBUG_fp=fopen("bug.out","wb"); fprintf(DEBUG_fp,":>"A); fflush(DEBUG_fp); }
#define DEBUG_W2(A) { if (DEBUG_fp==NULL) DEBUG_fp=fopen("bug.out","wb"); fprintf(DEBUG_fp,A); fflush(DEBUG_fp); }
#endif

/* variables globales */
int _DEBUG_HEAD;
FILE* ioinfo;

#if HTS_USEOPENSSL
 SSL_CTX *openssl_ctx = NULL;
#endif
int IPV6_resolver = 0;


/* détection complémentaire */
const char hts_detect[][32] = {
  "archive",
  "background",
  "data",         // OBJECT
  "dynsrc",
  "lowsrc",
  "profile",      // element META
  "src",
  "swurl",
  "url",
  "usemap",
  "longdesc",     // accessibility
  "xlink:href",   // xml/svg tag
  ""
};

/* détecter début */
const char hts_detectbeg[][32] = {
  "hotspot",      /* hotspot1=..,hotspot2=.. */
  ""
};

/* ne pas détcter de liens dedans */
const char hts_nodetect[][32] = {
  "accept-charset",
  "accesskey",
  "action",
  "align",
  "alt",
  "axes",
  "axis",
  "char",
  "charset",
  "cite",
  "class",
  "classid",
  "code",
  "color",
  "datetime",
  "dir",
  "enctype",
  "face",
  "height",
  "id",
  "lang",
  "language",
  "media",
  "method",
  "name",
  "prompt",
  "scheme",
  "size",
  "style",
  "target",
  "title",
  "type",
  "valign",
  "version",
  "width",
  ""
};


/* détection de mini-code javascript */
/* ALSO USED: detection based on the name: onXXX="<tag>" where XXX starts with upper case letter */
const char hts_detect_js[][32] = {
  "onAbort",
  "onBlur",
  "onChange",
  "onClick",
  "onDblClick",
  "onDragDrop",
  "onError",
  "onFocus",
  "onKeyDown",
  "onKeyPress",
  "onKeyUp",
  "onLoad",
  "onMouseDown",
  "onMouseMove",
  "onMouseOut",
  "onMouseOver",
  "onMouseUp",
  "onMove",
  "onReset",
  "onResize",
  "onSelect",
  "onSubmit",
  "onUnload",
  ""
};

/* détection "...URL=<url>" */
const char hts_detectURL[][32] = {
  "content",
  ""
};

/* tags où l'URL doit être réécrite mais non capturée */
const char hts_detectandleave[][32] = {
  "action",
  ""
};

/* ne pas renommer les types renvoyés (couvent types inconnus) */
const char hts_mime_keep[][32] = {
  "application/octet-stream",
  "text/plain",
  ""
};

/* pas de type mime connu, mais extension connue */
const char hts_ext_dynamic[][32] = {
  "php3",
  "php",
  "php4",
  "php2",
  "cgi",
  "asp",
  "jsp",
  "pl",
  /*"exe",*/
  "cfm",
  ""
};

/* types MIME */
const char hts_mime[][2][32] = {
  {"application/acad","dwg"},
  {"application/arj","arj"},
  {"application/clariscad","ccad"},
  {"application/drafting","drw"},
  {"application/dxf","dxf"},
  {"application/excel","xls"},
  {"application/i-deas","unv"},
  {"application/iges","isg"},
  {"application/iges","iges"},
  {"application/mac-binhex40","hqx"},
  {"application/mac-compactpro","cpt"},
  {"application/msword","doc"},
  {"application/msword","w6w"},
  {"application/msword","word"},
  {"application/mswrite","wri"},
  /*{"application/octet-stream","dms"},*/
  /*{"application/octet-stream","lzh"},*/
  /*{"application/octet-stream","lha"},*/
  /*{"application/octet-stream","bin"},*/
  {"application/oda","oda"},
  {"application/pdf","pdf"},
  {"application/postscript","ps"},
  {"application/postscript","ai"},
  {"application/postscript","eps"},
  {"application/powerpoint","ppt"},
  {"application/pro_eng","prt"},
  {"application/pro_eng","part"},
  {"application/rtf","rtf"},
  {"application/set","set"},
  {"application/sla","stl"},
  {"application/smil","smi"},
  {"application/smil","smil"},
  {"application/smil","sml"},
  {"application/solids","sol"},
  {"application/STEP","stp"},
  {"application/STEP","step"},
  {"application/vda","vda"},
  {"application/x-authorware-map","aam"},     
  {"application/x-authorware-seg","aas"},
  {"application/x-authorware-bin","aab"},
  {"application/x-cocoa","cco"},
  {"application/x-csh","csh"},
  {"application/x-director","dir"},
  {"application/x-director","dcr"},
  {"application/x-director","dxr"},
  {"application/x-mif","mif"},
  {"application/x-dvi","dvi"},
  {"application/x-gzip","gz"},
  {"application/x-gzip","gzip"},
  {"application/x-hdf","hdf"},
  {"application/x-javascript","js"},
  {"application/x-koan","skp"},
  {"application/x-koan","skd"},
  {"application/x-koan","skt"},
  {"application/x-koan","skm"},
  {"application/x-latex","latex"},
  {"application/x-netcdf","nc"},
  {"application/x-netcdf","cdf"},
  /* {"application/x-sh","sh"}, */
  /* {"application/x-csh","csh"}, */
  /* {"application/x-ksh","ksh"}, */
  {"application/x-shar","shar"},
  {"application/x-stuffit","sit"},
  {"application/x-tcl","tcl"},
  {"application/x-tex","tex"},
  {"application/x-texinfo","texinfo"},
  {"application/x-texinfo","texi"},
  {"application/x-troff","t"},
  {"application/x-troff","tr"},
  {"application/x-troff","roff"},
  {"application/x-troff-man","man"},
  {"application/x-troff-me","ms"},
  {"application/x-wais-source","src"},
  {"application/zip","zip"},
  {"application/x-zip-compressed","zip"},
  {"application/x-bcpio","bcpio"},
  {"application/x-cdlink","vcd"},
  {"application/x-cpio","cpio"},
  {"application/x-gtar","tgz"},
  {"application/x-gtar","gtar"},
  {"application/x-shar","shar"},
  {"application/x-shockwave-flash","swf"},
  {"application/x-sv4cpio","sv4cpio"},
  {"application/x-sv4crc","sv4crc"},
  {"application/x-tar","tar"},
  {"application/x-ustar","ustar"},
  {"application/x-winhelp","hlp"},
  {"audio/midi","mid"},
  {"audio/midi","midi"},
  {"audio/midi","kar"},
  {"audio/mpeg","mp3"},
  {"audio/mpeg","mpga"},
  {"audio/mpeg","mp2"},
  {"audio/basic","au"},
  {"audio/basic","snd"},
  {"audio/x-aiff","aif"},
  {"audio/x-aiff","aiff"},
  {"audio/x-aiff","aifc"},
  {"audio/x-pn-realaudio","rm"},
  {"audio/x-pn-realaudio","ram"},
  {"audio/x-pn-realaudio","ra"},
  {"audio/x-pn-realaudio-plugin","rpm"},
  {"audio/x-wav","wav"},
  {"chemical/x-pdb","pdb"},
  {"chemical/x-pdb","xyz"},
  {"drawing/x-dwf","dwf"},
  {"image/gif","gif"},
  {"image/ief","ief"},
  {"image/jpeg","jpg"},
  {"image/jpeg","jpe"},
  {"image/jpeg","jpeg"},
  {"image/pict","pict"},
  {"image/png","png"},
  {"image/tiff","tiff"},
  {"image/tiff","tif"},
  {"image/svg+xml","svg"},
  {"image/svg-xml","svg"},
  {"image/x-cmu-raster","ras"},
  {"image/x-freehand","fh4"},
  {"image/x-freehand","fh7"},
  {"image/x-freehand","fh5"},  
  {"image/x-freehand","fhc"},
  {"image/x-freehand","fh"},   
  {"image/x-portable-anymap","pnm"},
  {"image/x-portable-bitmap","pgm"},
  {"image/x-portable-pixmap","ppm"},
  {"image/x-rgb","rgb"},
  {"image/x-xbitmap","xbm"},
  {"image/x-xpixmap","xpm"},
  {"image/x-xwindowdump","xwd"},
  {"model/mesh","msh"},  
  {"model/mesh","mesh"},  
  {"model/mesh","silo"},  
  {"multipart/x-zip","zip"},
  {"multipart/x-gzip","gzip"},
  {"text/css","css"},
  {"text/html","html"},
  {"text/html","htm"},
  {"text/plain","txt"},
  {"text/plain","g"},
  {"text/plain","h"},
  {"text/plain","c"},
  {"text/plain","cc"},
  {"text/plain","hh"},
  {"text/plain","m"},
  {"text/plain","f90"},
  {"text/richtext","rtx"},
  {"text/tab-separated-values","tsv"},
  {"text/x-setext","etx"},
  {"text/x-sgml","sgml"},
  {"text/x-sgml","sgm"},
  {"text/xml","xml"},  
  {"text/xml","dtd"},  
  {"video/mpeg","mpeg"},
  {"video/mpeg","mpg"},
  {"video/mpeg","mpe"},
  {"video/quicktime","qt"},
  {"video/quicktime","mov"},
  {"video/x-msvideo","avi"},
  {"video/x-sgi-movie","movie"},
  {"x-conference/x-cooltalk","ice"},
  /*{"application/x-httpd-cgi","cgi"},*/
  {"x-world/x-vrml","wrl"},
  
  {"*","class"},
  
  {"",""}};


// Reserved (RFC2396)
#define CHAR_RESERVED(c)  ( strchr(";/?:@&=+$,",(unsigned char)(c)) != 0 )
// Delimiters (RFC2396)
#define CHAR_DELIM(c)     ( strchr("<>#%\"",(unsigned char)(c)) != 0 )
// Unwise (RFC2396)
#define CHAR_UNWISE(c)    ( strchr("{}|\\^[]`",(unsigned char)(c)) != 0 )
// Special (escape chars) (RFC2396 + >127 )
#define CHAR_LOW(c)       ( ((unsigned char)(c) <= 31) )
#define CHAR_HIG(c)       ( ((unsigned char)(c) >= 127) )
#define CHAR_SPECIAL(c)   ( CHAR_LOW(c) || CHAR_HIG(c) )
// We try to avoid them and encode them instead
#define CHAR_XXAVOID(c)   ( strchr(" *'\"!",(unsigned char)(c)) != 0 )


// conversion éventuelle / vers antislash
#if HTS_WIN
char* antislash(char* s) {
  char* buff;
  char* a;
  NOSTATIC_RESERVE(buff, char, HTS_URLMAXSIZE*2);

  strcpy(buff,s);
  while(a=strchr(buff,'/')) *a='\\';
  return buff;
}
#endif



// Récupération d'un fichier http sur le net.
// Renvoie une adresse sur le bloc de mémoire, ou bien
// NULL si un retour.msgeur (buffer retour.msg) est survenue. 
//
// Une adresse de structure htsmsg peut être transmise pour
// suivre l'évolution du chargement si le process a été lancé 
// en background

htsblk httpget(char* url) {
  char adr[HTS_URLMAXSIZE*2];   // adresse
  char fil[HTS_URLMAXSIZE*2];   // chemin
  
  // séparer URL en adresse+chemin
  if (ident_url_absolute(url,adr,fil)==-1) {
    htsblk retour;
    memset(&retour, 0, sizeof(htsblk));    // effacer
    // retour prédéfini: erreur
    retour.adr=NULL;
    retour.size=0;
    retour.msg[0]='\0';
    retour.statuscode=-1;    
    strcpy(retour.msg,"Error invalid URL");
    return retour;
  }
  
  return xhttpget(adr,fil);
}

// ouvre une liaison http, envoie une requète GET et réceptionne le header
// retour: socket
int http_fopen(char* adr,char* fil,htsblk* retour) {
  //                / GET, traiter en-tête
  return http_xfopen(0,1,1,NULL,adr,fil,retour);
}

// ouverture d'une liaison http, envoi d'une requète
// mode: 0 GET  1 HEAD  [2 POST]
// treat: traiter header?
// waitconnect: attendre le connect()
// note: dans retour, on met les params du proxy
int http_xfopen(int mode,int treat,int waitconnect,char* xsend,char* adr,char* fil,htsblk* retour) {
  //htsblk retour;
  //int bufl=TAILLE_BUFFER;    // 8Ko de buffer
  T_SOC soc=INVALID_SOCKET;
  //char *p,*q;
  
  // retour prédéfini: erreur
  if (retour) {
    retour->adr=NULL;
    retour->size=0;
    retour->msg[0]='\0';
    retour->statuscode=-5;          // a priori erreur non fatale
  }

#if HDEBUG
  printf("adr=%s\nfichier=%s\n",adr,fil);
#endif
  
  // ouvrir liaison
#if HDEBUG
  printf("Création d'une socket sur %s\n",adr);
#endif

#if CNXDEBUG
  printf("..newhttp\n");
#endif

  /* connexion */
  if (retour) {
    if ( (!(retour->req.proxy.active)) 
      ||
      (
        (strcmp(adr,"file://")==0) 
        ||
        (strncmp(adr,"https://", 8)==0) 
      )
      ) {    /* pas de proxy, ou non utilisable ici */
      soc=newhttp(adr,retour,-1,waitconnect);
    } else {
      soc=newhttp(retour->req.proxy.name,retour,retour->req.proxy.port,waitconnect);  // ouvrir sur le proxy à la place
    }
  } else {
    soc=newhttp(adr,NULL,-1,waitconnect);    
  }

  // copier index socket retour
  if (retour) retour->soc=soc;

  /* Check for errors */
  if (soc == INVALID_SOCKET) {
    if (retour) {
      if (retour->msg) {
        if (!strnotempty(retour->msg)) {
          strcpy(retour->msg,"Connect error");
        }
      }
    }
  }

  // --------------------
  // court-circuit (court circuite aussi le proxy..)
  // LOCAL_SOCKET_ID est une pseudo-socket locale
  if (soc==LOCAL_SOCKET_ID) {
    retour->is_file=1;  // fichier local
    if (mode==0) {    // GET

      // Test en cas de file:///C|...
      if (!fexist(fconv(unescape_http(fil))))
        if (fexist(fconv(unescape_http(fil+1)))) {
          char tempo[HTS_URLMAXSIZE*2];
          strcpy(tempo,fil+1);
          strcpy(fil,tempo);
        }

      // Ouvrir
      retour->totalsize=fsize(fconv(unescape_http(fil)));  // taille du fichier
      retour->msg[0]='\0';
      soc=INVALID_SOCKET;
      if (retour->totalsize<0)
        strcpy(retour->msg,"Unable to open file");
      else if (retour->totalsize==0)
        strcpy(retour->msg,"File empty");
      else {
        // Note: On passe par un FILE* (plus propre)
        //soc=open(fil,O_RDONLY,0);    // en lecture seule!
        retour->fp=fopen(fconv(unescape_http(fil)),"rb");  // ouvrir
        if (retour->fp==NULL)
          soc=INVALID_SOCKET;
        else
          soc=LOCAL_SOCKET_ID;
      }
      retour->soc=soc;
      if (soc!=INVALID_SOCKET) {
        retour->statuscode=200;   // OK
        strcpy(retour->msg,"OK");
        guess_httptype(retour->contenttype,fil);
      } else if (strnotempty(retour->msg)==0)
          strcpy(retour->msg,"Unable to open file");
      return soc;  // renvoyer
    } else {    // HEAD ou POST : interdit sur un local!!!! (c'est idiot!)
      strcpy(retour->msg,"Unexpected Head/Post local request");
      soc=INVALID_SOCKET;    // erreur
      retour->soc=soc;
      return soc;
    }
  } 
  // --------------------

  if (soc!=INVALID_SOCKET) {    
    char rcvd[1100];
    rcvd[0]='\0';
#if HDEBUG
    printf("Ok, connexion réussie, id=%d\n",soc);
#endif
    
    // connecté?
    if (waitconnect) {
      http_sendhead(NULL,mode,xsend,adr,fil,NULL,NULL,retour);
    } 
    
    if (soc!=INVALID_SOCKET) {
      
#if HDEBUG
      printf("Attente de la réponse:\n");
#endif
      
      // si GET (réception d'un fichier), réceptionner en-tête d'abord,
      // et ensuite le corps
      // si POST on ne réceptionne rien du tout, c'est après que l'on fera
      // une réception standard pour récupérer l'en tête
      if ((treat) && (waitconnect)) {  // traiter (attendre!) en-tête        
        // Réception de la status line et de l'en-tête (norme RFC1945)
        
        // status-line à récupérer
        finput(soc,rcvd,1024);
        if (strnotempty(rcvd)==0)
          finput(soc,rcvd,1024);    // "certains serveurs buggés envoient un \n au début" (RFC)

        // traiter status-line
        treatfirstline(retour,rcvd);

#if HDEBUG
        printf("Status-Code=%d\n",retour->statuscode);
#endif
        
        // en-tête
        
        // header // ** !attention! HTTP/0.9 non supporté
        do {
          finput(soc,rcvd,1024);          
#if HDEBUG
          printf(">%s\n",rcvd);      
#endif
          if (strnotempty(rcvd))
            treathead(NULL,NULL,NULL,retour,rcvd);  // traiter

        } while(strnotempty(rcvd));
        
        //rcvsize=-1;    // forCER CHARGEMENT INCONNU
        
        //if (retour)
        //  retour->totalsize=rcvsize;
        
      } else { // si GET, on recevra l'en tête APRES
        //rcvsize=-1;    // on ne connait pas la taille de l'en-tête
        if (retour)
          retour->totalsize=-1;
      }
      
    }
 
  }
    
  return soc;
}


// envoi d'une requète
int http_sendhead(t_cookie* cookie,int mode,char* xsend,char* adr,char* fil,char* referer_adr,char* referer_fil,htsblk* retour) {
  char buff[8192];
  //int use_11=0;     // HTTP 1.1 utilisé
  int direct_url=0; // ne pas analyser l'url (exemple: ftp://)
  char* search_tag=NULL;
  buff[0]='\0';

  // header Date
  //strcat(buff,"Date: ");
  //time_gmt_rfc822(buff);    // obtenir l'heure au format rfc822
  //sendc("\n");
  //strcat(buff,buff);

  // possibilité non documentée: >post: et >postfile:
  // si présence d'un tag >post: alors executer un POST
  // exemple: http://www.someweb.com/test.cgi?foo>post:posteddata=10&foo=5
  // si présence d'un tag >postfile: alors envoyer en tête brut contenu dans le fichier en question
  // exemple: http://www.someweb.com/test.cgi?foo>postfile:post0.txt
  search_tag=strstr(fil,POSTTOK":");
  if (!search_tag) {
    search_tag=strstr(fil,POSTTOK"file:");
    if (search_tag) {     // postfile
      if (mode==0) {      // GET!
        FILE* fp=fopen(unescape_http(search_tag+strlen(POSTTOK)+5),"rb");
        if (fp) {
          char line[1100];
          char protocol[256],url[HTS_URLMAXSIZE*2],method[256];
          linput(fp,line,1000);
          if (sscanf(line,"%s %s %s",method,url,protocol) == 3) {
            // selon que l'on a ou pas un proxy
            if (retour->req.proxy.active)
              sprintf(buff,"%s http://%s%s %s\r\n",method,adr,url,protocol);
            else
              sprintf(buff,"%s %s %s\r\n",method,url,protocol);
            // lire le reste en brut
            fread(buff+strlen(buff),8000-strlen(buff),1,fp);
          }
          fclose(fp);
        }
      }
    }
  }
  // Fin postfile
  
  if (strnotempty(buff)==0) {    // PAS POSTFILE
    // Type de requète?
    if ((search_tag) && (mode==0)) {
      strcat(buff,"POST ");
    } else if (mode==0) {    // GET
      strcat(buff,"GET ");
    } else {  // if (mode==1) {
      if (!retour->req.http11)        // forcer HTTP/1.0
        strcat(buff,"GET ");      // certains serveurs (cgi) buggent avec HEAD
      else
        strcat(buff,"HEAD ");
    }
    
    // si on gère un proxy, il faut une Absolute URI: on ajoute avant http://www.adr.dom
    if (retour->req.proxy.active) {
      if (!link_has_authority(adr)) {  // default http
#if HDEBUG
        printf("Proxy Use: for %s%s proxy %d port %d\n",adr,fil,retour->req.proxy.name,retour->req.proxy.port);
#endif
        strcat(buff,"http://");
        strcat(buff,jump_identification(adr));
      } else {          // ftp:// en proxy http
#if HDEBUG
        printf("Proxy Use for ftp: for %s%s proxy %d port %d\n",adr,fil,retour->req.proxy.name,retour->req.proxy.port);
#endif
        direct_url=1;             // ne pas analyser user/pass
        strcat(buff,adr);
      }
    } 
    
    // NOM DU FICHIER
    // on slash doit être présent en début, sinon attention aux bad request! (400)
    if (*fil!='/') strcat(buff,"/");
    {
      char tempo[HTS_URLMAXSIZE*2];
      tempo[0]='\0';
      if (search_tag)
        strncat(tempo,fil,(int) (search_tag - fil));
      else
        strcpy(tempo,fil);
      escape_check_url(tempo);
      strcat(buff,tempo);       // avec échappement
    }
    
    // protocole
    if (!retour->req.http11) {     // forcer HTTP/1.0
      //use_11=0;
      strcat(buff," HTTP/1.0\x0d\x0a");
    } else {                   // Requète 1.1
      //use_11=1;
      strcat(buff," HTTP/1.1\x0d\x0a");
    }

    /* supplemental data */
    if (xsend) strcat(buff,xsend);    // éventuelles autres lignes

    // tester proxy authentication
    if (retour->req.proxy.active) {
      if (link_has_authorization(retour->req.proxy.name)) {  // et hop, authentification proxy!
        char* a=jump_identification(retour->req.proxy.name);
        char* astart=jump_protocol(retour->req.proxy.name);
        char autorisation[1100];
        char user_pass[256];        
        autorisation[0]=user_pass[0]='\0';
        //
        strncat(user_pass,astart,(int) (a - astart) - 1);
        strcpy(user_pass,unescape_http(user_pass));
        code64(user_pass,autorisation);
        strcat(buff,"Proxy-Authorization: Basic ");
        strcat(buff,autorisation);
        strcat(buff,H_CRLF);
#if HDEBUG
        printf("Proxy-Authenticate, %s (code: %s)\n",user_pass,autorisation);
#endif
      }
    }
    
    // Referer?
    if ((referer_adr) && (referer_fil)) {       // existe
      if ((strnotempty(referer_adr)) && (strnotempty(referer_fil))) {   // non vide
        if (
          (strcmp(referer_adr,"file://") != 0)
          &&
          (  /* no https referer to http urls */
            (strncmp(referer_adr, "https://", 8) != 0)  /* referer is not https */
            ||
            (strncmp(adr, "https://", 8) == 0)          /* or referer AND addresses are https */
          )
          ) {      // PAS file://
          strcat(buff,"Referer: ");
          strcat(buff,"http://");
          strcat(buff,jump_identification(referer_adr));
          strcat(buff,referer_fil);
          strcat(buff,H_CRLF);
        }
      }
    }
    
    // POST?
    if (mode==0) {      // GET!
      if (search_tag) {
        char clen[256];
        sprintf(clen,"Content-length: %d"H_CRLF,(int)(strlen(unescape_http(search_tag+strlen(POSTTOK)+1))));
        strcat(buff,clen);
      }
    }
    
    // gestion cookies?
    if (cookie) {
      char* b=cookie->data;
      int cook=0;
      int max_cookies=8;
      int max_size=2048;
      max_size+=strlen(buff);
      do {
        b=cookie_find(b,"",jump_identification(adr),fil);       // prochain cookie satisfaisant aux conditions
        if (b) {
          max_cookies--;
          if (!cook) {
            strcat(buff,"Cookie: ");
            strcat(buff,"$Version=1; ");
            cook=1;
          } else
            strcat(buff,"; ");
          strcat(buff,cookie_get(b,5));
          strcat(buff,"=");
          strcat(buff,cookie_get(b,6));
          strcat(buff,"; $Path=");
          strcat(buff,cookie_get(b,2));
          b=cookie_nextfield(b);
        }
      } while( (b) && (max_cookies>0) && ((int)strlen(buff)<max_size));
      if (cook) {                           // on a envoyé un (ou plusieurs) cookie?
        strcat(buff,H_CRLF);
#if DEBUG_COOK
        printf("Header:\n%s\n",buff);
#endif
      }
    }
    
    // connection close?
    //if (use_11)     // Si on envoie une requète 1.1, préciser qu'on ne veut pas de keep-alive!!
    strcat(buff,"Connection: close"H_CRLF);
    
    // gérer le keep-alive (garder socket)
    //strcat(buff,"Connection: Keep-Alive\n");
    
    {
      char* real_adr=jump_identification(adr);
      //if ((use_11) || (retour->user_agent_send)) {   // Pour le 1.1 on utilise un Host:
      if (!direct_url) {     // pas ftp:// par exemple
        //if (!retour->req.proxy.active) {
        strcat(buff,"Host: "); strcat(buff,real_adr); strcat(buff,H_CRLF);
        //}
      }
      //}

      // Présence d'un user-agent?
      if (retour->req.user_agent_send) {  // ohh un user-agent
        char s[256];
        // HyperTextSeeker/"HTSVERSION
        sprintf(s,"User-Agent: %s"H_CRLF,retour->req.user_agent);
        strcat(buff,s);
        
        // pour les serveurs difficiles
        strcat(buff,"Accept: image/gif, image/x-xbitmap, image/jpeg, image/pjpeg, image/svg+xml, */*"H_CRLF);
        if (strnotempty(retour->req.lang_iso)) {
          strcat(buff,"Accept-Language: "); strcat(buff,retour->req.lang_iso); strcat(buff,H_CRLF);
        }
        strcat(buff,"Accept-Charset: iso-8859-1, *"H_CRLF);   
        if (retour->req.http11) {
#if HTS_USEZLIB
          if ((!retour->req.range_used) && (!retour->req.nocompression))
            strcat(buff,"Accept-Encoding: gzip, deflate, compress, identity"H_CRLF);
          else
            strcat(buff,"Accept-Encoding: identity"H_CRLF);       /* no compression */
#else
          strcat(buff,"Accept-Encoding: identity"H_CRLF);         /* no compression */
#endif
        }
      } else {
        strcat(buff,"Accept: */*"H_CRLF);         // le minimum
      }

      /* Authentification */
      {
        char autorisation[1100];
        char* a;
        autorisation[0]='\0';
        if (link_has_authorization(adr)) {  // ohh une authentification!
          char* a=jump_identification(adr);
          char* astart=jump_protocol(adr);
          if (!direct_url) {      // pas ftp:// par exemple
            char user_pass[256];
            user_pass[0]='\0';
            strncat(user_pass,astart,(int) (a - astart) - 1);
            strcpy(user_pass,unescape_http(user_pass));
            code64(user_pass,autorisation);
            if (strcmp(fil,"/robots.txt"))      /* pas robots.txt */
              bauth_add(cookie,astart,fil,autorisation);
          }
        } else if ( (a=bauth_check(cookie,real_adr,fil)) )
          strcpy(autorisation,a);
        /* On a une autorisation a donner?  */
        if (strnotempty(autorisation)) {
          strcat(buff,"Authorization: Basic ");
          strcat(buff,autorisation);
          strcat(buff,H_CRLF);
        }
      }

    }
    //strcat(buff,"Accept-Language: en\n");
    //strcat(buff,"Accept-Charset: iso-8859-1,*,utf-8\n");
    
    // CRLF de fin d'en tête
    strcat(buff,H_CRLF);
    
    // données complémentaires?
    if (search_tag)
    if (mode==0)      // GET!
      strcat(buff,unescape_http(search_tag+strlen(POSTTOK)+1));
  }
  
#if HDEBUG
#endif
  if (_DEBUG_HEAD) {
    if (ioinfo) {
      fprintf(ioinfo,"request for %s%s:\r\n",jump_identification(adr),fil);
      fprintfio(ioinfo,buff,"<<< ");
      fprintf(ioinfo,"\r\n");
      fflush(ioinfo);
    }
  }  // Fin test pas postfile
  //

  // Envoi
  if (sendc(retour, buff)<0) {  // ERREUR, socket rompue?...
  //if (sendc(retour->soc,buff) != strlen(buff)) {  // ERREUR, socket rompue?...
    deletesoc_r(retour);  // fermer tout de même
    // et tenter de reconnecter
    
    strcpy(retour->msg,"Broken pipe");
    retour->soc=INVALID_SOCKET;
  }
  
  // RX'98
  return 0;
}




// traiter 1ere ligne d'en tête
void treatfirstline(htsblk* retour,char* rcvd) {
  char* a=rcvd;
  // exemple:
  // HTTP/1.0 200 OK
  if (*a) {
    // note: certains serveurs buggés renvoient HTTP/1.0\n200 OK ou " HTTP/1.0 200 OK"
    while ((*a==' ') || (*a==10) || (*a==13) || (*a==9)) a++;      // épurer espaces au début
    if (strfield(a, "HTTP/")) {
      // sauter HTTP/1.x
      while ((*a!=' ') && (*a!='\0') && (*a!=10) && (*a!=13) && (*a!=9)) a++;   
      if (*a != '\0') {
        while ((*a==' ') || (*a==10) || (*a==13) || (*a==9)) a++;      // épurer espaces
        if ((*a>='0') && (*a<='9')) {
          sscanf(a,"%d",&(retour->statuscode));
          // sauter 200
          while ((*a!=' ') && (*a!='\0') && (*a!=10) && (*a!=13) && (*a!=9)) a++;   
          while ((*a==' ') || (*a==10) || (*a==13) || (*a==9)) a++;      // épurer espaces
          if ((strlen(a) > 1) && (strlen(a) < 64) )                // message retour
            strcpy(retour->msg,a);
          else
            infostatuscode(retour->msg,retour->statuscode);
          // type MIME par défaut2
          strcpy(retour->contenttype,HTS_HYPERTEXT_DEFAULT_MIME);
        } else {  // pas de code!
          retour->statuscode=-1;
          strcpy(retour->msg,"Unknown response structure");
        }
      } else {  // euhh??
        retour->statuscode=-1;
        strcpy(retour->msg,"Unknown response structure");
      }
    } else {
			if (*a == '<') {
        /* This is dirty .. */
        retour->statuscode=200;
        strcpy(retour->msg, "Unknown, assuming junky server");
        strcpy(retour->contenttype,HTS_HYPERTEXT_DEFAULT_MIME);
			} else if (strnotempty(a)) {
        retour->statuscode=-1;
        strcpy(retour->msg,"Unknown response structure, no HTTP/ response given");
      } else {
        /* This is dirty .. */
        retour->statuscode=200;
        strcpy(retour->msg, "Unknown, assuming junky server");
        strcpy(retour->contenttype,HTS_HYPERTEXT_DEFAULT_MIME);
      }
    }
  } else {  // vide!
    /*
    retour->statuscode=-1;
    strcpy(retour->msg,"Empty reponse or internal error");
    */
    /* This is dirty .. */
    retour->statuscode=200;
    strcpy(retour->msg, "Unknown, assuming junky server");
    strcpy(retour->contenttype,HTS_HYPERTEXT_DEFAULT_MIME);
  }
}

// traiter ligne par ligne l'en tête
// gestion des cookies
void treathead(t_cookie* cookie,char* adr,char* fil,htsblk* retour,char* rcvd) {
  int p;
  if ((p=strfield(rcvd,"Content-length:"))!=0) {
#if HDEBUG
    printf("ok, Content-length: détecté\n");
#endif
    sscanf(rcvd+p,LLintP,&(retour->totalsize));
  }
  else if ((p=strfield(rcvd,"Content-Disposition:"))!=0) {
    while(*(rcvd+p)==' ') p++;    // sauter espaces
    if ((int) strlen(rcvd+p)<250) { // pas trop long?
      char tmp[256];
      char *a=NULL,*b=NULL;
      strcpy(tmp,rcvd+p);
      a=strstr(tmp,"filename=");
      if (a) {
        a+=strlen("filename=");
        while(is_space(*a)) a++;
        //a=strchr(a,'"');
        if (a) {
          char *c=NULL;
          //a++;      /* jump " */
          while((c=strchr(a,'/')))    /* skip all / (see RFC2616) */
            a=c+1;
          //b=strchr(a+1,'"');
          b=a+strlen(a)-1;
          while(is_space(*b)) b--;
          b++;
          if (b) {
            *b='\0';
            if ((int) strlen(a) < 200) { // pas trop long?
              strcpy(retour->cdispo,a);
            }
          }
        }
      } 
    }
  }
  else if ((p=strfield(rcvd,"Last-Modified:"))!=0) {
    while(*(rcvd+p)==' ') p++;    // sauter espaces
    if ((int) strlen(rcvd+p)<64) { // pas trop long?
      //struct tm* tm_time=convert_time_rfc822(rcvd+p);
      strcpy(retour->lastmodified,rcvd+p);
    }
  }
  else if ((p=strfield(rcvd,"Date:"))!=0) {
    if (strnotempty(retour->lastmodified)==0) {          /* pas encore de last-modified */
      while(*(rcvd+p)==' ') p++;    // sauter espaces
      if ((int) strlen(rcvd+p)<64) { // pas trop long?
        //struct tm* tm_time=convert_time_rfc822(rcvd+p);
        strcpy(retour->lastmodified,rcvd+p);
      }
    }
  }
  else if ((p=strfield(rcvd,"Etag:"))!=0) {   /* Etag */
    if (retour) {
      while(*(rcvd+p)==' ') p++;    // sauter espaces
      if ((int) strlen(rcvd+p)<64)  // pas trop long?
        strcpy(retour->etag,rcvd+p);
      else    // erreur.. ignorer
        retour->etag[0]='\0';
    }
  }
  else if ((p=strfield(rcvd,"Transfer-Encoding: chunked"))!=0) {  // chunk!
    retour->is_chunk=1;     // chunked
    //retour->http11=2;     // chunked
#if HDEBUG
    printf("ok, Transfer-Encoding: détecté\n");
#endif
  }
  else if ((p=strfield(rcvd,"Content-type:"))!=0) {
    if (retour) {
      char tempo[1100];
      // éviter les text/html; charset=foo
      {
        char* a=strchr(rcvd+p,';');
        if (a) *a='\0';
      }
      sscanf(rcvd+p,"%s",tempo);
      if (strlen(tempo)<64)    // pas trop long!!
        strcpy(retour->contenttype,tempo);
      else
        strcpy(retour->contenttype,"application/octet-stream-unknown");    // erreur
    }
  }
  else if ((p=strfield(rcvd,"Content-Range:"))!=0) {
    char* a=strstr(rcvd+p,"*/");
    if (a) {
      if (sscanf(a+2,LLintP,&retour->crange) != 1) {
        retour->crange=0;
      }
    }
  }
  else if ((p=strfield(rcvd,"Content-Encoding:"))!=0) {
    if (retour) {
      char tempo[1100];
      {
        char* a=strchr(rcvd+p,';');
        if (a) *a='\0';
      }
      sscanf(rcvd+p,"%s",tempo);
      if (strlen(tempo)<64)    // pas trop long!!
        strcpy(retour->contentencoding,tempo);
      else
        retour->contentencoding[0]='\0';    // erreur
#if HTS_USEZLIB
      /* Check known encodings */
      if (retour->contentencoding[0]) {
        if (
          (strfield2(retour->contentencoding, "gzip"))
          || (strfield2(retour->contentencoding, "x-gzip"))
          /*
          || (strfield2(retour->contentencoding, "compress"))
          || (strfield2(retour->contentencoding, "x-compress"))
          */
          || (strfield2(retour->contentencoding, "deflate"))
          || (strfield2(retour->contentencoding, "x-deflate"))
          ) {
        retour->compressed=1;
        }
      }
#endif
    }
  }
  else if ((p=strfield(rcvd,"Location:"))!=0) {
    if (retour) {
      if (retour->location) {
        while(*(rcvd+p)==' ') p++;    // sauter espaces
        if ((int) strlen(rcvd+p)<HTS_URLMAXSIZE)  // pas trop long?
          strcpy(retour->location,rcvd+p);
        else    // erreur.. ignorer
          retour->location[0]='\0';
      }
    }
  }
  else if ((p=strfield(rcvd,"Connection: Keep-Alive"))!=0) {
    // non, pas de keep-alive! on déconnectera..          
  }
  else if ((p=strfield(rcvd,"Keep-Alive:"))!=0) {    // params keep-alive
    // rien à faire          
  }
  else if ( ((p=strfield(rcvd,"Set-Cookie:"))!=0) && (cookie) ) {    // ohh un cookie
    char* a = rcvd+p;           // pointeur
    char domain[256];           // domaine cookie (.netscape.com)
    char path[256];             // chemin (/)
    char cook_name[256];        // nom cookie (MYCOOK)
    char cook_value[8192];      // valeur (ID=toto,S=1234)
#if DEBUG_COOK
    printf("set-cookie detected\n");
#endif
    while(*a) {
      char *token_st,*token_end;
      char *value_st,*value_end;
      char name[256];
      char value[8192];
      int next=0;
      name[0]=value[0]='\0';
      //

      // initialiser cookie lu actuellement
      if (adr)
        strcpy(domain,jump_identification(adr));     // domaine
      strcpy(path,"/");         // chemin (/)
      strcpy(cook_name,"");     // nom cookie (MYCOOK)
      strcpy(cook_value,"");    // valeur (ID=toto,S=1234)
      // boucler jusqu'au prochain cookie ou la fin
      do {
        char* start_loop=a;
        while(is_space(*a)) a++;    // sauter espaces
        token_st=a;                 // départ token
        while((!is_space(*a)) && (*a) && (*a!=';') && (*a!='=')) a++;    // arrêter si espace, point virgule
        token_end=a;
        while(is_space(*a)) a++;    // sauter espaces
        if (*a=='=') {    // name=value
          a++;
          while(is_space(*a)) a++;    // sauter espaces
          value_st=a;
          while( (*a!=';') && (*a)) a++;    // prochain ;
          //while( ((*a!='"') || (*(a-1)=='\\')) && (*a)) a++;    // prochain " (et pas \")
          value_end=a;
          //if (*a==';') {  // finit par un ;
          // vérifier débordements
          if ( (((int) (token_end - token_st))<200) && (((int) (value_end - value_st))<8000)
            && (((int) (token_end - token_st))>0)   && (((int) (value_end - value_st))>0) ) {
            name[0]='\0';
            value[0]='\0';
            strncat(name,token_st,(int) (token_end - token_st));
            strncat(value,value_st,(int) (value_end - value_st));
#if DEBUG_COOK
            printf("detected cookie-av: name=\"%s\" value=\"%s\"\n",name,value);
#endif
            if (strfield2(name,"domain")) {
              strcpy(domain,value);
            }
            else if (strfield2(name,"path")) {
              strcpy(path,value);
            }
            else if (strfield2(name,"max-age")) {
              // ignoré..
            }
            else if (strfield2(name,"expires")) {
              // ignoré..
            }
            else if (strfield2(name,"version")) {
              // ignoré..
            }
            else if (strfield2(name,"comment")) {
              // ignoré
            }
            else if (strfield2(name,"secure")) {    // ne devrait pas arriver ici
              // ignoré
            }
            else {
              if (strnotempty(cook_name)==0) {          // noter premier: nom et valeur cookie
                strcpy(cook_name,name);
                strcpy(cook_value,value);
              } else {                             // prochain cookie
                a=start_loop;      // on devra recommencer à cette position
                next=1;            // enregistrer
              }
            }
          }
        }
        if (!next) {
          while((*a!=';') && (*a)) a++;    // prochain
          while(*a==';') a++;             // sauter ;
        }
      } while((*a) && (!next));
      if (strnotempty(cook_name)) {          // cookie?
#if DEBUG_COOK
        printf("new cookie: name=\"%s\" value=\"%s\" domain=\"%s\" path=\"%s\"\n",cook_name,cook_value,domain,path);
#endif
        cookie_add(cookie,cook_name,cook_value,domain,path);
      }
    }
  }
}


// transforme le message statuscode en chaîne
void infostatuscode(char* msg,int statuscode) {
  switch( statuscode) {    
    // Erreurs HTTP, selon RFC
  case 100: strcpy( msg,"Continue"); break; 
  case 101: strcpy( msg,"Switching Protocols"); break; 
  case 200: strcpy( msg,"OK"); break; 
  case 201: strcpy( msg,"Created"); break; 
  case 202: strcpy( msg,"Accepted"); break; 
  case 203: strcpy( msg,"Non-Authoritative Information"); break; 
  case 204: strcpy( msg,"No Content"); break; 
  case 205: strcpy( msg,"Reset Content"); break; 
  case 206: strcpy( msg,"Partial Content"); break; 
  case 300: strcpy( msg,"Multiple Choices"); break; 
  case 301: strcpy( msg,"Moved Permanently"); break; 
  case 302: strcpy( msg,"Moved Temporarily"); break; 
  case 303: strcpy( msg,"See Other"); break; 
  case 304: strcpy( msg,"Not Modified"); break; 
  case 305: strcpy( msg,"Use Proxy"); break; 
  case 306: strcpy( msg,"Undefined 306 error"); break; 
  case 307: strcpy( msg,"Temporary Redirect"); break; 
  case 400: strcpy( msg,"Bad Request"); break; 
  case 401: strcpy( msg,"Unauthorized"); break; 
  case 402: strcpy( msg,"Payment Required"); break; 
  case 403: strcpy( msg,"Forbidden"); break; 
  case 404: strcpy( msg,"Not Found"); break; 
  case 405: strcpy( msg,"Method Not Allowed"); break; 
  case 406: strcpy( msg,"Not Acceptable"); break; 
  case 407: strcpy( msg,"Proxy Authentication Required"); break; 
  case 408: strcpy( msg,"Request Time-out"); break; 
  case 409: strcpy( msg,"Conflict"); break; 
  case 410: strcpy( msg,"Gone"); break; 
  case 411: strcpy( msg,"Length Required"); break; 
  case 412: strcpy( msg,"Precondition Failed"); break; 
  case 413: strcpy( msg,"Request Entity Too Large"); break; 
  case 414: strcpy( msg,"Request-URI Too Large"); break; 
  case 415: strcpy( msg,"Unsupported Media Type"); break; 
  case 416: strcpy( msg,"Requested Range Not Satisfiable"); break; 
  case 417: strcpy( msg,"Expectation Failed"); break; 
  case 500: strcpy( msg,"Internal Server Error"); break; 
  case 501: strcpy( msg,"Not Implemented"); break; 
  case 502: strcpy( msg,"Bad Gateway"); break; 
  case 503: strcpy( msg,"Service Unavailable"); break; 
  case 504: strcpy( msg,"Gateway Time-out"); break; 
  case 505: strcpy( msg,"HTTP Version Not Supported"); break; 
    //
  default: if (strnotempty(msg)==0) strcpy( msg,"Unknown error"); break;
  }
}


// identique au précédent, sauf que l'on donne adr+fil et non url complète
htsblk xhttpget(char* adr,char* fil) {
  T_SOC soc;
  htsblk retour;
  
  memset(&retour, 0, sizeof(htsblk));
  soc=http_fopen(adr,fil,&retour);

  if (soc!=INVALID_SOCKET) {
    http_fread(soc,&retour);
#if HTS_DEBUG_CLOSESOCK
    DEBUG_W("xhttpget: deletehttp\n");
#endif
    if (retour.soc!=INVALID_SOCKET) deletehttp(&retour);  // fermer
    retour.soc=INVALID_SOCKET;
  }
  return retour;
}

// variation sur un thème...
// réceptionne uniquement un en-tête (HEAD)
// retourne dans xx.adr l'adresse pointant sur le bloc de mémoire de l'en tête
htsblk http_gethead(char* adr,char* fil) {
  T_SOC soc;
  htsblk retour;

  memset(&retour, 0, sizeof(htsblk));
  soc=http_xfopen(1,0,1,NULL,adr,fil,&retour);  // HEAD, pas de traitement en-tête

  if (soc!=INVALID_SOCKET) {
    http_fread(soc,&retour);    // réception en-tête
#if HTS_DEBUG_CLOSESOCK
    DEBUG_W("http_gethead: deletehttp\n");
#endif
    if (retour.soc!=INVALID_SOCKET) deletehttp(&retour);  // fermer
    retour.soc=INVALID_SOCKET;
  }
  return retour;
}
// oui ca ressemble vachement à xhttpget - en étant sobre on peut voir LA différence..


// lecture sur une socket ouverte, le header a déja été envoyé dans le cas de GET
// il ne reste plus qu'à lire les données
// (pour HEAD le header est lu ici!)
void http_fread(T_SOC soc,htsblk* retour) {  
  //int bufl=TAILLE_BUFFER;    // 8Ko de buffer
  
  if (retour) retour->soc=soc;
  if (soc!=INVALID_SOCKET) {    
    // fonction de lecture d'une socket (plus propre)
    while(http_fread1(retour)!=-1);
    soc=retour->soc;
    if (retour->adr==NULL) {
      if (strnotempty(retour->msg)==0)
        sprintf(retour->msg,"Unable to read");
      return ;    // erreur
    } 
    
#if HDEBUG
    printf("Ok, données reçues\n");
#endif   

    return ;
    
  } 
  
  return ;
}

// check if data is available
int check_readinput(htsblk* r) {
  if (r->soc != INVALID_SOCKET) {
    fd_set fds;           // poll structures
    struct timeval tv;          // structure for select
    FD_ZERO(&fds);
    FD_SET(r->soc,&fds);           
    tv.tv_sec=0;
    tv.tv_usec=0;
    select(r->soc + 1,&fds,NULL,NULL,&tv);
    if (FD_ISSET(r->soc,&fds))
      return 1;
    else
      return 0;
  } else
    return 0;
}

// lecture d'un bloc sur une socket (ou un fichier!)
// >=0 : nombre d'octets lus
// <0 : fin ou erreur
HTS_INLINE LLint http_fread1(htsblk* r) {
  //int bufl=TAILLE_BUFFER;  // taille d'un buffer max.
  return http_xfread1(r,TAILLE_BUFFER);
}

// idem, sauf qu'ici on peut choisir la taille max de données à recevoir
// SI bufl==0 alors le buffer est censé être de 8kos, et on recoit par bloc de lignes
// en éliminant les cr (ex: header), arrêt si double-lf
// SI bufl==-1 alors le buffer est censé être de 8kos, et on recoit ligne par ligne
// en éliminant les cr (ex: header), arrêt si double-lf
// Note: les +1 dans les malloc sont dûs à l'octet nul rajouté en fin de fichier
LLint http_xfread1(htsblk* r,int bufl) {
  int nl=-1;

  if (bufl>0) {
    if (!r->is_write) {     // stocker en mémoire
      if (r->totalsize>0) {    // totalsize déterminé ET ALLOUE
        if (r->adr==NULL) {
          r->adr=(char*) malloct((INTsys) r->totalsize + 1);
          r->size=0;
        }
        if (r->adr!=NULL) {
          // lecture
          nl = hts_read(r,r->adr + ((int) r->size),(int) (r->totalsize-r->size) );     /* NO 32 bit overlow possible here (no 4GB html!) */
          // nouvelle taille
          if (nl >= 0) r->size+=nl;
          
          if ((nl < 0) || (r->size >= r->totalsize))
            nl=-1;  // break
          
          r->adr[r->size]='\0';    // caractère NULL en fin au cas où l'on traite des HTML
        }
        
      } else {                 // inconnu..
        // réserver de la mémoire?
        if (r->adr==NULL) {
#if HDEBUG
          printf("..alloc xfread\n");
#endif
          r->adr=(char*) malloct(bufl + 1);
          r->size=0;
        }
        else {
#if HDEBUG
          printf("..realloc xfread1\n");
#endif
          r->adr=(char*) realloct(r->adr,(int)r->size+bufl + 1);
        }
        
        if (r->adr!=NULL) {
          // lecture
          nl = hts_read(r,r->adr+(int)r->size,bufl);
          if (nl>0) {
            // resize
            r->adr=(char*) realloct(r->adr,(int)r->size+nl + 1);
            // nouvelle taille
            r->size+=nl;
            // octet nul
            if (r->adr) r->adr[r->size]='\0';

          } // sinon on a fini
#if HDEBUG
          else if (nl < 0)
            printf("..end read (%d)\n", nl);
#endif
        }
#if HDEBUG
        else printf("..-> error\n");
#endif
      }

      // pas de adr=erreur
      if (r->adr==NULL) nl=-1;

    } else {    // stocker sur disque
      char* buff;
      buff=(char*) malloct(bufl);
      if (buff!=NULL) {
        // lecture
        nl = hts_read(r,buff,bufl);
        // nouvelle taille
        if (nl > 0) { 
          r->size+=nl;
          if ((int) fwrite(buff,1,nl,r->out)!=nl) {
            r->statuscode=-1;
            strcpy(r->msg,"Write error on disk");
            nl=-1;
          }
        }

        if ((nl < 0) || ((r->totalsize>0) && (r->size >= r->totalsize)))
          nl=-1;  // break

        // libérer bloc tempo
        freet(buff);
      } else
        nl=-1;
      
      if ((nl < 0) && (r->out!=NULL)) {
        fflush(r->out); 
      }
        
        
    } // stockage disque ou mémoire

  } else if (bufl == -2) {  // force reserve
    if (r->adr==NULL) {
      r->adr=(char*) malloct(8192);
      r->size=0;
      return 0;
    }
    return -1;
  } else {    // réception d'un en-tête octet par octet
    int count=256;
    int tot_nl=0;
    int lf_detected=0;
    int at_begining=1;
    do {
      nl=-1;
      count--;
      if (r->adr==NULL) {
        r->adr=(char*) malloct(8192);
        r->size=0;
      }
      if (r->adr!=NULL) {
        if (r->size < 8190) {
          // lecture
          nl = hts_read(r,r->adr+r->size,1);
          if (nl>0) {
            // exit if:
            // lf detected AND already detected before
            // or
            // lf detected AND first character read
            if (*(r->adr+r->size) == 10) {
              if (lf_detected || (at_begining) || (bufl<0))
                count=-1;
              lf_detected=1;
            }
            if (*(r->adr+r->size) != 13) {   // sauter caractères 13
              if (
                (*(r->adr+r->size) != 10)
                &&
                (*(r->adr+r->size) != 13)
                ) {
                // restart for new line
                lf_detected=0;
              }
              (r->size)++;
              at_begining=0;
            }
            *(r->adr+r->size)='\0';    // terminer par octet nul
          }
        }
      }
      if (nl >= 0) {
        tot_nl+=nl;
        if (!check_readinput(r))
          count=-1;
      }
    } while((nl >= 0) && (count>0));
    nl = tot_nl;
  }
#if HDEBUG
  //printf("add to %d / %d\n",r->size,r->totalsize);
#endif
  // nl == 0 may mean "no relevant data", for example is using cache or ssl
#if HTS_USEOPENSSL
  if (r->ssl)
    return nl;
  else
#endif
    return ((nl > 0) ? nl : -1);        // ==0 is fatal if direct read
}


// teste une adresse, et suit l'éventuel chemin "moved"
// retourne 200 ou le code d'erreur (404=NOT FOUND, etc)
// copie dans loc la véritable adresse si celle-ci est différente
htsblk http_location(char* adr,char* fil,char* loc) {
  htsblk retour;
  int retry=0;
  int tryagain;
  // note: "RFC says"
  // 5 boucles au plus, on en teste au plus 8 ici
  // sinon abandon..
  do {
    tryagain=0;
    switch ((retour=http_test(adr,fil,loc)).statuscode) {
    case 200: break;   // ok!
    case 301: case 302: case 303: case 307: // moved!
      // recalculer adr et fil!
      if (ident_url_absolute(loc,adr,fil)!=-1) {
        tryagain=1;  // retenter
        retry++;     // ..encore une fois
      }
    }
  } while((tryagain) && (retry<5+3));
  return retour;
}


// teste si une URL (validité, header, taille)
// retourne 200 ou le code d'erreur (404=NOT FOUND, etc)
// en cas de moved xx, dans location
// abandonne désormais au bout de 30 secondes (aurevoir les sites
// qui nous font poireauter 5 heures..) -> -2=timeout
htsblk http_test(char* adr,char* fil,char* loc) {
  T_SOC soc;
  htsblk retour;
  //int rcvsize=-1;
  //char* rcv=NULL;    // adresse de retour
  //int bufl=TAILLE_BUFFER;    // 8Ko de buffer
  TStamp tl;
  int timeout=30;  // timeout pour un check (arbitraire) // **

  // pour abandonner un site trop lent
  tl=time_local();

  loc[0]='\0';
  memset(&retour, 0, sizeof(htsblk));    // effacer
  retour.location=loc;    // si non nul, contiendra l'adresse véritable en cas de moved xx

  //soc=http_fopen(adr,fil,&retour,NULL);  // ouvrir, + header

  // on ouvre en head, et on traite l'en tête
  soc=http_xfopen(1,0,1,NULL,adr,fil,&retour);  // ouvrir HEAD, + envoi header
  
  if (soc!=INVALID_SOCKET) {
    int e=0;
    // tant qu'on a des données, et qu'on ne recoit pas deux LF, et que le timeout n'arrie pas
    do {
      if (http_xfread1(&retour,0) < 0)
        e=1;
      else {
        if (retour.adr!=NULL) {
          if ((retour.adr[retour.size-1]!=10) || (retour.adr[retour.size-2]!=10))
            e=1;
        }
      }
            
      if (!e) {
        if ((time_local()-tl)>=timeout) {
          e=-1;
        }
      }
      
    } while (!e);
    
    if (e==1) {
      if (adr!=NULL) {
        int ptr=0;
        char rcvd[1100];

        // note: en gros recopie du traitement de back_wait()
        //


        // ----------------------------------------
        // traiter en-tête!
        // status-line à récupérer
        ptr+=binput(retour.adr+ptr,rcvd,1024);
        if (strnotempty(rcvd)==0)
          ptr+=binput(retour.adr+ptr,rcvd,1024);    // "certains serveurs buggés envoient un \n au début" (RFC)
        
        // traiter status-line
        treatfirstline(&retour,rcvd);
        
#if HDEBUG
        printf("(Buffer) Status-Code=%d\n",retour.statuscode);
#endif
        
        // en-tête
        
        // header // ** !attention! HTTP/0.9 non supporté
        do {
          ptr+=binput(retour.adr+ptr,rcvd,1024);          
#if HDEBUG
          printf("(buffer)>%s\n",rcvd);      
#endif
          if (strnotempty(rcvd))
            treathead(NULL,NULL,NULL,&retour,rcvd);  // traiter
          
        } while(strnotempty(rcvd));
        // ----------------------------------------                    
        
        // libérer mémoire
        if (retour.adr!=NULL) { freet(retour.adr); retour.adr=NULL; }
      }
    } else {
      retour.statuscode=-2;
      strcpy(retour.msg,"Timeout While Testing");
    }
    
    
#if HTS_DEBUG_CLOSESOCK
    DEBUG_W("http_test: deletehttp\n");
#endif
    deletehttp(&retour);
    retour.soc=INVALID_SOCKET;
  }
  return retour;    
}

// Crée un lien (http) vers une adresse internet iadr
// retour: structure (adresse, taille, message si erreur (si !adr))
// peut ouvrir avec des connect() non bloquants: waitconnect=0/1
int newhttp(char* _iadr,htsblk* retour,int port,int waitconnect) {  
  t_fullhostent fullhostent_buffer;    // buffer pour resolver
  T_SOC soc;                           // descipteur de la socket
  char* iadr;
  // unsigned short int port;
  
  // tester un éventuel id:pass et virer id:pass@ si détecté
  iadr = jump_identification(_iadr);
  
  // si iadr="#" alors c'est une fausse URL, mais un vrai fichier
  // local.
  // utile pour les tests!
  //## if (iadr[0]!=lOCAL_CHAR) {
  if (strcmp(_iadr,"file://")) {           /* non fichier */
    SOCaddr server;
    int server_size=sizeof(server);
    t_hostent* hp;    
    // effacer structure
    memset(&server, 0, sizeof(server));

#if HDEBUG
    printf("gethostbyname\n");
#endif
    
    // tester un éventuel port
    if (port==-1) {
      char *a=jump_toport(iadr);
#if HTS_USEOPENSSL
      if (retour->ssl)
        port=443;
      else
        port=80;    // port par défaut
#else
      port=80;    // port par défaut
#endif
      if (a) {
        char iadr2[HTS_URLMAXSIZE*2];
        int i=-1;
        iadr2[0]='\0';
        sscanf(a+1,"%d",&i);
        if (i!=-1) {
          port=(unsigned short int) i;
        }
        
        // adresse véritable (sans :xx)
        strncat(iadr2,iadr,(int) (a - iadr));

        // adresse sans le :xx
        hp = hts_gethostbyname(iadr2, &fullhostent_buffer);
        
      } else {

        // adresse normale (port par défaut par la suite)
        hp = hts_gethostbyname(iadr, &fullhostent_buffer);
        
      }
      
    } else    // port défini
      hp = hts_gethostbyname(iadr, &fullhostent_buffer);

    
    // Conversion iadr -> adresse
    // structure recevant le nom de l'hôte, etc
    //struct 	hostent 	*hp;
    if (hp == NULL) {
#if DEBUG
      printf("erreur gethostbyname\n");
#endif
      if (retour)
      if (retour->msg)
        strcpy(retour->msg,"Unable to get server's address");
      return INVALID_SOCKET;
    }  
    // copie adresse
    SOCaddr_copyaddr(server, server_size, hp->h_addr_list[0], hp->h_length);
    // memcpy(&SOCaddr_sinaddr(server), hp->h_addr_list[0], hp->h_length);
     
    // créer ("attachement") une socket (point d'accès) internet,en flot
#if HDEBUG
    printf("socket\n");
#endif
#if HTS_WIDE_DEBUG    
    DEBUG_W("socket\n");
#endif
    soc=socket(SOCaddr_sinfamily(server), SOCK_STREAM, 0);
#if HTS_WIDE_DEBUG    
    DEBUG_W("socket done\n");
#endif
    if (soc==INVALID_SOCKET) {
      if (retour)
      if (retour->msg)
        strcpy(retour->msg,"Unable to create a socket");
      return INVALID_SOCKET;                        // erreur création socket impossible
    }
    // structure: connexion au domaine internet, port 80 (ou autre)
    SOCaddr_initport(server, port);
#if HDEBUG
    printf("==%d\n",soc);
#endif

    // connexion non bloquante?
    if (!waitconnect ) {
      unsigned long p=1;  // non bloquant
#if HTS_WIN
      ioctlsocket(soc,FIONBIO,&p);
#else
      ioctl(soc,FIONBIO,&p);
#endif
    }
    
    // Connexion au serveur lui même
#if HDEBUG
    printf("connect\n");
#endif
    
#if HTS_WIDE_DEBUG
    DEBUG_W("connect\n");
#endif
#if HTS_WIN
    if (connect(soc, (const struct sockaddr FAR *)&server, server_size) != 0) {
#else
      if (connect(soc, (struct sockaddr *)&server, server_size) == -1) {
#endif

        // no - non blocking
        //deletesoc(soc);
        //soc=INVALID_SOCKET;

        // bloquant
        if (waitconnect) {
#if HDEBUG
          printf("unable to connect!\n");
#endif
          if (retour)
          if (retour->msg)
            strcpy(retour->msg,"Unable to connect to the server");
          /* Close the socket and notify the error!!! */
          deletesoc(soc);
          return INVALID_SOCKET;
        }
      }
#if HTS_WIDE_DEBUG    
      DEBUG_W("connect done\n");
#endif
      
#if HDEBUG
      printf("connexion établie\n");
#endif
    
    // A partir de maintenant, on peut envoyer et recevoir des données
    // via le flot identifié par soc (socket): write(soc,adr,taille) et 
    // read(soc,adr,taille)

  } else {    // on doit ouvrir un fichier local!
    // il sera géré de la même manière qu'une socket (c'est idem!)

    soc=LOCAL_SOCKET_ID;    // pseudo-socket locale..
    // soc sera remplacé lors d'un http_fopen() par un handle véritable!

  }   // teste fichier local ou http
  
  return soc;
}



// couper http://www.truc.fr/pub/index.html -> www.truc.fr /pub/index.html
// retour=-1 si erreur.
// si file://... alors adresse=file:// (et coupe le ?query dans ce cas)
int ident_url_absolute(char* url,char* adr,char* fil) {
  int pos=0;
  int scheme=0;

  // effacer adr et fil
  adr[0]=fil[0]='\0';
  
#if HDEBUG
  printf("protocol: %s\n",url);
#endif

  // Scheme?
  {
    char* a=url;
    while (isalpha((unsigned char)*a))
      a++;
    if (*a == ':')
      scheme=1;
  }

  // 1. optional scheme ":"
  if ((pos=strfield(url,"file:"))) {    // fichier local!! (pour les tests)
    //!! p+=3;
    strcpy(adr,"file://");
  } else if ((pos=strfield(url,"http:"))) {    // HTTP
    //!!p+=3;
  } else if ((pos=strfield(url,"ftp:"))) {    // FTP
    strcpy(adr,"ftp://");    // FTP!!
    //!!p+=3;
#if HTS_USEOPENSSL
  } else if ((pos=strfield(url,"https:"))) {    // HTTPS
    strcpy(adr,"https://");
#endif
  } else if (scheme) {
    return -1;    // erreur non reconnu
  } else
    pos=0;

  // 2. optional "//" authority
  if (strncmp(url+pos,"//",2)==0)
    pos+=2;

  // (url+pos) now points to the path (not net path)

  //## if (adr[0]!=lOCAL_CHAR) {    // adresse normale http
  if (!strfield(adr,"file:")) {      // PAS file://
    char *p,*q;
    p=url+pos;

    // p pointe sur le début de l'adresse, ex: www.truc.fr/sommaire/index.html
    q=strchr(jump_identification(p),'/');
    if (q==0) q=strchr(jump_identification(p),'?');     // http://www.foo.com?bar=1
    if (q==0) q=p+strlen(p);  // pointe sur \0
    // q pointe sur le chemin, ex: index.html?query=recherche
    
    // chemin www... trop long!!
    if ( ( ((int) (q - p)) )  > HTS_URLMAXSIZE) {
      //strcpy(retour.msg,"Path too long");
      return -1;    // erreur
    }
    
    // recopier adresse www..
    strncat(adr,p, ((int) (q - p)) );
    // *( adr+( ((int) q) - ((int) p) ) )=0;  // faut arrêter la fumette!
    // recopier chemin /pub/..
    if (q[0] != '/')    // page par défaut (/)
      strcat(fil,"/");
    strcat(fil,q);
    // SECURITE:
    // simplifier url pour les ../
    fil_simplifie(fil);
  } else {    // localhost file://
    char *p;
    int i;
    char* a;

    p=url+pos;
    
    strcat(fil,p);    // fichier local ; adr="#"
    a=strchr(fil,'?');
    if (a) 
      *a='\0';      /* couper query (inutile pour file:// lors de la requête) */
    // filtrer les \\ -> / pour les fichiers DOS
    for(i=0;i<(int) strlen(fil);i++)
      if (fil[i]=='\\')
        fil[i]='/';
  }

  // no hostname
  if (!strnotempty(adr))
    return -1;    // erreur non reconnu

  // nommer au besoin.. (non utilisé normalement)
  if (!strnotempty(fil))
    strcpy(fil,"default-index.html");

  // case insensitive pour adresse
  {
    char *a=jump_identification(adr);
    while(*a) {
      if ((*a>='A') && (*a<='Z'))
        *a+='a'-'A';       
      a++;
    }
  }
  
  return 0;
}

// simplification des ../
void fil_simplifie(char* f) {
  int i=0;
  int last=0;
  char* a;
 
  // éliminer ../
  while (f[i]) {
    
    if (f[i]=='/') {
      if (f[i+1]=='.')
      if (f[i+2]=='.')      // couper dernier répertoire
      if (f[i+3]=='/')      // éviter les /tmp/..coolandlamedir/
      {    // couper dernier répertoire
        char tempo[HTS_URLMAXSIZE*2];
        tempo[0]='\0';
        //
        if (!last)                /* can't go upper.. */
          strcpy(tempo,"/");
        else
          strncpy(tempo,f,last+1);
        tempo[last+1]='\0';
        strcat(tempo,f+i+4);
        strcpy(f,tempo);    // remplacer
        i=-1;             // recommencer
        last=0;
      }
      
      if (i>=0)
        last=i;
      else
        last=0;
    }
    
    i++;
  }

  // éliminer ./
  while ( (a=strstr(f,"./")) ) {
    char tempo[HTS_URLMAXSIZE*2];
    tempo[0]='\0';
    strcpy(tempo,a+2);
    strcpy(a,tempo);
  }
  // delete all remaining ../ (potential threat)
  while ( (a=strstr(f,"../")) ) {
    char tempo[HTS_URLMAXSIZE*2];
    tempo[0]='\0';
    strcpy(tempo,a+3);
    strcpy(a,tempo);
  }
  
}


// fermer liaison fichier ou socket
HTS_INLINE void deletehttp(htsblk* r) {
#if HTS_DEBUG_CLOSESOCK
    char info[256];
    sprintf(info,"deletehttp: (htsblk*) %d\n",r);
    DEBUG_W2(info);
#endif
  if (r->soc!=INVALID_SOCKET) {
    if (r->is_file) {
      if (r->fp)
        fclose(r->fp);
      r->fp=NULL;
    } else {
      if (r->soc!=LOCAL_SOCKET_ID)
        deletesoc_r(r);
    }
    r->soc=INVALID_SOCKET;
  }
}

// fermer une socket
HTS_INLINE void deletesoc(T_SOC soc) {
  if (soc!=INVALID_SOCKET) {
// J'ai planté.. pas de shutdown
//#if HTS_WIDE_DEBUG    
//    DEBUG_W("shutdown\n");
//#endif
//    shutdown(soc,2);  // shutdown
//#if HTS_WIDE_DEBUG    
//    DEBUG_W("shutdown done\n");
//#endif
    // Ne pas oublier de fermer la connexion avant de partir.. (plus propre)
#if HTS_WIDE_DEBUG    
    DEBUG_W("close\n");
#endif
#if HTS_WIN
    closesocket(soc);
#else
    close(soc);
#endif
#if HTS_WIDE_DEBUG    
    DEBUG_W("close done\n");
#endif
  }
}

/* Will also clean other things */
HTS_INLINE void deletesoc_r(htsblk* r) {
#if HTS_USEOPENSSL
  if (r->ssl_con) {
    SSL_shutdown(r->ssl_con);
    // SSL_CTX_set_quiet_shutdown(r->ssl_con->ctx, 1);
    SSL_free(r->ssl_con);
    r->ssl_con=NULL;
  }
#endif
  deletesoc(r->soc);
  r->soc=INVALID_SOCKET;
}

// renvoi le nombre de secondes depuis 1970
HTS_INLINE TStamp time_local(void) {
  return ((TStamp) time(NULL));
}

// number of millisec since 1970
HTS_INLINE TStamp mtime_local(void) {
#ifndef HTS_DO_NOT_USE_FTIME
  struct timeb B;
  ftime( &B );
  return (TStamp) ( ((TStamp) B.time * (TStamp) 1000)
        + ((TStamp) B.millitm) );
#else
  // not precise..
  return (TStamp) ( ((TStamp) time_local() * (TStamp) 1000)
        + ((TStamp) 0) );
#endif
}

// convertit un nombre de secondes en temps (chaine)
void sec2str(char *st,TStamp t) {
  int j,h,m,s;
  
  j=(int) (t/(3600*24));
  t-=((TStamp) j)*(3600*24);
  h=(int) (t/(3600));
  t-=((TStamp) h)*3600;
  m=(int) (t/60);
  t-=((TStamp) m)*60;
  s=(int) t;
  
  if (j>0)
    sprintf(st,"%d days, %d hours %d minutes %d seconds",j,h,m,s);
  else if (h>0)
    sprintf(st,"%d hours %d minutes %d seconds",h,m,s);
  else if (m>0)
    sprintf(st,"%d minutes %d seconds",m,s);
  else
    sprintf(st,"%d seconds",s);
}

// idem, plus court (chaine)
void qsec2str(char *st,TStamp t) {
  int j,h,m,s;
  
  j=(int) (t/(3600*24));
  t-=((TStamp) j)*(3600*24);
  h=(int) (t/(3600));
  t-=((TStamp) h)*3600;
  m=(int) (t/60);
  t-=((TStamp) m)*60;
  s=(int) t;
  
  if (j>0)
    sprintf(st,"%dd,%02dh,%02dmin%02ds",j,h,m,s);
  else if (h>0)
    sprintf(st,"%dh,%02dmin%02ds",h,m,s);
  else if (m>0)
    sprintf(st,"%dmin%02ds",m,s);
  else
    sprintf(st,"%ds",s);
}


// heure actuelle, GMT, format rfc (taille buffer 256o)
void time_gmt_rfc822(char* s) {
  time_t tt;
  struct tm* A;
  tt=time(NULL);
  A=gmtime(&tt);
  if (A==NULL)
    A=localtime(&tt);
  time_rfc822(s,A);
}

// heure actuelle, format rfc (taille buffer 256o)
void time_local_rfc822(char* s) {
  time_t tt;
  struct tm* A;
  tt=time(NULL);
  A=localtime(&tt);
  time_rfc822_local(s,A);
}

/* convertir une chaine en temps */
struct tm* convert_time_rfc822(char* s) {
  struct tm* result;
  /* */
  char months[]="jan feb mar apr may jun jul aug sep oct nov dec";
  char str[256];
  char* a;
  /* */
  int result_mm=-1;
  int result_dd=-1;
  int result_n1=-1;
  int result_n2=-1;
  int result_n3=-1;
  int result_n4=-1;
  /* */
  NOSTATIC_RESERVE(result, struct tm, 1);

  if ((int) strlen(s) > 200)
    return NULL;
  strcpy(str,s);
  hts_lowcase(str);
  /* éliminer :,- */
  while( (a=strchr(str,'-')) ) *a=' ';
  while( (a=strchr(str,':')) ) *a=' ';
  while( (a=strchr(str,',')) ) *a=' ';
  /* tokeniser */
  a=str;
  while(*a) {
    char *first,*last;
    char tok[256];
    /* découper mot */
    while(*a==' ') a++;   /* sauter espaces */
    first=a;
    while((*a) && (*a!=' ')) a++;
    last=a;
    tok[0]='\0';
    if (first!=last) {
      char* pos;
      strncat(tok,first,(int) (last - first));
      /* analyser */
      if ( (pos=strstr(months,tok)) ) {               /* month always in letters */
        result_mm=((int) (pos - months))/4;
      } else {
        int number;
        if (sscanf(tok,"%d",&number) == 1) {      /* number token */
          if (result_dd<0)                        /* day always first number */
            result_dd=number;
          else if (result_n1<0)
            result_n1=number;
          else if (result_n2<0)
            result_n2=number;
          else if (result_n3<0)
            result_n3=number;
          else if (result_n4<0)
            result_n4=number;
        }   /* sinon, bruit de fond(+1GMT for exampel) */
      }
    }
  }
  if ((result_n1>=0) && (result_mm>=0) && (result_dd>=0) && (result_n2>=0) && (result_n3>=0) && (result_n4>=0)) {
    if (result_n4>=1000) {               /* Sun Nov  6 08:49:37 1994 */
      result->tm_year=result_n4-1900;
      result->tm_hour=result_n1;
      result->tm_min=result_n2;
      result->tm_sec=max(result_n3,0);
    } else {                            /* Sun, 06 Nov 1994 08:49:37 GMT or Sunday, 06-Nov-94 08:49:37 GMT */
      result->tm_hour=result_n2;
      result->tm_min=result_n3;
      result->tm_sec=max(result_n4,0);
      if (result_n1<=50)                /* 00 means 2000 */
        result->tm_year=result_n1+100;
      else if (result_n1<1000)          /* 99 means 1999 */
        result->tm_year=result_n1;
      else                              /* 2000 */
        result->tm_year=result_n1-1900;
    }
    result->tm_isdst=0;        /* assume GMT */
    result->tm_yday=-1;        /* don't know */
    result->tm_wday=-1;        /* don't know */
    result->tm_mon=result_mm;
    result->tm_mday=result_dd;
    return result;
  }
  return NULL;
}

/* sets file time. -1 if error */
int set_filetime(char* file,struct tm* tm_time) {
  struct utimbuf tim;
#ifndef HTS_DO_NOT_USE_FTIME
  struct timeb B;
  B.timezone=0;
  ftime( &B );
  tim.actime=tim.modtime=mktime(tm_time) - B.timezone*60; 
#else
  // bogus time (GMT/local)..
  tim.actime=tim.modtime=mktime(tm_time); 
#endif
  return utime(file,&tim);
}

/* sets file time from RFC822 date+time, -1 if error*/
int set_filetime_rfc822(char* file,char* date) {
  struct tm* tm_s=convert_time_rfc822(date);
  if (tm_s) {
    return set_filetime(file,tm_s);
  } else return -1;
}


// heure au format rfc (taille buffer 256o)
HTS_INLINE void time_rfc822(char* s,struct tm * A) {
  strftime(s,256,"%a, %d %b %Y %H:%M:%S GMT",A);
}

// heure locale au format rfc (taille buffer 256o)
HTS_INLINE void time_rfc822_local(char* s,struct tm * A) {
  strftime(s,256,"%a, %d %b %Y %H:%M:%S",A);
}

// conversion en b,Kb,Mb
char* int2bytes(LLint n) {
  char** a=int2bytes2(n);
  char* buff;
  NOSTATIC_RESERVE(buff, char, 256);

  strcpy(buff,a[0]);
  strcat(buff,a[1]);
  return concat(buff,"");
}

// conversion en b/s,Kb/s,Mb/s
char* int2bytessec(long int n) {
  char* buff;
  char** a=int2bytes2(n);
  NOSTATIC_RESERVE(buff, char, 256);

  strcpy(buff,a[0]);
  strcat(buff,a[1]);
  return concat(buff,"/s");
}
char* int2char(int n) {
  char* buffer;
  NOSTATIC_RESERVE(buffer, char, 32);
  sprintf(buffer,"%d",n);
  return concat(buffer,"");
}

// conversion en b,Kb,Mb, nombre et type séparés
// limite: 2.10^9.10^6B

/* See http://physics.nist.gov/cuu/Units/binary.html */
#define ToLLint(a) ((LLint)(a))
#define ToLLintKiB (ToLLint(1024))
#define ToLLintMiB (ToLLintKiB*ToLLintKiB)
#ifdef HTS_LONGLONG
#define ToLLintGiB (ToLLintKiB*ToLLintKiB*ToLLintKiB)
#define ToLLintTiB (ToLLintKiB*ToLLintKiB*ToLLintKiB*ToLLintKiB)
#define ToLLintPiB (ToLLintKiB*ToLLintKiB*ToLLintKiB*ToLLintKiB*ToLLintKiB)
#endif
typedef struct {
  char buff1[256];
  char buff2[32];
  char* buffadr[2];
} strc_int2bytes2;
char** int2bytes2(LLint n) {
  strc_int2bytes2* strc;
  NOSTATIC_RESERVE(strc, strc_int2bytes2, 1);

  if (n < ToLLintKiB) {
    sprintf(strc->buff1,"%d",(int)(LLint)n);
    strcpy(strc->buff2,"B");
  } else if (n < ToLLintMiB) {
    sprintf(strc->buff1,"%d,%02d",(int)((LLint)(n/ToLLintKiB)),(int)((LLint)((n%ToLLintKiB)*100)/ToLLintKiB));
    strcpy(strc->buff2,"KiB");
  }
#ifdef HTS_LONGLONG
  else if (n < ToLLintGiB) {
    sprintf(strc->buff1,"%d,%02d",(int)((LLint)(n/(ToLLintMiB))),(int)((LLint)(((n%(ToLLintMiB))*100)/(ToLLintMiB))));
    strcpy(strc->buff2,"MiB");
  } else if (n < ToLLintTiB) {
    sprintf(strc->buff1,"%d,%02d",(int)((LLint)(n/(ToLLintGiB))),(int)((LLint)(((n%(ToLLintGiB))*100)/(ToLLintGiB))));
    strcpy(strc->buff2,"GiB");
  } else if (n < ToLLintPiB) {
    sprintf(strc->buff1,"%d,%02d",(int)((LLint)(n/(ToLLintTiB))),(int)((LLint)(((n%(ToLLintTiB))*100)/(ToLLintTiB))));
    strcpy(strc->buff2,"TiB");
  } else {
    sprintf(strc->buff1,"%d,%02d",(int)((LLint)(n/(ToLLintPiB))),(int)((LLint)(((n%(ToLLintPiB))*100)/(ToLLintPiB))));
    strcpy(strc->buff2,"PiB");
  }
#else
  else {
    sprintf(strc->buff1,"%d,%02d",(int)((LLint)(n/(ToLLintMiB))),(int)((LLint)(((n%(ToLLintMiB))*100)/(ToLLintMiB))));
    strcpy(strc->buff2,"MiB");
  }
#endif
  strc->buffadr[0]=strc->buff1;
  strc->buffadr[1]=strc->buff2;
  return strc->buffadr;
}

#if HTS_WIN
#else
// ignore sigpipe?
int sig_ignore_flag( int setflag ) {     // flag ignore
  static int flag=0;   /* YES, this one is true static */
  if (setflag>=0)
    flag=setflag;
  return flag;
}
#endif

// envoi de texte (en têtes généralement) sur la socket soc
HTS_INLINE int sendc(htsblk* r, char* s) {
  int n;

#if HTS_WIN
#else
  sig_ignore_flag(1);
#endif
#if HDEBUG
  write(0,s,strlen(s));
#endif

#if HTS_USEOPENSSL
  if (r->ssl) {
    n = SSL_write(r->ssl_con, s, strlen(s));
  } else
#endif
    n = send(r->soc,s,strlen(s),0);

#if HTS_WIN
#else
  sig_ignore_flag(0);
#endif

  return n;
}


// Remplace read
void finput(int fd,char* s,int max) {
  char c;
  int j=0;
  do {
    //c=fgetc(fp);
    if (read(fd,&c,1)<=0) {
      c=0;
    }
    if (c!=0) {
      switch(c) {
      case 10: c=0; break;
      case 13: break;  // sauter ces caractères
      default: s[j++]=c; break;
      }
    }
  }  while((c!=0) && (j<max-1));
  s[j++]='\0';
} 

// Like linput, but in memory (optimized)
int binput(char* buff,char* s,int max) {
  char* end;
  int count;

  // clear buffer
  s[0]='\0';
  // end of buffer?
  if ( *buff == '\0')
    return 1;
  // find ending \n
  end=strchr(buff,'\n');
  // ..or end of buffer
  if (!end)
    end=buff+strlen(buff);
  // then count number of bytes, maximum=max
  count=min(max,end-buff);
  // and strip annoying ending cr
  while( (count>0) && (buff[count] == '\r'))
    count--;
  // copy
  if (count > 0) {
    strncat(s, buff, count);
  }
  // and terminate with a null char
  s[count]='\0';
  // then return the supplemental jump offset
  return (end-buff)+1;
} 

// Lecture d'une ligne (peut être unicode à priori)
int linput(FILE* fp,char* s,int max) {
  int c;
  int j=0;
  do {
    c=fgetc(fp);
    if (c!=EOF) {
      switch(c) {
        case 13: break;  // sauter CR
        case 10: c=-1; break;
        case 9: case 12: break;  // sauter ces caractères
        default: s[j++]=(char) c; break;
      }
    }
  }  while((c!=-1) && (c!=EOF) && (j<(max-1)));
  s[j]='\0';
  return j;
}
int linput_trim(FILE* fp,char* s,int max) {
  int rlen=0;
  char* ls=(char*) malloct(max+2);
  s[0]='\0';
  if (ls) {
    char* a;
    // lire ligne
    rlen=linput(fp,ls,max);
    if (rlen) {
      // sauter espaces et tabs en fin
      while( (rlen>0) && ((ls[max(rlen-1,0)]==' ') || (ls[max(rlen-1,0)]=='\t')) )
        ls[--rlen]='\0';
      // sauter espaces en début
      a=ls;
      while((rlen>0) && ((*a==' ') || (*a=='\t'))) {
        a++;
        rlen--;
      }
      if (rlen>0) {
        memcpy(s,a,rlen);      // can copy \0 chars
        s[rlen]='\0';
      }
    }
    //
    freet(ls);
  }
  return rlen;
}
int linput_cpp(FILE* fp,char* s,int max) {
  int rlen=0;
  s[0]='\0';
  do {
    int ret;
    if (rlen>0)
    if (s[rlen-1]=='\\')
      s[--rlen]='\0';      // couper \ final
    // lire ligne
    ret=linput_trim(fp,s+rlen,max-rlen);
    if (ret>0)
      rlen+=ret;
  } while((s[max(rlen-1,0)]=='\\') && (rlen<max));
  return rlen;
}

// idem avec les car spéciaux
void rawlinput(FILE* fp,char* s,int max) {
  int c;
  int j=0;
  do {
    c=fgetc(fp);
    if (c!=EOF) {
      switch(c) {
        case 13: break;  // sauter CR
        case 10: c=-1; break;
        default: s[j++]=(char) c; break;
      }
    }
  }  while((c!=-1) && (c!=EOF) && (j<(max-1)));
  s[j++]='\0';
}


// compare le début de f avec s et retourne la position de la fin
// 'A=a' (case insensitive)
int strfield(const char* f,const char* s) {
  int r=0;
  while (streql(*f,*s) && ((*f)!=0) && ((*s)!=0)) { f++; s++; r++; }
  if (*s==0)
    return r;
  else
    return 0;
}

//cherche chaine, case insensitive
char* strstrcase(char *s,char *o) {
  while((*s) && (strfield(s,o)==0)) s++;
  if (*s=='\0') return NULL;
  return s;  
}


// Unicode detector
// See http://www.unicode.org/unicode/reports/tr28/
// (sect Table 3.1B. Legal UTF-8 Byte Sequences)
typedef struct {
  unsigned int pos;
  unsigned char data[4];
} t_auto_seq;

// char between a and b
#define CHAR_BETWEEN(c, a, b)       ( (c) >= 0x##a ) && ( (c) <= 0x##b )
// sequence start
#define SEQBEG                      ( inseq == 0 )
// in this block
#define BLK(n,a, b)                 ( (seq.pos >= n) && ((err = CHAR_BETWEEN(seq.data[n], a, b))) )
#define ELT(n,a)                    BLK(n,a,a)
// end
#define SEQEND                      ((ok = 1))
// sequence started, character will fail if error
#define IN_SEQ                      ( (inseq = 1) )
// decoding error
#define BAD_SEQ                     ( (ok == 0) && (inseq != 0) && (!err) )
// no sequence started
#define NO_SEQ                      ( inseq == 0 )

// is this block an UTF unicode textfile?
// 0 : no
// 1 : yes
// -1: don't know
int is_unicode_utf8(unsigned char* buffer, unsigned int size) {
  t_auto_seq seq;
  unsigned int i;
  int is_utf=-1;

  seq.pos=0;
  for(i=0 ; i < size ; i++) {
    unsigned int ok=0;
    unsigned int inseq=0;
    unsigned int err=0;

    seq.data[seq.pos]=buffer[i];
    /**/ if ( SEQBEG && BLK(0,00,7F) && IN_SEQ && SEQEND                                                 ) { }
    else if ( SEQBEG && BLK(0,C2,DF) && IN_SEQ && BLK(1,80,BF) && SEQEND                                 ) { }
    else if ( SEQBEG && ELT(0,E0   ) && IN_SEQ && BLK(1,A0,BF) && BLK(2,80,BF) && SEQEND                 ) { }
    else if ( SEQBEG && BLK(0,E1,EC) && IN_SEQ && BLK(1,80,BF) && BLK(2,80,BF) && SEQEND                 ) { }
    else if ( SEQBEG && ELT(0,ED   ) && IN_SEQ && BLK(1,80,9F) && BLK(2,80,BF) && SEQEND                 ) { }
    else if ( SEQBEG && BLK(0,EE,EF) && IN_SEQ && BLK(1,80,BF) && BLK(2,80,BF) && SEQEND                 ) { }
    else if ( SEQBEG && ELT(0,F0   ) && IN_SEQ && BLK(1,90,BF) && BLK(2,80,BF) && BLK(3,80,BF) && SEQEND ) { }
    else if ( SEQBEG && BLK(0,F1,F3) && IN_SEQ && BLK(1,80,BF) && BLK(2,80,BF) && BLK(3,80,BF) && SEQEND ) { }
    else if ( SEQBEG && ELT(0,F4   ) && IN_SEQ && BLK(1,80,8F) && BLK(2,80,BF) && BLK(3,80,BF) && SEQEND ) { }
    else if ( NO_SEQ ) {    // bad, unknown
      return 0;
    }
    /* */
    
    /* Error */
    if ( BAD_SEQ ) {
      return 0;
    }

    /* unicode character */
    if (seq.pos > 0)
      is_utf=1;

    /* Next */
    if (ok)
      seq.pos=0;
    else
      seq.pos++;

    /* Internal error */
    if (seq.pos >= 4)
      return 0;

  }

  return is_utf;
}

void map_characters(unsigned char* buffer, unsigned int size, unsigned int* map) {
  unsigned int i;
  memset(map, 0, sizeof(unsigned int) * 256);
  for(i = 0 ; i < size ; i++) {
    map[buffer[i]]++;
  }
}


// le fichier est-il un fichier html?
//  0 : non
//  1 : oui
// -1 : on sait pas
// -2 : on sait pas, pas d'extension
int ishtml(char* fil) {
  char *a;

  // patch pour les truc.html?Choix=toto
  if ( (a=strchr(fil,'?')) )  // paramètres?
    a--;  // pointer juste avant le ?
  else
    a=fil+strlen(fil)-1;  // pointer sur le dernier caractère

  if (*a=='/') return -1;    // répertoire, on sait pas!!
  //if (*a=='/') return 1;    // ok répertoire, html

  while ( (*a!='.') && (*a!='/')  && ( a > fil)) a--;
  if (*a=='.') {  // a une extension
    char fil_noquery[HTS_URLMAXSIZE*2];
    fil_noquery[0]='\0';
    a++;  // pointer sur extension
    strncat(fil_noquery,a,HTS_URLMAXSIZE);
    a=strchr(fil_noquery,'?');
    if (a)
      *a='\0';
    return ishtml_ext(fil_noquery);     // retour
  } else return -2;   // indéterminé, par exemple /truc
}

// idem, mais pour uniquement l'extension
int ishtml_ext(char* a) {
  int html=0;  
  //
  if (strfield2(a,"html"))       html = 1;
  else if (strfield2(a,"htm"))   html = 1;
  else if (strfield2(a,"shtml")) html = 1;
  else if (strfield2(a,"phtml")) html = 1;
  else if (strfield2(a,"htmlx")) html = 1;
  else if (strfield2(a,"shtm"))  html = 1;
  else if (strfield2(a,"phtm"))  html = 1;
  else if (strfield2(a,"htmx"))  html = 1;
  //
  // insuccès..
  else {
    switch(is_knowntype(a)) {
    case 1:
      html = 0;     // connu, non html
      break;
    case 2:
      html = 1;     // connu, html
      break;
    default:
      html = -1;    // inconnu..
      break;
    }
  }
  return html;  
}

// error (404,500..)
HTS_INLINE int ishttperror(int err) {
  switch (err/100) {
    case 4: case 5: return 1;
      break;
  }
  return 0;
}


// retourne le pointeur ou le pointeur + offset si il existe dans la chaine un @ signifiant 
// une identification
char* jump_identification(char* source) {
  char *a,*trytofind;
  // rechercher dernier @ (car parfois email transmise dans adresse!)
  // mais sauter ftp:// éventuel
  a = jump_protocol(source);
  trytofind = strrchr_limit(a, '@', strchr(a,'/'));
  return (trytofind != NULL)?trytofind:a;
}

// find port (:80) or NULL if not found
// can handle IPV6 addresses
char* jump_toport(char* source) {
  char *a,*trytofind;
  a = jump_identification(source);
  trytofind = strrchr_limit(a, ']', strchr(source, '/'));    // find last ] (http://[3ffe:b80:1234::1]:80/foo.html)
  a = strchr( (trytofind)?trytofind:a, ':');
  return a;
}

// strrchr, but not too far
char* strrchr_limit(char* s, char c, char* limit) {
  if (limit == NULL) {
    char* p = strchr(s, c);
    return p?(p+1):NULL;
  } else {
    char *a=NULL, *p;
    for(;;) {
      p=strchr((a)?a:s, c);
      if ((p >= limit) || (p == NULL))
        return a;
      a=p+1;
    }
  }
}

// retourner adr sans ftp://
HTS_INLINE char* jump_protocol(char* source) {
  int p;
  // scheme
  // "Comparisons of scheme names MUST be case-insensitive" (RFC2616)
  if ((p=strfield(source,"http:")))
    source+=p;
  else if ((p=strfield(source,"ftp:")))
    source+=p;
  else if ((p=strfield(source,"https:")))
    source+=p;
  else if ((p=strfield(source,"file:")))
    source+=p;
  // net_path
  if (strncmp(source,"//",2)==0)
    source+=2;
  return source;
}

// codage base 64 a vers b
void code64(char* a,char* b) {
  int i1=0,i2=0,i3=0,i4=0;
  unsigned long store;
  int n;
  const char _hts_base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  b[0]='\0';
  while(*a) {  
    // 24 bits
    n=1; store=0; store |= ((*a++) & 0xff);
    if (*a) { n=2; store <<= 8; store |= ((*a++) & 0xff); }
    if (*a) { n=3; store <<= 8; store |= ((*a++) & 0xff); }
    if (n==3) {
      i4=store & 63;
      i3=(store>>6) & 63;
      i2=(store>>12) & 63;
      i1=(store>>18) & 63;
    } else if (n==2) {
      store<<=2;    
      i3=store & 63;
      i2=(store>>6) & 63;
      i1=(store>>12) & 63;
    } else {
      store<<=4;
      i2=store & 63;
      i1=(store>>6) & 63;
    }
    
    *b++ = _hts_base64[i1];
    *b++ = _hts_base64[i2];
    if (n>=2)
      *b++ = _hts_base64[i3];
    else
      *b++ = '=';
    if (n>=3)
      *b++ = _hts_base64[i4];
    else
      *b++ = '=';
  }
  *b++='\0';
}

// remplacer &quot; par " etc..
// buffer MAX 1Ko
#define strcmpbeg(a, b) strncmp(a, b, strlen(b))
void unescape_amp(char* s) {
  while(*s) {
    if (*s=='&') {
      char* end=strchr(s,';');
      if ( end && (((int) (end - s)) <= 8) ) {
        unsigned char c=0;
        
        // http://www.w3.org/TR/xhtml-modularization/dtd_module_defs.html
        if (strcmpbeg(s, "&#") == 0) {
          int num=0;
          if ( (s[2] == 'x') || (s[2] == 'X')) {
            if (sscanf(s+3, "%x", &num) == 1) {
              c=(unsigned char)num;
            }
          } else {
            if (sscanf(s+2, "%d", &num) == 1) {
              c=(unsigned char)num;
            }
          }
        } else if (strcmpbeg(s, "&nbsp;")==0)
          c=32; // hack - c=160;
        else if (strcmpbeg(s, "&iexcl;")==0)
          c=161;
        else if (strcmpbeg(s, "&cent;")==0)
          c=162;
        else if (strcmpbeg(s, "&pound;")==0)
          c=163;
        else if (strcmpbeg(s, "&curren;")==0)
          c=164;
        else if (strcmpbeg(s, "&yen;")==0)
          c=165;
        else if (strcmpbeg(s, "&brvbar;")==0)
          c=166;
        else if (strcmpbeg(s, "&sect;")==0)
          c=167;
        else if (strcmpbeg(s, "&uml;")==0)
          c=168;
        else if (strcmpbeg(s, "&copy;")==0)
          c=169;
        else if (strcmpbeg(s, "&ordf;")==0)
          c=170;
        //else if (strcmpbeg(s, "&laquo;")==0)
        //  c=171;
        else if (strcmpbeg(s, "&not;")==0)
          c=172;
        //else if (strcmpbeg(s, "&shy;")==0)
        //  c=173;
        else if (strcmpbeg(s, "&reg;")==0)
          c=174;
        else if (strcmpbeg(s, "&macr;")==0)
          c=175;
        else if (strcmpbeg(s, "&deg;")==0)
          c=176;
        else if (strcmpbeg(s, "&plusmn;")==0)
          c=177;
        else if (strcmpbeg(s, "&sup2;")==0)
          c=178;
        else if (strcmpbeg(s, "&sup3;")==0)
          c=179;
        else if (strcmpbeg(s, "&acute;")==0)
          c=180;
        else if (strcmpbeg(s, "&micro;")==0)
          c=181;
        else if (strcmpbeg(s, "&para;")==0)
          c=182;
        else if (strcmpbeg(s, "&middot;")==0)
          c=183;
        else if (strcmpbeg(s, "&cedil;")==0)
          c=184;
        else if (strcmpbeg(s, "&sup1;")==0)
          c=185;
        else if (strcmpbeg(s, "&ordm;")==0)
          c=186;
        //else if (strcmpbeg(s, "&raquo;")==0)
        //  c=187;
        else if (strcmpbeg(s, "&frac14;")==0)
          c=188;
        else if (strcmpbeg(s, "&frac12;")==0)
          c=189;
        else if (strcmpbeg(s, "&frac34;")==0)
          c=190;
        else if (strcmpbeg(s, "&iquest;")==0)
          c=191;
        else if (strcmpbeg(s, "&Agrave;")==0)
          c=192;
        else if (strcmpbeg(s, "&Aacute;")==0)
          c=193;
        else if (strcmpbeg(s, "&Acirc;")==0)
          c=194;
        else if (strcmpbeg(s, "&Atilde;")==0)
          c=195;
        else if (strcmpbeg(s, "&Auml;")==0)
          c=196;
        else if (strcmpbeg(s, "&Aring;")==0)
          c=197;
        else if (strcmpbeg(s, "&AElig;")==0)
          c=198;
        else if (strcmpbeg(s, "&Ccedil;")==0)
          c=199;
        else if (strcmpbeg(s, "&Egrave;")==0)
          c=200;
        else if (strcmpbeg(s, "&Eacute;")==0)
          c=201;
        else if (strcmpbeg(s, "&Ecirc;")==0)
          c=202;
        else if (strcmpbeg(s, "&Euml;")==0)
          c=203;
        else if (strcmpbeg(s, "&Igrave;")==0)
          c=204;
        else if (strcmpbeg(s, "&Iacute;")==0)
          c=205;
        else if (strcmpbeg(s, "&Icirc;")==0)
          c=206;
        else if (strcmpbeg(s, "&Iuml;")==0)
          c=207;
        else if (strcmpbeg(s, "&ETH;")==0)
          c=208;
        else if (strcmpbeg(s, "&Ntilde;")==0)
          c=209;
        else if (strcmpbeg(s, "&Ograve;")==0)
          c=210;
        else if (strcmpbeg(s, "&Oacute;")==0)
          c=211;
        else if (strcmpbeg(s, "&Ocirc;")==0)
          c=212;
        else if (strcmpbeg(s, "&Otilde;")==0)
          c=213;
        else if (strcmpbeg(s, "&Ouml;")==0)
          c=214;
        else if (strcmpbeg(s, "&times;")==0)
          c=215;
        else if (strcmpbeg(s, "&Oslash;")==0)
          c=216;
        else if (strcmpbeg(s, "&Ugrave;")==0)
          c=217;
        else if (strcmpbeg(s, "&Uacute;")==0)
          c=218;
        else if (strcmpbeg(s, "&Ucirc;")==0)
          c=219;
        else if (strcmpbeg(s, "&Uuml;")==0)
          c=220;
        else if (strcmpbeg(s, "&Yacute;")==0)
          c=221;
        else if (strcmpbeg(s, "&THORN;")==0)
          c=222;
        else if (strcmpbeg(s, "&szlig;")==0)
          c=223;
        else if (strcmpbeg(s, "&agrave;")==0)
          c=224;
        else if (strcmpbeg(s, "&aacute;")==0)
          c=225;
        else if (strcmpbeg(s, "&acirc;")==0)
          c=226;
        else if (strcmpbeg(s, "&atilde;")==0)
          c=227;
        else if (strcmpbeg(s, "&auml;")==0)
          c=228;
        else if (strcmpbeg(s, "&aring;")==0)
          c=229;
        else if (strcmpbeg(s, "&aelig;")==0)
          c=230;
        else if (strcmpbeg(s, "&ccedil;")==0)
          c=231;
        else if (strcmpbeg(s, "&egrave;")==0)
          c=232;
        else if (strcmpbeg(s, "&eacute;")==0)
          c=233;
        else if (strcmpbeg(s, "&ecirc;")==0)
          c=234;
        else if (strcmpbeg(s, "&euml;")==0)
          c=235;
        else if (strcmpbeg(s, "&igrave;")==0)
          c=236;
        else if (strcmpbeg(s, "&iacute;")==0)
          c=237;
        else if (strcmpbeg(s, "&icirc;")==0)
          c=238;
        else if (strcmpbeg(s, "&iuml;")==0)
          c=239;
        else if (strcmpbeg(s, "&eth;")==0)
          c=240;
        else if (strcmpbeg(s, "&ntilde;")==0)
          c=241;
        else if (strcmpbeg(s, "&ograve;")==0)
          c=242;
        else if (strcmpbeg(s, "&oacute;")==0)
          c=243;
        else if (strcmpbeg(s, "&ocirc;")==0)
          c=244;
        else if (strcmpbeg(s, "&otilde;")==0)
          c=245;
        else if (strcmpbeg(s, "&ouml;")==0)
          c=246;
        else if (strcmpbeg(s, "&divide;")==0)
          c=247;
        else if (strcmpbeg(s, "&oslash;")==0)
          c=248;
        else if (strcmpbeg(s, "&ugrave;")==0)
          c=249;
        else if (strcmpbeg(s, "&uacute;")==0)
          c=250;
        else if (strcmpbeg(s, "&ucirc;")==0)
          c=251;
        else if (strcmpbeg(s, "&uuml;")==0)
          c=252;
        else if (strcmpbeg(s, "&yacute;")==0)
          c=253;
        else if (strcmpbeg(s, "&thorn;")==0)
          c=254;
        else if (strcmpbeg(s, "&yuml;")==0)
          c=255;
        //        
        else if (strcmpbeg(s,"&amp;")==0)
          c='&';
        else if (strcmpbeg(s,"&gt;")==0)
          c='>';
        else if (strcmpbeg(s,"&laquo;")==0)
          c='\"';
        else if (strcmpbeg(s,"&lt;")==0)
          c='<';
        else if (strcmpbeg(s,"&nbsp;")==0)
          c=' ';
        else if (strcmpbeg(s,"&quot;")==0)
          c='\"';
        else if (strcmpbeg(s,"&raquo;")==0)
          c='\"';
        else if (strcmpbeg(s,"&shy;")==0)
          c='-';
        else if (strcmpbeg(s,"&tilde;")==0)
          c='~';
        // remplacer?
        if (c) {
          char buff[HTS_URLMAXSIZE*2];
          buff[0]=(char) c;
          strcpy(buff+1,end+1);
          strcpy(s,buff);
        }
      }
    }
    s++;
  }
}

// remplacer %20 par ' ', | par : etc..
// buffer MAX 1Ko
char* unescape_http(char* s) {
  char* tempo;
  int i,j=0;
  NOSTATIC_RESERVE(tempo, char, HTS_URLMAXSIZE*2);
  for (i=0;i<(int) strlen(s);i++) {
    if (s[i]=='%') {
      i++;
      tempo[j++]=(char) ehex(s+i);
      i++;    // sauter 2 caractères finalement
    }
    /*
    NON a cause de trucs comme /home/0,1837,1|7|1173|Content,00.html
    else if (s[i]=='|') {                     // exemple: file:///C|Program%20Files...
      tempo[j++]=':';
    }
    */
    else
      tempo[j++]=s[i];
  }
  tempo[j++]='\0';
  return tempo;
}

// unescape in URL/URI ONLY what has to be escaped, to form a standard URL/URI
char* unescape_http_unharm(char* s, int no_high) {
  char* tempo;
  int i,j=0;
  NOSTATIC_RESERVE(tempo, char, HTS_URLMAXSIZE*2);
  for (i=0;i<(int) strlen(s);i++) {
    if (s[i]=='%') {
      int nchar=(char) ehex(s+i+1);

      int test = (  CHAR_RESERVED(nchar)
                || CHAR_DELIM(nchar)
                || CHAR_UNWISE(nchar)
                || CHAR_LOW(nchar)        /* CHAR_SPECIAL */
                || CHAR_XXAVOID(nchar) 
                || (
                  (no_high)
                  &&
                  CHAR_HIG(nchar)
                )
                );

      if (!test) {
        tempo[j++]=(char) ehex(s+i+1);
        i+=2;
      } else {
        tempo[j++]='%';
      }
    }
    /*
    NON a cause de trucs comme /home/0,1837,1|7|1173|Content,00.html
    else if (s[i]=='|') {                     // exemple: file:///C|Program%20Files...
      tempo[j++]=':';
    }
    */
    else
      tempo[j++]=s[i];
  }
  tempo[j++]='\0';
  return tempo;
}

// remplacer " par %xx etc..
// buffer MAX 1Ko
void escape_spc_url(char* s) {
  x_escape_http(s,2);
}
// smith / john -> smith%20%2f%20john
void escape_in_url(char* s) {
  x_escape_http(s,1);
}
// smith / john -> smith%20/%20john
void escape_uri(char* s) {
  x_escape_http(s,3);
}
void escape_uri_utf(char* s) {
  x_escape_http(s,30);
}
void escape_check_url(char* s) {
  x_escape_http(s,0);
}
// same as escape_check_url, but returns char*
char* escape_check_url_addr(char* s) {
  char* adr;
  escape_check_url(adr = concat(s,""));
  return adr;
}


void x_escape_http(char* s,int mode) {
  while(*s) {
    int test=0;
    if (mode == 0)
      test=(strchr("\" ",*s)!=0);
    else if (mode==1) {
      test = (  CHAR_RESERVED(*s)
             || CHAR_DELIM(*s)
             || CHAR_UNWISE(*s)
             || CHAR_SPECIAL(*s)
             || CHAR_XXAVOID(*s) );
    }
    else if (mode==2)
      test=(strchr(" ",*s)!=0);           // n'escaper que espace
    else if (mode==3) {                   // échapper que ce qui est nécessaire
      test = (
                CHAR_SPECIAL(*s)
             || CHAR_XXAVOID(*s) );
    }
    else if (mode==30) {                   // échapper que ce qui est nécessaire
      test = (
                CHAR_LOW(*s)
             || CHAR_XXAVOID(*s) );
    }

    if (test) {
      char buffer[HTS_URLMAXSIZE*2];
      int n;
      n=(int)(unsigned char) *s;
      strcpy(buffer,s+1);
      sprintf(s,"%%%02x",n);
      strcat(s,buffer);
    }
    s++;
  }
}


HTS_INLINE int ehexh(char c) {
  if ((c>='0') && (c<='9')) return c-'0';
  if ((c>='a') && (c<='f')) c-=('a'-'A');
  if ((c>='A') && (c<='F')) return (c-'A'+10);
  return 0;
}

HTS_INLINE int ehex(char* s) {
  return 16*ehexh(*s)+ehexh(*(s+1));

}

// concat, concatène deux chaines et renvoi le résultat
// permet d'alléger grandement le code
// il faut savoir qu'on ne peut mettre plus de 16 concat() dans une expression
typedef struct {
  char buff[16][HTS_URLMAXSIZE*2*2];
  int rol;
} concat_strc;
char* concat(const char* a,const char* b) {
  concat_strc* strc;
  NOSTATIC_RESERVE(strc, concat_strc, 1);
  strc->rol=((strc->rol+1)%16);    // roving pointer
  strcpy(strc->buff[strc->rol],a);
  if (b) strcat(strc->buff[strc->rol],b);
  return strc->buff[strc->rol];
}
// conversion fichier / -> antislash
#if HTS_DOSNAME
char* __fconv(char* a) {
  int i;
  for(i=0;i<(int) strlen(a);i++)
    if (a[i]=='/')  // convertir
      a[i]='\\';
  return a;
}
char* fconcat(char* a,char* b) {
  return __fconv(concat(a,b));
}
char* fconv(char* a) {
  return __fconv(concat(a,""));
}
#endif

/* / et \\ en / */
char* __fslash(char* a) {
  int i;
  for(i=0;i<(int) strlen(a);i++)
    if (a[i]=='\\')  // convertir
      a[i]='/';
  return a;
}
char* fslash(char* a) {
  return __fslash(concat(a,""));
}

// conversion minuscules, avec buffer
char* convtolower(char* a) {
  concat_strc* strc;
  NOSTATIC_RESERVE(strc, concat_strc, 1);
  strc->rol=((strc->rol+1)%16);    // roving pointer
  strcpy(strc->buff[strc->rol],a);
  hts_lowcase(strc->buff[strc->rol]);  // lower case
  return strc->buff[strc->rol];
}

// conversion en minuscules
void hts_lowcase(char* s) {
  int i;
  for(i=0;i<(int) strlen(s);i++)
    if ((s[i]>='A') && (s[i]<='Z'))
      s[i]+=('a'-'A');
}

// remplacer un caractère d'une chaîne dans une autre
HTS_INLINE void hts_replace(char *s,char from,char to) { 
  char* a;
  while ((a=strchr(s,from))!=NULL) {
    *a=to;
  }
}


// caractère espace, guillemets, CR, LF etc..
/* SECTION OPTIMISEE:
  #define  is_space(c) (strchr(" \"\x0d\x0a\x09'",c)!=NULL)
  #define  is_realspace(c) (strchr(" \x0d\x0a\x09\x0c",c)!=NULL)
*/
/*
HTS_INLINE int is_space(char c) {
  if (c==' ')  return 1;  // spc
  if (c=='"')  return 1;  // quote
  if (c==10)   return 1;  // lf
  if (c==13)   return 1;  // cr
  if (c=='\'') return 1;  // quote
  //if (c=='`')  return 1;  // backquote      << non
  if (c==9)    return 1;  // tab
  return 0;
}
*/

// caractère espace, CR, LF, TAB
/*
HTS_INLINE int is_realspace(char c) {
  if (c==' ')  return 1;  // spc
  if (c==10)   return 1;  // lf
  if (c==13)   return 1;  // cr
  if (c==9)    return 1;  // tab
  return 0;
}
*/





// deviner type d'un fichier local..
// ex: fil="toto.gif" -> s="image/gif"
void guess_httptype(char *s,char *fil) {
  get_httptype(s,fil,1);
}
// idem
// flag: 1 si toujours renvoyer un type
void get_httptype(char *s,char *fil,int flag) {
  if (ishtml(fil)==1)
    strcpy(s,"text/html");
  else {
    char *a=fil+strlen(fil)-1;    
    while ( (*a!='.') && (*a!='/')  && (a>fil)) a--;
    if (*a=='.') {
      int ok=0;
      int j=0;
      a++;
      while( (!ok) && (strnotempty(hts_mime[j][1])) ) {
        if (strfield2(hts_mime[j][1],a)) {
          if (hts_mime[j][0][0]!='*') {    // Une correspondance existe
            strcpy(s,hts_mime[j][0]);
            ok=1;
          }
        }
        j++;
      }
      
      if (!ok) if (flag) sprintf(s,"application/%s",a);
    } else {
      if (flag) strcpy(s,"application/octet-stream");
    }
  }
}

// get type of fil (php)
// s: buffer (text/html) or NULL
// return: 1 if known by user
int get_userhttptype(int setdefs,char *s,char *ext) {
  char** buffer=NULL;
  NOSTATIC_RESERVE(buffer, char*, 1);
  if (setdefs) {
    *buffer=s;
    return 1;
  } else {
    if (s)
      s[0]='\0';
    if (!ext)
      return 0;
    if (*buffer) {
      char search[1024];
      char* detect;
      sprintf(search,"\n%s=",ext);    // php=text/html
      detect=strstr(*buffer,search);
      if (!detect) {
        sprintf(search,"\n%s\n",ext); // php\ncgi=text/html
        detect=strstr(*buffer,search);
      }
      if (detect) {
        detect=strchr(detect,'=');
        if (detect) {
          detect++;
          if (s) {
            char* a;
            a=strchr(detect,'\n');
            if (a) {
              strncat(s,detect,(int) (a - detect));
            }
          }
          return 1;
        }
      }
    }
  }
  return 0;
}
// renvoyer extesion d'un type mime..
// ex: "image/gif" -> gif
void give_mimext(char *s,char *st) {   
  int ok=0;
  int j=0;
  s[0]='\0';
  while( (!ok) && (strnotempty(hts_mime[j][1])) ) {
    if (strfield2(hts_mime[j][0],st)) {
      if (hts_mime[j][1][0]!='*') {    // Une correspondance existe
        strcpy(s,hts_mime[j][1]);
        ok=1;
      }
    }
    j++;
  }
  // wrap "x" mimetypes, such as:
  // application/x-mp3
  // or
  // application/mp3
  if (!ok) {
    int p;
    char* a=NULL;
    if ((p=strfield(st,"application/x-")))
      a=st+p;
    else if ((p=strfield(st,"application/")))
      a=st+p;
    if (a) {
      if ((int)strlen(a) >= 1) {
        if ((int)strlen(a) <= 4) {
          strcpy(s,a);
          ok=1;
        }
      }
    }
  }
}
// extension connue?..
//  0 : non
//  1 : oui
//  2 : html
int is_knowntype(char *fil) {
  int j=0;
  if (!fil)
    return 0;
  while(strnotempty(hts_mime[j][1])) {
    if (strfield2(hts_mime[j][1],fil)) {
      if (strfield2(hts_mime[j][0],"text/html"))
        return 2;
      else
        return 1;
    }
    j++;
  }

  // Known by user?
  return (is_userknowntype(fil));
}
// extension : html,gif..
char* get_ext(char *fil) {
  char* fil_noquery;
  char *a=fil+strlen(fil)-1;    
  NOSTATIC_RESERVE(fil_noquery, char, HTS_URLMAXSIZE*2);

  while ( (*a!='.') && (*a!='/')  && (a>fil)) a--;
  if (*a=='.') {
    fil_noquery[0]='\0';
    a++;  // pointer sur extension
    strncat(fil_noquery,a,HTS_URLMAXSIZE);
    a=strchr(fil_noquery,'?');
    if (a)
      *a='\0';
    return concat(fil_noquery,"");
  }
  else
    return "";
}
// known type?..
//  0 : no
//  1 : yes
//  2 : html
// setdefs : set mime buffer:
//   file=(char*) "asp=text/html\nphp=text/html\n"
int is_userknowntype(char *fil) {
  char mime[1024];
  if (!fil)
    return 0;
  if (!strnotempty(fil))
    return 0;
  mime[0]='\0';
  get_userhttptype(0,mime,fil);
  if (!strnotempty(mime))
    return 0;
  else if (strfield2(mime,"text/html"))
    return 2;
  else
    return 1;
}

// page dynamique?
// is_dyntype(get_ext("foo.asp"))
int is_dyntype(char *fil) {
  int j=0;
  if (!fil)
    return 0;
  if (!strnotempty(fil))
    return 0;
  while(strnotempty(hts_ext_dynamic[j])) {
    if (strfield2(hts_ext_dynamic[j],fil)) {
      return 1;
    }
    j++;
  }
  return 0;
}

// types critiques qui ne doivent pas être changés car renvoyés par des serveurs qui ne
// connaissent pas le type
int may_unknown(char* st) {
  int j=0;
  // types média
  if (may_be_hypertext_mime(st))
    return 1;
  while(strnotempty(hts_mime_keep[j])) {
    if (strfield2(hts_mime_keep[j],st)) {      // trouvé
      return 1;
    }
    j++;
  }    
  return 0;
}



// -- Utils fichiers

// pretty print for i/o
void fprintfio(FILE* fp,char* buff,char* prefix) {
  char nl=1;
  while(*buff) {
    switch(*buff) {
    case 13: break;
    case 10:
      fprintf(fp,"\r\n");
      nl=1;
    break;
    default:
      if (nl)
        fprintf(fp,prefix);
      nl=0;
      fputc(*buff,fp);
    }
    buff++;
  }
}

/* Le fichier existe-t-il? (ou est-il accessible?) */
int fexist(char* s) {
  FILE* fp;
  if (strnotempty(s)==0)     // nom vide: non trouvé
    return 0;
  fp=fopen(fconv(s),"rb");
  if (fp!=NULL) fclose(fp);
  return (fp!=NULL);
} 

/* Taille d'un fichier, -1 si n'existe pas */
/* fp->_cnt ne fonctionne pas sur toute les plate-formes :-(( */
/* Note: NOT YET READY FOR 64-bit */
//LLint fsize(char* s) {
int fsize(char* s) {
  /*
#if HTS_WIN
  HANDLE hFile;
  DWORD dwSizeHigh = 0;
  DWORD dwSizeLow  = 0;
  hFile = CreateFile(s,0,0,NULL,OPEN_EXISTING,0,NULL);
  if (hFile) {
    dwSizeLow = GetFileSize (hFile, & dwSizeHigh) ;
    CloseHandle(hFile);
    if (dwSizeLow != 0xFFFFFFFF)
      return (dwSizeLow & (dwSizeHigh<<32));
    else
      return -1;
  } else
    return -1;
#else
    */
  FILE* fp;
  if (strnotempty(s)==0)     // nom vide: erreur
    return -1;
  fp=fopen(fconv(s),"rb");
  if (fp!=NULL) {
    int i;
    fseek(fp,0,SEEK_END);
    i=ftell(fp);
    fclose(fp);
    return i;
  } else return -1;
  /*
#endif
  */
}

int fpsize(FILE* fp) {
  int oldpos,size;
  if (!fp)
    return -1;
  oldpos=ftell(fp);
  fseek(fp,0,SEEK_END);
  size=ftell(fp);
  fseek(fp,oldpos,SEEK_SET);
  return size;
}

/* root dir, with ending / */
typedef struct {
  char path[1024+4];
  int init;
} hts_rootdir_strc;
char* hts_rootdir(char* file) {
  static hts_rootdir_strc strc = {"", 0};
  //NOSTATIC_RESERVE(strc, hts_rootdir_strc, 1);
  if (file) {
    if (!strc.init) {
      strc.path[0]='\0';
      strc.init=1;
      if (strnotempty(file)) {
        char* a;
        strcpy(strc.path,file);
        while((a=strrchr(strc.path,'\\'))) *a='/';
        if ((a=strrchr(strc.path,'/'))) {
          *(a+1)='\0';
        } else
          strc.path[0]='\0';
      }
      if (!strnotempty(strc.path)) {
        if( getcwd( strc.path, 1024 ) == NULL )
            strc.path[0]='\0';
        else
          strcat(strc.path,"/");
      }
    }
    return NULL;
  } else if (strc.init)
    return strc.path;
  else
    return "";
}



hts_stat_struct HTS_STAT;
//
// return  number of downloadable bytes, depending on rate limiter
// see engine_stats() routine, too
// this routine works quite well for big files and regular ones, but apparently the rate limiter has
// some problems with very small files (rate too high)
LLint check_downloadable_bytes(int rate) {
  if (rate>0) {
    TStamp time_now;
    TStamp elapsed_useconds;
    LLint bytes_transfered_during_period;
    LLint left;

    // get the older timer
    int id_timer = (HTS_STAT.istat_idlasttimer + 1) % 2;

    time_now=mtime_local();
    elapsed_useconds = time_now - HTS_STAT.istat_timestart[id_timer];
    // NO totally stupid - elapsed_useconds+=1000;      // for the next second, too
    bytes_transfered_during_period = (HTS_STAT.HTS_TOTAL_RECV-HTS_STAT.istat_bytes[id_timer]);
    
    left = ((rate * elapsed_useconds)/1000) - bytes_transfered_during_period;
    if (left <= 0)
      left = 0;
    
    return left;
  } else
    return TAILLE_BUFFER;
}

//
// 0 : OK
// 1 : slow down
#if 0
int HTS_TOTAL_RECV_CHECK(int var) {
  if (HTS_STAT.HTS_TOTAL_RECV_STATE)
    return 1;
    /*
    {
    if (HTS_STAT.HTS_TOTAL_RECV_STATE==3) { 
      var = min(var,32); 
      Sleep(250); 
    } else if (HTS_STAT.HTS_TOTAL_RECV_STATE==2) { 
      var = min(var,256); 
      Sleep(100); 
    } else { 
      var/=2; 
      if (var<=0) var=1; 
      Sleep(50); 
    } 
  }
    */
  return 0;
}
#endif

// Lecture dans buff de size octets au maximum en utilisant la socket r (structure htsblk)
// >0 : data received
// == 0 : not yet data
// <0 : no more data or error
HTS_INLINE int hts_read(htsblk* r,char* buff,int size) {
  int retour;
  //  return read(soc,buff,size);
  if (r->is_file) {
#if HTS_WIDE_DEBUG    
    DEBUG_W("read\n");
#endif
    if (r->fp)
      retour=fread(buff,1,size,r->fp);
    else
      retour=-1;
  } else {
#if HTS_WIDE_DEBUG    
    DEBUG_W("recv\n");
    if (r->soc==INVALID_SOCKET)
      printf("!!WIDE_DEBUG ERROR, soc==INVALID hts_read\n");
#endif
    //HTS_TOTAL_RECV_CHECK(size);         // Diminuer au besoin si trop de données reçues
#if HTS_USEOPENSSL
    if (r->ssl) {
      retour = SSL_read(r->ssl_con, buff, size);
      if (retour <= 0) {
        int err_code = SSL_get_error(r->ssl_con, retour);
        if (
          (err_code == SSL_ERROR_WANT_READ)
          ||
          (err_code == SSL_ERROR_WANT_WRITE)
          ) 
        {
          retour = 0;             /* no data yet (ssl cache) */
        } else {
          retour = -1;            /* eof or error */
        }
      }
    } else {
#endif
    retour=recv(r->soc,buff,size,0);
  }
  if (retour > 0)    // compter flux entrant
    HTS_STAT.HTS_TOTAL_RECV+=retour;
#if HTS_USEOPENSSL
  }
#endif
#if HTS_WIDE_DEBUG    
  DEBUG_W("recv/read done\n");
#endif
  return retour;
}


// -- Gestion cache DNS --
// 'RX98
#if HTS_DNSCACHE

// 'capsule' contenant uniquement le cache
t_dnscache* _hts_cache(void) {
  t_dnscache* cache;
  NOSTATIC_RESERVE(cache, t_dnscache, 1);
  return cache;
}

// lock le cache dns pour tout opération d'ajout
// plus prudent quand plusieurs threads peuvent écrire dedans..
// -1: status? 0: libérer 1:locker

/* 
  Simple lock function for cache

  Return value: always 0
  Parameter:
  1 wait for lock (mutex) available and lock it
  0 unlock the mutex
  [-1 check if locked (always return 0 with mutex)]
  -999 initialize
*/
#if USE_BEGINTHREAD
int _hts_lockdns(int i) {
  static PTHREAD_LOCK_TYPE hMutex; 
  return htsSetLock(&hMutex,i);
}
#else
int _hts_lockdns(int i) {
  int l=0;
  if (i>=0)
    l=i;
  return l;
}
#endif

// routine pour le cache - retour optionnel à donner à chaque fois
// NULL: nom non encore testé dans le cache
// si h_length==0 alors le nom n'existe pas dans le dns
t_hostent* _hts_ghbn(t_dnscache* cache,char* iadr,t_hostent* retour) {
  // attendre que le cache dns soit prêt
  while(_hts_lockdns(-1));  // attendre libération
  _hts_lockdns(1);          // locker

  while(1) {
    if (strcmp(cache->iadr,iadr)==0) {  // ok trouvé
      if (cache->host_length>0) {  // entrée valide
        if (retour->h_addr_list[0])
          memcpy(retour->h_addr_list[0], cache->host_addr, cache->host_length);
        retour->h_length=cache->host_length;
      } else if (cache->host_length==0) {  // en cours
        _hts_lockdns(0);          // délocker
        return NULL;
      } else {                    // erreur dans le dns, déja vérifié
        if (retour->h_addr_list[0])
          retour->h_addr_list[0][0]='\0';
        retour->h_length=0;  // erreur, n'existe pas
      }
      _hts_lockdns(0);          // délocker
      return retour;
    } else {    // on a pas encore trouvé
      if (cache->n!=NULL) { // chercher encore
        cache=cache->n;   // suivant!
      } else {
        _hts_lockdns(0);          // délocker
        return NULL;    // non présent        
      }
    }    
  }
}

// tester si iadr a déja été testé (ou en cours de test)
// 0 non encore
// 1 ok
// 2 non présent
int hts_dnstest(char* _iadr) {
  char* iadr;
  t_dnscache* cache=_hts_cache();  // adresse du cache 
  NOSTATIC_RESERVE(iadr, char, HTS_URLMAXSIZE*2);

  // sauter user:pass@ éventuel
  strcpy(iadr,jump_identification(_iadr));
  // couper éventuel :
  {
    char *a;
    if ( (a=jump_toport(iadr)) )
      *a='\0';
  }

#if HTS_WIN
  if (inet_addr(iadr)!=INADDR_NONE)  // numérique
#else
  if (inet_addr(iadr)!=(in_addr_t) -1 )  // numérique
#endif
    return 1;

  while(_hts_lockdns(-1));  // attendre libération
  _hts_lockdns(1);          // locker
  while(1) {
    if (strcmp(cache->iadr,iadr)==0) {  // ok trouvé
      _hts_lockdns(0);          // délocker
      return 1;    // présent!
    } else {    // on a pas encore trouvé
      if (cache->n!=NULL) { // chercher encore
        cache=cache->n;   // suivant!
      } else {
        _hts_lockdns(0);          // délocker
        return 2;    // non présent        
      }
    }    
  }
}


t_hostent* vxgethostbyname(char* hostname, void* v_buffer) {
  t_fullhostent* buffer = (t_fullhostent*) v_buffer;
  /* Clear */
  fullhostent_init(buffer);

  /* Protection */
  if (!strnotempty(hostname)) {
    return NULL;
  }

  /* 
    Strip [] if any : [3ffe:b80:1234:1::1] 
    The resolver doesn't seem to handle IP6 addresses in brackets
  */
  if ((hostname[0] == '[') && (hostname[strlen(hostname)-1] == ']')) {
    char tempo[HTS_URLMAXSIZE*2];
    tempo[0]='\0';
    strncat(tempo, hostname+1, strlen(hostname)-2);
    strcpy(hostname, tempo);
  }

  {
#if HTS_INET6==0
  /*
  ipV4 resolver
    */
    t_hostent* hp=gethostbyname(hostname);
    if (hp!=NULL) {
      if ( (hp->h_length) && ( ((unsigned int) hp->h_length) <= buffer->addr_maxlen) ) {
        memcpy(buffer->hp.h_addr_list[0], hp->h_addr_list[0], hp->h_length);
        buffer->hp.h_length = hp->h_length;
        return &(buffer->hp);
      }
    }
#else
    /*
    ipV6 resolver
    */
    /*
    int error_num=0;
    t_hostent* hp=getipnodebyname(hostname, AF_INET6, AI_DEFAULT, &error_num);
    oops, deprecated :(
    */
    struct addrinfo* res = NULL;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    if (IPV6_resolver == 1)        // V4 only (for bogus V6 entries)
      hints.ai_family = PF_INET;
    else if (IPV6_resolver == 2)   // V6 only (for testing V6 only)
      hints.ai_family = PF_INET6;
    else                           // V4 + V6
      hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (getaddrinfo(hostname, NULL, &hints, &res) == 0) {
      if (res) {
        if ( (res->ai_addr) && (res->ai_addrlen) && (res->ai_addrlen <= buffer->addr_maxlen) ) {
          memcpy(buffer->hp.h_addr_list[0], res->ai_addr, res->ai_addrlen);
          buffer->hp.h_length = res->ai_addrlen;
          freeaddrinfo(res);
          return &(buffer->hp);
        }
      }
    }
    if (res) {
      freeaddrinfo(res);
    }
    
#endif
  }
  return NULL;
}

// cache dns interne à HTS // ** FREE A FAIRE sur la chaine
t_hostent* hts_gethostbyname(char* _iadr, void* v_buffer) {
  char iadr[HTS_URLMAXSIZE*2];
  t_fullhostent* buffer = (t_fullhostent*) v_buffer;
  t_dnscache* cache=_hts_cache();  // adresse du cache
  t_hostent* hp;

  /* Clear */
  fullhostent_init(buffer);

  strcpy(iadr,jump_identification(_iadr));
  // couper éventuel :
  {
    char *a;
    if ( (a=jump_toport(iadr)) )
      *a='\0';
  }

  // effacer structure de retour, créer nouvelle
  /*
  memset(&host, 0, sizeof(t_hostent));  
  host.h_addr_list=he;
  he[0]=NULL;
  he[1]=NULL;  
  host.h_length=0;  
  */
  cache->iadr[0]='*';
  cache->iadr[1]='\0';
  
  /* get IP from the dns cache */
  hp = _hts_ghbn(cache, iadr, &buffer->hp);
  if (hp) {
    if (hp->h_length>0)
      return hp;
    else
      return NULL;    // entrée erronée (erreur DNS) dans le DNS
  } else {  // non présent dans le cache dns, tester
    t_dnscache* c=cache;
    while(c->n) c=c->n;    // calculer queue
    
#if HTS_WIDE_DEBUG    
    DEBUG_W("gethostbyname\n");
#endif      
#if HDEBUG
    printf("gethostbyname (not in cache)\n");
#endif
    {
      unsigned long inetaddr;
#if HTS_WIN
      if ((inetaddr=inet_addr(iadr))==INADDR_NONE) {
#else
      if ((inetaddr=inet_addr(iadr))==(in_addr_t) -1 ) {
#endif        
#if DEBUGDNS 
        printf("resolving (not cached) %s\n",iadr);
#endif
        hp=vxgethostbyname(iadr, buffer);  // calculer IP host
      } else {     // numérique, convertir sans passer par le dns
        buffer->hp.h_addr_list[0]=(char*) &inetaddr;
        buffer->hp.h_length=4;
        hp=&buffer->hp;
      }
    }
#if HTS_WIDE_DEBUG    
    DEBUG_W("gethostbyname done\n");
#endif
    cache->n=(t_dnscache*) calloct(1,sizeof(t_dnscache));
    if (cache->n!=NULL) {
      strcpy(cache->n->iadr,iadr);
      if (hp!=NULL) {
        memcpy(cache->n->host_addr, hp->h_addr_list[0], hp->h_length);
        cache->n->host_length=hp->h_length;
      } else {
        cache->n->host_addr[0]='\0';
        cache->n->host_length=0;  // non existant dans le dns
      }
      cache->n->n=NULL;
      return hp;
    } else {  // on peut pas noter, mais on peut renvoyer le résultat
      return hp;
    }        
  }  // retour hp du cache
}

#else
HTS_INLINE t_hostent* hts_gethostbyname(char* iadr, t_fullhostent* buffer) {
  t_hostent* retour;
#if HTS_WIDE_DEBUG    
  DEBUG_W("gethostbyname (2)\n");
#endif
#if DEBUGDNS 
    printf("blocking method gethostbyname() in progress for %s\n",iadr);
#endif
  retour=vxgethostbyname(jump_identification(iadr), );
#if HTS_WIDE_DEBUG    
  DEBUG_W("gethostbyname (2) done\n");
#endif
  return retour;
}
#endif


// --- Tracage des mallocs() ---
#if HTS_TRACE_MALLOC
typedef struct _mlink {
  void* adr;
  int len;
  int id;
  struct _mlink* next;
} mlink;
mlink trmalloc = {NULL,0,0,NULL};
int trmalloc_id=0;

HTS_INLINE void* hts_malloc(size_t len,size_t len2) {
  mlink* lnk = (mlink*) calloc(1,sizeof(mlink));
  void*  r   = NULL;
  if (lnk) {
    if (len2)
      r = calloc(len,len2);
    else
      r = malloc(len);
    if (r) {
      lnk->adr=r;
      if (len2)
        lnk->len=len*len2;
      else
        lnk->len=len;
      lnk->id=trmalloc_id++;
      lnk->next=trmalloc.next;
      trmalloc.next=lnk;
#if MEMDEBUG
      //printf("malloc: %d\n",r);
#endif
    } else free(lnk);
  }
  return r;
}
HTS_INLINE void  hts_free(void* adr) {
  mlink* lnk = &trmalloc;
  if (!adr) {
#if MEMDEBUG
    printf("* unexpected free() error at %d\n",adr);
#endif
    return;
  }
  do {
    if (lnk->next->adr==adr) {
      mlink* blk_free=lnk->next;
#if 1
      lnk->next=lnk->next->next;
      free((void*) blk_free);
#else
#if MEMDEBUG
      if (blk_free->id==-1) {
        printf("* memory has already been freed: %d (id=%d)\n",blk_free->adr,blk_free->id);
      }
#endif
      blk_free->id=-1;
#endif
      free(adr);
#if MEMDEBUG
      //printf("free: %d (id=%d)\n",blk_free->adr,blk_free->id);
#endif
      return;
    }
    lnk=lnk->next;
  } while(lnk->next != NULL);
#if MEMDEBUG
  printf("* unexpected free() error at %d\n",adr);
#endif
  free(adr);
}
HTS_INLINE void* hts_realloc(void* adr,size_t len) {
  mlink* lnk = &trmalloc;
  do {
    if (lnk->next->adr==adr) {
      adr = realloc(adr,len);
      lnk->next->adr = adr;
      lnk->next->len = len;
#if MEMDEBUG
      //printf("realloc: %d (id=%d)\n",lnk->next->adr,lnk->next->id);
#endif
      return adr;
    }
    lnk=lnk->next;
  } while(lnk->next != NULL);
#if MEMDEBUG
  printf("* unexpected realloc() error at %d\n",adr);
#endif
  return realloc(adr,len);
}
// check the malloct() and calloct() trace stack
void  hts_freeall(void) {
  while(trmalloc.next) {
#if MEMDEBUG
    printf("* block %d\t not released: at %d\t (%d\t bytes)\n",trmalloc.next->id,trmalloc.next->adr,trmalloc.next->len);
#endif
    if (trmalloc.next->id != -1) {
      freet(trmalloc.next->adr);
    }
  }
}
#endif


// -- divers //

// cut path and project name
// patch also initial path
void cut_path(char* fullpath,char* path,char* pname) {
  path[0]=pname[0]='\0';
  if (strnotempty(fullpath)) {
    if ((fullpath[strlen(fullpath)-1]=='/') || (fullpath[strlen(fullpath)-1]=='\\'))
      fullpath[strlen(fullpath)-1]='\0';
    if (strlen(fullpath)>1) {
      char* a;
      while( (a=strchr(fullpath,'\\')) ) *a='/';     // remplacer par /
      a=fullpath+strlen(fullpath)-2;
      while( (*a!='/') && ( a > fullpath)) a--;
      if (*a=='/') a++;
      strcpy(pname,a);
      strncat(path,fullpath,(int) (a - fullpath));
    }
  }
}



// -- Gestion protocole ftp --

#if HTS_WIN
int ftp_available(void) {
  return 1;
}
#else
int ftp_available(void) {
  return 1;   // ok!
  //return 0;   // SOUS UNIX, PROBLEMES
}
#endif



int hts_init(void) {
  static int hts_init_ok = 0;
  if (!hts_init_ok) {
    hts_init_ok = 1;
    // default wrappers
    htswrap_init();
    htswrap_add("init",htsdefault_init);
    htswrap_add("free",htsdefault_uninit);
    htswrap_add("start",htsdefault_start);
    htswrap_add("change-options",htsdefault_chopt);
    htswrap_add("end",htsdefault_end);
    htswrap_add("check-html",htsdefault_checkhtml);
    htswrap_add("loop",htsdefault_loop);
    htswrap_add("query",htsdefault_query);
    htswrap_add("query2",htsdefault_query2);
    htswrap_add("query3",htsdefault_query3);
    htswrap_add("check-link",htsdefault_check);
    htswrap_add("pause",htsdefault_pause);
    htswrap_add("save-file",htsdefault_filesave);
    htswrap_add("link-detected",htsdefault_linkdetected);
    htswrap_add("transfer-status",htsdefault_xfrstatus);
    htswrap_add("save-name",htsdefault_savename);
  }
  
#if HTS_USEOPENSSL
  /*
  Initialize the OpensSSL library
  */
  if (!openssl_ctx) {
    SSL_library_init();
    SSL_load_error_strings();
    ERR_load_crypto_strings();
    ERR_load_SSL_strings();
    SSLeay_add_ssl_algorithms();
    // OpenSSL_add_all_algorithms();
    openssl_ctx = SSL_CTX_new(SSLv23_client_method());
    if (!openssl_ctx) {
      fprintf(stderr, "fatal: unable to initialize TLS: SSL_CTX_new(SSLv23_client_method)\n");
      abort();
    }
  }
#endif
  
  /* Init vars and thread-specific values */
  hts_initvar();
  
  return 1;
}
int hts_uninit(void) {
  hts_freevar();
  /* htswrap_free(); */
  return 1;
}

// defaut wrappers
void __cdecl htsdefault_init(void) {
}
void __cdecl htsdefault_uninit(void) {
  hts_freevar();
}
int __cdecl htsdefault_start(void* opt) {
  return 1; 
}
int __cdecl htsdefault_chopt(void* opt) {
  return 1;
}
int  __cdecl htsdefault_end(void) { 
  return 1; 
}
int __cdecl htsdefault_checkhtml(char* html,int len,char* url_adresse,char* url_fichier) {
  return 1;
}
int __cdecl htsdefault_loop(void* back,int back_max,int back_index,int lien_n,int lien_tot,int stat_time,hts_stat_struct* stats) {    // appelé à chaque boucle de HTTrack
  return 1;
}
char* __cdecl htsdefault_query(char* question) {
  return "";
}
char* __cdecl htsdefault_query2(char* question) {
  return "";
}
char* __cdecl htsdefault_query3(char* question) {
  return "";
}
int __cdecl htsdefault_check(char* adr,char* fil,int status) {
  return -1;
}
void __cdecl htsdefault_pause(char* lockfile) {
  while (fexist(lockfile)) {
    Sleep(1000);
  }
}
void __cdecl htsdefault_filesave(char* file) {
}
int __cdecl htsdefault_linkdetected(char* link) {
  return 1;
}
int __cdecl htsdefault_xfrstatus(void* back) {
  return 1;
}
int __cdecl htsdefault_savename(char* adr_complete,char* fil_complete,char* referer_adr,char* referer_fil,char* save) {
  return 1;
}
// end defaut wrappers



// Fin

