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
/* File: Subroutines .h                                         */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

// Fichier librairie .h

#ifndef HTS_DEFH
#define HTS_DEFH 

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_htsrequest
#define HTS_DEF_FWSTRUCT_htsrequest
typedef struct htsrequest htsrequest;
#endif
#ifndef HTS_DEF_FWSTRUCT_htsblk
#define HTS_DEF_FWSTRUCT_htsblk
typedef struct htsblk htsblk;
#endif
#ifndef HTS_DEF_FWSTRUCT_t_dnscache
#define HTS_DEF_FWSTRUCT_t_dnscache
typedef struct t_dnscache t_dnscache;
#endif

/* définitions globales */
#include "htsglobal.h"

/* basic net definitions */
#include "htsbase.h"
#include "htsbasenet.h"
#include "htsnet.h"
#include "htsdefines.h"

/* readdir() */
#ifndef _WIN32
#include <sys/types.h>
#include <dirent.h>
#endif

/* cookies et auth */
#include "htsbauth.h"

// Attention, définition existante également dans le shell
// (à modifier avec celle-ci)
#define POSTTOK "?>post"

#include "htsopt.h"

#define READ_ERROR (-1)
#define READ_EOF (-2)
#define READ_TIMEOUT (-3)
#define READ_INTERNAL_ERROR (-4)

/* concat */

#if ( defined(_WIN32) && defined(_MSC_VER) && ( _MSC_VER >= 1300 ) && (_MSC_VER <= 1310 ) )
/* NOTE: VC2003 inlining bug in optim mode not respecting function call sequence point */
#define MSVC2003INLINEBUG __declspec(noinline)
#else
#define MSVC2003INLINEBUG
#endif
MSVC2003INLINEBUG static char* getHtsOptBuff_(httrackp *opt) {
	opt->state.concat.index = ( opt->state.concat.index + 1 ) % 16;
	return opt->state.concat.buff[opt->state.concat.index];
}
#undef MSVC2003INLINEBUG
#define OPT_GET_BUFF(OPT) ( getHtsOptBuff_(OPT) )

// structure pour paramètres supplémentaires lors de la requête
#ifndef HTS_DEF_FWSTRUCT_htsrequest_proxy
#define HTS_DEF_FWSTRUCT_htsrequest_proxy
typedef struct htsrequest_proxy htsrequest_proxy;
#endif
struct htsrequest_proxy {
  int active;
  char name[1024];
  int port;
  char bindhost[256];   // bind this host
}; 
#ifndef HTS_DEF_FWSTRUCT_htsrequest
#define HTS_DEF_FWSTRUCT_htsrequest
typedef struct htsrequest htsrequest;
#endif
struct htsrequest {
  short int user_agent_send;  // user agent (ex: httrack/1.0 [sun])
  short int http11;           // l'en tête peut (doit) être signé HTTP/1.1 et non HTTP/1.0
  short int nokeepalive;      // pas de keep-alive
  short int range_used;       // Range utilisé
  short int nocompression;    // Pas de compression
  short int flush_garbage;    // recycled
  char user_agent[128];
  char referer[256];
  char from[256];
  char lang_iso[64];
  htsrequest_proxy proxy;              // proxy
};


// structure pour retour d'une connexion/prise d'en tête
#ifndef HTS_DEF_FWSTRUCT_htsblk
#define HTS_DEF_FWSTRUCT_htsblk
typedef struct htsblk htsblk;
#endif
struct htsblk {
  int statuscode;        // status-code, -1=erreur, 200=OK,201=..etc (cf RFC1945)
  short int notmodified; // page ou fichier NON modifié (transféré)
  short int is_write;    // sortie sur disque (out) ou en mémoire (adr)
  short int is_chunk;    // mode chunk
  short int compressed;  // compressé?
  short int empty;       // vide?
  short int keep_alive;  // Keep-Alive?
  short int keep_alive_trailers;  // ..with trailers extension
  int keep_alive_t;      // KA timeout
  int keep_alive_max;    // KA number of requests
  char* adr;             // adresse du bloc de mémoire, NULL=vide
  char* headers;         // adresse des en têtes si présents
  FILE* out;             // écriture directe sur disque (si is_write=1)
  LLint size;            // taille fichier
  char msg[80];          // message éventuel si échec ("\0"=non précisé)
  char contenttype[64];  // content-type ("text/html" par exemple)
  char charset[64];      // charset ("iso-8859-1" par exemple)
  char contentencoding[64];  // content-encoding ("gzip" par exemple)
  char* location;        // on copie dedans éventuellement la véritable 'location'
  LLint totalsize;       // taille totale à télécharger (-1=inconnue)
  short int is_file;     // ce n'est pas une socket mais un descripteur de fichier si 1
  T_SOC soc;             // ID socket
  SOCaddr address;       // IP address
  int     address_size;  // IP address structure length
  FILE* fp;              // fichier pour file://
#if HTS_USEOPENSSL
  short int ssl;         // is this connection a SSL one? (https)
  // BIO* ssl_soc;          // SSL structure
  SSL * ssl_con;         // connection structure
#endif
  char lastmodified[64]; // Last-Modified
  char etag[64];         // Etag
  char cdispo[256];      // Content-Disposition coupé
  LLint  crange;         // Content-Range
  int debugid;           // debug connection
  /* */
  htsrequest req;        // paramètres pour la requête
  /*char digest[32+2];   // digest md5 généré par le moteur ("" si non généré)*/
};


/* ANCIENNE STURCTURE pour cache 1.0 */
#ifndef HTS_DEF_FWSTRUCT_OLD_t_proxy
#define HTS_DEF_FWSTRUCT_OLD_t_proxy
typedef struct OLD_t_proxy OLD_t_proxy;
#endif
struct OLD_t_proxy {
  int active;
  char name[1024];
  int port;
}; 
#ifndef HTS_DEF_FWSTRUCT_OLD_htsblk
#define HTS_DEF_FWSTRUCT_OLD_htsblk
typedef struct OLD_htsblk OLD_htsblk;
#endif
struct OLD_htsblk {
  int statuscode;  // ANCIENNE STURCTURE - status-code, -1=erreur, 200=OK,201=..etc (cf RFC1945)
  int notmodified; // ANCIENNE STURCTURE - page ou fichier NON modifié (transféré)
  int is_write;    // ANCIENNE STURCTURE - sortie sur disque (out) ou en mémoire (adr)
  char* adr;       // ANCIENNE STURCTURE - adresse du bloc de mémoire, NULL=vide
  FILE* out;       // ANCIENNE STURCTURE - écriture directe sur disque (si is_write=1)
  int size;        // ANCIENNE STURCTURE - taille fichier
  char msg[80];    // ANCIENNE STURCTURE - message éventuel si échec ("\0"=non précisé)
  char contenttype[64];  // ANCIENNE STURCTURE - content-type ("text/html" par exemple)
  char* location;  // ANCIENNE STURCTURE - on copie dedans éventuellement la véritable 'location'
  int totalsize;   // ANCIENNE STURCTURE - taille totale à télécharger (-1=inconnue)
  int is_file;     // ANCIENNE STURCTURE - ce n'est pas une socket mais un descripteur de fichier si 1
  T_SOC soc;       // ANCIENNE STURCTURE - ID socket
  FILE* fp;        // ANCIENNE STURCTURE - fichier pour file://
  OLD_t_proxy proxy;   // ANCIENNE STURCTURE - proxy
  int user_agent_send;  // ANCIENNE STURCTURE - user agent (ex: httrack/1.0 [sun])
  char user_agent[64];
  int http11;           // ANCIENNE STURCTURE - l'en tête doit être signé HTTP/1.1 et non HTTP/1.0
};
/* fin ANCIENNE STURCTURE pour cache 1.0 */

// cache pour le dns, pour éviter de faire des gethostbyname sans arrêt
#ifndef HTS_DEF_FWSTRUCT_t_dnscache
#define HTS_DEF_FWSTRUCT_t_dnscache
typedef struct t_dnscache t_dnscache;
#endif
struct t_dnscache {
  char iadr[1024];
  struct t_dnscache* n;
  char host_addr[HTS_MAXADDRLEN];    // 4 octets (v4), ou 16 octets (v6)
  int host_length;                   // 4 normalement - ==0  alors en cours de résolution
                                     // ou >16 si sockaddr
                                     //                 ==-1 alors erreur (host n'éxiste pas)
};


/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

extern htsmutex dns_lock;

// fonctions unix/winsock
int hts_read(htsblk* r,char* buff,int size);
//int HTS_TOTAL_RECV_CHECK(int var);
LLint check_downloadable_bytes(int rate);

#ifndef HTTRACK_DEFLIB
HTSEXT_API int hts_init(void);
HTSEXT_API int hts_uninit(void);
HTSEXT_API int hts_uninit_module(void);
HTSEXT_API int hts_resetvar(void);  /* dummy */
HTSEXT_API void hts_debug(int level);
HTSEXT_API httrackp* hts_create_opt(void);
HTSEXT_API void hts_free_opt(httrackp *opt);
HTSEXT_API void set_wrappers(httrackp *opt);    /* LEGACY */
HTSEXT_API int plug_wrapper(httrackp *opt, const char *moduleName, const char* argv);

HTSEXT_API char* hts_strdup(const char* string);
HTSEXT_API void* hts_malloc(size_t size);
HTSEXT_API void* hts_realloc(void* data, size_t size);
HTSEXT_API void hts_free(void* data);
#endif
extern int hts_dgb_init;
extern FILE* hts_dgb_(void);
#undef _
#define _ ,
#define HTS_DBG(FMT) do {     \
  if (hts_dgb_init > 0) {     \
    FILE *fp = hts_dgb_();    \
    fprintf(fp, FMT);         \
    fprintf(fp, "\n");        \
    fflush(fp);               \
  }                           \
} while(0)

// fonctions principales
T_SOC http_fopen(httrackp *opt,char* adr,char* fil,htsblk* retour);
T_SOC http_xfopen(httrackp *opt,int mode,int treat,int waitconnect,char* xsend,char* adr,char* fil,htsblk* retour);
int http_sendhead(httrackp *opt,t_cookie* cookie,int mode,char* xsend,char* adr,char* fil,char* referer_adr,char* referer_fil,htsblk* retour);
htsblk httpget(httrackp *opt,char* url);
//int newhttp(char* iadr,char* err=NULL);
T_SOC newhttp(httrackp *opt,const char* iadr,htsblk* retour,int port,int waitconnect);
HTS_INLINE void deletehttp(htsblk* r);
HTS_INLINE int  deleteaddr(htsblk* r);
HTS_INLINE void deletesoc(T_SOC soc);
HTS_INLINE void deletesoc_r(htsblk* r);
htsblk http_location(httrackp *opt,char* adr,char* fil,char* loc);
htsblk http_test(httrackp *opt,char* adr,char* fil,char* loc);
int check_readinput(htsblk* r);
int check_readinput_t(T_SOC soc, int timeout);
void http_fread(T_SOC soc,htsblk* retour);
LLint http_fread1(htsblk* r);
void treathead(t_cookie* cookie,char* adr,char* fil,htsblk* retour,char* rcvd);
void treatfirstline(htsblk* retour,char* rcvd);
#ifndef HTTRACK_DEFLIB
HTSEXT_API void infostatuscode(char* msg,int statuscode);
#endif

// sous-fonctions
htsblk xhttpget(httrackp *opt,char* adr,char* fil);
htsblk http_gethead(httrackp *opt,char* adr,char* fil);
LLint http_xfread1(htsblk* r,int bufl);
HTS_INLINE t_hostent* hts_gethostbyname(httrackp *opt,const char* iadr, void* v_buffer);
#ifndef HTTRACK_DEFLIB
HTSEXT_API t_hostent* vxgethostbyname(char* hostname, void* v_buffer);
#endif
t_hostent* _hts_ghbn(t_dnscache* cache,const char* iadr,t_hostent* retour);
int ftp_available(void);
#if HTS_DNSCACHE
void hts_cache_free(t_dnscache* cache);
int hts_dnstest(httrackp *opt, const char* _iadr);
t_dnscache* _hts_cache(httrackp *opt);
#endif

// outils divers
HTS_INLINE TStamp time_local(void);
#ifndef HTTRACK_DEFLIB
HTSEXT_API HTS_INLINE TStamp mtime_local(void);
#endif
void sec2str(char *s,TStamp t);
#ifndef HTTRACK_DEFLIB
HTSEXT_API void qsec2str(char *st,TStamp t);
#endif
void time_gmt_rfc822(char* s);
void time_local_rfc822(char* s);
struct tm* convert_time_rfc822(struct tm* buffer, const char* s);
int set_filetime(const char* file,struct tm* tm_time);
int set_filetime_rfc822(const char* file,const char* date);
int get_filetime_rfc822(const char* file,char* date);
HTS_INLINE void time_rfc822(char* s,struct tm * A);
HTS_INLINE void time_rfc822_local(char* s,struct tm * A);
#ifndef HTTRACK_DEFLIB
HTSEXT_API char* int2char(strc_int2bytes2* strc, int n);
HTSEXT_API char* int2bytes(strc_int2bytes2* strc, LLint n);
HTSEXT_API char* int2bytessec(strc_int2bytes2* strc, long int n);
HTSEXT_API char** int2bytes2(strc_int2bytes2* strc, LLint n);
#endif
HTS_INLINE int sendc(htsblk* r, const char* s);
int finput(int fd,char* s,int max);
int binput(char* buff,char* s,int max);
int linput(FILE* fp,char* s,int max);
int linputsoc(T_SOC soc, char* s, int max);
int linputsoc_t(T_SOC soc, char* s, int max, int timeout);
int linput_trim(FILE* fp,char* s,int max);
int linput_cpp(FILE* fp,char* s,int max);
void rawlinput(FILE* fp,char* s,int max);
char* strstrcase(char *s,char *o);
int ident_url_absolute(const char* url,char* adr,char* fil);
void fil_simplifie(char* f);
int is_unicode_utf8(const unsigned char* buffer, size_t size);
void map_characters(unsigned char* buffer, unsigned int size, unsigned int* map);
int ishtml(httrackp *opt,const char* urlfil);
int ishtml_ext(const char* a);
int ishttperror(int err);
void guess_httptype(httrackp *opt,char *s,const char *fil);
#ifndef HTTRACK_DEFLIB
HTSEXT_API void get_httptype(httrackp *opt,char *s,const char *fil,int flag);
#endif
int get_userhttptype(httrackp *opt,char *s,const char *fil);
void give_mimext(char *s,const char *st);
#ifndef HTTRACK_DEFLIB
HTSEXT_API int is_knowntype(httrackp *opt,const char *fil);
HTSEXT_API int is_userknowntype(httrackp *opt,const char *fil);
HTSEXT_API int is_dyntype(const char *fil);
HTSEXT_API char* get_ext(char *catbuff, const char *fil);
#endif
int may_unknown(httrackp *opt,const char* st);
int may_bogus_multiple(httrackp *opt, const char* mime, const char *filename);
int may_unknown2(httrackp *opt,const char* mime, const char *filename);
#ifndef HTTRACK_DEFLIB
HTSEXT_API char* jump_identification(const char*);
HTSEXT_API char* jump_normalized(const char*);
HTSEXT_API char* jump_toport(const char*);
HTSEXT_API char* fil_normalized(const char* source, char* dest);
HTSEXT_API char* adr_normalized(const char* source, char* dest);
#endif
char* strrchr_limit(const char* s, char c, const char* limit);
char* strstr_limit(const char* s, const char* sub, const char* limit);
HTS_INLINE char* jump_protocol(const char* source);
void code64(unsigned char* a,int size_a,unsigned char* b,int crlf);
#ifndef HTTRACK_DEFLIB
HTSEXT_API void unescape_amp(char* s);
HTSEXT_API void escape_spc_url(char* s);
HTSEXT_API void escape_in_url(char* s);
HTSEXT_API void escape_uri(char* s);
HTSEXT_API void escape_uri_utf(char* s);
HTSEXT_API void escape_check_url(char* s);
HTSEXT_API char* escape_check_url_addr(char *catbuff, const char* s);
HTSEXT_API void x_escape_http(char* s,int mode);
HTSEXT_API void x_escape_html(char* s);
HTSEXT_API void escape_remove_control(char* s);
HTSEXT_API void escape_for_html_print(char* s, char* d);
HTSEXT_API void escape_for_html_print_full(char* s, char* d);
#endif
#ifndef HTTRACK_DEFLIB
HTSEXT_API char* unescape_http(char *catbuff, const char* s);
HTSEXT_API char* unescape_http_unharm(char *catbuff, const char* s, int no_high);
HTSEXT_API char* antislash_unescaped(char *catbuff, const char* s);
HTSEXT_API char* concat(char *catbuff,const char* a,const char* b);
HTSEXT_API char* fconcat(char *catbuff, const char* a, const char* b);
HTSEXT_API char* fconv(char *catbuff, const char* a);
#endif
#define copychar(catbuff,a) concat(catbuff,(a),NULL)
char* fslash(char *catbuff, const char* a);
#ifndef HTTRACK_DEFLIB
HTSEXT_API int hts_log(httrackp *opt, const char* prefix, const char *msg);
#endif

char* convtolower(char *catbuff, const char* a);
void hts_lowcase(char* s);
void hts_replace(char *s,char from,char to);
int multipleStringMatch(const char *s, const char *match);


void fprintfio(FILE* fp,char* buff,char* prefix);

#ifdef _WIN32
#else
int sig_ignore_flag( int setflag );     // flag ignore
#endif

void cut_path(char* fullpath,char* path,char* pname);
int fexist(const char* s);
int fexist_utf8(const char* s);
/*LLint fsize(const char* s);    */
off_t fpsize(FILE* fp);
off_t fsize(const char* s);    
off_t fsize_utf8(const char* s);
/* root dir */
#ifndef HTTRACK_DEFLIB
HTSEXT_API char* hts_rootdir(char* file);
#endif

// Threads
typedef void* ( *beginthread_type )( void * );
/*unsigned long _beginthread( beginthread_type start_address, unsigned stack_size, void *arglist );*/

/* variables globales */
extern HTSEXT_API hts_stat_struct HTS_STAT;
extern int _DEBUG_HEAD;
extern FILE* ioinfo;

/* constantes */
extern const char* hts_mime_keep[];
extern const char* hts_mime[][2];
extern const char* hts_main_mime[];
extern const char* hts_detect[];
extern const char* hts_detectbeg[];
extern const char* hts_nodetect[];
extern const char* hts_detectURL[];
extern const char* hts_detectandleave[];
extern const char* hts_detect_js[];

// htsmodule.c definitions
extern void* openFunctionLib(const char* file_);
extern void* getFunctionPtr(void* handle, const char* fncname);
extern void  closeFunctionLib(void* handle);

extern void clearCallbacks(htscallbacks* chain);
extern size_t hts_get_callback_offs(const char *name);
int hts_set_callback(t_hts_htmlcheck_callbacks *callbacks, const char *name, void *function);
void *hts_get_callback(t_hts_htmlcheck_callbacks *callbacks, const char *name);

#define CBSTRUCT(OPT) ((t_hts_htmlcheck_callbacks*) ((OPT)->callbacks_fun))
#define GET_USERCALLBACK(OPT, NAME) ( CBSTRUCT(OPT)-> NAME .fun )
#define GET_USERARG(OPT, NAME) ( CBSTRUCT(OPT)-> NAME .carg )
#define GET_USERDEF(OPT, NAME) ( \
  (CBSTRUCT(OPT) != NULL && CBSTRUCT(OPT)-> NAME .fun != NULL) \
	? ( GET_USERARG(OPT, NAME) ) \
	: ( default_callbacks. NAME .carg ) \
)
#define GET_CALLBACK(OPT, NAME) ( \
  (CBSTRUCT(OPT) != NULL && CBSTRUCT(OPT)-> NAME .fun != NULL) \
	? ( GET_USERCALLBACK(OPT, NAME ) ) \
  : ( default_callbacks. NAME .fun ) \
)

/* Predefined macros */
#define RUN_CALLBACK_NOARG(OPT, NAME) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME))
#define RUN_CALLBACK0(OPT, NAME) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME), OPT)
#define RUN_CALLBACK1(OPT, NAME, ARG1) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME), OPT, ARG1)
#define RUN_CALLBACK2(OPT, NAME, ARG1, ARG2) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME), OPT, ARG1, ARG2)
#define RUN_CALLBACK3(OPT, NAME, ARG1, ARG2, ARG3) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME), OPT, ARG1, ARG2, ARG3)
#define RUN_CALLBACK4(OPT, NAME, ARG1, ARG2, ARG3, ARG4) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME), OPT, ARG1, ARG2, ARG3, ARG4)
#define RUN_CALLBACK5(OPT, NAME, ARG1, ARG2, ARG3, ARG4, ARG5) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME), OPT, ARG1, ARG2, ARG3, ARG4, ARG5)
#define RUN_CALLBACK6(OPT, NAME, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME), OPT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6)
#define RUN_CALLBACK7(OPT, NAME, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME), OPT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7)
#define RUN_CALLBACK8(OPT, NAME, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME), OPT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8)

/*
#define GET_CALLBACK(OPT, NAME, ARG) ( \
  ( \
    ( ARG ) = GET_USERDEF(OPT, NAME), \
    ( \
 	    (CBSTRUCT(OPT) != NULL && CBSTRUCT(OPT)-> NAME .fun != NULL) \
	    ? ( GET_USERCALLBACK(OPT, NAME ) ) \
      : ( default_callbacks. NAME .fun ) \
    ) \
  ) \
)
*/

/* UTF-8 aware FILE API */
#ifndef HTS_DEF_FILEAPI
#ifdef _WIN32
#define FOPEN hts_fopen_utf8
HTSEXT_API FILE* hts_fopen_utf8(const char *path, const char *mode);
#define STAT hts_stat_utf8
typedef struct _stat STRUCT_STAT;
HTSEXT_API int hts_stat_utf8(const char *path, STRUCT_STAT *buf);
#define UNLINK hts_unlink_utf8
HTSEXT_API int hts_unlink_utf8(const char *pathname);
#define RENAME hts_rename_utf8
HTSEXT_API int hts_rename_utf8(const char *oldpath, const char *newpath);
#define MKDIR(F) hts_mkdir_utf8(F)
HTSEXT_API int hts_mkdir_utf8(const char *pathname);
#define UTIME(A,B) hts_utime_utf8(A,B)
typedef struct _utimbuf STRUCT_UTIMBUF;
HTSEXT_API int hts_utime_utf8(const char *filename, const STRUCT_UTIMBUF *times);
#else
/* The underlying filesystem charset is supposed to be UTF-8 */
#define FOPEN fopen
#define STAT stat
typedef struct stat STRUCT_STAT;
#define UNLINK unlink
#define RENAME rename
#define MKDIR(F) mkdir(F, HTS_ACCESS_FOLDER)
typedef struct utimbuf STRUCT_UTIMBUF;
#define UTIME(A,B) utime(A,B)
#endif
#define HTS_DEF_FILEAPI
#endif

#endif    // internals

#undef PATH_SEPARATOR
#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

/* Spaces: CR,LF,TAB,FF */
#define  is_space(c)      ( ((c)==' ') || ((c)=='\"') || ((c)==10) || ((c)==13) || ((c)==9) || ((c)==12) || ((c)==11) || ((c)=='\'') )
#define  is_realspace(c)  ( ((c)==' ')                || ((c)==10) || ((c)==13) || ((c)==9) || ((c)==12) || ((c)==11)                )
#define  is_taborspace(c) ( ((c)==' ')                                          || ((c)==9)                             )
#define  is_quote(c)      (               ((c)=='\"')                                                    || ((c)=='\'') )
#define  is_retorsep(c)   (                              ((c)==10) || ((c)==13) || ((c)==9)                                          )
//HTS_INLINE int is_space(char);
//HTS_INLINE int is_realspace(char);

#define HTTP_IS_REDIRECT(code) ( \
     (code) == 301               \
  || (code) == 302               \
  || (code) == 303               \
  || (code) == 307               \
  )
#define HTTP_IS_NOTMODIFIED(code) ( \
     (code) == 304               \
  )
#define HTTP_IS_OK(code) ( ( (code) / 100 ) == 2 )
#define HTTP_IS_ERROR(code) ( !HTTP_IS_OK(code) && !HTTP_IS_REDIRECT(code) && !HTTP_IS_NOTMODIFIED(code) )

// compare le début de f avec s et retourne la position de la fin
// 'A=a' (case insensitive)
HTS_STATIC int strfield(const char* f,const char* s) {
  int r=0;
  while (streql(*f,*s) && ((*f)!=0) && ((*s)!=0)) { f++; s++; r++; }
  if (*s==0)
    return r;
  else
    return 0;
}
HTS_STATIC int strcmpnocase(char* a,char* b) {
  while(*a) {
    int cmp = hichar(*a) - hichar(*b);
    if (cmp != 0)
      return cmp;
    a++;
    b++;
  }
  return 0;
}

#ifdef _WIN32
#define strcasecmp(a,b) stricmp(a,b)
#define strncasecmp(a,b,n) strnicmp(a,b,n)
#endif

#define strfield2(f,s) ( (strlen(f)!=strlen(s)) ? 0 : (strfield(f,s)) )

// is this MIME an hypertext MIME (text/html), html/js-style or other script/text type?
#define HTS_HYPERTEXT_DEFAULT_MIME "text/html"

#if HTS_USEMMS
#define OPT_MMS(a) (strfield2((a), "video/x-ms-asf") != 0)
#else
#define OPT_MMS(a) (0)
#endif

#define is_html_mime_type(a) \
  ( (strfield2((a),"text/html")!=0)\
  || (strfield2((a),"application/xhtml+xml")!=0) \
  )
#define is_hypertext_mime__(a) \
  ( \
  is_html_mime_type(a)\
  || (strfield2((a),"application/x-javascript")!=0) \
  || (strfield2((a),"text/css")!=0) \
  /*|| (strfield2((a),"text/vnd.wap.wml")!=0)*/ \
  || (strfield2((a),"image/svg+xml")!=0) \
  || (strfield2((a),"image/svg-xml")!=0) \
  /*|| (strfield2((a),"audio/x-pn-realaudio")!=0) */\
  || (strfield2((a),"application/x-authorware-map")!=0) \
  )
#define may_be_hypertext_mime__(a) \
   (\
     (strfield2((a),"audio/x-pn-realaudio")!=0) \
     || (strfield2((a),"audio/x-mpegurl")!=0) \
     /*|| (strfield2((a),"text/xml")!=0) || (strfield2((a),"application/xml")!=0) : TODO: content check */ \
     || OPT_MMS(a) \
  )

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

// check if (mime, file) is hypertext
HTS_STATIC int is_hypertext_mime(httrackp *opt,const char* mime, const char* file) {
  if (is_hypertext_mime__(mime))
    return 1;
  if (may_unknown(opt,mime)) {
    char guessed[256];
    guessed[0] = '\0';
    guess_httptype(opt,guessed, file);
    return is_hypertext_mime__(guessed);
  }
  return 0;
}

// check if (mime, file) might be "false" hypertext
HTS_STATIC int may_be_hypertext_mime(httrackp *opt,const char* mime, const char* file) {
  if (may_be_hypertext_mime__(mime))
    return 1;
  if (file != NULL && file[0] != '\0' && may_unknown(opt,mime)) {
    char guessed[256];
    guessed[0] = '\0';
    guess_httptype(opt,guessed, file);
    return may_be_hypertext_mime__(guessed);
  }
  return 0;
}

// compare (mime, file) with reference
HTS_STATIC int compare_mime(httrackp *opt,const char* mime, const char* file, const char* reference) {
  if (is_hypertext_mime__(mime) || may_be_hypertext_mime__(mime))
    return strfield2(mime, reference);
  if (file != NULL && file[0] != '\0' && may_unknown(opt,mime)) {
    char guessed[256];
    guessed[0] = '\0';
    guess_httptype(opt,guessed, file);
    return strfield2(guessed, reference);
  }
  return 0;
}

#endif

#ifdef _WIN32_WCE_XXC
extern char cwd[MAX_PATH+1];
HTS_STATIC char *getcwd_ce(char *buffer, int maxlen) {
	TCHAR fileUnc[MAX_PATH+1];
	char* plast;
	
	if(cwd[0] == 0)
	{
		GetModuleFileName(NULL, fileUnc, MAX_PATH);
		WideCharToMultiByte(CP_ACP, 0, fileUnc, -1, cwd, MAX_PATH, NULL, NULL);
		plast = strrchr(cwd, '\\');
		if(plast)
			*plast = 0;
		/* Special trick to keep start menu clean... */
		if(_stricmp(cwd, "\\windows\\start menu") == 0)
			strcpy(cwd, "\\Apps");
	}
	if(buffer)
		strncpy(buffer, cwd, maxlen);
	return cwd;
}
#undef getcwd
#define getcwd getcwd_ce
#endif

/* dirent() compatibility */
#ifdef _WIN32
#define HTS_DIRENT_SIZE 256
struct dirent {
  ino_t          d_ino;       /* ignored */
  off_t          d_off;       /* ignored */
  unsigned short d_reclen;    /* ignored */
  unsigned char  d_type;      /* ignored */
  char           d_name[HTS_DIRENT_SIZE]; /* filename */
};
typedef struct DIR DIR;
struct DIR {
  HANDLE h;
  struct dirent entry;
  char *name;
};
DIR *opendir(const char *name);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);
#endif

#endif
