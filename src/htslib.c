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
/* File: Subroutines                                            */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

// Fichier librairie .c

#include "htscore.h"

/* specific definitions */
#include "htsbase.h"
#include "htsnet.h"
#include "htsbauth.h"
#include "htsthread.h"
#include "htsback.h"
#include "htswrap.h"
#include "htsmd5.h"
#include "htsmodules.h"
#include "htscharset.h"
#include "htsencoding.h"

#ifdef _WIN32
#include <direct.h>
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif /* _WIN32 */
#include <stdarg.h>

#include <string.h>
#include <time.h>
#include <stdarg.h>

#include <sys/timeb.h>
#include <fcntl.h>

// pour utimbuf
#ifdef _WIN32
#include <sys/utime.h>
#else
#include <utime.h>
#endif /* _WIN32 */

#include <sys/stat.h>

#ifdef __ANDROID__
#define timezone 0
#endif
/* END specific definitions */

/* Windows might be missing va_copy */
#ifdef _WIN32
#ifndef va_copy
#define va_copy(dst, src) ((dst) = (src))
#endif
#endif

// Debugging
#if _HTS_WIDE
FILE *DEBUG_fp = NULL;
#endif

/* variables globales */
int _DEBUG_HEAD;
FILE *ioinfo;

#if HTS_USEOPENSSL
SSL_CTX *openssl_ctx = NULL;
#endif
int IPV6_resolver = 0;

/* détection complémentaire */
const char *hts_detect[] = {
  "archive",
  "background",
  "data",                       // OBJECT
  "dynsrc",
  "lowsrc",
  "profile",                    // element META
  "src",
  "swurl",
  "url",
  "usemap",
  "longdesc",                   // accessibility
  "xlink:href",                 // xml/svg tag
  "poster",                     // HTML5
  ""
};

/* détecter début */
const char *hts_detectbeg[] = {
  "hotspot",                    /* hotspot1=..,hotspot2=.. */
  ""
};

/* ne pas détcter de liens dedans */
const char *hts_nodetect[] = {
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
const char *hts_detect_js[] = {
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
  "style",                      /* hack for CSS code data */
  ""
};

const char *hts_main_mime[] = {
  "application",
  "audio",
  "image",
  "message",
  "multipart",
  "text",
  "video",
  ""
};

/* détection "...URL=<url>" */
const char *hts_detectURL[] = {
  "content",
  ""
};

/* tags où l'URL doit être réécrite mais non capturée */
const char *hts_detectandleave[] = {
  "action",
  ""
};

/* ne pas renommer les types renvoyés (souvent types inconnus) */
const char *hts_mime_keep[] = {
  "application/octet-stream",
  "text/plain",
  "application/xml",
  "text/xml",
  ""
};

/* bogus servers returns these mime types when the extension is seen within the filename */
const char *hts_mime_bogus_multiple[] = {
  "application/x-wais-source",  /* src (src.rpm) */
  ""
};

/* pas de type mime connu, mais extension connue */
const char *hts_ext_dynamic[] = {
  "php3",
  "php",
  "php4",
  "php2",
  "cgi",
  "asp",
  "jsp",
  "pl",
  /*"exe", */
  "cfm",
  "nsf",                        /* lotus */
  ""
};

/* types MIME 
   note: application/octet-stream should not be used here
*/
const char *hts_mime[][2] = {
  {"application/acad", "dwg"},
  {"application/arj", "arj"},
  {"application/clariscad", "ccad"},
  {"application/drafting", "drw"},
  {"application/dxf", "dxf"},
  {"application/excel", "xls"},
  {"application/i-deas", "unv"},
  {"application/iges", "isg"},
  {"application/iges", "iges"},
  {"application/mac-binhex40", "hqx"},
  {"application/mac-compactpro", "cpt"},
  {"application/msword", "doc"},
  {"application/msword", "w6w"},
  {"application/msword", "word"},
  {"application/mswrite", "wri"},
  /*{"application/octet-stream","dms"}, */
  /*{"application/octet-stream","lzh"}, */
  /*{"application/octet-stream","lha"}, */
  /*{"application/octet-stream","bin"}, */
  {"application/oda", "oda"},
  {"application/pdf", "pdf"},
  {"application/postscript", "ps"},
  {"application/postscript", "ai"},
  {"application/postscript", "eps"},
  {"application/powerpoint", "ppt"},
  {"application/pro_eng", "prt"},
  {"application/pro_eng", "part"},
  {"application/rtf", "rtf"},
  {"application/set", "set"},
  {"application/sla", "stl"},
  {"application/smil", "smi"},
  {"application/smil", "smil"},
  {"application/smil", "sml"},
  {"application/solids", "sol"},
  {"application/STEP", "stp"},
  {"application/STEP", "step"},
  {"application/vda", "vda"},
  {"application/x-authorware-map", "aam"},
  {"application/x-authorware-seg", "aas"},
  {"application/x-authorware-bin", "aab"},
  {"application/x-bzip2", "bz2"},
  {"application/x-cocoa", "cco"},
  {"application/x-csh", "csh"},
  {"application/x-director", "dir"},
  {"application/x-director", "dcr"},
  {"application/x-director", "dxr"},
  {"application/x-mif", "mif"},
  {"application/x-dvi", "dvi"},
  {"application/x-gzip", "gz"},
  {"application/x-gzip", "gzip"},
  {"application/x-hdf", "hdf"},
  {"application/x-javascript", "js"},
  {"application/x-koan", "skp"},
  {"application/x-koan", "skd"},
  {"application/x-koan", "skt"},
  {"application/x-koan", "skm"},
  {"application/x-latex", "latex"},
  {"application/x-netcdf", "nc"},
  {"application/x-netcdf", "cdf"},
  /* {"application/x-sh","sh"}, */
  /* {"application/x-csh","csh"}, */
  /* {"application/x-ksh","ksh"}, */
  {"application/x-shar", "shar"},
  {"application/x-stuffit", "sit"},
  {"application/x-tcl", "tcl"},
  {"application/x-tex", "tex"},
  {"application/x-texinfo", "texinfo"},
  {"application/x-texinfo", "texi"},
  {"application/x-troff", "t"},
  {"application/x-troff", "tr"},
  {"application/x-troff", "roff"},
  {"application/x-troff-man", "man"},
  {"application/x-troff-me", "ms"},
  {"application/x-wais-source", "src"},
  {"application/zip", "zip"},
  {"application/x-zip-compressed", "zip"},
  {"application/x-bcpio", "bcpio"},
  {"application/x-cdlink", "vcd"},
  {"application/x-cpio", "cpio"},
  {"application/x-gtar", "tgz"},
  {"application/x-gtar", "gtar"},
  {"application/x-shar", "shar"},
  {"application/x-shockwave-flash", "swf"},
  {"application/x-sv4cpio", "sv4cpio"},
  {"application/x-sv4crc", "sv4crc"},
  {"application/x-tar", "tar"},
  {"application/x-ustar", "ustar"},
  {"application/x-winhelp", "hlp"},
  {"application/xml", "xml"},
  {"audio/midi", "mid"},
  {"audio/midi", "midi"},
  {"audio/midi", "kar"},
  {"audio/mpeg", "mp3"},
  {"audio/mpeg", "mpga"},
  {"audio/mpeg", "mp2"},
  {"audio/basic", "au"},
  {"audio/basic", "snd"},
  {"audio/x-aiff", "aif"},
  {"audio/x-aiff", "aiff"},
  {"audio/x-aiff", "aifc"},
  {"audio/x-pn-realaudio", "rm"},
  {"audio/x-pn-realaudio", "ram"},
  {"audio/x-pn-realaudio", "ra"},
  {"audio/x-pn-realaudio-plugin", "rpm"},
  {"audio/x-wav", "wav"},
  {"chemical/x-pdb", "pdb"},
  {"chemical/x-pdb", "xyz"},
  {"drawing/x-dwf", "dwf"},
  {"image/gif", "gif"},
  {"image/ief", "ief"},
  {"image/jpeg", "jpg"},
  {"image/jpeg", "jpe"},
  {"image/jpeg", "jpeg"},
  {"image/pict", "pict"},
  {"image/png", "png"},
  {"image/tiff", "tiff"},
  {"image/tiff", "tif"},
  {"image/svg+xml", "svg"},
  {"image/svg-xml", "svg"},
  {"image/x-cmu-raster", "ras"},
  {"image/x-freehand", "fh4"},
  {"image/x-freehand", "fh7"},
  {"image/x-freehand", "fh5"},
  {"image/x-freehand", "fhc"},
  {"image/x-freehand", "fh"},
  {"image/x-portable-anymap", "pnm"},
  {"image/x-portable-bitmap", "pgm"},
  {"image/x-portable-pixmap", "ppm"},
  {"image/x-rgb", "rgb"},
  {"image/x-xbitmap", "xbm"},
  {"image/x-xpixmap", "xpm"},
  {"image/x-xwindowdump", "xwd"},
  {"model/mesh", "msh"},
  {"model/mesh", "mesh"},
  {"model/mesh", "silo"},
  {"multipart/x-zip", "zip"},
  {"multipart/x-gzip", "gzip"},
  {"text/css", "css"},
  {"text/html", "html"},
  {"text/html", "htm"},
  {"text/plain", "txt"},
  {"text/plain", "g"},
  {"text/plain", "h"},
  {"text/plain", "c"},
  {"text/plain", "cc"},
  {"text/plain", "hh"},
  {"text/plain", "m"},
  {"text/plain", "f90"},
  {"text/richtext", "rtx"},
  {"text/tab-separated-values", "tsv"},
  {"text/x-setext", "etx"},
  {"text/x-sgml", "sgml"},
  {"text/x-sgml", "sgm"},
  {"text/xml", "xml"},
  {"text/xml", "dtd"},
  {"video/mpeg", "mpeg"},
  {"video/mpeg", "mpg"},
  {"video/mpeg", "mpe"},
  {"video/quicktime", "qt"},
  {"video/quicktime", "mov"},
  {"video/x-msvideo", "avi"},
  {"video/x-sgi-movie", "movie"},
  {"x-conference/x-cooltalk", "ice"},
  /*{"application/x-httpd-cgi","cgi"}, */
  {"x-world/x-vrml", "wrl"},

  /* More from w3schools.com */
  {"application/envoy", "evy"},
  {"application/fractals", "fif"},
  {"application/futuresplash", "spl"},
  {"application/hta", "hta"},
  {"application/internet-property-stream", "acx"},
  {"application/msword", "dot"},
  {"application/olescript", "axs"},
  {"application/pics-rules", "prf"},
  {"application/pkcs10", "p10"},
  {"application/pkix-crl", "crl"},
  {"application/set-payment-initiation", "setpay"},
  {"application/set-registration-initiation", "setreg"},
  {"application/vnd.ms-excel", "xls"},
  {"application/vnd.ms-excel", "xla"},
  {"application/vnd.ms-excel", "xlc"},
  {"application/vnd.ms-excel", "xlm"},
  {"application/vnd.ms-excel", "xlt"},
  {"application/vnd.ms-excel", "xlw"},
  {"application/vnd.ms-pkicertstore", "sst"},
  {"application/vnd.ms-pkiseccat", "cat"},
  {"application/vnd.ms-powerpoint", "ppt"},
  {"application/vnd.ms-powerpoint", "pot"},
  {"application/vnd.ms-powerpoint", "pps"},
  {"application/vnd.ms-project", "mpp"},
  {"application/vnd.ms-works", "wcm"},
  {"application/vnd.ms-works", "wdb"},
  {"application/vnd.ms-works", "wks"},
  {"application/vnd.ms-works", "wps"},
  {"application/x-compress", "z"},
  {"application/x-compressed", "tgz"},
  {"application/x-internet-signup", "ins"},
  {"application/x-internet-signup", "isp"},
  {"application/x-iphone", "iii"},
  {"application/x-javascript", "js"},
  {"application/x-msaccess", "mdb"},
  {"application/x-mscardfile", "crd"},
  {"application/x-msclip", "clp"},
  {"application/x-msmediaview", "m13"},
  {"application/x-msmediaview", "m14"},
  {"application/x-msmediaview", "mvb"},
  {"application/x-msmetafile", "wmf"},
  {"application/x-msmoney", "mny"},
  {"application/x-mspublisher", "pub"},
  {"application/x-msschedule", "scd"},
  {"application/x-msterminal", "trm"},
  {"application/x-perfmon", "pma"},
  {"application/x-perfmon", "pmc"},
  {"application/x-perfmon", "pml"},
  {"application/x-perfmon", "pmr"},
  {"application/x-perfmon", "pmw"},
  {"application/x-pkcs12", "p12"},
  {"application/x-pkcs12", "pfx"},
  {"application/x-pkcs7-certificates", "p7b"},
  {"application/x-pkcs7-certificates", "spc"},
  {"application/x-pkcs7-certreqresp", "p7r"},
  {"application/x-pkcs7-mime", "p7c"},
  {"application/x-pkcs7-mime", "p7m"},
  {"application/x-pkcs7-signature", "p7s"},
  {"application/x-troff-me", "me"},
  {"application/x-x509-ca-cert", "cer"},
  {"application/x-x509-ca-cert", "crt"},
  {"application/x-x509-ca-cert", "der"},
  {"application/ynd.ms-pkipko", "pko"},
  {"audio/mid", "mid"},
  {"audio/mid", "rmi"},
  {"audio/mpeg", "mp3"},
  {"audio/x-mpegurl", "m3u"},
  {"image/bmp", "bmp"},
  {"image/cis-cod", "cod"},
  {"image/pipeg", "jfif"},
  {"image/x-cmx", "cmx"},
  {"image/x-icon", "ico"},
  {"image/x-portable-bitmap", "pbm"},
  {"message/rfc822", "mht"},
  {"message/rfc822", "mhtml"},
  {"message/rfc822", "nws"},
  {"text/css", "css"},
  {"text/h323", "323"},
  {"text/html", "stm"},
  {"text/iuls", "uls"},
  {"text/plain", "bas"},
  {"text/scriptlet", "sct"},
  {"text/webviewhtml", "htt"},
  {"text/x-component", "htc"},
  {"text/x-vcard", "vcf"},
  {"video/mpeg", "mp2"},
  {"video/mpeg", "mpa"},
  {"video/mpeg", "mpv2"},
  {"video/x-la-asf", "lsf"},
  {"video/x-la-asf", "lsx"},
  {"video/x-ms-asf", "asf"},
  {"video/x-ms-asf", "asr"},
  {"video/x-ms-asf", "asx"},
  {"video/x-ms-wmv", "wmv"},
  {"x-world/x-vrml", "flr"},
  {"x-world/x-vrml", "vrml"},
  {"x-world/x-vrml", "wrz"},
  {"x-world/x-vrml", "xaf"},
  {"x-world/x-vrml", "xof"},

  /* Various */
  {"application/ogg", "ogg"},

  {"application/x-java-vm", "class"},
  {"application/x-bittorrent","torrent"},

  {"", ""}
};

// Reserved (RFC2396)
#define CIS(c,ch) ( ((unsigned char)(c)) == (ch) )
#define CHAR_RESERVED(c)  ( CIS(c,';') \
                         || CIS(c,'/') \
                         || CIS(c,'?') \
                         || CIS(c,':') \
                         || CIS(c,'@') \
                         || CIS(c,'&') \
                         || CIS(c,'=') \
                         || CIS(c,'+') \
                         || CIS(c,'$') \
                         || CIS(c,',') )
//#define CHAR_RESERVED(c)  ( strchr(";/?:@&=+$,",(unsigned char)(c)) != 0 )
// Delimiters (RFC2396)
#define CHAR_DELIM(c)     ( CIS(c,'<') \
                         || CIS(c,'>') \
                         || CIS(c,'#') \
                         || CIS(c,'%') \
                         || CIS(c,'\"') )
//#define CHAR_DELIM(c)     ( strchr("<>#%\"",(unsigned char)(c)) != 0 )
// Unwise (RFC2396)
#define CHAR_UNWISE(c)    ( CIS(c,'{') \
                         || CIS(c,'}') \
                         || CIS(c,'|') \
                         || CIS(c,'\\') \
                         || CIS(c,'^') \
                         || CIS(c,'[') \
                         || CIS(c,']') \
                         || CIS(c,'`') )
//#define CHAR_UNWISE(c)    ( strchr("{}|\\^[]`",(unsigned char)(c)) != 0 )
// Special (escape chars) (RFC2396 + >127 )
#define CHAR_LOW(c)       ( ((unsigned char)(c) <= 31) )
#define CHAR_HIG(c)       ( ((unsigned char)(c) >= 127) )
#define CHAR_SPECIAL(c)   ( CHAR_LOW(c) || CHAR_HIG(c) )
// We try to avoid them and encode them instead
#define CHAR_XXAVOID(c)   ( CIS(c,' ') \
                         || CIS(c,'*') \
                         || CIS(c,'\'') \
                         || CIS(c,'\"') \
                         || CIS(c,'&') \
                         || CIS(c,'!') )
//#define CHAR_XXAVOID(c)   ( strchr(" *'\"!",(unsigned char)(c)) != 0 )
#define CHAR_MARK(c)      ( CIS(c,'-') \
                         || CIS(c,'_') \
                         || CIS(c,'.') \
                         || CIS(c,'!') \
                         || CIS(c,'~') \
                         || CIS(c,'*') \
                         || CIS(c,'\'') \
                         || CIS(c,'(') \
                         || CIS(c,')') )
//#define CHAR_MARK(c)      ( strchr("-_.!~*'()",(unsigned char)(c)) != 0 )

// conversion éventuelle / vers antislash
#ifdef _WIN32
char *antislash(char *catbuff, const char *s) {
  char *a;

  strcpybuff(catbuff, s);
  while(a = strchr(catbuff, '/'))
    *a = '\\';
  return catbuff;
}
#endif

// Initialize a htsblk structure
void hts_init_htsblk(htsblk * r) {
  memset(r, 0, sizeof(htsblk)); // effacer
  r->soc = INVALID_SOCKET;
  r->msg[0] = '\0';
  r->statuscode = STATUSCODE_INVALID;
  r->totalsize = -1;
}

// ouvre une liaison http, envoie une requète GET et réceptionne le header
// retour: socket
T_SOC http_fopen(httrackp * opt, const char *adr, const char *fil, htsblk * retour) {
  //                / GET, traiter en-tête
  return http_xfopen(opt, 0, 1, 1, NULL, adr, fil, retour);
}

// ouverture d'une liaison http, envoi d'une requète
// mode: 0 GET  1 HEAD  [2 POST]
// treat: traiter header?
// waitconnect: attendre le connect()
// note: dans retour, on met les params du proxy
T_SOC http_xfopen(httrackp * opt, int mode, int treat, int waitconnect,
                  const char *xsend, const char *adr, const char *fil, htsblk * retour) {
  //htsblk retour;
  //int bufl=TAILLE_BUFFER;    // 8Ko de buffer
  T_SOC soc = INVALID_SOCKET;
  char BIGSTK tempo_fil[HTS_URLMAXSIZE * 2];

  //char *p,*q;

  // retour prédéfini: erreur
  if (retour) {
    retour->adr = NULL;
    retour->size = 0;
    retour->msg[0] = '\0';
    retour->statuscode = STATUSCODE_NON_FATAL;  // a priori erreur non fatale
  }
#if HDEBUG
  printf("adr=%s\nfichier=%s\n", adr, fil);
#endif

  // ouvrir liaison
#if HDEBUG
  printf("Création d'une socket sur %s\n", adr);
#endif

#if CNXDEBUG
  printf("..newhttp\n");
#endif

  /* connexion */
  if (retour) {
    if ((!(retour->req.proxy.active))
        || ((strcmp(adr, "file://") == 0)
            || (strncmp(adr, "https://", 8) == 0)
        )
      ) {                       /* pas de proxy, ou non utilisable ici */
      soc = newhttp(opt, adr, retour, -1, waitconnect);
    } else {
      soc = newhttp(opt, retour->req.proxy.name, retour, retour->req.proxy.port, waitconnect);  // ouvrir sur le proxy à la place
    }
  } else {
    soc = newhttp(opt, adr, NULL, -1, waitconnect);
  }

  // copier index socket retour
  if (retour)
    retour->soc = soc;

  /* Check for errors */
  if (soc == INVALID_SOCKET) {
    if (retour) {
      if (retour->msg) {
        if (!strnotempty(retour->msg)) {
#ifdef _WIN32
          int last_errno = WSAGetLastError();

          sprintf(retour->msg, "Connect error: %s", strerror(last_errno));
#else
          int last_errno = errno;

          sprintf(retour->msg, "Connect error: %s", strerror(last_errno));
#endif
        }
      }
    }
  }
  // --------------------
  // court-circuit (court circuite aussi le proxy..)
  // LOCAL_SOCKET_ID est une pseudo-socket locale
  if (soc == LOCAL_SOCKET_ID) {
    retour->is_file = 1;        // fichier local
    if (mode == 0) {            // GET

      // Test en cas de file:///C|...
      if (!fexist
          (fconv(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
          unescape_http(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), fil))))
        if (fexist
            (fconv
             (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
             unescape_http(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), fil + 1)))) {
          strcpybuff(tempo_fil, fil + 1);
          fil = tempo_fil;
        }
      // Ouvrir
      retour->totalsize = fsize(fconv(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), 
        unescape_http(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), fil)));       // taille du fichier
      retour->msg[0] = '\0';
      soc = INVALID_SOCKET;
      if (retour->totalsize < 0)
        strcpybuff(retour->msg, "Unable to open local file");
      else {
        // Note: On passe par un FILE* (plus propre)
        //soc=open(fil,O_RDONLY,0);    // en lecture seule!
        retour->fp = FOPEN(fconv(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), 
          unescape_http(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), fil)), "rb");      // ouvrir
        if (retour->fp == NULL)
          soc = INVALID_SOCKET;
        else
          soc = LOCAL_SOCKET_ID;
      }
      retour->soc = soc;
      if (soc != INVALID_SOCKET) {
        retour->statuscode = HTTP_OK;   // OK
        strcpybuff(retour->msg, "OK");
        guess_httptype(opt, retour->contenttype, fil);
      } else if (strnotempty(retour->msg) == 0)
        strcpybuff(retour->msg, "Unable to open local file");
      return soc;               // renvoyer
    } else {                    // HEAD ou POST : interdit sur un local!!!! (c'est idiot!)
      strcpybuff(retour->msg, "Unexpected Head/Post local request");
      soc = INVALID_SOCKET;     // erreur
      retour->soc = soc;
      return soc;
    }
  }
  // --------------------

  if (soc != INVALID_SOCKET) {
    char rcvd[1100];

    rcvd[0] = '\0';
#if HDEBUG
    printf("Ok, connexion réussie, id=%d\n", soc);
#endif

    // connecté?
    if (waitconnect) {
      http_sendhead(opt, NULL, mode, xsend, adr, fil, NULL, NULL, retour);
    }

    if (soc != INVALID_SOCKET) {

#if HDEBUG
      printf("Attente de la réponse:\n");
#endif

      // si GET (réception d'un fichier), réceptionner en-tête d'abord,
      // et ensuite le corps
      // si POST on ne réceptionne rien du tout, c'est après que l'on fera
      // une réception standard pour récupérer l'en tête
      if ((treat) && (waitconnect)) {   // traiter (attendre!) en-tête
        // Réception de la status line et de l'en-tête (norme RFC1945)

        // status-line à récupérer
        finput(soc, rcvd, 1024);
        if (strnotempty(rcvd) == 0)
          finput(soc, rcvd, 1024);      // "certains serveurs buggés envoient un \n au début" (RFC)

        // traiter status-line
        treatfirstline(retour, rcvd);

#if HDEBUG
        printf("Status-Code=%d\n", retour->statuscode);
#endif

        // en-tête

        // header // ** !attention! HTTP/0.9 non supporté
        do {
          finput(soc, rcvd, 1024);
#if HDEBUG
          printf(">%s\n", rcvd);
#endif
          if (strnotempty(rcvd))
            treathead(NULL, NULL, NULL, retour, rcvd);  // traiter

        } while(strnotempty(rcvd));

        //rcvsize=-1;    // forCER CHARGEMENT INCONNU

        //if (retour)
        //  retour->totalsize=rcvsize;

      } else {                  // si GET, on recevra l'en tête APRES
        //rcvsize=-1;    // on ne connait pas la taille de l'en-tête
        if (retour)
          retour->totalsize = -1;
      }

    }

  }

  return soc;
}

/* Buffer printing */
typedef struct buff_struct {
  /** Buffer **/
  char *buffer;
  /** Buffer capacity in bytes **/
  size_t capacity;
  /** Buffer write position ; MUST point to a valid \0. **/
  size_t pos;
} buff_struct;

static void print_buffer(buff_struct*const str, const char *format, ...)
  HTS_PRINTF_FUN(2, 3);

/* Prints on a static buffer. asserts in case of overflow. */
static void print_buffer(buff_struct*const str, const char *format, ...) {
  size_t result;
  va_list args;
  size_t remaining;
  char *position;

  /* Security check. */
  assertf(str != NULL);
  assertf(str->pos < str->capacity);

  /* Print */
  position = &str->buffer[str->pos];
  remaining = str->capacity - str->pos;
  va_start(args, format);
  result = (size_t) vsnprintf(position, remaining, format, args);
  va_end(args);
  assertf(result < remaining);

  /* Increment. */
  str->pos += strlen(position);
  assertf(str->pos < str->capacity);
}

// envoi d'une requète
int http_sendhead(httrackp * opt, t_cookie * cookie, int mode,
                  const char *xsend, const char *adr, const char *fil,
                  const char *referer_adr, const char *referer_fil,
                  htsblk * retour) {
  char BIGSTK buffer_head_request[8192];
  buff_struct bstr = { buffer_head_request, sizeof(buffer_head_request), 0 };

  //int use_11=0;     // HTTP 1.1 utilisé
  int direct_url = 0;           // ne pas analyser l'url (exemple: ftp://)
  const char *search_tag = NULL;

  // Initialize buffer
  buffer_head_request[0] = '\0';

  // header Date
  //strcatbuff(buff,"Date: ");
  //time_gmt_rfc822(buff);    // obtenir l'heure au format rfc822
  //sendc("\n");
  //strcatbuff(buff,buff);

  // possibilité non documentée: >post: et >postfile:
  // si présence d'un tag >post: alors executer un POST
  // exemple: http://www.someweb.com/test.cgi?foo>post:posteddata=10&foo=5
  // si présence d'un tag >postfile: alors envoyer en tête brut contenu dans le fichier en question
  // exemple: http://www.someweb.com/test.cgi?foo>postfile:post0.txt
  search_tag = strstr(fil, POSTTOK ":");
  if (!search_tag) {
    search_tag = strstr(fil, POSTTOK "file:");
    if (search_tag) {           // postfile
      if (mode == 0) {          // GET!
        FILE *fp =
          FOPEN(unescape_http(OPT_GET_BUFF(opt), 
                OPT_GET_BUFF_SIZE(opt), search_tag + strlen(POSTTOK) + 5), "rb");
        if (fp) {
          char BIGSTK line[1100];
          char BIGSTK protocol[256], url[HTS_URLMAXSIZE * 2], method[256];

          linput(fp, line, 1000);
          if (sscanf(line, "%s %s %s", method, url, protocol) == 3) {
            size_t ret;
            // selon que l'on a ou pas un proxy
            if (retour->req.proxy.active) {
              print_buffer(&bstr,
                      "%s http://%s%s %s\r\n", method, adr, url,
                      protocol);
            } else {
              print_buffer(&bstr,
                       "%s %s %s\r\n", method, url, protocol);
            }
            // lire le reste en brut
            ret = fread(&bstr.buffer[bstr.pos],
                        bstr.capacity - bstr.pos, 1, fp);
            if ((int) ret < 0) {
              return -1;
            }
            bstr.pos += strlen(&bstr.buffer[bstr.pos]);
          }
          fclose(fp);
        }
      }
    }
  }
  // Fin postfile

  if (bstr.pos == 0) { // PAS POSTFILE
    // Type de requète?
    if ((search_tag) && (mode == 0)) {
      print_buffer(&bstr, "POST ");
    } else if (mode == 0) {     // GET
      print_buffer(&bstr, "GET ");
    } else {                    // if (mode==1) {
      if (!retour->req.http11)  // forcer HTTP/1.0
        print_buffer(&bstr, "GET ");       // certains serveurs (cgi) buggent avec HEAD
      else
        print_buffer(&bstr, "HEAD ");
    }

    // si on gère un proxy, il faut une Absolute URI: on ajoute avant http://www.adr.dom
    if (retour->req.proxy.active && (strncmp(adr, "https://", 8) != 0)) {
      if (!link_has_authority(adr)) {   // default http
#if HDEBUG
        printf("Proxy Use: for %s%s proxy %d port %d\n", adr, fil,
               retour->req.proxy.name, retour->req.proxy.port);
#endif
        print_buffer(&bstr, "http://%s", jump_identification_const(adr));
      } else {                  // ftp:// en proxy http
#if HDEBUG
        printf("Proxy Use for ftp: for %s%s proxy %d port %d\n", adr, fil,
               retour->req.proxy.name, retour->req.proxy.port);
#endif
        direct_url = 1;         // ne pas analyser user/pass
        print_buffer(&bstr, "%s", adr);
      }
    }
    // NOM DU FICHIER
    // on slash doit être présent en début, sinon attention aux bad request! (400)
    if (*fil != '/')
      print_buffer(&bstr, "/");

    {
      char BIGSTK tempo[HTS_URLMAXSIZE * 2];

      tempo[0] = '\0';
      if (search_tag)
        strncatbuff(tempo, fil, (int) (search_tag - fil));
      else
        strcpybuff(tempo, fil);
      inplace_escape_check_url(tempo, sizeof(tempo));
      print_buffer(&bstr, "%s", tempo);  // avec échappement
    }

    // protocole
    if (!retour->req.http11) {  // forcer HTTP/1.0
      //use_11=0;
      print_buffer(&bstr, " HTTP/1.0\x0d\x0a");
    } else {                    // Requète 1.1
      //use_11=1;
      print_buffer(&bstr, " HTTP/1.1\x0d\x0a");
    }

    /* supplemental data */
    if (xsend)
      print_buffer(&bstr, "%s", xsend);  // éventuelles autres lignes

    // tester proxy authentication
    if (retour->req.proxy.active) {
      if (link_has_authorization(retour->req.proxy.name)) {     // et hop, authentification proxy!
        const char *a = jump_identification_const(retour->req.proxy.name);
        const char *astart = jump_protocol_const(retour->req.proxy.name);
        char autorisation[1100];
        char user_pass[256];

        autorisation[0] = user_pass[0] = '\0';
        //
        strncatbuff(user_pass, astart, (int) (a - astart) - 1);
        strcpybuff(user_pass, unescape_http(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), user_pass));
        code64((unsigned char *) user_pass, (int) strlen(user_pass),
               (unsigned char *) autorisation, 0);
        print_buffer(&bstr, "Proxy-Authorization: Basic %s"H_CRLF,
                     autorisation);
#if HDEBUG
        printf("Proxy-Authenticate, %s (code: %s)\n", user_pass, autorisation);
#endif
      }
    }
    // Referer?
    if (referer_adr != NULL && referer_fil != NULL && strnotempty(referer_adr)
        && strnotempty(referer_fil)
      ) {                       // non vide
      if ((strcmp(referer_adr, "file://") != 0)
          && (                  /* no https referer to http urls */
               (strncmp(referer_adr, "https://", 8) != 0)       /* referer is not https */
               ||(strncmp(adr, "https://", 8) == 0)     /* or referer AND addresses are https */
          )
        ) {                     // PAS file://
        print_buffer(&bstr, "Referer: http://%s%s"H_CRLF,
                     jump_identification_const(referer_adr), referer_fil);
      }
    }
    // HTTP field: referer
    else if (strnotempty(retour->req.referer)) {
      print_buffer(&bstr, "Referer: %s"H_CRLF, retour->req.referer);
    }
    // POST?
    if (mode == 0) {            // GET!
      if (search_tag) {
        print_buffer(&bstr, "Content-length: %d" H_CRLF,
                (int) (strlen
                       (unescape_http
                        (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                         search_tag + strlen(POSTTOK) + 1))));
      }
    }
    // gestion cookies?
    if (cookie) {
      char buffer[8192];
      char *b = cookie->data;
      int cook = 0;
      int max_cookies = 8;

      do {
        b = cookie_find(b, "", jump_identification_const(adr), fil);  // prochain cookie satisfaisant aux conditions
        if (b != NULL) {
          max_cookies--;
          if (!cook) {
            print_buffer(&bstr, "Cookie: $Version=1; ");
            cook = 1;
          } else
            print_buffer(&bstr, "; ");
          print_buffer(&bstr, "%s", cookie_get(buffer, b, 5));
          print_buffer(&bstr, "=%s", cookie_get(buffer, b, 6));
          print_buffer(&bstr, "; $Path=%s", cookie_get(buffer, b, 2));
          b = cookie_nextfield(b);
        }
      } while(b != NULL && max_cookies > 0);
      if (cook) {               // on a envoyé un (ou plusieurs) cookie?
        print_buffer(&bstr, H_CRLF);
#if DEBUG_COOK
        printf("Header:\n%s\n", bstr.buffer);
#endif
      }
    }
    // gérer le keep-alive (garder socket)
    if (retour->req.http11 && !retour->req.nokeepalive) {
      print_buffer(&bstr, "Connection: keep-alive" H_CRLF);
    } else {
      print_buffer(&bstr, "Connection: close" H_CRLF);
    }

    {
      const char *real_adr = jump_identification_const(adr);

      // Mandatory per RFC2616
      if (!direct_url) {        // pas ftp:// par exemple
        print_buffer(&bstr, "Host: %s"H_CRLF, real_adr);
      }

      // HTTP field: from
      if (strnotempty(retour->req.from)) {        // HTTP from
        print_buffer(&bstr, "From: %s" H_CRLF, retour->req.from);
      }

      // Présence d'un user-agent?
      if (retour->req.user_agent_send
          && strnotempty(retour->req.user_agent)) {
        print_buffer(&bstr, "User-Agent: %s" H_CRLF, retour->req.user_agent);
      }

      // Accept
      if (strnotempty(retour->req.accept)) {
        print_buffer(&bstr, "Accept: %s" H_CRLF, retour->req.accept);
      }

      // Accept-language
      if (strnotempty(retour->req.lang_iso)) {
        print_buffer(&bstr, "Accept-Language: %s"H_CRLF, retour->req.lang_iso);
      }

      // Compression accepted ?
      if (retour->req.http11) {
#if HTS_USEZLIB
        if ((!retour->req.range_used)
            && (!retour->req.nocompression))
          print_buffer(&bstr, "Accept-Encoding: " "gzip" /* gzip if the preffered encoding */
                     ", " "identity;q=0.9" H_CRLF);
        else
          print_buffer(&bstr, "Accept-Encoding: identity" H_CRLF);       /* no compression */
#else
        print_buffer(&bstr, "Accept-Encoding: identity" H_CRLF); /* no compression */
#endif
      }

      /* Authentification */
      {
        char autorisation[1100];
        const char *a;

        autorisation[0] = '\0';
        if (link_has_authorization(adr)) {      // ohh une authentification!
          const char *a = jump_identification_const(adr);
          const char *astart = jump_protocol_const(adr);

          if (!direct_url) {    // pas ftp:// par exemple
            char user_pass[256];

            user_pass[0] = '\0';
            strncatbuff(user_pass, astart, (int) (a - astart) - 1);
            strcpybuff(user_pass, 
              unescape_http(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), user_pass));
            code64((unsigned char *) user_pass, (int) strlen(user_pass),
                   (unsigned char *) autorisation, 0);
            if (strcmp(fil, "/robots.txt"))     /* pas robots.txt */
              bauth_add(cookie, astart, fil, autorisation);
          }
        } else if ((a = bauth_check(cookie, real_adr, fil)))
          strcpybuff(autorisation, a);
        /* On a une autorisation a donner?  */
        if (strnotempty(autorisation)) {
          print_buffer(&bstr, "Authorization: Basic %s"H_CRLF, autorisation);
        }
      }

    }
    //strcatbuff(buff,"Accept-Charset: iso-8859-1,*,utf-8\n");

    // Custom header(s)
    if (strnotempty(retour->req.headers)) {
      print_buffer(&bstr, "%s", retour->req.headers);
    }

    // CRLF de fin d'en tête
    print_buffer(&bstr, H_CRLF);

    // données complémentaires?
    if (search_tag)
      if (mode == 0)            // GET!
        print_buffer(&bstr, "%s",
                   unescape_http(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                 search_tag + strlen(POSTTOK) + 1));
  }
#if HDEBUG
#endif
  if (_DEBUG_HEAD) {
    if (ioinfo) {
      fprintf(ioinfo, "[%d] request for %s%s:\r\n", retour->debugid,
              jump_identification_const(adr), fil);
      fprintfio(ioinfo, bstr.buffer, "<<< ");
      fprintf(ioinfo, "\r\n");
      fflush(ioinfo);
    }
  }                             // Fin test pas postfile
  //

  // Callback
  {
    int test_head =
      RUN_CALLBACK6(opt, sendhead, bstr.buffer, adr, fil, referer_adr, referer_fil,
                    retour);
    if (test_head != 1) {
      deletesoc_r(retour);
      strcpybuff(retour->msg, "Header refused by external wrapper");
      retour->soc = INVALID_SOCKET;
    }
  }

  // Envoi
  HTS_STAT.last_request = mtime_local();
  if (sendc(retour, bstr.buffer) < 0) {        // ERREUR, socket rompue?...
    deletesoc_r(retour);        // fermer tout de même
    // et tenter de reconnecter

    strcpybuff(retour->msg, "Write error");
    retour->soc = INVALID_SOCKET;
  }

  // RX'98
  return 0;
}

// traiter 1ere ligne d'en tête
void treatfirstline(htsblk * retour, const char *rcvd) {
  const char *a = rcvd;

  // exemple:
  // HTTP/1.0 200 OK
  if (*a) {
    // note: certains serveurs buggés renvoient HTTP/1.0\n200 OK ou " HTTP/1.0 200 OK"
    while((*a == ' ') || (*a == 10) || (*a == 13) || (*a == 9))
      a++;                      // épurer espaces au début
    if (strfield(a, "HTTP/")) {
      // sauter HTTP/1.x
      while((*a != ' ') && (*a != '\0') && (*a != 10) && (*a != 13)
            && (*a != 9))
        a++;
      if (*a != '\0') {
        while((*a == ' ') || (*a == 10) || (*a == 13) || (*a == 9))
          a++;                  // épurer espaces
        if ((*a >= '0') && (*a <= '9')) {
          sscanf(a, "%d", &(retour->statuscode));
          // sauter 200
          while((*a != ' ') && (*a != '\0') && (*a != 10) && (*a != 13)
                && (*a != 9))
            a++;
          while((*a == ' ') || (*a == 10) || (*a == 13) || (*a == 9))
            a++;                // épurer espaces
          if ((strlen(a) > 1) && (strlen(a) < 64))      // message retour
            strcpybuff(retour->msg, a);
          else
            infostatuscode(retour->msg, retour->statuscode);
          // type MIME par défaut2
          strcpybuff(retour->contenttype, HTS_HYPERTEXT_DEFAULT_MIME);
        } else {                // pas de code!
          retour->statuscode = STATUSCODE_INVALID;
          strcpybuff(retour->msg, "Unknown response structure");
        }
      } else {                  // euhh??
        retour->statuscode = STATUSCODE_INVALID;
        strcpybuff(retour->msg, "Unknown response structure");
      }
    } else {
      if (*a == '<') {
        /* This is dirty .. */
        retour->statuscode = HTTP_OK;
        retour->keep_alive = 0;
        strcpybuff(retour->msg, "Unknown, assuming junky server");
        strcpybuff(retour->contenttype, HTS_HYPERTEXT_DEFAULT_MIME);
      } else if (strnotempty(a)) {
        retour->statuscode = STATUSCODE_INVALID;
        strcpybuff(retour->msg, "Unknown (not HTTP/xx) response structure");
      } else {
        /* This is dirty .. */
        retour->statuscode = HTTP_OK;
        retour->keep_alive = 0;
        strcpybuff(retour->msg, "Unknown, assuming junky server");
        strcpybuff(retour->contenttype, HTS_HYPERTEXT_DEFAULT_MIME);
      }
    }
  } else {                      // vide!
    /*
       retour->statuscode=STATUSCODE_INVALID;
       strcpybuff(retour->msg,"Empty reponse or internal error");
     */
    /* This is dirty .. */
    retour->statuscode = HTTP_OK;
    strcpybuff(retour->msg, "Unknown, assuming junky server");
    strcpybuff(retour->contenttype, HTS_HYPERTEXT_DEFAULT_MIME);
  }
}

// traiter ligne par ligne l'en tête
// gestion des cookies
void treathead(t_cookie * cookie, const char *adr, const char *fil, htsblk * retour,
               char *rcvd) {
  int p;

  if ((p = strfield(rcvd, "Content-length:")) != 0) {
#if HDEBUG
    printf("ok, Content-length: détecté\n");
#endif
    if (sscanf(rcvd + p, LLintP, &(retour->totalsize)) == 1) {
      if (retour->totalsize == 0) {
        retour->empty = 1;
      }
    }
  } else if ((p = strfield(rcvd, "Content-Disposition:")) != 0) {
    while(is_realspace(*(rcvd + p)))
      p++;                      // sauter espaces
    if ((int) strlen(rcvd + p) < 250) { // pas trop long?
      char tmp[256];
      char *a = NULL, *b = NULL;

      strcpybuff(tmp, rcvd + p);
      a = strstr(tmp, "filename=");
      if (a) {
        a += strlen("filename=");
        while(is_space(*a))
          a++;
        //a=strchr(a,'"');
        if (a) {
          char *c = NULL;

          //a++;      /* jump " */
          while((c = strchr(a, '/')))   /* skip all / (see RFC2616) */
            a = c + 1;
          //b=strchr(a+1,'"');
          b = a + strlen(a) - 1;
          while(is_space(*b))
            b--;
          b++;
          if (b) {
            *b = '\0';
            if ((int) strlen(a) < 200) {        // pas trop long?
              strcpybuff(retour->cdispo, a);
            }
          }
        }
      }
    }
  } else if ((p = strfield(rcvd, "Last-Modified:")) != 0) {
    while(is_realspace(*(rcvd + p)))
      p++;                      // sauter espaces
    if ((int) strlen(rcvd + p) < 64) {  // pas trop long?
      //struct tm* tm_time=convert_time_rfc822(rcvd+p);
      strcpybuff(retour->lastmodified, rcvd + p);
    }
  } else if ((p = strfield(rcvd, "Date:")) != 0) {
    if (strnotempty(retour->lastmodified) == 0) {       /* pas encore de last-modified */
      while(is_realspace(*(rcvd + p)))
        p++;                    // sauter espaces
      if ((int) strlen(rcvd + p) < 64) {        // pas trop long?
        //struct tm* tm_time=convert_time_rfc822(rcvd+p);
        strcpybuff(retour->lastmodified, rcvd + p);
      }
    }
  } else if ((p = strfield(rcvd, "Etag:")) != 0) {      /* Etag */
    if (retour) {
      while(is_realspace(*(rcvd + p)))
        p++;                    // sauter espaces
      if ((int) strlen(rcvd + p) < 64)  // pas trop long?
        strcpybuff(retour->etag, rcvd + p);
      else                      // erreur.. ignorer
        retour->etag[0] = '\0';
    }
  }
  // else if ((p=strfield(rcvd,"Transfer-Encoding: chunked"))!=0) {  // chunk!
  else if ((p = strfield(rcvd, "Transfer-Encoding:")) != 0) {   // chunk!
    while(is_realspace(*(rcvd + p)))
      p++;                      // sauter espaces
    if (strfield(rcvd + p, "chunked")) {
      retour->is_chunk = 1;     // chunked
      //retour->http11=2;     // chunked
#if HDEBUG
      printf("ok, Transfer-Encoding: détecté\n");
#endif
    }
  } else if ((p = strfield(rcvd, "Content-type:")) != 0) {
    if (retour) {
      char tempo[1100];

      // éviter les text/html; charset=foo
      {
        char *a = strchr(rcvd + p, ';');

        if (a) {                // extended information
          *a = '\0';
          a++;
          while(is_space(*a))
            a++;
          if (strfield(a, "charset")) {
            a += 7;
            while(is_space(*a))
              a++;
            if (*a == '=') {
              a++;
              while(is_space(*a))
                a++;
              if (*a == '\"')
                a++;
              while(is_space(*a))
                a++;
              if (*a) {
                char *chs = a;

                while(*a && !is_space(*a) && *a != '\"' && *a != ';')
                  a++;
                *a = '\0';
                if (*chs) {
                  if (strlen(chs) < sizeof(retour->charset) - 2) {
                    strcpybuff(retour->charset, chs);
                  }
                }
              }
            }
          }
        }
      }
      sscanf(rcvd + p, "%s", tempo);
      if (strlen(tempo) < sizeof(retour->contenttype) - 2)      // pas trop long!!
        strcpybuff(retour->contenttype, tempo);
      else
        strcpybuff(retour->contenttype, "application/octet-stream-unknown");    // erreur
    }
  } else if ((p = strfield(rcvd, "Content-Range:")) != 0) {
    // Content-Range: bytes 0-70870/70871
    const char *a;

    for(a = rcvd + p; is_space(*a); a++) ;
    if (strncasecmp(a, "bytes ", 6) == 0) {
      for(a += 6; is_space(*a); a++) ;
      if (sscanf
          (a, LLintP "-" LLintP "/" LLintP, &retour->crange_start,
           &retour->crange_end, &retour->crange) != 3) {
        retour->crange_start = 0;
        retour->crange_end = 0;
        retour->crange = 0;
        a = strchr(rcvd + p, '/');
        if (a != NULL) {
          a++;
          if (sscanf(a, LLintP, &retour->crange) == 1) {
            retour->crange_start = 0;
            retour->crange_end = retour->crange - 1;
          } else {
            retour->crange = 0;
          }
        }
      }
    }
  } else if ((p = strfield(rcvd, "Connection:")) != 0) {
    char *a = rcvd + p;

    while(is_space(*a))
      a++;
    if (*a) {
      if (strfield(a, "Keep-Alive")) {
        if (!retour->keep_alive) {
          retour->keep_alive_max = 10;
          retour->keep_alive_t = 15;
        }
        retour->keep_alive = 1;
      } else {
        retour->keep_alive = 0;
      }
    }
  } else if ((p = strfield(rcvd, "Keep-Alive:")) != 0) {
    char *a = rcvd + p;

    while(is_space(*a))
      a++;
    if (*a) {
      char *p;

      retour->keep_alive = 1;
      retour->keep_alive_max = 10;
      retour->keep_alive_t = 15;
      if ((p = strstr(a, "timeout="))) {
        p += strlen("timeout=");
        sscanf(p, "%d", &retour->keep_alive_t);
      }
      if ((p = strstr(a, "max="))) {
        p += strlen("max=");
        sscanf(p, "%d", &retour->keep_alive_max);
      }
      if (retour->keep_alive_max <= 1 || retour->keep_alive_t < 1) {
        retour->keep_alive = 0;
      }
    }
  } else if ((p = strfield(rcvd, "TE:")) != 0) {
    char *a = rcvd + p;

    while(is_space(*a))
      a++;
    if (*a) {
      if (strfield(a, "trailers")) {
        retour->keep_alive_trailers = 1;
      }
    }
  } else if ((p = strfield(rcvd, "Content-Encoding:")) != 0) {
    if (retour) {
      char tempo[1100];
      char *a = rcvd + p;

      while(is_space(*a))
        a++;
      {
        char *a = strchr(rcvd + p, ';');

        if (a)
          *a = '\0';
      }
      sscanf(a, "%s", tempo);
      if (strlen(tempo) < 64)   // pas trop long!!
        strcpybuff(retour->contentencoding, tempo);
      else
        retour->contentencoding[0] = '\0';      // erreur
#if HTS_USEZLIB
      /* Check known encodings */
      if (retour->contentencoding[0]) {
        if ((strfield2(retour->contentencoding, "gzip"))
            || (strfield2(retour->contentencoding, "x-gzip"))
            /*
               || (strfield2(retour->contentencoding, "compress"))
               || (strfield2(retour->contentencoding, "x-compress"))
             */
            || (strfield2(retour->contentencoding, "deflate"))
            || (strfield2(retour->contentencoding, "x-deflate"))
          ) {
          retour->compressed = 1;
        }
      }
#endif
    }
  } else if ((p = strfield(rcvd, "Location:")) != 0) {
    if (retour) {
      if (retour->location) {
        while(is_realspace(*(rcvd + p)))
          p++;                  // sauter espaces
        if ((int) strlen(rcvd + p) < HTS_URLMAXSIZE)    // pas trop long?
          strcpybuff(retour->location, rcvd + p);
        else                    // erreur.. ignorer
          retour->location[0] = '\0';
      }
    }
  } else if (((p = strfield(rcvd, "Set-Cookie:")) != 0) && (cookie)) {  // ohh un cookie
    char *a = rcvd + p;         // pointeur
    char domain[256];           // domaine cookie (.netscape.com)
    char path[256];             // chemin (/)
    char cook_name[256];        // nom cookie (MYCOOK)
    char BIGSTK cook_value[8192];       // valeur (ID=toto,S=1234)

#if DEBUG_COOK
    printf("set-cookie detected\n");
#endif
    while(*a) {
      char *token_st, *token_end;
      char *value_st, *value_end;
      char name[256];
      char BIGSTK value[8192];
      int next = 0;

      name[0] = value[0] = '\0';
      //

      // initialiser cookie lu actuellement
      if (adr)
        strcpybuff(domain, jump_identification_const(adr));   // domaine
      strcpybuff(path, "/");    // chemin (/)
      strcpybuff(cook_name, "");        // nom cookie (MYCOOK)
      strcpybuff(cook_value, "");       // valeur (ID=toto,S=1234)
      // boucler jusqu'au prochain cookie ou la fin
      do {
        char *start_loop = a;

        while(is_space(*a))
          a++;                  // sauter espaces
        token_st = a;           // départ token
        while((!is_space(*a)) && (*a) && (*a != ';') && (*a != '='))
          a++;                  // arrêter si espace, point virgule
        token_end = a;
        while(is_space(*a))
          a++;                  // sauter espaces
        if (*a == '=') {        // name=value
          a++;
          while(is_space(*a))
            a++;                // sauter espaces
          value_st = a;
          while((*a != ';') && (*a))
            a++;                // prochain ;
          //while( ((*a!='"') || (*(a-1)=='\\')) && (*a)) a++;    // prochain " (et pas \")
          value_end = a;
          //if (*a==';') {  // finit par un ;
          // vérifier débordements
          if ((((int) (token_end - token_st)) < 200)
              && (((int) (value_end - value_st)) < 8000)
              && (((int) (token_end - token_st)) > 0)
              && (((int) (value_end - value_st)) > 0)) {
            int name_len = (int) (token_end - token_st);
            int value_len = (int) (value_end - value_st);

            name[0] = '\0';
            value[0] = '\0';
            strncatbuff(name, token_st, name_len);
            strncatbuff(value, value_st, value_len);
#if DEBUG_COOK
            printf("detected cookie-av: name=\"%s\" value=\"%s\"\n", name,
                   value);
#endif
            if (strfield2(name, "domain")) {
              if (value_len < sizeof(domain) - 1) {
                strcpybuff(domain, value);
              } else {
                cook_name[0] = 0;
                break;
              }
            } else if (strfield2(name, "path")) {
              if (value_len < sizeof(path) - 1) {
                strcpybuff(path, value);
              } else {
                cook_name[0] = 0;
                break;
              }
            } else if (strfield2(name, "max-age")) {
              // ignoré..
            } else if (strfield2(name, "expires")) {
              // ignoré..
            } else if (strfield2(name, "version")) {
              // ignoré..
            } else if (strfield2(name, "comment")) {
              // ignoré
            } else if (strfield2(name, "secure")) {     // ne devrait pas arriver ici
              // ignoré
            } else {
              if (value_len < sizeof(cook_value) - 1
                  && name_len < sizeof(cook_name) - 1) {
                if (strnotempty(cook_name) == 0) {      // noter premier: nom et valeur cookie
                  strcpybuff(cook_name, name);
                  strcpybuff(cook_value, value);
                } else {        // prochain cookie
                  a = start_loop;       // on devra recommencer à cette position
                  next = 1;     // enregistrer
                }
              } else {
                cook_name[0] = 0;
                break;
              }
            }
          }
        }
        if (!next) {
          while((*a != ';') && (*a))
            a++;                // prochain
          while(*a == ';')
            a++;                // sauter ;
        }
      } while((*a) && (!next));
      if (strnotempty(cook_name)) {     // cookie?
#if DEBUG_COOK
        printf
          ("new cookie: name=\"%s\" value=\"%s\" domain=\"%s\" path=\"%s\"\n",
           cook_name, cook_value, domain, path);
#endif
        cookie_add(cookie, cook_name, cook_value, domain, path);
      }
    }
  }
}

// transforme le message statuscode en chaîne
HTSEXT_API void infostatuscode(char *msg, int statuscode) {
  switch (statuscode) {
    // Erreurs HTTP, selon RFC
  case 100:
    strcpybuff(msg, "Continue");
    break;
  case 101:
    strcpybuff(msg, "Switching Protocols");
    break;
  case 200:
    strcpybuff(msg, "OK");
    break;
  case 201:
    strcpybuff(msg, "Created");
    break;
  case 202:
    strcpybuff(msg, "Accepted");
    break;
  case 203:
    strcpybuff(msg, "Non-Authoritative Information");
    break;
  case 204:
    strcpybuff(msg, "No Content");
    break;
  case 205:
    strcpybuff(msg, "Reset Content");
    break;
  case 206:
    strcpybuff(msg, "Partial Content");
    break;
  case 300:
    strcpybuff(msg, "Multiple Choices");
    break;
  case 301:
    strcpybuff(msg, "Moved Permanently");
    break;
  case 302:
    strcpybuff(msg, "Moved Temporarily");
    break;
  case 303:
    strcpybuff(msg, "See Other");
    break;
  case 304:
    strcpybuff(msg, "Not Modified");
    break;
  case 305:
    strcpybuff(msg, "Use Proxy");
    break;
  case 306:
    strcpybuff(msg, "Undefined 306 error");
    break;
  case 307:
    strcpybuff(msg, "Temporary Redirect");
    break;
  case 400:
    strcpybuff(msg, "Bad Request");
    break;
  case 401:
    strcpybuff(msg, "Unauthorized");
    break;
  case 402:
    strcpybuff(msg, "Payment Required");
    break;
  case 403:
    strcpybuff(msg, "Forbidden");
    break;
  case 404:
    strcpybuff(msg, "Not Found");
    break;
  case 405:
    strcpybuff(msg, "Method Not Allowed");
    break;
  case 406:
    strcpybuff(msg, "Not Acceptable");
    break;
  case 407:
    strcpybuff(msg, "Proxy Authentication Required");
    break;
  case 408:
    strcpybuff(msg, "Request Time-out");
    break;
  case 409:
    strcpybuff(msg, "Conflict");
    break;
  case 410:
    strcpybuff(msg, "Gone");
    break;
  case 411:
    strcpybuff(msg, "Length Required");
    break;
  case 412:
    strcpybuff(msg, "Precondition Failed");
    break;
  case 413:
    strcpybuff(msg, "Request Entity Too Large");
    break;
  case 414:
    strcpybuff(msg, "Request-URI Too Large");
    break;
  case 415:
    strcpybuff(msg, "Unsupported Media Type");
    break;
  case 416:
    strcpybuff(msg, "Requested Range Not Satisfiable");
    break;
  case 417:
    strcpybuff(msg, "Expectation Failed");
    break;
  case 500:
    strcpybuff(msg, "Internal Server Error");
    break;
  case 501:
    strcpybuff(msg, "Not Implemented");
    break;
  case 502:
    strcpybuff(msg, "Bad Gateway");
    break;
  case 503:
    strcpybuff(msg, "Service Unavailable");
    break;
  case 504:
    strcpybuff(msg, "Gateway Time-out");
    break;
  case 505:
    strcpybuff(msg, "HTTP Version Not Supported");
    break;
    //
  default:
    if (strnotempty(msg) == 0)
      strcpybuff(msg, "Unknown error");
    break;
  }
}

// check if data is available
int check_readinput(htsblk * r) {
  if (r->soc != INVALID_SOCKET) {
    fd_set fds;                 // poll structures
    struct timeval tv;          // structure for select
    const int soc = (int) r->soc;

    assertf(soc == r->soc);
    FD_ZERO(&fds);
    FD_SET(soc, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    select(soc + 1, &fds, NULL, NULL, &tv);
    if (FD_ISSET(soc, &fds))
      return 1;
    else
      return 0;
  } else
    return 0;
}

// check if data is available
int check_readinput_t(T_SOC soc, int timeout) {
  if (soc != INVALID_SOCKET) {
    fd_set fds;                 // poll structures
    struct timeval tv;          // structure for select
    const int isoc = (int) soc;

    assertf(isoc == soc);
    FD_ZERO(&fds);
    FD_SET(isoc, &fds);
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    select(isoc + 1, &fds, NULL, NULL, &tv);
    if (FD_ISSET(isoc, &fds))
      return 1;
    else
      return 0;
  } else
    return 0;
}

// idem, sauf qu'ici on peut choisir la taille max de données à recevoir
// SI bufl==0 alors le buffer est censé être de 8kos, et on recoit par bloc de lignes
// en éliminant les cr (ex: header), arrêt si double-lf
// SI bufl==-1 alors le buffer est censé être de 8kos, et on recoit ligne par ligne
// en éliminant les cr (ex: header), arrêt si double-lf
// Note: les +1 dans les malloc sont dûs à l'octet nul rajouté en fin de fichier
LLint http_xfread1(htsblk * r, int bufl) {
  int nl = -1;

  // EOF
  if (r->totalsize >= 0 && r->size == r->totalsize) {
    return READ_EOF;
  }

  if (bufl > 0) {
    if (!r->is_write) {         // stocker en mémoire
      if (r->totalsize >= 0) {  // totalsize déterminé ET ALLOUE
        if (r->adr == NULL) {
          r->adr = (char *) malloct((size_t) r->totalsize + 1);
          r->size = 0;
        }
        if (r->adr != NULL) {
          // lecture
          const size_t req_size = r->totalsize - r->size;

          nl = req_size > 0 ? hts_read(r, r->adr + ((int) r->size), (int) req_size) : 0;        /* NO 32 bit overlow possible here (no 4GB html!) */
          // nouvelle taille
          if (nl >= 0)
            r->size += nl;

          /*
             if (r->size >= r->totalsize)
             nl = -1;  // break
           */

          r->adr[r->size] = '\0';       // caractère NULL en fin au cas où l'on traite des HTML
        }

      } else {                  // inconnu..
        // réserver de la mémoire?
        if (r->adr == NULL) {
#if HDEBUG
          printf("..alloc xfread\n");
#endif
          r->adr = (char *) malloct(bufl + 1);
          r->size = 0;
        } else {
#if HDEBUG
          printf("..realloc xfread1\n");
#endif
          r->adr = (char *) realloct(r->adr, (int) r->size + bufl + 1);
        }

        if (r->adr != NULL) {
          // lecture
          nl = hts_read(r, r->adr + (int) r->size, bufl);
          if (nl > 0) {
            // resize
            r->adr = (char *) realloct(r->adr, (int) r->size + nl + 1);
            // nouvelle taille
            r->size += nl;
            // octet nul
            if (r->adr)
              r->adr[r->size] = '\0';

          }                     // sinon on a fini
#if HDEBUG
          else if (nl < 0)
            printf("..end read (%d)\n", nl);
#endif
        }
#if HDEBUG
        else
          printf("..-> error\n");
#endif
      }

      // pas de adr=erreur
      if (r->adr == NULL)
        nl = READ_ERROR;

    } else {                    // stocker sur disque
      char *buff;

      buff = (char *) malloct(bufl);
      if (buff != NULL) {
        // lecture
        nl = hts_read(r, buff, bufl);
        // nouvelle taille
        if (nl > 0) {
          r->size += nl;
          if (fwrite(buff, 1, nl, r->out) != nl) {
            r->statuscode = STATUSCODE_INVALID;
            strcpybuff(r->msg, "Write error on disk");
            nl = READ_ERROR;
          }
        }
        //if ((nl < 0) || ((r->totalsize>0) && (r->size >= r->totalsize)))
        //  nl=-1;  // break

        // libérer bloc tempo
        freet(buff);
      } else
        nl = READ_ERROR;

      if ((nl < 0) && (r->out != NULL)) {
        fflush(r->out);
      }

    }                           // stockage disque ou mémoire

  } else if (bufl == -2) {      // force reserve
    if (r->adr == NULL) {
      r->adr = (char *) malloct(8192);
      r->size = 0;
      return 0;
    }
    return -1;
  } else {                      // réception d'un en-tête octet par octet
    int count = 256;
    int tot_nl = 0;
    int lf_detected = 0;
    int at_beginning = 1;

    do {
      nl = READ_INTERNAL_ERROR;
      count--;
      if (r->adr == NULL) {
        r->adr = (char *) malloct(8192);
        r->size = 0;
      }
      if (r->adr != NULL) {
        if (r->size < 8190) {
          // lecture
          nl = hts_read(r, r->adr + r->size, 1);
          if (nl > 0) {
            // exit if:
            // lf detected AND already detected before
            // or
            // lf detected AND first character read
            if (*(r->adr + r->size) == 10) {
              if (lf_detected || (at_beginning) || (bufl < 0))
                count = -1;
              lf_detected = 1;
            }
            if (*(r->adr + r->size) != 13) {    // sauter caractères 13
              if ((*(r->adr + r->size) != 10)
                  && (*(r->adr + r->size) != 13)
                ) {
                // restart for new line
                lf_detected = 0;
              }
              (r->size)++;
              at_beginning = 0;
            }
            *(r->adr + r->size) = '\0'; // terminer par octet nul
          }
        }
      }
      if (nl >= 0) {
        tot_nl += nl;
        if (!check_readinput(r))
          count = -1;
      }
    } while((nl >= 0) && (count > 0));
    if (nl >= 0) {
      nl = tot_nl;
    }
  }
  // EOF
  if (r->totalsize >= 0 && r->size == r->totalsize) {
    return READ_EOF;
  } else {
    return nl;
  }
}

// teste si une URL (validité, header, taille)
// retourne 200 ou le code d'erreur (404=NOT FOUND, etc)
// en cas de moved xx, dans location
// abandonne désormais au bout de 30 secondes (aurevoir les sites
// qui nous font poireauter 5 heures..) -> -2=timeout
htsblk http_test(httrackp * opt, const char *adr, const char *fil, char *loc) {
  T_SOC soc;
  htsblk retour;

  //int rcvsize=-1;
  //char* rcv=NULL;    // adresse de retour
  //int bufl=TAILLE_BUFFER;    // 8Ko de buffer
  TStamp tl;
  int timeout = 30;             // timeout pour un check (arbitraire) // **

  // pour abandonner un site trop lent
  tl = time_local();

  loc[0] = '\0';
  hts_init_htsblk(&retour);
  //memset(&retour, 0, sizeof(htsblk));    // effacer
  retour.location = loc;        // si non nul, contiendra l'adresse véritable en cas de moved xx

  //soc=http_fopen(adr,fil,&retour,NULL);  // ouvrir, + header

  // on ouvre en head, et on traite l'en tête
  soc = http_xfopen(opt, 1, 0, 1, NULL, adr, fil, &retour);     // ouvrir HEAD, + envoi header

  if (soc != INVALID_SOCKET) {
    int e = 0;

    // tant qu'on a des données, et qu'on ne recoit pas deux LF, et que le timeout n'arrie pas
    do {
      if (http_xfread1(&retour, 0) < 0)
        e = 1;
      else {
        if (retour.adr != NULL) {
          if ((retour.adr[retour.size - 1] != 10)
              || (retour.adr[retour.size - 2] != 10))
            e = 1;
        }
      }

      if (!e) {
        if ((time_local() - tl) >= timeout) {
          e = -1;
        }
      }

    } while(!e);

    if (e == 1) {
      if (adr != NULL) {
        int ptr = 0;
        char rcvd[1100];

        // note: en gros recopie du traitement de back_wait()
        //

        // ----------------------------------------
        // traiter en-tête!
        // status-line à récupérer
        ptr += binput(retour.adr + ptr, rcvd, 1024);
        if (strnotempty(rcvd) == 0)
          ptr += binput(retour.adr + ptr, rcvd, 1024);  // "certains serveurs buggés envoient un \n au début" (RFC)

        // traiter status-line
        treatfirstline(&retour, rcvd);

#if HDEBUG
        printf("(Buffer) Status-Code=%d\n", retour.statuscode);
#endif

        // en-tête

        // header // ** !attention! HTTP/0.9 non supporté
        do {
          ptr += binput(retour.adr + ptr, rcvd, 1024);
#if HDEBUG
          printf("(buffer)>%s\n", rcvd);
#endif
          if (strnotempty(rcvd))
            treathead(NULL, NULL, NULL, &retour, rcvd); // traiter

        } while(strnotempty(rcvd));
        // ----------------------------------------                    

        // libérer mémoire
        if (retour.adr != NULL) {
          freet(retour.adr);
          retour.adr = NULL;
        }
      }
    } else {
      retour.statuscode = STATUSCODE_TIMEOUT;
      strcpybuff(retour.msg, "Timeout While Testing");
    }

#if HTS_DEBUG_CLOSESOCK
    DEBUG_W("http_test: deletehttp\n");
#endif
    deletehttp(&retour);
    retour.soc = INVALID_SOCKET;
  }
  return retour;
}

// Crée un lien (http) vers une adresse internet iadr
// retour: structure (adresse, taille, message si erreur (si !adr))
// peut ouvrir avec des connect() non bloquants: waitconnect=0/1
T_SOC newhttp(httrackp * opt, const char *_iadr, htsblk * retour, int port,
              int waitconnect) {
  T_SOC soc;                    // descipteur de la socket

  if (strcmp(_iadr, "file://") != 0) {  /* non fichier */
    SOCaddr server;
    const char *error = "unknown error";

    // tester un éventuel id:pass et virer id:pass@ si détecté
    const char *const iadr = jump_identification_const(_iadr);

    SOCaddr_clear(server);

#if HDEBUG
    printf("gethostbyname\n");
#endif

    // tester un éventuel port
    if (port == -1) {
      const char *a = jump_toport_const(iadr);

#if HTS_USEOPENSSL
      if (retour->ssl)
        port = 443;
      else
        port = 80;              // port par défaut
#else
      port = 80;                // port par défaut
#endif

      if (a != NULL) {
        char BIGSTK iadr2[HTS_URLMAXSIZE * 2];
        int i = -1;

        iadr2[0] = '\0';
        sscanf(a + 1, "%d", &i);
        if (i != -1) {
          port = (unsigned short int) i;
        }

        // adresse véritable (sans :xx)
        strncatbuff(iadr2, iadr, (int) (a - iadr));

        // adresse sans le :xx
        hts_dns_resolve2(opt, iadr2, &server, &error);

      } else {

        // adresse normale (port par défaut par la suite)
        hts_dns_resolve2(opt, iadr, &server, &error);
      }

    } else {                    // port défini
      hts_dns_resolve2(opt, iadr, &server, &error);
    }

    if (!SOCaddr_is_valid(server)) {
#if DEBUG
      printf("erreur gethostbyname\n");
#endif
      if (retour && retour->msg) {
#ifdef _WIN32
        snprintf(retour->msg, sizeof(retour->msg),
                 "Unable to get server's address: %s", error);
#else
        snprintf(retour->msg, sizeof(retour->msg),
                 "Unable to get server's address: %s", error);
#endif
      }
      return INVALID_SOCKET;
    }

    // make a copy for external clients
    SOCaddr_copy_SOCaddr(retour->address, server);
    retour->address_size = SOCaddr_size(retour->address);

    // créer ("attachement") une socket (point d'accès) internet,en flot
#if HDEBUG
    printf("socket\n");
#endif
#if HTS_WIDE_DEBUG
    DEBUG_W("socket\n");
#endif
    soc = (T_SOC) socket(SOCaddr_sinfamily(server), SOCK_STREAM, 0);
    if (retour != NULL) {
      retour->debugid = HTS_STAT.stat_sockid++;
    }
#if HTS_WIDE_DEBUG
    DEBUG_W("socket()=%d\n" _(int) soc);
#endif
    if (soc == INVALID_SOCKET) {
      if (retour && retour->msg) {
#ifdef _WIN32
        int last_errno = WSAGetLastError();

        sprintf(retour->msg, "Unable to create a socket: %s",
                strerror(last_errno));
#else
        int last_errno = errno;

        sprintf(retour->msg, "Unable to create a socket: %s",
                strerror(last_errno));
#endif
      }
      return INVALID_SOCKET;    // erreur création socket impossible
    }
    // bind this address
    if (retour != NULL && strnotempty(retour->req.proxy.bindhost)) {
      const char *error = "unknown error";
      SOCaddr bind_addr;

      if (hts_dns_resolve2(opt, retour->req.proxy.bindhost,
                             &bind_addr, &error) == NULL
          || bind(soc, &SOCaddr_sockaddr(bind_addr), 
                  SOCaddr_size(bind_addr)) != 0) {
        if (retour && retour->msg) {
#ifdef _WIN32
          snprintf(retour->msg, sizeof(retour->msg),
                   "Unable to bind the specificied server address: %s",
                   error);
#else
          snprintf(retour->msg, sizeof(retour->msg),
                   "Unable to bind the specificied server address: %s",
                   error);
#endif
        }
        deletesoc(soc);
        return INVALID_SOCKET;
      }
    }
    // structure: connexion au domaine internet, port 80 (ou autre)
    SOCaddr_initport(server, port);
#if HDEBUG
    printf("==%d\n", soc);
#endif

    // connexion non bloquante?
    if (!waitconnect) {
#ifdef _WIN32
      unsigned long p = 1;      // non bloquant
      if (ioctlsocket(soc, FIONBIO, &p)) {
        const int last_errno = WSAGetLastError();
        snprintf(retour->msg, sizeof(retour->msg),
                 "Non-blocking socket failed: %s", strerror(last_errno));
        deletesoc(soc);
        return INVALID_SOCKET;
      }
#else
      const int flags = fcntl(soc, F_GETFL, 0);
      if (flags == -1 || fcntl(soc, F_SETFL, flags | O_NONBLOCK) == -1) {
        snprintf(retour->msg, sizeof(retour->msg),
                 "Non-blocking socket failed: %s", strerror(errno));
        deletesoc(soc);
        return INVALID_SOCKET;
      }
#endif
    }
    // Connexion au serveur lui même
#if HDEBUG
    printf("connect\n");
#endif
    HTS_STAT.last_connect = mtime_local();

#if HTS_WIDE_DEBUG
    DEBUG_W("connect\n");
#endif
    if (connect(soc, &SOCaddr_sockaddr(server), SOCaddr_size(server)) != 0) {
      // bloquant
      if (waitconnect) {
#if HDEBUG
        printf("unable to connect!\n");
#endif
        if (retour != NULL && retour->msg) {
#ifdef _WIN32
          const int last_errno = WSAGetLastError();

          sprintf(retour->msg, "Unable to connect to the server: %s",
                  strerror(last_errno));
#else
          const int last_errno = errno;

          sprintf(retour->msg, "Unable to connect to the server: %s",
                  strerror(last_errno));
#endif
        }
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

  } else {                      // on doit ouvrir un fichier local!
    // il sera géré de la même manière qu'une socket (c'est idem!)

    soc = LOCAL_SOCKET_ID;      // pseudo-socket locale..
    // soc sera remplacé lors d'un http_fopen() par un handle véritable!

  }                             // teste fichier local ou http

  return soc;
}

// couper http://www.truc.fr/pub/index.html -> www.truc.fr /pub/index.html
// retour=-1 si erreur.
// si file://... alors adresse=file:// (et coupe le ?query dans ce cas)
int ident_url_absolute(const char *url, lien_adrfil *adrfil) {
  int pos = 0;
  int scheme = 0;

  // effacer adrfil->adr et adrfil->fil
  adrfil->adr[0] = adrfil->fil[0] = '\0';

#if HDEBUG
  printf("protocol: %s\n", url);
#endif

  // Scheme?
  {
    const char *a = url;

    while(isalpha((unsigned char) *a))
      a++;
    if (*a == ':')
      scheme = 1;
  }

  // 1. optional scheme ":"
  if ((pos = strfield(url, "file:"))) { // fichier local!! (pour les tests)
    //!!p+=3;
    strcpybuff(adrfil->adr, "file://");
  } else if ((pos = strfield(url, "http:"))) {  // HTTP
    //!!p+=3;
  } else if ((pos = strfield(url, "ftp:"))) {   // FTP
    strcpybuff(adrfil->adr, "ftp://");  // FTP!!
    //!!p+=3;
#if HTS_USEOPENSSL
  } else if ((pos = strfield(url, "https:"))) {     // HTTPS
    strcpybuff(adrfil->adr, "https://");
#endif
  } else if (scheme) {
    return -1;                  // erreur non reconnu
  } else
    pos = 0;

  // 2. optional "//" authority
  if (strncmp(url + pos, "//", 2) == 0)
    pos += 2;

  // (url+pos) now points to the path (not net path)

  //## if (adrfil->adr[0]!=lOCAL_CHAR) {    // adrfil->adresse normale http
  if (!strfield(adrfil->adr, "file:")) {        // PAS adrfil->file://
    const char *p, *q;

    p = url + pos;

    // p pointe sur le début de l'adrfil->adresse, ex: www.truc.fr/sommaire/index.html
    q = strchr(jump_identification_const(p), '/');
    if (q == 0)
      q = strchr(jump_identification_const(p), '?');  // http://www.foo.com?bar=1
    if (q == 0)
      q = p + strlen(p);        // pointe sur \0
    // q pointe sur le chemin, ex: index.html?query=recherche

    // chemin www... trop long!!
    if ((((int) (q - p))) > HTS_URLMAXSIZE) {
      //strcpybuff(retour.msg,"Path too long");
      return -1;                // erreur
    }
    // recopier adrfil->adresse www..
    strncatbuff(adrfil->adr, p, ((int) (q - p)));
    // *( adrfil->adr+( ((int) q) - ((int) p) ) )=0;  // faut arrêter la fumette!
    // recopier chemin /pub/..
    if (q[0] != '/')            // page par défaut (/)
      strcatbuff(adrfil->fil, "/");
    strcatbuff(adrfil->fil, q);
    // SECURITE:
    // simplifier url pour les ../
    fil_simplifie(adrfil->fil);
  } else {                      // localhost adrfil->file://
    const char *p;
    size_t i;
    char *a;

    p = url + pos;
    if (*p == '/' || *p == '\\') {      /* adrfil->file:///.. */
      strcatbuff(adrfil->fil, p);       // fichier local ; adrfil->adr="#"
    } else {
      if (p[1] != ':') {
        strcatbuff(adrfil->fil, "//");  /* adrfil->file://server/foo */
        strcatbuff(adrfil->fil, p);
      } else {
        strcatbuff(adrfil->fil, p);     // adrfil->file://C:\..
      }
    }

    a = strchr(adrfil->fil, '?');
    if (a)
      *a = '\0';                /* couper query (inutile pour adrfil->file:// lors de la requête) */
    // adrfil->filtrer les \\ -> / pour les fichiers DOS
    for(i = 0; adrfil->fil[i] != '\0'; i++)
      if (adrfil->fil[i] == '\\')
        adrfil->fil[i] = '/';
  }

  // no hostname
  if (!strnotempty(adrfil->adr))
    return -1;                  // erreur non reconnu

  // nommer au besoin.. (non utilisé normalement)
  if (!strnotempty(adrfil->fil))
    strcpybuff(adrfil->fil, "default-index.html");

  // case insensitive pour adrfil->adresse
  {
    char *a = jump_identification(adrfil->adr);

    while(*a) {
      if ((*a >= 'A') && (*a <= 'Z'))
        *a += 'a' - 'A';
      a++;
    }
  }

  return 0;
}

/* simplify ../ and ./ */
void fil_simplifie(char *f) {
  char *a, *b;
  char *rollback[128];
  int rollid = 0;
  char lc = '/';
  int query = 0;
  int wasAbsolute = (*f == '/');

  for(a = b = f; *a != '\0';) {
    if (*a == '?')
      query = 1;
    if (query == 0 && lc == '/' && a[0] == '.' && a[1] == '/') {        /* foo/./bar or ./foo  */
      a += 2;
    } else if (query == 0 && lc == '/' && a[0] == '.' && a[1] == '.' && (a[2] == '/' || a[2] == '\0')) {        /* foo/../bar or ../foo or .. */
      if (a[2] == '\0')
        a += 2;
      else
        a += 3;
      if (rollid > 1) {
        rollid--;
        b = rollback[rollid - 1];
      } else {                  /* too many ../ */
        rollid = 0;
        b = f;
        if (wasAbsolute)
          b++;                  /* after the / */
      }
    } else {
      *b++ = lc = *a;
      if (*a == '/') {
        rollback[rollid++] = b;
        if (rollid >= 127) {
          *f = '\0';            /* ERROR */
          break;
        }
      }
      a++;
    }
  }
  *b = '\0';
  if (*f == '\0') {
    if (wasAbsolute) {
      f[0] = '/';
      f[1] = '\0';
    } else {
      f[0] = '.';
      f[1] = '/';
      f[2] = '\0';
    }
  }
}

// fermer liaison fichier ou socket
void deletehttp(htsblk * r) {
#if HTS_DEBUG_CLOSESOCK
  DEBUG_W("deletehttp: (htsblk*) 0x%p\n" _(void *)r);
#endif
#if HTS_USEOPENSSL
  /* Free OpenSSL structures */
  if (r->ssl_con) {
    SSL_shutdown(r->ssl_con);
    SSL_free(r->ssl_con);
    r->ssl_con = NULL;
  }
#endif
  if (r->soc != INVALID_SOCKET) {
    if (r->is_file) {
      if (r->fp)
        fclose(r->fp);
      r->fp = NULL;
    } else {
      if (r->soc != LOCAL_SOCKET_ID)
        deletesoc_r(r);
    }
    r->soc = INVALID_SOCKET;
  }
}

// free the addr buffer
// always returns 1
int deleteaddr(htsblk * r) {
  if (r->adr != NULL) {
    freet(r->adr);
    r->adr = NULL;
  }
  if (r->headers != NULL) {
    freet(r->headers);
    r->headers = NULL;
  }
  return 1;
}

// fermer une socket
void deletesoc(T_SOC soc) {
  if (soc != INVALID_SOCKET && soc != LOCAL_SOCKET_ID) {
#if HTS_WIDE_DEBUG
    DEBUG_W("close %d\n" _(int) soc);
#endif
#ifdef _WIN32
    if (closesocket(soc) != 0) {
      int err = WSAGetLastError();

      fprintf(stderr, "* error closing socket %d: %s\n", soc, strerror(err));
    }
#else
    if (close(soc) != 0) {
      const int err = errno;

      fprintf(stderr, "* error closing socket %d: %s\n", soc, strerror(err));
    }
#endif
#if HTS_WIDE_DEBUG
    DEBUG_W(".. done\n");
#endif
  }
}

/* Will also clean other things */
void deletesoc_r(htsblk * r) {
#if HTS_USEOPENSSL
  if (r->ssl_con) {
    SSL_shutdown(r->ssl_con);
    // SSL_CTX_set_quiet_shutdown(r->ssl_con->ctx, 1);
    SSL_free(r->ssl_con);
    r->ssl_con = NULL;
  }
#endif
  if (r->soc != INVALID_SOCKET) {
    deletesoc(r->soc);
    r->soc = INVALID_SOCKET;
  }
}

// renvoi le nombre de secondes depuis 1970
TStamp time_local(void) {
  return ((TStamp) time(NULL));
}

// number of millisec since 1970
HTSEXT_API TStamp mtime_local(void) {
#ifndef HTS_DO_NOT_USE_FTIME
  struct timeb B;

  ftime(&B);
  return (TStamp) (((TStamp) B.time * (TStamp) 1000)
                   + ((TStamp) B.millitm));
#else
  // not precise..
  return (TStamp) (((TStamp) time_local() * (TStamp) 1000)
                   + ((TStamp) 0));
#endif
}

// convertit un nombre de secondes en temps (chaine)
void sec2str(char *st, TStamp t) {
  int j, h, m, s;

  j = (int) (t / (3600 * 24));
  t -= ((TStamp) j) * (3600 * 24);
  h = (int) (t / (3600));
  t -= ((TStamp) h) * 3600;
  m = (int) (t / 60);
  t -= ((TStamp) m) * 60;
  s = (int) t;

  if (j > 0)
    sprintf(st, "%d days, %d hours %d minutes %d seconds", j, h, m, s);
  else if (h > 0)
    sprintf(st, "%d hours %d minutes %d seconds", h, m, s);
  else if (m > 0)
    sprintf(st, "%d minutes %d seconds", m, s);
  else
    sprintf(st, "%d seconds", s);
}

// idem, plus court (chaine)
HTSEXT_API void qsec2str(char *st, TStamp t) {
  int j, h, m, s;

  j = (int) (t / (3600 * 24));
  t -= ((TStamp) j) * (3600 * 24);
  h = (int) (t / (3600));
  t -= ((TStamp) h) * 3600;
  m = (int) (t / 60);
  t -= ((TStamp) m) * 60;
  s = (int) t;

  if (j > 0)
    sprintf(st, "%dd,%02dh,%02dmin%02ds", j, h, m, s);
  else if (h > 0)
    sprintf(st, "%dh,%02dmin%02ds", h, m, s);
  else if (m > 0)
    sprintf(st, "%dmin%02ds", m, s);
  else
    sprintf(st, "%ds", s);
}

// heure actuelle, GMT, format rfc (taille buffer 256o)
void time_gmt_rfc822(char *s) {
  time_t tt;
  struct tm *A;

  tt = time(NULL);
  A = gmtime(&tt);
  if (A == NULL)
    A = localtime(&tt);
  time_rfc822(s, A);
}

// heure actuelle, format rfc (taille buffer 256o)
void time_local_rfc822(char *s) {
  time_t tt;
  struct tm *A;

  tt = time(NULL);
  A = localtime(&tt);
  time_rfc822_local(s, A);
}

/* convertir une chaine en temps */
struct tm *convert_time_rfc822(struct tm *result, const char *s) {
  char months[] = "jan feb mar apr may jun jul aug sep oct nov dec";
  char str[256];
  char *a;

  /* */
  int result_mm = -1;
  int result_dd = -1;
  int result_n1 = -1;
  int result_n2 = -1;
  int result_n3 = -1;
  int result_n4 = -1;

  /* */

  if ((int) strlen(s) > 200)
    return NULL;
  strcpybuff(str, s);
  hts_lowcase(str);
  /* éliminer :,- */
  while((a = strchr(str, '-')))
    *a = ' ';
  while((a = strchr(str, ':')))
    *a = ' ';
  while((a = strchr(str, ',')))
    *a = ' ';
  /* tokeniser */
  a = str;
  while(*a) {
    char *first, *last;
    char tok[256];

    /* découper mot */
    while(*a == ' ')
      a++;                      /* sauter espaces */
    first = a;
    while((*a) && (*a != ' '))
      a++;
    last = a;
    tok[0] = '\0';
    if (first != last) {
      char *pos;

      strncatbuff(tok, first, (int) (last - first));
      /* analyser */
      if ((pos = strstr(months, tok))) {        /* month always in letters */
        result_mm = ((int) (pos - months)) / 4;
      } else {
        int number;

        if (sscanf(tok, "%d", &number) == 1) {  /* number token */
          if (result_dd < 0)    /* day always first number */
            result_dd = number;
          else if (result_n1 < 0)
            result_n1 = number;
          else if (result_n2 < 0)
            result_n2 = number;
          else if (result_n3 < 0)
            result_n3 = number;
          else if (result_n4 < 0)
            result_n4 = number;
        }                       /* sinon, bruit de fond(+1GMT for exampel) */
      }
    }
  }
  if ((result_n1 >= 0) && (result_mm >= 0) && (result_dd >= 0)
      && (result_n2 >= 0) && (result_n3 >= 0) && (result_n4 >= 0)) {
    if (result_n4 >= 1000) {    /* Sun Nov  6 08:49:37 1994 */
      result->tm_year = result_n4 - 1900;
      result->tm_hour = result_n1;
      result->tm_min = result_n2;
      result->tm_sec = max(result_n3, 0);
    } else {                    /* Sun, 06 Nov 1994 08:49:37 GMT or Sunday, 06-Nov-94 08:49:37 GMT */
      result->tm_hour = result_n2;
      result->tm_min = result_n3;
      result->tm_sec = max(result_n4, 0);
      if (result_n1 <= 50)      /* 00 means 2000 */
        result->tm_year = result_n1 + 100;
      else if (result_n1 < 1000)        /* 99 means 1999 */
        result->tm_year = result_n1;
      else                      /* 2000 */
        result->tm_year = result_n1 - 1900;
    }
    result->tm_isdst = 0;       /* assume GMT */
    result->tm_yday = -1;       /* don't know */
    result->tm_wday = -1;       /* don't know */
    result->tm_mon = result_mm;
    result->tm_mday = result_dd;
    return result;
  }
  return NULL;
}

static time_t getGMT(struct tm *tm) {   /* hey, time_t is local! */
  time_t t = mktime(tm);

  if (t != (time_t) - 1 && t != (time_t) 0) {
    /* BSD does not have static "timezone" declared */
#if (defined(BSD) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD_kernel__))
    time_t now = time(NULL);
    time_t timezone = -localtime(&now)->tm_gmtoff;
#endif
    return (time_t) (t - timezone);
  }
  return (time_t) - 1;
}

/* sets file time. -1 if error */
/* Note: utf-8 */
int set_filetime(const char *file, struct tm *tm_time) {
  time_t t = getGMT(tm_time);

  if (t != (time_t) - 1) {
    STRUCT_UTIMBUF tim;

    memset(&tim, 0, sizeof(tim));
    tim.actime = tim.modtime = t;
    return UTIME(file, &tim);
  }
  return -1;
}

/* sets file time from RFC822 date+time, -1 if error*/
/* Note: utf-8 */
int set_filetime_rfc822(const char *file, const char *date) {
  struct tm buffer;
  struct tm *tm_s = convert_time_rfc822(&buffer, date);

  if (tm_s) {
    return set_filetime(file, tm_s);
  } else
    return -1;
}

/* Note: utf-8 */
int get_filetime_rfc822(const char *file, char *date) {
  STRUCT_STAT buf;

  date[0] = '\0';
  if (STAT(file, &buf) == 0) {
    struct tm *A;
    time_t tt = buf.st_mtime;

    A = gmtime(&tt);
    if (A == NULL)
      A = localtime(&tt);
    if (A != NULL) {
      time_rfc822(date, A);
      return 1;
    }
  }
  return 0;
}

// heure au format rfc (taille buffer 256o)
void time_rfc822(char *s, struct tm *A) {
  if (A == NULL) {
    int localtime_returned_null = 0;

    assertf(localtime_returned_null);
  }
  strftime(s, 256, "%a, %d %b %Y %H:%M:%S GMT", A);
}

// heure locale au format rfc (taille buffer 256o)
void time_rfc822_local(char *s, struct tm *A) {
  if (A == NULL) {
    int localtime_returned_null = 0;

    assertf(localtime_returned_null);
  }
  strftime(s, 256, "%a, %d %b %Y %H:%M:%S", A);
}

// conversion en b,Kb,Mb
HTSEXT_API char *int2bytes(strc_int2bytes2 * strc, LLint n) {
  char **a = int2bytes2(strc, n);

  strcpybuff(strc->catbuff, a[0]);
  strcatbuff(strc->catbuff, a[1]);
  return strc->catbuff;
}

// conversion en b/s,Kb/s,Mb/s
HTSEXT_API char *int2bytessec(strc_int2bytes2 * strc, long int n) {
  char buff[256];
  char **a = int2bytes2(strc, n);

  strcpybuff(buff, a[0]);
  strcatbuff(buff, a[1]);
  return concat(strc->catbuff, sizeof(strc->catbuff), buff, "/s");
}
HTSEXT_API char *int2char(strc_int2bytes2 * strc, int n) {
  sprintf(strc->buff2, "%d", n);
  return strc->buff2;
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
HTSEXT_API char **int2bytes2(strc_int2bytes2 * strc, LLint n) {
  if (n < ToLLintKiB) {
    sprintf(strc->buff1, "%d", (int) (LLint) n);
    strcpybuff(strc->buff2, "B");
  } else if (n < ToLLintMiB) {
    sprintf(strc->buff1, "%d,%02d", (int) ((LLint) (n / ToLLintKiB)),
            (int) ((LLint) ((n % ToLLintKiB) * 100) / ToLLintKiB));
    strcpybuff(strc->buff2, "KiB");
  }
#ifdef HTS_LONGLONG
  else if (n < ToLLintGiB) {
    sprintf(strc->buff1, "%d,%02d", (int) ((LLint) (n / (ToLLintMiB))),
            (int) ((LLint) (((n % (ToLLintMiB)) * 100) / (ToLLintMiB))));
    strcpybuff(strc->buff2, "MiB");
  } else if (n < ToLLintTiB) {
    sprintf(strc->buff1, "%d,%02d", (int) ((LLint) (n / (ToLLintGiB))),
            (int) ((LLint) (((n % (ToLLintGiB)) * 100) / (ToLLintGiB))));
    strcpybuff(strc->buff2, "GiB");
  } else if (n < ToLLintPiB) {
    sprintf(strc->buff1, "%d,%02d", (int) ((LLint) (n / (ToLLintTiB))),
            (int) ((LLint) (((n % (ToLLintTiB)) * 100) / (ToLLintTiB))));
    strcpybuff(strc->buff2, "TiB");
  } else {
    sprintf(strc->buff1, "%d,%02d", (int) ((LLint) (n / (ToLLintPiB))),
            (int) ((LLint) (((n % (ToLLintPiB)) * 100) / (ToLLintPiB))));
    strcpybuff(strc->buff2, "PiB");
  }
#else
  else {
    sprintf(strc->buff1, "%d,%02d", (int) ((LLint) (n / (ToLLintMiB))),
            (int) ((LLint) (((n % (ToLLintMiB)) * 100) / (ToLLintMiB))));
    strcpybuff(strc->buff2, "MiB");
  }
#endif
  strc->buffadr[0] = strc->buff1;
  strc->buffadr[1] = strc->buff2;
  return strc->buffadr;
}

#ifdef _WIN32
#else
// ignore sigpipe?
int sig_ignore_flag(int setflag) {      // flag ignore
  static int flag = 0;          /* YES, this one is true static */

  if (setflag >= 0)
    flag = setflag;
  return flag;
}
#endif

// envoi de texte (en têtes généralement) sur la socket soc
int sendc(htsblk * r, const char *s) {
  int n, ssz = (int) strlen(s);

#ifdef _WIN32
#else
  sig_ignore_flag(1);
#endif
#if HDEBUG
  write(0, s, ssz);
#endif

#if HTS_USEOPENSSL
  if (r->ssl) {
    n = SSL_write(r->ssl_con, s, ssz);
  } else
#endif
    n = send(r->soc, s, ssz, 0);

#ifdef _WIN32
#else
  sig_ignore_flag(0);
#endif

  return (n == ssz) ? n : -1;
}

// Remplace read
int finput(T_SOC fd, char *s, int max) {
  char c;
  int j = 0;

  do {
    //c=fgetc(fp);
    if (read((int) fd, &c, 1) <= 0) {
      c = 0;
    }
    if (c != 0) {
      switch (c) {
      case 10:
        c = 0;
        break;
      case 13:
        break;                  // sauter ces caractères
      default:
        s[j++] = c;
        break;
      }
    }
  } while((c != 0) && (j < max - 1));
  s[j] = '\0';
  return j;
}

// Like linput, but in memory (optimized)
int binput(char *buff, char *s, int max) {
  int count = 0;
  int destCount = 0;

  // Note: \0 will return 1
  while(destCount < max && buff != NULL && buff[count] != '\0'
        && buff[count] != '\n') {
    if (buff[count] != '\r') {
      s[destCount++] = buff[count];
    }
    count++;
  }
  s[destCount] = '\0';

  // then return the supplemental jump offset
  return count + 1;
}

// Lecture d'une ligne (peut être unicode à priori)
int linput(FILE * fp, char *s, int max) {
  int c;
  int j = 0;

  do {
    c = fgetc(fp);
    if (c != EOF) {
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
    }
  } while((c != -1) && (c != EOF) && (j < (max - 1)));
  s[j] = '\0';
  return j;
}
int linputsoc(T_SOC soc, char *s, int max) {
  int c;
  int j = 0;

  do {
    unsigned char ch;

    if (recv(soc, &ch, 1, 0) == 1) {
      c = ch;
    } else {
      c = EOF;
    }
    if (c != EOF) {
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
    }
  } while((c != -1) && (c != EOF) && (j < (max - 1)));
  s[j] = '\0';
  return j;
}
int linputsoc_t(T_SOC soc, char *s, int max, int timeout) {
  if (check_readinput_t(soc, timeout)) {
    return linputsoc(soc, s, max);
  }
  return -1;
}
int linput_trim(FILE * fp, char *s, int max) {
  int rlen = 0;
  char *ls = (char *) malloct(max + 1);

  s[0] = '\0';
  if (ls) {
    char *a;

    // lire ligne
    rlen = linput(fp, ls, max);
    if (rlen) {
      // sauter espaces et tabs en fin
      while((rlen > 0)
            && ((ls[max(rlen - 1, 0)] == ' ')
                || (ls[max(rlen - 1, 0)] == '\t')))
        ls[--rlen] = '\0';
      // sauter espaces en début
      a = ls;
      while((rlen > 0) && ((*a == ' ') || (*a == '\t'))) {
        a++;
        rlen--;
      }
      if (rlen > 0) {
        memcpy(s, a, rlen);     // can copy \0 chars
        s[rlen] = '\0';
      }
    }
    //
    freet(ls);
  }
  return rlen;
}
int linput_cpp(FILE * fp, char *s, int max) {
  int rlen = 0;

  s[0] = '\0';
  do {
    int ret;

    if (rlen > 0)
      if (s[rlen - 1] == '\\')
        s[--rlen] = '\0';       // couper \ final
    // lire ligne
    ret = linput_trim(fp, s + rlen, max - rlen);
    if (ret > 0)
      rlen += ret;
  } while((s[max(rlen - 1, 0)] == '\\') && (rlen < max));
  return rlen;
}

// idem avec les car spéciaux
void rawlinput(FILE * fp, char *s, int max) {
  int c;
  int j = 0;

  do {
    c = fgetc(fp);
    if (c != EOF) {
      switch (c) {
      case 13:
        break;                  // sauter CR
      case 10:
        c = -1;
        break;
      default:
        s[j++] = (char) c;
        break;
      }
    }
  } while((c != -1) && (c != EOF) && (j < (max - 1)));
  s[j++] = '\0';
}

//cherche chaine, case insensitive
const char *strstrcase(const char *s, const char *o) {
  while(*s && strfield(s, o) == 0)
    s++;
  if (*s == '\0')
    return NULL;
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
int is_unicode_utf8(const char *buffer_, const size_t size) {
  const unsigned char *buffer = (const unsigned char *) buffer_;
  t_auto_seq seq;
  size_t i;
  int is_utf = -1;

  RUNTIME_TIME_CHECK_SIZE(size);

  seq.pos = 0;
  for(i = 0; i < size; i++) {
    unsigned int ok = 0;
    unsigned int inseq = 0;
    unsigned int err = 0;

    seq.data[seq.pos] = buffer[i];
     /**/ if (SEQBEG && BLK(0, 00, 7F) && IN_SEQ && SEQEND) {
    } else if (SEQBEG && BLK(0, C2, DF) && IN_SEQ && BLK(1, 80, BF) && SEQEND) {
    } else if (SEQBEG && ELT(0, E0) && IN_SEQ && BLK(1, A0, BF)
               && BLK(2, 80, BF) && SEQEND) {
    } else if (SEQBEG && BLK(0, E1, EC) && IN_SEQ && BLK(1, 80, BF)
               && BLK(2, 80, BF) && SEQEND) {
    } else if (SEQBEG && ELT(0, ED) && IN_SEQ && BLK(1, 80, 9F)
               && BLK(2, 80, BF) && SEQEND) {
    } else if (SEQBEG && BLK(0, EE, EF) && IN_SEQ && BLK(1, 80, BF)
               && BLK(2, 80, BF) && SEQEND) {
    } else if (SEQBEG && ELT(0, F0) && IN_SEQ && BLK(1, 90, BF)
               && BLK(2, 80, BF) && BLK(3, 80, BF) && SEQEND) {
    } else if (SEQBEG && BLK(0, F1, F3) && IN_SEQ && BLK(1, 80, BF)
               && BLK(2, 80, BF) && BLK(3, 80, BF) && SEQEND) {
    } else if (SEQBEG && ELT(0, F4) && IN_SEQ && BLK(1, 80, 8F)
               && BLK(2, 80, BF) && BLK(3, 80, BF) && SEQEND) {
    } else if (NO_SEQ) {        // bad, unknown
      return 0;
    }
    /* */

    /* Error */
    if (BAD_SEQ) {
      return 0;
    }

    /* unicode character */
    if (seq.pos > 0)
      is_utf = 1;

    /* Next */
    if (ok)
      seq.pos = 0;
    else
      seq.pos++;

    /* Internal error */
    if (seq.pos >= 4)
      return 0;

  }

  return is_utf;
}

void map_characters(unsigned char *buffer, unsigned int size, unsigned int *map) {
  unsigned int i;

  memset(map, 0, sizeof(unsigned int) * 256);
  for(i = 0; i < size; i++) {
    map[buffer[i]]++;
  }
}

// le fichier est-il un fichier html?
//  0 : non
//  1 : oui
// -1 : on sait pas
// -2 : on sait pas, pas d'extension
int ishtml(httrackp * opt, const char *fil) {
  /* User-defined MIME types (overrides ishtml()) */
  char BIGSTK fil_noquery[HTS_URLMAXSIZE * 2];
  char mime[256];
  char *a;

  strcpybuff(fil_noquery, fil);
  if ((a = strchr(fil_noquery, '?')) != NULL) {
    *a = '\0';
  }
  if (get_userhttptype(opt, mime, fil_noquery)) {
    if (is_html_mime_type(mime)) {
      return 1;
    } else {
      return 0;
    }
  }

  if (!strnotempty(fil_noquery)) {
    return -2;
  }

  /* Search for known ext */
  for(a = fil_noquery + strlen(fil_noquery) - 1;
      *a != '.' && *a != '/' && a > fil_noquery; a--) ;
  if (*a == '.') {              // a une extension
    char BIGSTK fil_noquery[HTS_URLMAXSIZE * 2];
    char *b;
    int ret;
    char *dotted = a;

    fil_noquery[0] = '\0';
    a++;                        // pointer sur extension
    strncatbuff(fil_noquery, a, HTS_URLMAXSIZE);
    b = strchr(fil_noquery, '?');
    if (b)
      *b = '\0';
    ret = ishtml_ext(fil_noquery);      // retour
    if (ret == -1) {
      switch (is_knowntype(opt, dotted)) {
      case 1:
        ret = 0;                // connu, non html
        break;
      case 2:
        ret = 1;                // connu, html
        break;
      default:
        ret = -1;               // inconnu..
        break;
      }
    }
    return ret;
  } else
    return -2;                  // indéterminé, par exemple /truc
}

// idem, mais pour uniquement l'extension
int ishtml_ext(const char *a) {
  int html = 0;

  //
  if (strfield2(a, "html"))
    html = 1;
  else if (strfield2(a, "htm"))
    html = 1;
  else if (strfield2(a, "shtml"))
    html = 1;
  else if (strfield2(a, "phtml"))
    html = 1;
  else if (strfield2(a, "htmlx"))
    html = 1;
  else if (strfield2(a, "shtm"))
    html = 1;
  else if (strfield2(a, "phtm"))
    html = 1;
  else if (strfield2(a, "htmx"))
    html = 1;
  //
  // insuccès..
  else {
#if 1
    html = -1;                  // inconnu..
#else
    // XXXXXX not suitable (ext)
    switch (is_knownext(a)) {
    case 1:
      html = 0;                 // connu, non html
      break;
    case 2:
      html = 1;                 // connu, html
      break;
    default:
      html = -1;                // inconnu..
      break;
    }
#endif
  }
  return html;
}

// error (404,500..)
int ishttperror(int err) {
  switch (err / 100) {
  case 4:
  case 5:
    return 1;
    break;
  }
  return 0;
}

/* Declare a non-const version of FUN */
#define DECLARE_NON_CONST_VERSION(FUN) \
char *FUN(char *source) { \
  const char *const ret = FUN ##_const(source); \
  return ret != NULL ? source + ( ret - source ) : NULL; \
}

// retourne le pointeur ou le pointeur + offset si il existe dans la chaine un @ signifiant 
// une identification
HTSEXT_API const char *jump_identification_const(const char *source) {
  const char *a, *trytofind;

  if (strcmp(source, "file://") == 0)
    return source;
  // rechercher dernier @ (car parfois email transmise dans adresse!)
  // mais sauter ftp:// éventuel
  a = jump_protocol_const(source);
  trytofind = strrchr_limit(a, '@', strchr(a, '/'));
  return trytofind != NULL ? trytofind : a;
}

HTSEXT_API DECLARE_NON_CONST_VERSION(jump_identification)

HTSEXT_API const char *jump_normalized_const(const char *source) {
  if (strcmp(source, "file://") == 0)
    return source;
  source = jump_identification_const(source);
  if (strfield(source, "www") && source[3] != '\0') {
    if (source[3] == '.') {     // www.foo.com -> foo.com
      source += 4;
    } else {                    // www-4.foo.com -> foo.com
      const char *a = source + 3;

      while(*a && (isdigit(*a) || *a == '-'))
        a++;
      if (*a == '.') {
        source = a + 1;
      }
    }
  }
  return source;
}

HTSEXT_API DECLARE_NON_CONST_VERSION(jump_normalized)

static int sortNormFnc(const void *a_, const void *b_) {
  const char *const*const a = (const char *const*) a_;
  const char *const*const b = (const char *const*) b_;

  return strcmp(*a + 1, *b + 1);
}

HTSEXT_API char *fil_normalized(const char *source, char *dest) {
  char lastc = 0;
  int gotquery = 0;
  int ampargs = 0;
  size_t i, j;
  char *query = NULL;

  for(i = j = 0; source[i] != '\0'; i++) {
    if (!gotquery && source[i] == '?')
      gotquery = ampargs = 1;
    if ((!gotquery && lastc == '/' && source[i] == '/') // foo//bar -> foo/bar
      ) {
    } else {
      if (gotquery && source[i] == '&') {
        ampargs++;
      }
      dest[j++] = source[i];
    }
    lastc = source[i];
  }
  dest[j++] = '\0';

  /* Sort arguments (&foo=1&bar=2 == &bar=2&foo=1) */
  if (ampargs > 1) {
    char **amps = malloct(ampargs * sizeof(char *));
    char *copyBuff = NULL;
    size_t qLen = 0;

    assertf(amps != NULL);
    gotquery = 0;
    for(i = j = 0; dest[i] != '\0'; i++) {
      if ((gotquery && dest[i] == '&') || (!gotquery && dest[i] == '?')) {
        if (!gotquery) {
          gotquery = 1;
          query = &dest[i];
          qLen = strlen(query);
        }
        assertf(j < ampargs);
        amps[j++] = &dest[i];
        dest[i] = '\0';
      }
    }
    assertf(gotquery);
    assertf(j == ampargs);

    /* Sort 'em all */
    qsort(amps, ampargs, sizeof(char *), sortNormFnc);

    /* Replace query by sorted query */
    copyBuff = malloct(qLen + 1);
    assertf(copyBuff != NULL);
    copyBuff[0] = '\0';
    for(i = 0; i < ampargs; i++) {
      if (i == 0)
        strcatbuff(copyBuff, "?");
      else
        strcatbuff(copyBuff, "&");
      strcatbuff(copyBuff, amps[i] + 1);
    }
    assertf(strlen(copyBuff) == qLen);
    strcpybuff(query, copyBuff);

    /* Cleanup */
    freet(amps);
    freet(copyBuff);
  }

  return dest;
}

#define endwith(a) ( (len >= (sizeof(a)-1)) ? ( strncmp(dest, a+len-(sizeof(a)-1), sizeof(a)-1) == 0 ) : 0 );
HTSEXT_API char *adr_normalized(const char *source, char *dest) {
  /* not yet too aggressive (no com<->net<->org checkings) */
  strcpybuff(dest, jump_normalized_const(source));
  return dest;
}

#undef endwith

// find port (:80) or NULL if not found
// can handle IPV6 addresses
HTSEXT_API const char *jump_toport_const(const char *source) {
  const char *a, *trytofind;

  a = jump_identification_const(source);
  trytofind = strrchr_limit(a, ']', strchr(source, '/'));       // find last ] (http://[3ffe:b80:1234::1]:80/foo.html)
  a = strchr((trytofind) ? trytofind : a, ':');
  return a;
}

HTSEXT_API DECLARE_NON_CONST_VERSION(jump_toport)

// strrchr, but not too far
const char *strrchr_limit(const char *s, char c, const char *limit) {
  if (limit == NULL) {
    const char *p = strrchr(s, c);

    return p ? (p + 1) : NULL;
  } else {
    const char *a = NULL, *p;

    for(;;) {
      p = strchr((a) ? a : s, c);
      if ((p >= limit) || (p == NULL))
        return a;
      a = p + 1;
    }
  }
}

// retourner adr sans ftp://
const char *jump_protocol_const(const char *source) {
  int p;

  // scheme
  // "Comparisons of scheme names MUST be case-insensitive" (RFC2616)
  if ((p = strfield(source, "http:")))
    source += p;
  else if ((p = strfield(source, "ftp:")))
    source += p;
  else if ((p = strfield(source, "https:")))
    source += p;
  else if ((p = strfield(source, "file:")))
    source += p;
  // net_path
  if (strncmp(source, "//", 2) == 0)
    source += 2;
  return source;
}

DECLARE_NON_CONST_VERSION(jump_protocol)

// codage base 64 a vers b
void code64(unsigned char *a, int size_a, unsigned char *b, int crlf) {
  int i1 = 0, i2 = 0, i3 = 0, i4 = 0;
  int loop = 0;
  unsigned long int store;
  int n;
  const char _hts_base64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  while(size_a-- > 0) {
    // 24 bits
    n = 1;
    store = *a++;
    if (size_a-- > 0) {
      n = 2;
      store <<= 8;
      store |= *a++;
    }
    if (size_a-- > 0) {
      n = 3;
      store <<= 8;
      store |= *a++;
    }
    if (n == 3) {
      i4 = store & 63;
      i3 = (store >> 6) & 63;
      i2 = (store >> 12) & 63;
      i1 = (store >> 18) & 63;
    } else if (n == 2) {
      store <<= 2;
      i3 = store & 63;
      i2 = (store >> 6) & 63;
      i1 = (store >> 12) & 63;
    } else {
      store <<= 4;
      i2 = store & 63;
      i1 = (store >> 6) & 63;
    }

    *b++ = _hts_base64[i1];
    *b++ = _hts_base64[i2];
    if (n >= 2)
      *b++ = _hts_base64[i3];
    else
      *b++ = '=';
    if (n >= 3)
      *b++ = _hts_base64[i4];
    else
      *b++ = '=';

    if (crlf && ((loop += 3) % 60) == 0) {
      *b++ = '\r';
      *b++ = '\n';
    }
  }
  *b++ = '\0';
}

// return the hex character value, or -1 on error.
static HTS_INLINE int ehexh(const char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  else if (c >= 'a' && c <= 'f')
    return (c - 'a' + 10);
  else if (c >= 'A' && c <= 'F')
    return (c - 'A' + 10);
  else
    return -1;
}

// return the two-hex character value, or -1 on error.
static HTS_INLINE int ehex(const char *s) {
  const int c1 = ehexh(s[0]);
  if (c1 >= 0) {
    const int c2 = ehexh(s[1]);
    if (c2 >= 0) {
      return 16*c1 + c2;
    }
  }
  return -1;
}

void unescape_amp(char *s) {
  if (hts_unescapeEntities(s, s, strlen(s) + 1) != 0) {
    assertf(! "error escaping html entities");
  }
}

// remplacer %20 par ' ', etc..
// buffer MAX 1Ko
HTSEXT_API char *unescape_http(char *const catbuff, const size_t size, const char *const s) {
  size_t i, j;

  RUNTIME_TIME_CHECK_SIZE(size);

  for(i = 0, j = 0; s[i] != '\0' && j + 1 < size ; i++) {
    int h;
    if (s[i] == '%' && (h = ehex(&s[i + 1])) >= 0) {
      catbuff[j++] = (char) h;
      i += 2;
    }
    else
      catbuff[j++] = s[i];
  }
  catbuff[j++] = '\0';
  return catbuff;
}

// unescape in URL/URI ONLY what has to be escaped, to form a standard URL/URI
// DOES NOT DECODE %25 (part of CHAR_DELIM)
// no_high & 1: decode high chars
// no_high & 2: decode space
HTSEXT_API char *unescape_http_unharm(char *const catbuff, const size_t size, 
                                      const char *s, const int no_high) {
  size_t i, j;

  RUNTIME_TIME_CHECK_SIZE(size);

  for(i = 0, j = 0; s[i] != '\0' && j + 1 < size ; i++) {
    if (s[i] == '%') {
      const int nchar = ehex(&s[i + 1]);

      const int test = 
        ( CHAR_RESERVED(nchar) && nchar != '+' )        /* %2B => + (not in query!) */
        || CHAR_DELIM(nchar)
        || CHAR_UNWISE(nchar)
        || CHAR_LOW(nchar)    /* CHAR_SPECIAL */
        || ( CHAR_XXAVOID(nchar) && ( nchar != ' ' || ( no_high & 2) == 0 ) )
        || ( ( no_high & 1 ) && CHAR_HIG(nchar) )
        ;

      if (!test && nchar >= 0) {  /* can safely unescape */
        catbuff[j++] = (char) nchar;
        i += 2;
      } else {
        catbuff[j++] = '%';
      }
    } else {
      catbuff[j++] = s[i];
    }
  }
  catbuff[j++] = '\0';
  return catbuff;
}

// remplacer " par %xx etc..
// buffer MAX 1Ko
HTSEXT_API size_t escape_spc_url(const char *const src, 
                                 char *const dest, const size_t size) {
  return x_escape_http(src, dest, size, 2);
}

// smith / john -> smith%20%2f%20john
HTSEXT_API size_t escape_in_url(const char *const src, 
                                char *const dest, const size_t size) {
  return x_escape_http(src, dest, size, 1);
}

// smith / john -> smith%20/%20john
HTSEXT_API size_t escape_uri(const char *const src, 
                             char *const dest, const size_t size) {
  return x_escape_http(src, dest, size, 3);
}

HTSEXT_API size_t escape_uri_utf(const char *const src, 
                                 char *const dest, const size_t size) {
  return x_escape_http(src, dest, size, 30);
}

HTSEXT_API size_t escape_check_url(const char *const src, 
                                   char *const dest, const size_t size) {
  return x_escape_http(src, dest, size, 0);
}

// same as escape_check_url, but returns char*
HTSEXT_API char *escape_check_url_addr(const char *const src, 
                                       char *const dest, const size_t size) {
  escape_check_url(src, dest, size);
  return dest;
}

// Same as above, but appending to "dest"
#undef DECLARE_APPEND_ESCAPE_VERSION
#define DECLARE_APPEND_ESCAPE_VERSION(NAME) \
HTSEXT_API size_t append_ ##NAME(const char *const src, char *const dest, const size_t size) { \
  const size_t len = strnlen(dest, size); \
  assertf(len < size); \
  return NAME(src, dest + len, size - len); \
}

DECLARE_APPEND_ESCAPE_VERSION(escape_in_url)
DECLARE_APPEND_ESCAPE_VERSION(escape_spc_url)
DECLARE_APPEND_ESCAPE_VERSION(escape_uri_utf)
DECLARE_APPEND_ESCAPE_VERSION(escape_check_url)
DECLARE_APPEND_ESCAPE_VERSION(escape_uri)

#undef DECLARE_APPEND_ESCAPE_VERSION

// Same as above, but in-place
#undef DECLARE_INPLACE_ESCAPE_VERSION
#define DECLARE_INPLACE_ESCAPE_VERSION(NAME) \
HTSEXT_API size_t inplace_ ##NAME(char *const dest, const size_t size) { \
  char buffer[256]; \
  const size_t len = strnlen(dest, size); \
  const int in_buffer = len + 1 < sizeof(buffer); \
  char *src = in_buffer ? buffer : malloct(len + 1); \
  size_t ret; \
  assertf(src != NULL); \
  assertf(len < size); \
  memcpy(src, dest, len + 1); \
  ret = NAME(src, dest, size); \
  if (!in_buffer) { \
    freet(src); \
  } \
  return ret; \
}

DECLARE_INPLACE_ESCAPE_VERSION(escape_in_url)
DECLARE_INPLACE_ESCAPE_VERSION(escape_spc_url)
DECLARE_INPLACE_ESCAPE_VERSION(escape_uri_utf)
DECLARE_INPLACE_ESCAPE_VERSION(escape_check_url)
DECLARE_INPLACE_ESCAPE_VERSION(escape_uri)

#undef DECLARE_INPLACE_ESCAPE_VERSION


HTSEXT_API size_t make_content_id(const char *const adr, const char *const fil, 
                                  char *const dest, const size_t size) {
  char *a;
  size_t esc_size = escape_in_url(adr, dest, size);
  esc_size += escape_in_url(fil, dest + esc_size, size - esc_size);
  RUNTIME_TIME_CHECK_SIZE(size);
  for(a = dest ; (a = strchr(a, '%')) != NULL ; a++) {
    *a = 'X';
  }
  return esc_size;
}

// strip all control characters
HTSEXT_API void escape_remove_control(char *const s) {
  size_t i, j;
  for(i = 0, j = 0 ; s[i] != '\0' ; i++) {
    const unsigned char c = (unsigned  char) s[i];
    if (c >= 32) {
      if (i != j) {
        assertf(j < i);
        s[j] = s[i];
      }
      j++;
    }
  }
}

#undef ADD_CHAR
#define ADD_CHAR(C) do {   \
      assertf(j < size);    \
      if (j + 1 == size) { \
        dest[j] = '\0';    \
        return size;       \
      }                    \
      dest[j++] = (C);     \
  } while(0)

/* Returns the number of characters written (not taking in account the terminating \0), or 'size' upon overflow. */
HTSEXT_API size_t x_escape_http(const char *const s, char *const dest, 
                                const size_t size, const int mode) {
  static const char hex[] = "0123456789abcdef";
  size_t i, j;

  RUNTIME_TIME_CHECK_SIZE(size);

  // Out-of-bound.
  // Previous character is supposed to be the terminating \0.
  if (size == 0) {
    return 0;
  }

  for(i = 0, j = 0 ; s[i] != '\0' ; i++) {
    const unsigned char c = (unsigned char) s[i];
    int test = 0;

    if (mode == 0)
      test = c == '"' || c == ' ' || CHAR_SPECIAL(c);
    else if (mode == 1)
      test = CHAR_RESERVED(c)
             || CHAR_DELIM(c)
             || CHAR_UNWISE(c)
             || CHAR_SPECIAL(c)
             || CHAR_XXAVOID(c)
             || CHAR_MARK(c);
    else if (mode == 2)
      test = c == ' ';       // n'escaper que espace
    else if (mode == 3)         // échapper que ce qui est nécessaire
      test = CHAR_SPECIAL(c)
             || CHAR_XXAVOID(c);
    else if (mode == 30)      // échapper que ce qui est nécessaire
      test = (c != '/' && CHAR_RESERVED(c))
        || CHAR_DELIM(c)
        || CHAR_UNWISE(c)
        || CHAR_SPECIAL(c)
        || CHAR_XXAVOID(c);

    if (!test) {
      ADD_CHAR(c);
    } else {
      ADD_CHAR('%');
      ADD_CHAR(hex[c / 16]);
      ADD_CHAR(hex[c % 16]);
    }
  }

  assertf(j < size);
  dest[j] = '\0';
  return j;
}

HTSEXT_API size_t escape_for_html_print(const char *const s, char *const dest, const size_t size) {
  size_t i, j;

  RUNTIME_TIME_CHECK_SIZE(size);

  for(i = 0, j = 0 ; s[i] != '\0' ; i++) {
    const unsigned char c = (unsigned char) s[i];
    if (c == '&') {
      ADD_CHAR('&');
      ADD_CHAR('a');
      ADD_CHAR('m');
      ADD_CHAR('p');
      ADD_CHAR(';');
    } else {
      ADD_CHAR(c);
    }
  }
  assertf(j < size);
  dest[j] = '\0';
  return j;
}

HTSEXT_API size_t escape_for_html_print_full(const char *const s, char *const dest, const size_t size) {
  static const char hex[] = "0123456789abcdef";
  size_t i, j;

  RUNTIME_TIME_CHECK_SIZE(size);

  for(i = 0, j = 0 ; s[i] != '\0' ; i++) {
    const unsigned char c = (unsigned char) s[i];
    if (c == '&') {
      ADD_CHAR('&');
      ADD_CHAR('a');
      ADD_CHAR('m');
      ADD_CHAR('p');
      ADD_CHAR(';');
    } else if (CHAR_HIG(c)) {
      ADD_CHAR('&');
      ADD_CHAR('#');
      ADD_CHAR('x');
      ADD_CHAR(hex[c / 16]);
      ADD_CHAR(hex[c % 16]);
      ADD_CHAR(';');
    } else {
      ADD_CHAR(c);
    }
  }
  assertf(j < size);
  dest[j] = '\0';
  return j;
}

#undef ADD_CHAR

// conversion minuscules, avec buffer
char *convtolower(char *catbuff, const char *a) {
  strcpybuff(catbuff, a);
  hts_lowcase(catbuff);         // lower case
  return catbuff;
}

// conversion en minuscules
void hts_lowcase(char *s) {
  size_t i;

  for(i = 0; s[i] != '\0'; i++)
    if ((s[i] >= 'A') && (s[i] <= 'Z'))
      s[i] += ('a' - 'A');
}

// remplacer un caractère d'une chaîne dans une autre
void hts_replace(char *s, char from, char to) {
  char *a;

  while((a = strchr(s, from)) != NULL) {
    *a = to;
  }
}

// deviner type d'un fichier local..
// ex: fil="toto.gif" -> s="image/gif"
void guess_httptype(httrackp * opt, char *s, const char *fil) {
  get_httptype(opt, s, fil, 1);
}

// idem
// flag: 1 si toujours renvoyer un type
HTSEXT_API void get_httptype(httrackp * opt, char *s, const char *fil, int flag) {
  // userdef overrides get_httptype
  if (get_userhttptype(opt, s, fil)) {
    return;
  }
  // regular tests
  if (ishtml(opt, fil) == 1) {
    strcpybuff(s, "text/html");
  } else {
    /* Check html -> text/html */
    const char *a = fil + strlen(fil) - 1;

    while((*a != '.') && (*a != '/') && (a > fil))
      a--;
    if (*a == '.' && strlen(a) < 32) {
      int j = 0;

      a++;
      while(strnotempty(hts_mime[j][1])) {
        if (strfield2(hts_mime[j][1], a)) {
          if (hts_mime[j][0][0] != '*') {       // Une correspondance existe
            strcpybuff(s, hts_mime[j][0]);
            return;
          }
        }
        j++;
      }

      if (flag)
        sprintf(s, "application/%s", a);
    } else {
      if (flag)
        strcpybuff(s, "application/octet-stream");
    }
  }
}

// get type of fil (php)
// s: buffer (text/html) or NULL
// return: 1 if known by user
int get_userhttptype(httrackp * opt, char *s, const char *fil) {
  if (s != NULL) {
    if (s)
      s[0] = '\0';
    if (fil == NULL || *fil == '\0')
      return 0;
#if 1
    if (StringLength(opt->mimedefs) > 0) {

      /* Check --assume foooo/foo/bar.cgi=text/html, then foo/bar.cgi=text/html, then bar.cgi=text/html */
      /* also: --assume baz,bar,foooo/foo/bar.cgi=text/html */
      /* start from path beginning */
      do {
        const char *next;
        const char *mimedefs = StringBuff(opt->mimedefs);       /* loop through mime definitions : \nfoo=bar\nzoo=baz\n.. */

        while(*mimedefs != '\0') {
          const char *segment = fil + 1;

          if (*mimedefs == '\n') {
            mimedefs++;
          }
          /* compare current segment with user's definition */
          do {
            int i;

            /* check current item */
            for(i = 0; mimedefs[i] != '\0'      /* end of all defs */
                && mimedefs[i] != ' '   /* next item in left list */
                && mimedefs[i] != '='   /* end of left list */
                && mimedefs[i] != '\n'  /* end of this def (?) */
                && mimedefs[i] == segment[i]    /* same item */
                ; i++) ;
            /* success */
            if ((mimedefs[i] == '=' || mimedefs[i] == ' ')
                && segment[i] == '\0') {
              int i2;

              while(mimedefs[i] != 0 && mimedefs[i] != '\n'
                    && mimedefs[i] != '=')
                i++;
              if (mimedefs[i] == '=') {
                i++;
                for(i2 = 0;
                    mimedefs[i + i2] != '\n' && mimedefs[i + i2] != '\0';
                    i2++) {
                  s[i2] = mimedefs[i + i2];
                }
                s[i2] = '\0';
                return 1;       /* SUCCESS! */
              }
            }
            /* next item in list */
            for(mimedefs += i;
                *mimedefs != '\0' && *mimedefs != '\n' && *mimedefs != '='
                && *mimedefs != ' '; mimedefs++) ;
            if (*mimedefs == ' ') {
              mimedefs++;
            }
          } while(*mimedefs != '\0' && *mimedefs != '\n' && *mimedefs != '=');
          /* next user-def */
          for(; *mimedefs != '\0' && *mimedefs != '\n'; mimedefs++) ;
        }
        /* shorten segment */
        next = strchr(fil + 1, '/');
        if (next == NULL) {
          /* ext tests */
          next = strchr(fil + 1, '.');
        }
        fil = next;
      } while(fil != NULL);
    }
#else
    if (*buffer) {
      char BIGSTK search[1024];
      char *detect;

      sprintf(search, "\n%s=", ext);    // php=text/html
      detect = strstr(*buffer, search);
      if (!detect) {
        sprintf(search, "\n%s\n", ext); // php\ncgi=text/html
        detect = strstr(*buffer, search);
      }
      if (detect) {
        detect = strchr(detect, '=');
        if (detect) {
          detect++;
          if (s) {
            char *a;

            a = strchr(detect, '\n');
            if (a) {
              strncatbuff(s, detect, (int) (a - detect));
            }
          }
          return 1;
        }
      }
    }
#endif
  }
  return 0;
}

// renvoyer extesion d'un type mime..
// ex: "image/gif" -> gif
void give_mimext(char *s, const char *st) {
  int ok = 0;
  int j = 0;

  s[0] = '\0';
  while((!ok) && (strnotempty(hts_mime[j][1]))) {
    if (strfield2(hts_mime[j][0], st)) {
      if (hts_mime[j][1][0] != '*') {   // Une correspondance existe
        strcpybuff(s, hts_mime[j][1]);
        ok = 1;
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
    const char *a = NULL;

    if ((p = strfield(st, "application/x-")))
      a = st + p;
    else if ((p = strfield(st, "application/")))
      a = st + p;
    if (a) {
      if ((int) strlen(a) >= 1) {
        if ((int) strlen(a) <= 4) {
          strcpybuff(s, a);
          ok = 1;
        }
      }
    }
  }
}

// extension connue?..
//  0 : non
//  1 : oui
//  2 : html
HTSEXT_API int is_knowntype(httrackp * opt, const char *fil) {
  char catbuff[CATBUFF_SIZE];
  const char *ext;
  int j = 0;

  if (!fil)
    return 0;
  ext = get_ext(catbuff, sizeof(catbuff), fil);
  while(strnotempty(hts_mime[j][1])) {
    if (strfield2(hts_mime[j][1], ext)) {
      if (is_html_mime_type(hts_mime[j][0]))
        return 2;
      else
        return 1;
    }
    j++;
  }

  // Known by user?
  return (is_userknowntype(opt, fil));
}

// known type?..
//  0 : no
//  1 : yes
//  2 : html
// setdefs : set mime buffer:
//   file=(char*) "asp=text/html\nphp=text/html\n"
HTSEXT_API int is_userknowntype(httrackp * opt, const char *fil) {
  char BIGSTK mime[1024];

  if (!fil)
    return 0;
  if (!strnotempty(fil))
    return 0;
  mime[0] = '\0';
  get_userhttptype(opt, mime, fil);
  if (!strnotempty(mime))
    return 0;
  else if (is_html_mime_type(mime))
    return 2;
  else
    return 1;
}

// page dynamique?
// is_dyntype(get_ext("foo.asp"))
HTSEXT_API int is_dyntype(const char *fil) {
  int j = 0;

  if (!fil)
    return 0;
  if (!strnotempty(fil))
    return 0;
  while(strnotempty(hts_ext_dynamic[j])) {
    if (strfield2(hts_ext_dynamic[j], fil)) {
      return 1;
    }
    j++;
  }
  return 0;
}

// types critiques qui ne doivent pas être changés car renvoyés par des serveurs qui ne
// connaissent pas le type
int may_unknown(httrackp * opt, const char *st) {
  int j = 0;

  // types média
  if (may_be_hypertext_mime(opt, st, "")) {
    return 1;
  }
  while(strnotempty(hts_mime_keep[j])) {
    if (strfield2(hts_mime_keep[j], st)) {      // trouvé
      return 1;
    }
    j++;
  }
  return 0;
}

/* returns 1 if the mime/filename seems to be bogus because of badly recognized multiple extension
  ; such as "application/x-wais-source" for "httrack-3.42-1.el5.src.rpm"
  reported by Hippy Dave 08/2008 (3.43) */
int may_bogus_multiple(httrackp * opt, const char *mime, const char *filename) {
  int j;

  for(j = 0; strnotempty(hts_mime_bogus_multiple[j]); j++) {
    if (strfield2(hts_mime_bogus_multiple[j], mime)) {  /* found mime type in suspicious list */
      char ext[64];

      ext[0] = '\0';
      give_mimext(ext, mime);
      if (ext[0] != 0) {        /* we have an extension for that */
        const size_t ext_size = strlen(ext);
        const char *file = strrchr(filename, '/');      /* fetch terminal filename */

        if (file != NULL) {
          int i;

          for(i = 0; file[i] != 0; i++) {
            if (i > 0 && file[i - 1] == '.'
                && strncasecmp(&file[i], ext, ext_size) == 0
                && (file[i + ext_size] == 0 || file[i + ext_size] == '.'
                    || file[i + ext_size] == '?')) {
              return 1;         /* is ambiguous */
            }
          }
        }
      }
      return 0;
    }
  }
  return 0;
}

/* filename extension should not be changed because potentially bogus ; replaces may_unknown() (3.43) */
int may_unknown2(httrackp * opt, const char *mime, const char *filename) {
  int ret = may_unknown(opt, mime);

  if (ret == 0) {
    ret = may_bogus_multiple(opt, mime, filename);
  }
  return ret;
}

// -- Utils fichiers

// pretty print for i/o
void fprintfio(FILE * fp, const char *buff, const char *prefix) {
  char nl = 1;

  while(*buff) {
    switch (*buff) {
    case 13:
      break;
    case 10:
      fprintf(fp, "\r\n");
      nl = 1;
      break;
    default:
      if (nl)
        fprintf(fp, "%s", prefix);
      nl = 0;
      fputc(*buff, fp);
    }
    buff++;
  }
}

/* Le fichier existe-t-il? (ou est-il accessible?) */
/* Note: NOT utf-8 */
/* Note: preserve errno */
int fexist(const char *s) {
  char catbuff[CATBUFF_SIZE];
  const int err = errno;
  struct stat st;

  memset(&st, 0, sizeof(st));
  if (stat(fconv(catbuff, sizeof(catbuff), s), &st) == 0) {
    if (S_ISREG(st.st_mode)) {
      return 1;
    } else {
      return 0;
    }
  }
  errno = err;
  return 0;
}

/* Le fichier existe-t-il? (ou est-il accessible?) */
/* Note: utf-8 */
/* Note: preserve errno */
int fexist_utf8(const char *s) {
  char catbuff[CATBUFF_SIZE];
  const int err = errno;
  STRUCT_STAT st;

  memset(&st, 0, sizeof(st));
  if (STAT(fconv(catbuff, sizeof(catbuff), s), &st) == 0) {
    if (S_ISREG(st.st_mode)) {
      return 1;
    } else {
      return 0;
    }
  }
  errno = err;
  return 0;
}

/* Taille d'un fichier, -1 si n'existe pas */
/* Note: NOT utf-8 */
off_t fsize(const char *s) {
  struct stat st;

  if (!strnotempty(s))          // nom vide: erreur
    return -1;
  if (stat(s, &st) == 0 && S_ISREG(st.st_mode)) {
    return st.st_size;
  } else {
    return -1;
  }
}

/* Taille d'un fichier, -1 si n'existe pas */
/* Note: utf-8 */
off_t fsize_utf8(const char *s) {
  STRUCT_STAT st;

  if (!strnotempty(s))          // nom vide: erreur
    return -1;
  if (STAT(s, &st) == 0 && S_ISREG(st.st_mode)) {
    return st.st_size;
  } else {
    return -1;
  }
}

off_t fpsize(FILE * fp) {
  off_t oldpos, size;

  if (!fp)
    return -1;
#ifdef HTS_FSEEKO
  oldpos = ftello(fp);
#else
  oldpos = ftell(fp);
#endif
  fseek(fp, 0, SEEK_END);
#ifdef HTS_FSEEKO
  size = ftello(fp);
  fseeko(fp, oldpos, SEEK_SET);
#else
  size = ftell(fp);
  fseek(fp, oldpos, SEEK_SET);
#endif
  return size;
}

/* root dir, with ending / */
typedef struct {
  char path[1024 + 4];
  int init;
} hts_rootdir_strc;
HTSEXT_API const char *hts_rootdir(char *file) {
  static hts_rootdir_strc strc = { "", 0 };
  if (file) {
    if (!strc.init) {
      strc.path[0] = '\0';
      strc.init = 1;
      if (strnotempty(file)) {
        const size_t file_len = strlen(file);
        char *a;

        assertf(file_len < sizeof(strc.path));
        strcpybuff(strc.path, file);
        while((a = strrchr(strc.path, '\\')))
          *a = '/';
        if ((a = strrchr(strc.path, '/'))) {
          *(a + 1) = '\0';
        } else
          strc.path[0] = '\0';
      }
      if (!strnotempty(strc.path)) {
        if (getcwd(strc.path, sizeof(strc.path)) == NULL)
          strc.path[0] = '\0';
        else
          strcatbuff(strc.path, "/");
      }
    }
    return NULL;
  } else if (strc.init)
    return strc.path;
  else
    return "";
}

HTSEXT_API hts_stat_struct HTS_STAT;

//
// return  number of downloadable bytes, depending on rate limiter
// see engine_stats() routine, too
// this routine works quite well for big files and regular ones, but apparently the rate limiter has
// some problems with very small files (rate too high)
LLint check_downloadable_bytes(int rate) {
  if (rate > 0) {
    TStamp time_now;
    TStamp elapsed_useconds;
    LLint bytes_transferred_during_period;
    LLint left;

    // get the older timer
    int id_timer = (HTS_STAT.istat_idlasttimer + 1) % 2;

    time_now = mtime_local();
    elapsed_useconds = time_now - HTS_STAT.istat_timestart[id_timer];
    // NO totally stupid - elapsed_useconds+=1000;      // for the next second, too
    bytes_transferred_during_period =
      (HTS_STAT.HTS_TOTAL_RECV - HTS_STAT.istat_bytes[id_timer]);

    left = ((rate * elapsed_useconds) / 1000) - bytes_transferred_during_period;
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
// returns: 
// >0 : data received
// == 0 : not yet data
// <0: error or no data: READ_ERROR, READ_EOF or READ_TIMEOUT
int hts_read(htsblk * r, char *buff, int size) {
  int retour;

  //  return read(soc,buff,size);
  if (r->is_file) {
#if HTS_WIDE_DEBUG
    DEBUG_W("read(%p, %d, %d)\n" _(void *)buff _(int) size _(int) r->fp);
#endif
    if (r->fp) {
      retour = (int) fread(buff, 1, size, r->fp);
      if (retour == 0)  // can happen with directories (!)
        retour = READ_ERROR;
    } else
      retour = READ_ERROR;
  } else {
#if HTS_WIDE_DEBUG
    DEBUG_W("recv(%d, %p, %d)\n" _(int) r->soc _(void *)buff _(int) size);

    if (r->soc == INVALID_SOCKET)
      printf("!!WIDE_DEBUG ERROR, soc==INVALID hts_read\n");
#endif
    //HTS_TOTAL_RECV_CHECK(size);         // Diminuer au besoin si trop de données reçues
#if HTS_USEOPENSSL
    if (r->ssl) {
      retour = SSL_read(r->ssl_con, buff, size);
      if (retour <= 0) {
        int err_code = SSL_get_error(r->ssl_con, retour);

        if ((err_code == SSL_ERROR_WANT_READ)
            || (err_code == SSL_ERROR_WANT_WRITE)
          ) {
          retour = 0;           /* no data yet (ssl cache) */
        } else if (err_code == SSL_ERROR_ZERO_RETURN) {
          retour = READ_EOF;    /* completed */
        } else {
          retour = READ_ERROR;  /* eof or error */
        }
      }
    } else {
#endif
      retour = recv(r->soc, buff, size, 0);
      if (retour == 0) {
        retour = READ_EOF;
      } else if (retour < 0) {
        retour = READ_ERROR;
      }
    }
    if (retour > 0)             // compter flux entrant
      HTS_STAT.HTS_TOTAL_RECV += retour;
#if HTS_USEOPENSSL
  }
#endif
#if HTS_WIDE_DEBUG
  DEBUG_W("recv/read done (%d bytes)\n" _(int) retour);
#endif
  return retour;
}

// -- Gestion cache DNS --
// 'RX98

// 'capsule' contenant uniquement le cache
t_dnscache *hts_cache(httrackp * opt) {
  assertf(opt != NULL);
  if (opt->state.dns_cache == NULL) {
    opt->state.dns_cache = (t_dnscache *) malloct(sizeof(t_dnscache));
    memset(opt->state.dns_cache, 0, sizeof(t_dnscache));
  }
  assertf(opt->state.dns_cache != NULL);
  /* first entry is NULL */
  assertf(opt->state.dns_cache->iadr == NULL);
  return opt->state.dns_cache;
}

// Free DNS cache.
void hts_cache_free(t_dnscache *const root) {
  if (root != NULL) {
    t_dnscache *cache;
    for(cache = root; cache != NULL; ) {
      t_dnscache *const next = cache->next;
      cache->next = NULL;
      freet(cache);
      cache = next;
    }
  }
}

// lock le cache dns pour tout opération d'ajout
// plus prudent quand plusieurs threads peuvent écrire dedans..
// -1: status? 0: libérer 1:locker

// MUST BE LOCKED
// routine pour le cache - retour optionnel à donner à chaque fois
// NULL: nom non encore testé dans le cache
// si h_length==0 alors le nom n'existe pas dans le dns
static SOCaddr* hts_ghbn(const t_dnscache *cache, const char *const iadr, SOCaddr *const addr) {
  assertf(addr != NULL);
  assertf(iadr != NULL);
  if (*iadr == '\0') {
    return NULL;
  }
  /* first entry is empty */
  if (cache->iadr == NULL) {
    cache = cache->next;
  }
  for(; cache != NULL; cache = cache->next) {
    assertf(cache != NULL);
    assertf(cache->iadr != NULL);
    assertf(cache->iadr == (const char*) cache + sizeof(t_dnscache));
    if (strcmp(cache->iadr, iadr) == 0) {       // ok trouvé
      if (cache->host_length != 0) {     // entrée valide
        assertf(cache->host_length <= sizeof(cache->host_addr));
        SOCaddr_copyaddr2(*addr, cache->host_addr, cache->host_length);
        return addr;
      } else {                  // erreur dans le dns, déja vérifié
        SOCaddr_clear(*addr);
        return addr;
      }
    }
  }
  return NULL;
}

static SOCaddr* hts_dns_resolve_nocache2_(const char *const hostname,
                                          SOCaddr *const addr, 
                                          const char **error) {
  {
#if HTS_INET6==0
    /* IPv4 resolver */
    struct hostent *const hp = gethostbyname(hostname);

    if (hp != NULL) {
      SOCaddr_copyaddr2(addr, hp->h_addr_list[0], hp->h_length);
      return SOCaddr_is_valid(addr) ? &addr : NULL;
    } else {
      SOCaddr_clear(*addr);
    }
#else
    /* IPv6 resolver */
    struct addrinfo *res = NULL;
    struct addrinfo hints;
    int gerr;

    SOCaddr_clear(*addr);
    memset(&hints, 0, sizeof(hints));
    if (IPV6_resolver == 1)     // V4 only (for bogus V6 entries)
      hints.ai_family = PF_INET;
    else if (IPV6_resolver == 2)        // V6 only (for testing V6 only)
      hints.ai_family = PF_INET6;
    else                        // V4 + V6
      hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if ( ( gerr = getaddrinfo(hostname, NULL, &hints, &res) ) == 0) {
      if (res != NULL) {
        if (res->ai_addr != NULL && res->ai_addrlen != 0) {
          SOCaddr_copyaddr2(*addr, res->ai_addr, res->ai_addrlen);
        }
      }
    } else {
      if (error != NULL) {
        *error = gai_strerror(gerr);
      }
    }
    if (res) {
      freeaddrinfo(res);
    }
#endif
  }

  return SOCaddr_is_valid(*addr) ? addr : NULL;
}

HTSEXT_API SOCaddr* hts_dns_resolve_nocache2(const char *const hostname, 
                                     SOCaddr *const addr, const char **error) {
  /* Protection */
  if (!strnotempty(hostname)) {
    return NULL;
  }

  /*
     Strip [] if any : [3ffe:b80:1234:1::1] 
     The resolver doesn't seem to handle IP6 addresses in brackets
   */
  if ((hostname[0] == '[') && (hostname[strlen(hostname) - 1] == ']')) {
    SOCaddr *ret;
    size_t size = strlen(hostname);
    char *copy = malloct(size + 1);
    assertf(copy != NULL);
    copy[0] = '\0';
    strncat(copy, hostname + 1, size - 2);
    ret =  hts_dns_resolve_nocache2_(copy, addr, error);
    freet(copy);
    return ret;
  } else {
    return hts_dns_resolve_nocache2_(hostname, addr, error);
  }
}

HTSEXT_API SOCaddr* hts_dns_resolve_nocache(const char *const hostname, SOCaddr *const addr) {
  return hts_dns_resolve_nocache2(hostname, addr, NULL);
}

HTSEXT_API int check_hostname_dns(const char *const hostname) {
  SOCaddr buffer;
  return hts_dns_resolve_nocache(hostname, &buffer) != NULL;
}

// Needs locking
// cache dns interne à HTS // ** FREE A FAIRE sur la chaine
static SOCaddr* hts_dns_resolve_(httrackp * opt, const char *_iadr,
                                 SOCaddr *const addr, const char **error) {
  char BIGSTK iadr[HTS_URLMAXSIZE * 2];
  t_dnscache *cache = hts_cache(opt);  // adresse du cache
  SOCaddr *sa;

  assertf(opt != NULL);
  assertf(_iadr != NULL);
  assertf(addr != NULL);

  strcpybuff(iadr, jump_identification_const(_iadr));
  // couper éventuel :
  {
    char *a;

    if ((a = jump_toport(iadr)))
      *a = '\0';
  }

  /* get IP from the dns cache */
  sa = hts_ghbn(cache, iadr, addr);
  if (sa != NULL) {
    return SOCaddr_is_valid(*sa) ? sa : NULL;
  } else {                      // non présent dans le cache dns, tester
    const size_t iadr_len = strlen(iadr) + 1;

    // find queue
    for(; cache->next != NULL; cache = cache->next) ;

#if DEBUGDNS
    printf("resolving (not cached) %s\n", iadr);
#endif

    sa = hts_dns_resolve_nocache2(iadr, addr, error);     // calculer IP host

#if HTS_WIDE_DEBUG
    DEBUG_W("gethostbyname done\n");
#endif

    /* attempt to store new entry */
    cache->next = malloct(sizeof(t_dnscache) + iadr_len);
    if (cache->next != NULL) {
      t_dnscache *const next = cache->next;
      char *const block = (char*) cache->next;
      char *const str = block + sizeof(t_dnscache);
      memcpy(str, iadr, iadr_len);
      next->iadr = str;
      if (sa != NULL) {
        next->host_length = SOCaddr_size(*sa);
        assertf(next->host_length <= sizeof(next->host_addr));
        memcpy(next->host_addr, &SOCaddr_sockaddr(*sa), next->host_length);
      } else {
        next->host_length = 0;      // non existant dans le dns
      }
      next->next = NULL;
      return sa;
    }

    /* return result if any */
    return sa;
  }                             // retour hp du cache
}

SOCaddr* hts_dns_resolve2(httrackp * opt, const char *_iadr, SOCaddr *const addr, const char **error) {
  SOCaddr *ret;
  hts_mutexlock(&opt->state.lock);
  ret = hts_dns_resolve_(opt, _iadr, addr, error);
  hts_mutexrelease(&opt->state.lock);
  return ret;
}

SOCaddr* hts_dns_resolve(httrackp * opt, const char *_iadr, SOCaddr *const addr) {
  return hts_dns_resolve2(opt, _iadr, addr, NULL);
}

// --- Tracage des mallocs() ---
#ifdef HTS_TRACE_MALLOC
//#define htsLocker(A, N) htsLocker(A, N)
#define htsLocker(A, N) do {} while(0)
static mlink trmalloc = { NULL, 0, 0, NULL };

static int trmalloc_id = 0;
static htsmutex *mallocMutex = NULL;
static void hts_meminit(void) {
  //if (mallocMutex == NULL) {
  //  mallocMutex = calloc(sizeof(*mallocMutex), 1);
  //  htsLocker(mallocMutex, -999);
  //}
}
void *hts_malloc(size_t len) {
  void *adr;

  hts_meminit();
  htsLocker(mallocMutex, 1);
  assertf(len > 0);
  adr = hts_xmalloc(len, 0);
  htsLocker(mallocMutex, 0);
  return adr;
}
void *hts_calloc(size_t len, size_t len2) {
  void *adr;

  hts_meminit();
  assertf(len > 0);
  assertf(len2 > 0);
  htsLocker(mallocMutex, 1);
  adr = hts_xmalloc(len, len2);
  htsLocker(mallocMutex, 0);
  memset(adr, 0, len * len2);
  return adr;
}
void *hts_strdup(char *str) {
  size_t size = str ? strlen(str) : 0;
  char *adr = (char *) hts_malloc(size + 1);

  assertf(adr != NULL);
  strcpy(adr, str ? str : "");
  return adr;
}
void *hts_xmalloc(size_t len, size_t len2) {
  mlink *lnk = (mlink *) calloc(1, sizeof(mlink));

  assertf(lnk != NULL);
  assertf(len > 0);
  assertf(len2 >= 0);
  if (lnk) {
    void *r = NULL;
    int size, bsize = sizeof(t_htsboundary);

    if (len2)
      size = len * len2;
    else
      size = len;
    size += ((bsize - (size % bsize)) % bsize); /* check alignement */
    r = malloc(size + bsize * 2);
    assertf(r != NULL);
    if (r) {
      *((t_htsboundary *) ((char *) r))
        = *((t_htsboundary *) ((char *) r + size + bsize))
        = htsboundary;
      ((char *) r) += bsize;    /* boundary */
      lnk->adr = r;
      lnk->len = size;
      lnk->id = trmalloc_id++;
      lnk->next = trmalloc.next;
      trmalloc.next = lnk;
      return r;
    } else {
      free(lnk);
    }
  }
  return NULL;
}
void hts_free(void *adr) {
  mlink *lnk = &trmalloc;
  int bsize = sizeof(t_htsboundary);

  assertf(adr != NULL);
  if (!adr) {
    return;
  }
  htsLocker(mallocMutex, 1);
  while(lnk->next != NULL) {
    if (lnk->next->adr == adr) {
      mlink *blk_free = lnk->next;

      assertf(blk_free->id != -1);
      assertf(*((t_htsboundary *) ((char *) adr - bsize)) == htsboundary);
      assertf(*((t_htsboundary *) ((char *) adr + blk_free->len)) ==
              htsboundary);
      lnk->next = lnk->next->next;
      free((void *) blk_free);
      //blk_free->id=-1;
      free((char *) adr - bsize);
      htsLocker(mallocMutex, 0);
      return;
    }
    lnk = lnk->next;
    assertf(lnk->next != NULL);
  }
  free(adr);
  htsLocker(mallocMutex, 0);
}
void *hts_realloc(void *adr, size_t len) {
  int bsize = sizeof(t_htsboundary);

  len += ((bsize - (len % bsize)) % bsize);     /* check alignement */
  if (adr != NULL) {
    mlink *lnk = &trmalloc;

    htsLocker(mallocMutex, 1);
    while(lnk->next != NULL) {
      if (lnk->next->adr == adr) {
        {
          mlink *blk_free = lnk->next;

          assertf(blk_free->id != -1);
          assertf(*((t_htsboundary *) ((char *) adr - bsize)) == htsboundary);
          assertf(*((t_htsboundary *) ((char *) adr + blk_free->len)) ==
                  htsboundary);
        }
        adr = realloc((char *) adr - bsize, len + bsize * 2);
        assertf(adr != NULL);
        lnk->next->adr = (char *) adr + bsize;
        lnk->next->len = len;
        *((t_htsboundary *) ((char *) adr))
          = *((t_htsboundary *) ((char *) adr + len + bsize))
          = htsboundary;
        htsLocker(mallocMutex, 0);
        return (char *) adr + bsize;
      }
      lnk = lnk->next;
      assertf(lnk->next != NULL);
    }
    htsLocker(mallocMutex, 0);
  }
  return hts_malloc(len);
}
mlink *hts_find(char *adr) {
  char *stkframe = (char *) &stkframe;
  mlink *lnk = &trmalloc;
  int bsize = sizeof(t_htsboundary);

  assertf(adr != NULL);
  if (!adr) {
    return NULL;
  }
  htsLocker(mallocMutex, 1);
  while(lnk->next != NULL) {
    if (adr >= lnk->next->adr && adr <= lnk->next->adr + lnk->next->len) {      /* found */
      htsLocker(mallocMutex, 0);
      return lnk->next;
    }
    lnk = lnk->next;
  }
  htsLocker(mallocMutex, 0);
  {
    int depl = (int) (adr - stkframe);

    if (depl < 0)
      depl = -depl;
    //assertf(depl < 512000);   /* near the stack frame.. doesn't look like malloc but stack variable */
    return NULL;
  }
}

// check the malloct() and calloct() trace stack
void hts_freeall(void) {
  int bsize = sizeof(t_htsboundary);

  while(trmalloc.next) {
#if MEMDEBUG
    printf("* block %d\t not released: at %d\t (%d\t bytes)\n",
           trmalloc.next->id, trmalloc.next->adr, trmalloc.next->len);
#endif
    if (trmalloc.next->id != -1) {
      free((char *) trmalloc.next->adr - bsize);
    }
  }
}
#endif

// -- divers //

// cut path and project name
// patch also initial path
void cut_path(char *fullpath, char *path, char *pname) {
  path[0] = pname[0] = '\0';
  if (strnotempty(fullpath)) {
    if ((fullpath[strlen(fullpath) - 1] == '/')
        || (fullpath[strlen(fullpath) - 1] == '\\'))
      fullpath[strlen(fullpath) - 1] = '\0';
    if (strlen(fullpath) > 1) {
      char *a;

      while((a = strchr(fullpath, '\\')))
        *a = '/';               // remplacer par /
      a = fullpath + strlen(fullpath) - 2;
      while((*a != '/') && (a > fullpath))
        a--;
      if (*a == '/')
        a++;
      strcpybuff(pname, a);
      strncatbuff(path, fullpath, (int) (a - fullpath));
    }
  }
}

// -- Gestion protocole ftp --

#ifdef _WIN32
int ftp_available(void) {
  return 1;
}
#else
int ftp_available(void) {
  return 1;                     // ok!
  //return 0;   // SOUS UNIX, PROBLEMESs
}
#endif

static void hts_debug_log_print(const char *format, ...);

static int hts_dgb_init = 0;
static FILE *hts_dgb_init_fp = NULL;
HTSEXT_API void hts_debug(int level) {
  hts_dgb_init = level;
  if (hts_dgb_init > 0) {
    hts_debug_log_print("hts_debug() called");
  }
}

static FILE *hts_dgb_(void) {
  if (hts_dgb_init_fp == NULL) {
    if ((hts_dgb_init & 0x80) == 0) {
      hts_dgb_init_fp = stderr;
    } else {
      hts_dgb_init_fp = FOPEN("hts-debug.txt", "wb");
      if (hts_dgb_init_fp != NULL) {
        fprintf(hts_dgb_init_fp, "* Creating file\r\n");
      }
    }
  }
  return hts_dgb_init_fp;
}

static void hts_debug_log_print(const char *format, ...) {
  if (hts_dgb_init > 0) {
    const int error = errno;
    FILE *const fp = hts_dgb_();
    va_list args;

    assertf(format != NULL);
    va_start(args, format);
    (void) vfprintf(fp, format, args);
    va_end(args);
    fputs("\n", fp);
    fflush(fp);
    errno = error;
  }
}

HTSEXT_API const char* hts_version(void) {
  return HTTRACK_VERSIONID;
}

static int ssl_vulnerable(const char *version) {
#ifdef _WIN32
  static const char *const match = "OpenSSL 1.0.1";
  const size_t match_len = strlen(match);
  if (version != NULL && strncmp(version, match, match_len) == 0) {
    // CVE-2014-0160
    // "OpenSSL 1.0.1g 7 Apr 2014"
    const char minor = version[match_len];
    return minor == ' ' || ( minor >= 'a' && minor <= 'f' );
  }
#endif
  return 0;
}

/* user abort callback */
htsErrorCallback htsCallbackErr = NULL;

HTSEXT_API void hts_set_error_callback(htsErrorCallback handler) {
  htsCallbackErr = handler;
}

HTSEXT_API htsErrorCallback hts_get_error_callback(void) {
  return htsCallbackErr;
}

static void default_coucal_asserthandler(void *arg, const char* exp, const char* file, int line) {
  abortf_(exp, file, line);
}

static int get_loglevel_from_coucal(coucal_loglevel level) {
  switch(level) {
  case coucal_log_critical:
    return LOG_PANIC;
    break;
  case coucal_log_warning:
    return LOG_WARNING;
    break;
  case coucal_log_info:
    return LOG_INFO;
    break;
  case coucal_log_debug:
    return LOG_DEBUG;
    break;
  case coucal_log_trace:
    return LOG_TRACE;
    break;
  default:
    return LOG_ERROR;
    break;
  }
}

/* log to default console */
static void default_coucal_loghandler(void *arg, coucal_loglevel level, 
                                       const char* format, va_list args) {

  if (level <= coucal_log_warning) {
    fprintf(stderr, "** warning: ");
  }
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
}

/* log to project log */
static void htsopt_coucal_loghandler(void *arg, coucal_loglevel level, 
                                      const char* format, va_list args) {
  httrackp *const opt = (httrackp*) arg;
  if (opt != NULL && opt->log != NULL) {
    hts_log_vprint(opt, get_loglevel_from_coucal(level), 
      format, args);
  } else {
    default_coucal_loghandler(NULL, level, format, args);
  }
}

/* attach hashtable logger to project log */
void hts_set_hash_handler(coucal hashtable, httrackp *opt) {
  /* Init hashtable default assertion handler. */
  coucal_set_assert_handler(hashtable,
    htsopt_coucal_loghandler, 
    default_coucal_asserthandler,
    opt);
}

static int hts_init_ok = 0;
HTSEXT_API int hts_init(void) {
  const char *dbg_env;

  /* */
  if (hts_init_ok)
    return 1;
  hts_init_ok = 1;

  /* enable debugging ? */
  dbg_env = getenv("HTS_LOG");
  if (dbg_env != NULL && *dbg_env != 0) {
    int level = 0;

    if (sscanf(dbg_env, "%d", &level) == 1) {
      hts_debug(level);
    }
  }

  hts_debug_log_print("entering hts_init()");   /* debug */

  /* Init hashtable default assertion handler. */
  coucal_set_global_assert_handler(default_coucal_loghandler, 
    default_coucal_asserthandler);

  /* Init threads (lazy init) */
  htsthread_init();

  /* Ensure external modules are loaded */
  hts_debug_log_print("calling htspe_init()");  /* debug */
  htspe_init();                 /* module load (lazy) */

  /* MD5 Auto-test */
  {
    char digest[32 + 2];
    const char *atest = "MD5 Checksum Autotest";

    digest[0] = '\0';
    domd5mem(atest, strlen(atest), digest, 1);  /* a42ec44369da07ace5ec1d660ba4a69a */
    if (strcmp(digest, "a42ec44369da07ace5ec1d660ba4a69a") != 0) {
      int fatal_broken_md5 = 0;

      assertf(fatal_broken_md5);
    }
  }

  hts_debug_log_print("initializing SSL");      /* debug */
#if HTS_USEOPENSSL
  /*
     Initialize the OpensSSL library
   */
  if (!openssl_ctx) {
    const char *version;

    SSL_load_error_strings();
    SSL_library_init();

    // Check CVE-2014-0160.
    version = SSLeay_version(SSLEAY_VERSION);
    if (ssl_vulnerable(version)) {
      fprintf(stderr,
              "SSLeay_version(SSLEAY_VERSION) == '%s'\n", version);
      abortLog("unable to initialize TLS: OpenSSL version seems vulnerable to heartbleed bug (CVE-2014-0160)");
      assertf("OpenSSL version seems vulnerable to heartbleed bug (CVE-2014-0160)" == NULL);
    }

    // OpenSSL_add_all_algorithms();
    openssl_ctx = SSL_CTX_new(SSLv23_client_method());
    if (!openssl_ctx) {
      fprintf(stderr,
              "fatal: unable to initialize TLS: SSL_CTX_new(SSLv23_client_method)\n");
      abortLog("unable to initialize TLS: SSL_CTX_new(SSLv23_client_method)");
      assertf("unable to initialize TLS" == NULL);
    }
  }
#endif

  hts_debug_log_print("ending hts_init()");     /* debug */
  return 1;
}

/* will not free thread env. */
HTSEXT_API int hts_uninit(void) {
  /* hts_init() is a lazy initializer, with limited a allocation (one or two mutexes) ;
     we won't free anything here as the .h semantic was never being very clear */
  return 1;
}

HTSEXT_API int hts_uninit_module(void) {
  if (!hts_init_ok)
    return 1;
  htsthread_uninit();
  htspe_uninit();
  hts_init_ok = 0;
  return 1;
}

// legacy. do not use
HTSEXT_API int hts_log(httrackp * opt, const char *prefix, const char *msg) {
  if (opt->log != NULL) {
    fspc(opt, opt->log, prefix);
    fprintf(opt->log, "%s" LF, msg);
    return 0;
  }
  return 1;                     /* Error */
}

static void (*hts_log_print_callback)(httrackp * opt, int type, const char *format, va_list args) = NULL;

HTSEXT_API void hts_set_log_vprint_callback(void (*callback)(httrackp * opt,
                                            int type, const char *format, va_list args)) {
  hts_log_print_callback = callback;
}

HTSEXT_API void hts_log_vprint(httrackp * opt, int type, const char *format, va_list args) {
  assertf(format != NULL);
  if (hts_log_print_callback != NULL) {
    va_list args_copy;
    va_copy(args_copy, args);
    hts_log_print_callback(opt, type, format, args);
    va_end(args_copy);
  }
  if (opt != NULL && opt->log != NULL) {
    const int save_errno = errno;
    const char *s_type = "unknown";
    const int level = type & 0xff;

    // Check log level
    if (opt->debug < level) {
      return;
    }

    switch (level) {
    case LOG_TRACE:
      s_type = "trace";
      break;
    case LOG_DEBUG:
      s_type = "debug";
      break;
    case LOG_INFO:
      s_type = "info";
      break;
    case LOG_NOTICE:
    case LOG_WARNING:
      s_type = "warning";
      break;
    case LOG_ERROR:
      s_type = "error";
      break;
    case LOG_PANIC:
      s_type = "panic";
      break;
    }
    fspc(opt, opt->log, s_type);
    (void) vfprintf(opt->log, format, args);
    if ((type & LOG_ERRNO) != 0) {
      fprintf(opt->log, ": %s", strerror(save_errno));
    }
    fputs(LF, opt->log);
    if (opt->flush) {
      fflush(opt->log);
    }
    errno = save_errno;
  }
}

HTSEXT_API void hts_log_print(httrackp * opt, int type, const char *format, ...) {
  va_list args;
  assertf(format != NULL);
  va_start(args, format);
  hts_log_vprint(opt, type, format, args);
  va_end(args);
}

HTSEXT_API void set_wrappers(httrackp * opt) {  // LEGACY
}

HTSEXT_API int plug_wrapper(httrackp * opt, const char *moduleName,
                            const char *argv) {
  void *handle = openFunctionLib(moduleName);

  if (handle != NULL) {
    t_hts_plug plug = (t_hts_plug) getFunctionPtr(handle, "hts_plug");
    t_hts_unplug unplug = (t_hts_unplug) getFunctionPtr(handle, "hts_unplug");

    if (plug != NULL) {
      int ret = plug(opt, argv);

      if (hts_dgb_init > 0 && opt->log != NULL) {
        hts_debug_log_print("plugged module '%s' (return code=%d)", moduleName,
                            ret);
      }
      if (ret == 1) {           /* Success! */
        opt->libHandles.handles =
          (htslibhandle *) realloct(opt->libHandles.handles,
                                    (opt->libHandles.count +
                                     1) * sizeof(htslibhandle));
        opt->libHandles.handles[opt->libHandles.count].handle = handle;
        opt->libHandles.handles[opt->libHandles.count].moduleName =
          strdupt(moduleName);
        opt->libHandles.count++;
        return 1;
      } else {
        hts_debug_log_print
          ("* note: error while running entry point 'hts_plug' in %s",
           moduleName);
        if (unplug)
          unplug(opt);
      }
    } else {
      int last_errno = errno;

      hts_debug_log_print("* note: can't find entry point 'hts_plug' in %s: %s",
                          moduleName, strerror(last_errno));
    }
    closeFunctionLib(handle);
    return 0;
  } else {
    int last_errno = errno;

    hts_debug_log_print("* note: can't load %s: %s", moduleName,
                        strerror(last_errno));
  }
  return -1;
}

static void unplug_wrappers(httrackp * opt) {
  if (opt->libHandles.handles != NULL) {
    int i;

    for(i = 0; i < opt->libHandles.count; i++) {
      if (opt->libHandles.handles[i].handle != NULL) {
        /* hts_unplug(), the dll exit point (finalizer) */
        t_hts_unplug unplug =
          (t_hts_unplug) getFunctionPtr(opt->libHandles.handles[i].handle,
                                        "hts_unplug");
        if (unplug != NULL)
          unplug(opt);
        closeFunctionLib(opt->libHandles.handles[i].handle);
        opt->libHandles.handles[i].handle = NULL;
      }
      if (opt->libHandles.handles[i].moduleName != NULL) {
        freet(opt->libHandles.handles[i].moduleName);
        opt->libHandles.handles[i].moduleName = NULL;
      }
    }
    freet(opt->libHandles.handles);
    opt->libHandles.handles = NULL;
    opt->libHandles.count = 0;
  }
}

int multipleStringMatch(const char *s, const char *match) {
  int ret = 0;
  String name = STRING_EMPTY;

  if (match == NULL || s == NULL || *s == 0)
    return 0;
  for(; *match != 0; match++) {
    StringClear(name);
    for(; *match != 0 && *match != '\n'; match++) {
      StringAddchar(name, *match);
    }
    if (StringLength(name) > 0 && strstr(s, StringBuff(name)) != NULL) {
      ret = 1;
      break;
    }
  }
  StringFree(name);
  return ret;
}

HTSEXT_API httrackp *hts_create_opt(void) {
#if ( defined(_WIN32) || defined(__ANDROID__) )
  static const char *defaultModules[] = {
    "htsswf", "htsjava", "httrack-plugin", NULL
  };
#else
  static const char *defaultModules[] = {
    "libhtsswf.so.1", "libhtsjava.so.2", "httrack-plugin", NULL
  };
#endif
  httrackp *opt = malloc(sizeof(httrackp));

  /* default options */
  memset(opt, 0, sizeof(httrackp));
  opt->size_httrackp = sizeof(httrackp);

  /* mutexes */
  hts_mutexinit(&opt->state.lock);

  /* custom wrappers */
  opt->libHandles.count = 0;

  /* default settings */

  opt->wizard = 2;              // wizard automatique
  opt->quiet = 0;               // questions
  //  
  opt->travel = 0;              // même adresse
  opt->depth = 9999;            // mirror total par défaut
  opt->extdepth = 0;            // mais pas à l'extérieur
  opt->seeker = 1;              // down 
  opt->urlmode = 2;             // relatif par défaut
  opt->no_type_change = 0;      // change file types
  opt->debug = LOG_NOTICE;      // small log
  opt->getmode = 3;             // linear scan
  opt->maxsite = -1;            // taille max site (aucune)
  opt->maxfile_nonhtml = -1;    // taille max fichier non html
  opt->maxfile_html = -1;       // idem pour html
  opt->maxsoc = 4;              // nbre socket max
  opt->fragment = -1;           // pas de fragmentation
  opt->nearlink = 0;            // ne pas prendre les liens non-html "adjacents"
  opt->makeindex = 1;           // faire un index
  opt->kindex = 0;              // index 'keyword'
  opt->delete_old = 1;          // effacer anciens fichiers
  opt->background_on_suspend = 1;       // Background the process if Control Z calls signal suspend.
  opt->makestat = 0;            // pas de fichier de stats
  opt->maketrack = 0;           // ni de tracking
  opt->timeout = 120;           // timeout par défaut (2 minutes)
  opt->cache = 1;               // cache prioritaire
  opt->shell = 0;               // pas de shell par defaut
  opt->proxy.active = 0;        // pas de proxy
  opt->user_agent_send = 1;     // envoyer un user-agent
  StringCopy(opt->user_agent,
             "Mozilla/4.5 (compatible; HTTrack 3.0x; Windows 98)");
  StringCopy(opt->referer, "");
  StringCopy(opt->from, "");
  opt->savename_83 = 0;         // noms longs par défaut
  opt->savename_type = 0;       // avec structure originale
  opt->savename_delayed = 2;    // hard delayed type (default)
  opt->delayed_cached = 1;      // cached delayed type (default)
  opt->mimehtml = 0;            // pas MIME-html
  opt->parsejava = HTSPARSE_DEFAULT;    // parser classes
  opt->hostcontrol = 0;         // PAS de control host pour timeout et traffic jammer
  opt->retry = 2;               // 2 retry par défaut
  opt->errpage = 1;             // copier ou générer une page d'erreur en cas d'erreur (404 etc.)
  opt->check_type = 1;          // vérifier type si inconnu (cgi,asp..) SAUF / considéré comme html
  opt->all_in_cache = 0;        // ne pas tout stocker en cache
  opt->robots = 2;              // traiter les robots.txt
  opt->external = 0;            // liens externes normaux
  opt->passprivacy = 0;         // mots de passe dans les fichiers
  opt->includequery = 1;        // include query-string par défaut
  opt->mirror_first_page = 0;   // pas mode mirror links
  opt->accept_cookie = 1;       // gérer les cookies
  opt->cookie = NULL;
  opt->http10 = 0;              // laisser http/1.1
  opt->nokeepalive = 0;         // pas keep-alive
  opt->nocompression = 0;       // pas de compression
  opt->tolerant = 0;            // ne pas accepter content-length incorrect
  opt->parseall = 1;            // tout parser (tags inconnus, par exemple)
  opt->parsedebug = 0;          // pas de mode débuggage
  opt->norecatch = 0;           // ne pas reprendre les fichiers effacés par l'utilisateur
  opt->verbosedisplay = 0;      // pas d'animation texte
  opt->sizehack = 0;            // size hack
  opt->urlhack = 1;             // url hack (normalizer)
  StringCopy(opt->footer, HTS_DEFAULT_FOOTER);
  opt->ftp_proxy = 1;           // proxy http pour ftp
  opt->convert_utf8 = 1;        // convert html to UTF-8
  StringCopy(opt->filelist, "");
  StringCopy(opt->lang_iso, "en, *");
  StringCopy(opt->accept,
    "text/html,image/png,image/jpeg,image/pjpeg,image/x-xbitmap,image/svg+xml,image/gif;q=0.9,*/*;q=0.1");
  StringCopy(opt->headers, "");
  StringCopy(opt->mimedefs, "\n");      // aucun filtre mime (\n IMPORTANT)
  StringClear(opt->mod_blacklist);
  //
  opt->log = stdout;
  opt->errlog = stderr;
  opt->flush = 1;               // flush sur les fichiers log
  //opt->aff_progress=0;
  opt->keyboard = 0;
  //
  StringCopy(opt->path_html, "");
  StringCopy(opt->path_html_utf8, "");
  StringCopy(opt->path_log, "");
  StringCopy(opt->path_bin, "");
  //
  opt->maxlink = 100000;        // 100,000 liens max par défaut
  opt->maxfilter = 200;         // 200 filtres max par défaut
  opt->maxcache = 1048576 * 32; // a peu près 32Mo en cache max -- OPTION NON PARAMETRABLE POUR L'INSTANT --
  //opt->maxcache_anticipate=256;  // maximum de liens à anticiper
  opt->maxtime = -1;            // temps max en secondes
  opt->maxrate = 25000;         // taux maxi
  opt->maxconn = 5.0;           // nombre connexions/s
  opt->waittime = -1;           // wait until.. hh*3600+mm*60+ss
  //
  opt->exec = "";
  opt->is_update = 0;           // not an update (yet)
  opt->dir_topindex = 0;        // do not built top index (yet)
  //
  opt->bypass_limits = 0;       // enforce limits by default
  opt->state.stop = 0;          // stopper
  opt->state.exit_xh = 0;       // abort
  //
  opt->state.is_ended = 0;

  /* Alocated buffers */

  opt->callbacks_fun =
    (t_hts_htmlcheck_callbacks *) malloct(sizeof(t_hts_htmlcheck_callbacks));
  memset(opt->callbacks_fun, 0, sizeof(t_hts_htmlcheck_callbacks));

  /* Preload callbacks : java and flash parser, and the automatic user-defined callback */

  {
    int i;

    for(i = 0; defaultModules[i] != NULL; i++) {
      int ret = plug_wrapper(opt, defaultModules[i], defaultModules[i]);

      if (ret == 0) {           /* Module aborted initialization */
        /* Ignored. */
      }
    }
  }

  return opt;
}

HTSEXT_API size_t hts_sizeof_opt(void) {
  return sizeof(httrackp);
}

HTSEXT_API void hts_free_opt(httrackp * opt) {
  if (opt != NULL) {

    /* Alocated callbacks */

    if (opt->callbacks_fun != NULL) {
      int i;
      t_hts_htmlcheck_callbacks_item *items =
        (t_hts_htmlcheck_callbacks_item *) opt->callbacks_fun;
      const int size =
        (int) sizeof(t_hts_htmlcheck_callbacks) /
        sizeof(t_hts_htmlcheck_callbacks_item);
      assertf(sizeof(t_hts_htmlcheck_callbacks_item) * size ==
              sizeof(t_hts_htmlcheck_callbacks));

      /* Free all linked lists */
      for(i = 0; i < size; i++) {
        t_hts_callbackarg *carg, *next_carg;

        for(carg = items[i].carg;
            carg != NULL && (next_carg = carg->prev.carg, carg != NULL);
            carg = next_carg) {
          hts_free(carg);
        }
      }

      freet(opt->callbacks_fun);
      opt->callbacks_fun = NULL;
    }

    /* Close library handles */
    unplug_wrappers(opt);

    /* Cache */
    if (opt->state.dns_cache != NULL) {
      t_dnscache *root;

      hts_mutexlock(&opt->state.lock);
      root = opt->state.dns_cache;
      opt->state.dns_cache = NULL;
      hts_mutexrelease(&opt->state.lock);

      hts_cache_free(root);
    }

    /* Cancel chain */
    if (opt->state.cancel != NULL) {
      htsoptstatecancel *cancel;

      for(cancel = opt->state.cancel; cancel != NULL;) {
        htsoptstatecancel *next = cancel->next;

        if (cancel->url != NULL) {
          freet(cancel->url);
        }
        freet(cancel);
        cancel = next;
      }
      opt->state.cancel = NULL;
    }

    /* Free strings */

    StringFree(opt->proxy.name);
    StringFree(opt->proxy.bindhost);

    StringFree(opt->savename_userdef);
    StringFree(opt->user_agent);
    StringFree(opt->referer);
    StringFree(opt->from);
    StringFree(opt->lang_iso);
    StringFree(opt->sys_com);
    StringFree(opt->mimedefs);
    StringFree(opt->filelist);
    StringFree(opt->urllist);
    StringFree(opt->footer);
    StringFree(opt->mod_blacklist);

    StringFree(opt->path_html);
    StringFree(opt->path_html_utf8);
    StringFree(opt->path_log);
    StringFree(opt->path_bin);

    /* mutexes */
    hts_mutexfree(&opt->state.lock);

    /* Free structure */
    free(opt);
  }
}

// TEMPORARY - PUT THIS STRUCTURE INSIDE httrackp !
const hts_stat_struct* hts_get_stats(httrackp * opt) {
  if (opt == NULL) {
    return NULL;
  }

  HTS_STAT.stat_nsocket = 0;
  HTS_STAT.stat_errors = fspc(opt, NULL, "error");
  HTS_STAT.stat_warnings = fspc(opt, NULL, "warning");
  HTS_STAT.stat_infos = fspc(opt, NULL, "info");
  HTS_STAT.nbk = 0;
  HTS_STAT.nb = 0;

  return &HTS_STAT;
}

// defaut wrappers
static void __cdecl htsdefault_init(t_hts_callbackarg * carg) {
}
static void __cdecl htsdefault_uninit(t_hts_callbackarg * carg) {
  // hts_freevar();
}
static int __cdecl htsdefault_start(t_hts_callbackarg * carg, httrackp * opt) {
  return 1;
}
static int __cdecl htsdefault_chopt(t_hts_callbackarg * carg, httrackp * opt) {
  return 1;
}
static int __cdecl htsdefault_end(t_hts_callbackarg * carg, httrackp * opt) {
  return 1;
}
static int __cdecl htsdefault_preprocesshtml(t_hts_callbackarg * carg,
                                             httrackp * opt, char **html,
                                             int *len, const char *url_adresse,
                                             const char *url_fichier) {
  return 1;
}
static int __cdecl htsdefault_postprocesshtml(t_hts_callbackarg * carg,
                                              httrackp * opt, char **html,
                                              int *len, const char *url_adresse,
                                              const char *url_fichier) {
  return 1;
}
static int __cdecl htsdefault_checkhtml(t_hts_callbackarg * carg,
                                        httrackp * opt, char *html, int len,
                                        const char *url_adresse,
                                        const char *url_fichier) {
  return 1;
}
static int __cdecl htsdefault_loop(t_hts_callbackarg * carg, httrackp * opt, lien_back * back, int back_max, int back_index, int lien_n, int lien_tot, int stat_time, hts_stat_struct * stats) {        // appelé à chaque boucle de HTTrack
  return 1;
}
static const char *__cdecl htsdefault_query(t_hts_callbackarg * carg,
                                            httrackp * opt,
                                            const char *question) {
  return "";
}
static const char *__cdecl htsdefault_query2(t_hts_callbackarg * carg,
                                             httrackp * opt,
                                             const char *question) {
  return "";
}
static const char *__cdecl htsdefault_query3(t_hts_callbackarg * carg,
                                             httrackp * opt,
                                             const char *question) {
  return "";
}
static int __cdecl htsdefault_check(t_hts_callbackarg * carg, httrackp * opt,
                                    const char *adr, const char *fil,
                                    int status) {
  return -1;
}
static int __cdecl htsdefault_check_mime(t_hts_callbackarg * carg,
                                         httrackp * opt, const char *adr,
                                         const char *fil, const char *mime,
                                         int status) {
  return -1;
}
static void __cdecl htsdefault_pause(t_hts_callbackarg * carg, httrackp * opt,
                                     const char *lockfile) {
  while(fexist(lockfile)) {
    Sleep(1000);
  }
}
static void __cdecl htsdefault_filesave(t_hts_callbackarg * carg,
                                        httrackp * opt, const char *file) {
}
static void __cdecl htsdefault_filesave2(t_hts_callbackarg * carg,
                                         httrackp * opt, const char *adr,
                                         const char *file, const char *sav,
                                         int is_new, int is_modified,
                                         int not_updated) {
}
static int __cdecl htsdefault_linkdetected(t_hts_callbackarg * carg,
                                           httrackp * opt, char *link) {
  return 1;
}
static int __cdecl htsdefault_linkdetected2(t_hts_callbackarg * carg,
                                            httrackp * opt, char *link,
                                            const char *start_tag) {
  return 1;
}
static int __cdecl htsdefault_xfrstatus(t_hts_callbackarg * carg,
                                        httrackp * opt, lien_back * back) {
  return 1;
}
static int __cdecl htsdefault_savename(t_hts_callbackarg * carg, httrackp * opt,
                                       const char *adr_complete,
                                       const char *fil_complete,
                                       const char *referer_adr,
                                       const char *referer_fil, char *save) {
  return 1;
}
static int __cdecl htsdefault_sendhead(t_hts_callbackarg * carg, httrackp * opt,
                                       char *buff, const char *adr,
                                       const char *fil, const char *referer_adr,
                                       const char *referer_fil,
                                       htsblk * outgoing) {
  return 1;
}
static int __cdecl htsdefault_receivehead(t_hts_callbackarg * carg,
                                          httrackp * opt, char *buff,
                                          const char *adr, const char *fil,
                                          const char *referer_adr,
                                          const char *referer_fil,
                                          htsblk * incoming) {
  return 1;
}
static int __cdecl htsdefault_detect(t_hts_callbackarg * carg, httrackp * opt,
                                     htsmoduleStruct * str) {
  return 0;
}
static int __cdecl htsdefault_parse(t_hts_callbackarg * carg, httrackp * opt,
                                    htsmoduleStruct * str) {
  return 0;
}

/* Default internal dummy callbacks */
const t_hts_htmlcheck_callbacks default_callbacks = {
  {htsdefault_init, NULL},
  {htsdefault_uninit, NULL},
  {htsdefault_start, NULL},
  {htsdefault_end, NULL},
  {htsdefault_chopt, NULL},
  {htsdefault_preprocesshtml, NULL},
  {htsdefault_postprocesshtml, NULL},
  {htsdefault_checkhtml, NULL},
  {htsdefault_query, NULL},
  {htsdefault_query2, NULL},
  {htsdefault_query3, NULL},
  {htsdefault_loop, NULL},
  {htsdefault_check, NULL},
  {htsdefault_check_mime, NULL},
  {htsdefault_pause, NULL},
  {htsdefault_filesave, NULL},
  {htsdefault_filesave2, NULL},
  {htsdefault_linkdetected, NULL},
  {htsdefault_linkdetected2, NULL},
  {htsdefault_xfrstatus, NULL},
  {htsdefault_savename, NULL},
  {htsdefault_sendhead, NULL},
  {htsdefault_receivehead, NULL},
  {htsdefault_detect, NULL},
  {htsdefault_parse, NULL}
};

#define CALLBACK_OP(CB, NAME, OPERATION, S, FUN) do {   \
  if (strcmp(NAME, S) == 0) {                           \
    OPERATION(t_hts_htmlcheck_ ##FUN, (CB)->FUN.fun);   \
  }                                                     \
} while(0)

#define DISPATCH_CALLBACK(CB, NAME, OPERATION) do { \
  CALLBACK_OP(CB, NAME, OPERATION, "init", init); \
  CALLBACK_OP(CB, NAME, OPERATION, "free", uninit); \
  CALLBACK_OP(CB, NAME, OPERATION, "start", start); \
  CALLBACK_OP(CB, NAME, OPERATION, "end", end); \
  CALLBACK_OP(CB, NAME, OPERATION, "change-options", chopt); \
  CALLBACK_OP(CB, NAME, OPERATION, "preprocess-html", preprocess); \
  CALLBACK_OP(CB, NAME, OPERATION, "postprocess-html", postprocess); \
  CALLBACK_OP(CB, NAME, OPERATION, "check-html", check_html); \
  CALLBACK_OP(CB, NAME, OPERATION, "query", query); \
  CALLBACK_OP(CB, NAME, OPERATION, "query2", query2); \
  CALLBACK_OP(CB, NAME, OPERATION, "query3", query3); \
  CALLBACK_OP(CB, NAME, OPERATION, "loop", loop); \
  CALLBACK_OP(CB, NAME, OPERATION, "check-link", check_link); \
  CALLBACK_OP(CB, NAME, OPERATION, "check-mime", check_mime); \
  CALLBACK_OP(CB, NAME, OPERATION, "pause", pause); \
  CALLBACK_OP(CB, NAME, OPERATION, "save-file", filesave); \
  CALLBACK_OP(CB, NAME, OPERATION, "save-file2", filesave2); \
  CALLBACK_OP(CB, NAME, OPERATION, "link-detected", linkdetected); \
  CALLBACK_OP(CB, NAME, OPERATION, "link-detected2", linkdetected2); \
  CALLBACK_OP(CB, NAME, OPERATION, "transfer-status", xfrstatus); \
  CALLBACK_OP(CB, NAME, OPERATION, "save-name", savename); \
  CALLBACK_OP(CB, NAME, OPERATION, "send-header", sendhead); \
  CALLBACK_OP(CB, NAME, OPERATION, "receive-header", receivehead); \
} while(0)

int hts_set_callback(t_hts_htmlcheck_callbacks * callbacks, const char *name,
                     void *function) {
  int error = 1;
#define CALLBACK_OPERATION(TYPE, FUNCTION) do { \
    FUNCTION = (TYPE) function;                 \
    error = 0;                                  \
  } while(0)
  DISPATCH_CALLBACK(callbacks, name, CALLBACK_OPERATION);
#undef CALLBACK_OPERATION
  return error;
}

void *hts_get_callback(t_hts_htmlcheck_callbacks * callbacks, const char *name) {
#define CALLBACK_OPERATION(TYPE, FUNCTION) do { \
    return (void*) FUNCTION;                    \
  } while(0)
  DISPATCH_CALLBACK(callbacks, name, CALLBACK_OPERATION);
#undef CALLBACK_OPERATION
  return NULL;
}

// end defaut wrappers

/* libc stubs */

HTSEXT_API char *hts_strdup(const char *str) {
  return strdup(str);
}

HTSEXT_API void *hts_malloc(size_t size) {
  return malloc(size);
}

HTSEXT_API void *hts_realloc(void *const data, const size_t size) {
  return realloc(data, size);
}

HTSEXT_API void hts_free(void *data) {
  free(data);
}

/* Dummy functions */
HTSEXT_API int hts_resetvar(void) {
  return 0;
}

#ifdef _WIN32

typedef struct dirent dirent;
DIR *opendir(const char *name) {
  WIN32_FILE_ATTRIBUTE_DATA st;
  DIR *dir;
  size_t len;
  int i;

  if (name == NULL || *name == '\0') {
    errno = ENOENT;
    return NULL;
  }
  if (!GetFileAttributesEx(name, GetFileExInfoStandard, &st)
      || (st.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
    errno = ENOENT;
    return NULL;
  }
  dir = calloc(sizeof(DIR), 1);
  if (dir == NULL) {
    errno = ENOMEM;
    return NULL;
  }
  len = strlen(name);
  dir->h = INVALID_HANDLE_VALUE;
  dir->name = malloc(len + 2 + 1);
  strcpy(dir->name, name);
  for(i = 0; dir->name[i] != '\0'; i++) {
    if (dir->name[i] == '/') {
      dir->name[i] = '\\';
    }
  }
  strcat(dir->name, "\\*");
  return dir;
}

struct dirent *readdir(DIR * dir) {
  WIN32_FIND_DATAA find;

  if (dir->h == INVALID_HANDLE_VALUE) {
    dir->h = FindFirstFileA(dir->name, &find);
  } else {
    if (!FindNextFile(dir->h, &find)) {
      FindClose(dir->h);
      dir->h = INVALID_HANDLE_VALUE;
    }
  }
  if (dir->h != INVALID_HANDLE_VALUE) {
    dir->entry.d_name[0] = 0;
    strncat(dir->entry.d_name, find.cFileName, HTS_DIRENT_SIZE - 1);
    return &dir->entry;
  }
  errno = ENOENT;
  return NULL;
}

int closedir(DIR * dir) {
  if (dir != NULL) {
    if (dir->h != INVALID_HANDLE_VALUE) {
      CloseHandle(dir->h);
    }
    if (dir->name != NULL) {
      free(dir->name);
    }
    free(dir);
    return 0;
  }
  errno = EBADF;
  return -1;
}

// UTF-8 aware FILE API

static void copyWchar(LPWSTR dest, const char *src) {
  int i;

  for(i = 0; src[i]; i++) {
    dest[i] = src[i];
  }
  dest[i] = '\0';
}

FILE *hts_fopen_utf8(const char *path, const char *mode) {
  WCHAR wmode[32];
  LPWSTR wpath = hts_convertUTF8StringToUCS2(path, (int) strlen(path), NULL);

  assertf(strlen(mode) < sizeof(wmode) / sizeof(WCHAR));
  copyWchar(wmode, mode);
  if (wpath != NULL) {
    FILE *const fp = _wfopen(wpath, wmode);

    free(wpath);
    return fp;
  } else {
    // Fallback on conversion error.
    return fopen(path, mode);
  }
}

int hts_stat_utf8(const char *path, STRUCT_STAT * buf) {
  LPWSTR wpath = hts_convertUTF8StringToUCS2(path, (int) strlen(path), NULL);

  if (wpath != NULL) {
    const int result = _wstat(wpath, buf);

    free(wpath);
    return result;
  } else {
    // Fallback on conversion error.
    return _stat(path, buf);
  }
}

int hts_unlink_utf8(const char *path) {
  LPWSTR wpath = hts_convertUTF8StringToUCS2(path, (int) strlen(path), NULL);

  if (wpath != NULL) {
    const int result = _wunlink(wpath);

    free(wpath);
    return result;
  } else {
    // Fallback on conversion error.
    return _unlink(path);
  }
}

int hts_rename_utf8(const char *oldpath, const char *newpath) {
  LPWSTR woldpath =
    hts_convertUTF8StringToUCS2(oldpath, (int) strlen(oldpath), NULL);
  LPWSTR wnewpath =
    hts_convertUTF8StringToUCS2(newpath, (int) strlen(newpath), NULL);
  if (woldpath != NULL && wnewpath != NULL) {
    const int result = _wrename(woldpath, wnewpath);

    free(woldpath);
    free(wnewpath);
    return result;
  } else {
    if (woldpath != NULL)
      free(woldpath);
    if (wnewpath != NULL)
      free(wnewpath);
    // Fallback on conversion error.
    return rename(oldpath, newpath);
  }
}

int hts_mkdir_utf8(const char *path) {
  LPWSTR wpath = hts_convertUTF8StringToUCS2(path, (int) strlen(path), NULL);

  if (wpath != NULL) {
    const int result = _wmkdir(wpath);

    free(wpath);
    return result;
  } else {
    // Fallback on conversion error.
    return _mkdir(path);
  }
}

HTSEXT_API int hts_utime_utf8(const char *path, const STRUCT_UTIMBUF * times) {
  STRUCT_UTIMBUF mtimes = *times;
  LPWSTR wpath = hts_convertUTF8StringToUCS2(path, (int) strlen(path), NULL);

  if (wpath != NULL) {
    const int result = _wutime(wpath, &mtimes);

    free(wpath);
    return result;
  } else {
    // Fallback on conversion error.
    return _utime(path, &mtimes);
  }
}

#endif

// Fin
