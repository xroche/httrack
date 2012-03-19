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
/* File: Subroutines .h                                         */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

// Fichier librairie .h

#ifndef HTS_DEFH
#define HTS_DEFH 

/* définitions globales */
#include "htsglobal.h"

/* basic net definitions */
#include "htsbase.h"
#include "htsbasenet.h"
#include "htsnet.h"

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

// structure pour paramètres supplémentaires lors de la requête
typedef struct htsrequest {
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
  t_proxy proxy;              // proxy
} htsrequest;


// structure pour retour d'une connexion/prise d'en tête
typedef struct htsblk {
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
} htsblk;


/* ANCIENNE STURCTURE pour cache 1.0 */
typedef struct {
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
  t_proxy proxy;   // ANCIENNE STURCTURE - proxy
  int user_agent_send;  // ANCIENNE STURCTURE - user agent (ex: httrack/1.0 [sun])
  char user_agent[64];
  int http11;           // ANCIENNE STURCTURE - l'en tête doit être signé HTTP/1.1 et non HTTP/1.0
} OLD_htsblk;
/* fin ANCIENNE STURCTURE pour cache 1.0 */

// cache pour le dns, pour éviter de faire des gethostbyname sans arrêt
typedef struct t_dnscache {
  char iadr[1024];
  struct t_dnscache* n;
  char host_addr[HTS_MAXADDRLEN];    // 4 octets (v4), ou 16 octets (v6)
  int host_length;                   // 4 normalement - ==0  alors en cours de résolution
                                     // ou >16 si sockaddr
                                     //                 ==-1 alors erreur (host n'éxiste pas)
} t_dnscache;



/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

// fonctions unix/winsock
int hts_read(htsblk* r,char* buff,int size);
//int HTS_TOTAL_RECV_CHECK(int var);
LLint check_downloadable_bytes(int rate);

#ifndef HTTRACK_DEFLIB
HTSEXT_API int hts_init(void);
HTSEXT_API int hts_uninit(void);
#endif

// fonctions principales
int http_fopen(char* adr,char* fil,htsblk* retour);
int http_xfopen(int mode,int treat,int waitconnect,char* xsend,char* adr,char* fil,htsblk* retour);
int http_sendhead(t_cookie* cookie,int mode,char* xsend,char* adr,char* fil,char* referer_adr,char* referer_fil,htsblk* retour);
htsblk httpget(char* url);
//int newhttp(char* iadr,char* err=NULL);
int newhttp(char* iadr,htsblk* retour,int port,int waitconnect);
HTS_INLINE void deletehttp(htsblk* r);
HTS_INLINE int  deleteaddr(htsblk* r);
HTS_INLINE void deletesoc(T_SOC soc);
HTS_INLINE void deletesoc_r(htsblk* r);
htsblk http_location(char* adr,char* fil,char* loc);
htsblk http_test(char* adr,char* fil,char* loc);
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
htsblk xhttpget(char* adr,char* fil);
htsblk http_gethead(char* adr,char* fil);
LLint http_xfread1(htsblk* r,int bufl);
HTS_INLINE t_hostent* hts_gethostbyname(char* iadr, void* v_buffer);
#ifndef HTTRACK_DEFLIB
HTSEXT_API t_hostent* vxgethostbyname(char* hostname, void* v_buffer);
#endif
t_hostent* _hts_ghbn(t_dnscache* cache,char* iadr,t_hostent* retour);
int ftp_available(void);
#if HTS_DNSCACHE
void hts_cache_free(t_dnscache* cache);
int hts_dnstest(char* _iadr);
t_dnscache* _hts_cache(void);
int _hts_lockdns(int i);
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
struct tm* convert_time_rfc822(char* s);
int set_filetime(char* file,struct tm* tm_time);
int set_filetime_rfc822(char* file,char* date);
int get_filetime_rfc822(char* file,char* date);
HTS_INLINE void time_rfc822(char* s,struct tm * A);
HTS_INLINE void time_rfc822_local(char* s,struct tm * A);
#ifndef HTTRACK_DEFLIB
HTSEXT_API char* int2char(int n);
HTSEXT_API char* int2bytes(LLint n);
HTSEXT_API char* int2bytessec(long int n);
HTSEXT_API char** int2bytes2(LLint n);
#endif
HTS_INLINE int sendc(htsblk* r, char* s);
int finput(int fd,char* s,int max);
int binput(char* buff,char* s,int max);
int linput(FILE* fp,char* s,int max);
int linputsoc(T_SOC soc, char* s, int max);
int linputsoc_t(T_SOC soc, char* s, int max, int timeout);
int linput_trim(FILE* fp,char* s,int max);
int linput_cpp(FILE* fp,char* s,int max);
void rawlinput(FILE* fp,char* s,int max);
char* strstrcase(char *s,char *o);
int ident_url_absolute(char* url,char* adr,char* fil);
void fil_simplifie(char* f);
int is_unicode_utf8(unsigned char* buffer, unsigned int size);
void map_characters(unsigned char* buffer, unsigned int size, unsigned int* map);
int ishtml(const char* urlfil);
int ishtml_ext(const char* a);
int ishttperror(int err);
void guess_httptype(char *s,const char *fil);
void get_httptype(char *s,const char *fil,int flag);
int get_userhttptype(int setdefs,char *s,const char *ext);
void give_mimext(char *s,char *st);
int is_knowntype(const char *fil);
int is_userknowntype(const char *fil);
int is_dyntype(const char *fil);
char* get_ext(const char *fil);
int may_unknown(const char* st);
#ifndef HTTRACK_DEFLIB
HTSEXT_API char* jump_identification(char*);
HTSEXT_API char* jump_normalized(char*);
HTSEXT_API char* jump_toport(char*);
HTSEXT_API char* fil_normalized(char* source, char* dest);
HTSEXT_API char* adr_normalized(char* source, char* dest);
#endif
char* strrchr_limit(char* s, char c, char* limit);
char* strstr_limit(char* s, char* sub, char* limit);
HTS_INLINE char* jump_protocol(char* source);
void code64(unsigned char* a,int size_a,unsigned char* b,int crlf);
#ifndef HTTRACK_DEFLIB
HTSEXT_API void unescape_amp(char* s);
HTSEXT_API void escape_spc_url(char* s);
HTSEXT_API void escape_in_url(char* s);
HTSEXT_API void escape_uri(char* s);
HTSEXT_API void escape_uri_utf(char* s);
HTSEXT_API void escape_check_url(char* s);
HTSEXT_API char* escape_check_url_addr(char* s);
HTSEXT_API void x_escape_http(char* s,int mode);
HTSEXT_API void x_escape_html(char* s);
HTSEXT_API void escape_remove_control(char* s);
HTSEXT_API void escape_for_html_print(char* s, char* d);
HTSEXT_API void escape_for_html_print_full(char* s, char* d);
#endif
#ifndef HTTRACK_DEFLIB
HTSEXT_API char* unescape_http(char* s);
HTSEXT_API char* unescape_http_unharm(char* s, int no_high);
HTSEXT_API char* antislash_unescaped(char* s);
#endif
char* concat(const char* a,const char* b);
#define copychar(a) concat((a),NULL)
#if HTS_DOSNAME
char* fconcat(char* a,char* b);
char* fconv(char* a);
#else
#define fconv(a) (a)
#define fconcat(a,b) concat(a,b)
#endif
char* fslash(char* a);
char* __fslash(char* a);

char* convtolower(char* a);
char* concat(const char* a,const char* b);
void hts_lowcase(char* s);
void hts_replace(char *s,char from,char to);


void fprintfio(FILE* fp,char* buff,char* prefix);

#if HTS_WIN
#else
int sig_ignore_flag( int setflag );     // flag ignore
#endif

void cut_path(char* fullpath,char* path,char* pname);
int fexist(char* s);
/*LLint fsize(char* s);    */
INTsys fpsize(FILE* fp);
INTsys fsize(char* s);    
/* root dir */
#ifndef HTTRACK_DEFLIB
HTSEXT_API char* hts_rootdir(char* file);
#endif

// Threads
#if USE_PTHREAD
typedef void* ( *beginthread_type )( void * );
unsigned long _beginthread( beginthread_type start_address, unsigned stack_size, void *arglist );
#endif




/* variables globales */
//extern LLint HTS_TOTAL_RECV;  // flux entrant reçu
//extern int HTS_TOTAL_RECV_STATE;  // status: 0 tout va bien 1: ralentir un peu 2: ralentir 3: beaucoup
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

// defaut wrappers
void  __cdecl htsdefault_init(void);
void  __cdecl htsdefault_uninit(void);
int   __cdecl htsdefault_start(void* opt);
int   __cdecl htsdefault_chopt(void* opt);
int   __cdecl htsdefault_end(void);
int   __cdecl htsdefault_preprocesshtml(char** html,int* len,char* url_adresse,char* url_fichier);
int   __cdecl htsdefault_postprocesshtml(char** html,int* len,char* url_adresse,char* url_fichier);
int   __cdecl htsdefault_checkhtml(char* html,int len,char* url_adresse,char* url_fichier);
int   __cdecl htsdefault_loop(void* back,int back_max,int back_index,int lien_n,int lien_tot,int stat_time,hts_stat_struct* stats);
char* __cdecl htsdefault_query(char* question);
char* __cdecl htsdefault_query2(char* question);
char* __cdecl htsdefault_query3(char* question);
int   __cdecl htsdefault_check(char* adr,char* fil,int status);
void  __cdecl htsdefault_pause(char* lockfile);
void  __cdecl htsdefault_filesave(char*);
int   __cdecl htsdefault_linkdetected(char* link);
int   __cdecl htsdefault_linkdetected2(char* link, char* tag_start);
int   __cdecl htsdefault_xfrstatus(void* back);
int   __cdecl htsdefault_savename(char* adr_complete,char* fil_complete,char* referer_adr,char* referer_fil,char* save);
int   __cdecl htsdefault_sendheader(char* buff, char* adr, char* fil, char* referer_adr, char* referer_fil, htsblk* outgoing);
int   __cdecl htsdefault_receiveheader(char* buff, char* adr, char* fil, char* referer_adr, char* referer_fil, htsblk* incoming);

// end defaut wrappers


// htsmodule.c definitions
extern void* getFunctionPtr(httrackp* opt, char* file, char* fncname);
extern void clearCallbacks(htscallbacks* chain);



#endif    // internals


/* Spaces: CR,LF,TAB,FF */
#define  is_space(c)      ( ((c)==' ') || ((c)=='\"') || ((c)==10) || ((c)==13) || ((c)==9) || ((c)==12) || ((c)==11) || ((c)=='\'') )
#define  is_realspace(c)  ( ((c)==' ')                || ((c)==10) || ((c)==13) || ((c)==9) || ((c)==12) || ((c)==11)                )
#define  is_taborspace(c) ( ((c)==' ')                                          || ((c)==9)                             )
#define  is_quote(c)      (               ((c)=='\"')                                                    || ((c)=='\'') )
#define  is_retorsep(c)   (                              ((c)==10) || ((c)==13) || ((c)==9)                                          )
//HTS_INLINE int is_space(char);
//HTS_INLINE int is_realspace(char);

// compare le début de f avec s et retourne la position de la fin
// 'A=a' (case insensitive)
static int strfield(const char* f,const char* s) {
  int r=0;
  while (streql(*f,*s) && ((*f)!=0) && ((*s)!=0)) { f++; s++; r++; }
  if (*s==0)
    return r;
  else
    return 0;
}
static int strcmpnocase(char* a,char* b) {
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
#define is_hypertext_mime__(a) \
  ( (strfield2((a),"text/html")!=0)\
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
  )


/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

// check if (mime, file) is hypertext
static int is_hypertext_mime(const char* mime, const char* file) {
  if (is_hypertext_mime__(mime))
    return 1;
  if (may_unknown(mime)) {
    char guessed[256];
    guessed[0] = '\0';
    guess_httptype(guessed, file);
    return is_hypertext_mime__(guessed);
  }
  return 0;
}

// check if (mime, file) might be "false" hypertext
static int may_be_hypertext_mime(const char* mime, const char* file) {
  if (may_be_hypertext_mime__(mime))
    return 1;
  if (file != NULL && file[0] != '\0' && may_unknown(mime)) {
    char guessed[256];
    guessed[0] = '\0';
    guess_httptype(guessed, file);
    return may_be_hypertext_mime__(guessed);
  }
  return 0;
}

// compare (mime, file) with reference
static int compare_mime(const char* mime, const char* file, const char* reference) {
  if (is_hypertext_mime__(mime) || may_be_hypertext_mime__(mime))
    return strfield2(mime, reference);
  if (file != NULL && file[0] != '\0' && may_unknown(mime)) {
    char guessed[256];
    guessed[0] = '\0';
    guess_httptype(guessed, file);
    return strfield2(guessed, reference);
  }
  return 0;
}

#endif

#ifdef _WIN32_WCE_XXC
extern char cwd[MAX_PATH+1];
static char *getcwd_ce(char *buffer, int maxlen)
{
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

#endif


