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
/* File: HTTrack parameters block                               */
/*       Called by httrack.h and some other files               */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTTRACK_DEFOPT
#define HTTRACK_DEFOPT

#include <stdio.h>

#include "htsglobal.h"
#include "htsnet.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_t_hts_htmlcheck_callbacks
#define HTS_DEF_FWSTRUCT_t_hts_htmlcheck_callbacks
typedef struct t_hts_htmlcheck_callbacks t_hts_htmlcheck_callbacks;
#endif
#ifndef HTS_DEF_FWSTRUCT_t_dnscache
#define HTS_DEF_FWSTRUCT_t_dnscache
typedef struct t_dnscache t_dnscache;
#endif
#ifndef HTS_DEF_FWSTRUCT_hash_struct
#define HTS_DEF_FWSTRUCT_hash_struct
typedef struct hash_struct hash_struct;
#endif
#ifndef HTS_DEF_FWSTRUCT_robots_wizard
#define HTS_DEF_FWSTRUCT_robots_wizard
typedef struct robots_wizard robots_wizard;
#endif
#ifndef HTS_DEF_FWSTRUCT_t_cookie
#define HTS_DEF_FWSTRUCT_t_cookie
typedef struct t_cookie t_cookie;
#endif

/** Forward definitions **/
#ifndef HTS_DEF_FWSTRUCT_String
#define HTS_DEF_FWSTRUCT_String
typedef struct String String;
#endif
#ifndef HTS_DEF_STRUCT_String
#define HTS_DEF_STRUCT_String

struct String {
  char *buffer_;
  size_t length_;
  size_t capacity_;
};
#endif

/* Defines */
#define CATBUFF_SIZE (STRING_SIZE * 2 * 2)

#define STRING_SIZE 2048

/* Proxy configuration. */
#ifndef HTS_DEF_FWSTRUCT_t_proxy
#define HTS_DEF_FWSTRUCT_t_proxy
typedef struct t_proxy t_proxy;
#endif
struct t_proxy {
  int active;      /**< nonzero if a proxy is configured */
  String name;     /**< proxy host name */
  int port;        /**< proxy port */
  String bindhost; /**< local address to bind the outgoing socket to */
};

/* Bundle of filter pointers, kept together for bulk copy. */
#ifndef HTS_DEF_FWSTRUCT_htsfilters
#define HTS_DEF_FWSTRUCT_htsfilters
typedef struct htsfilters htsfilters;
#endif
struct htsfilters {
  char ***filters; /**< pointer to the +/-pattern filter array */
  int *filptr;     /**< pointer to the current filter count */
};

/* User callbacks chain */
typedef int (*htscallbacksfncptr)(void);

typedef struct htscallbacks htscallbacks;

struct htscallbacks {
  void *moduleHandle; /**< handle of the module that registered the callback */
  htscallbacksfncptr exitFnc; /**< function to run on engine exit */
  htscallbacks *next;         /**< next entry in the callback chain */
};

/* filenote() internal file structure */
#ifndef HTS_DEF_FWSTRUCT_filenote_strc
#define HTS_DEF_FWSTRUCT_filenote_strc
typedef struct filenote_strc filenote_strc;
#endif
struct filenote_strc {
  FILE *lst;
  char path[STRING_SIZE * 2];
};

/* concat() functions */
#ifndef HTS_DEF_FWSTRUCT_concat_strc
#define HTS_DEF_FWSTRUCT_concat_strc
typedef struct concat_strc concat_strc;
#endif
struct concat_strc {
  int index;
  char buff[16][STRING_SIZE * 2 * 2];
};

/* int2 functions */
#ifndef HTS_DEF_FWSTRUCT_strc_int2bytes2
#define HTS_DEF_FWSTRUCT_strc_int2bytes2
typedef struct strc_int2bytes2 strc_int2bytes2;
#endif
struct strc_int2bytes2 {
  char catbuff[CATBUFF_SIZE];
  char buff1[256];
  char buff2[32];
  char *buffadr[2];
};

/* cmd callback */
#ifndef HTS_DEF_FWSTRUCT_usercommand_strc
#define HTS_DEF_FWSTRUCT_usercommand_strc
typedef struct usercommand_strc usercommand_strc;
#endif
struct usercommand_strc {
  int exe;
  char cmd[2048];
};

/* error logging */
#ifndef HTS_DEF_FWSTRUCT_fspc_strc
#define HTS_DEF_FWSTRUCT_fspc_strc
typedef struct fspc_strc fspc_strc;
#endif
struct fspc_strc {
  int error;
  int warning;
  int info;
};

/* lien_url */
#ifndef HTS_DEF_FWSTRUCT_lien_url
#define HTS_DEF_FWSTRUCT_lien_url
typedef struct lien_url lien_url;
#endif

#ifndef HTS_DEF_DEFSTRUCT_hts_log_type
#define HTS_DEF_DEFSTRUCT_hts_log_type

typedef enum hts_log_type {
  LOG_PANIC,
  LOG_ERROR,
  LOG_WARNING,
  LOG_NOTICE,
  LOG_INFO,
  LOG_DEBUG,
  LOG_TRACE,
  LOG_ERRNO = 1 << 8
} hts_log_type;
#endif

/* Mirror cancellation list node. */
#ifndef HTS_DEF_FWSTRUCT_htsoptstatecancel
#define HTS_DEF_FWSTRUCT_htsoptstatecancel
typedef struct htsoptstatecancel htsoptstatecancel;
#endif
struct htsoptstatecancel {
  char *url;               /**< URL flagged to be cancelled */
  htsoptstatecancel *next; /**< next cancellation entry */
};

/* Mutexes */
#ifndef HTS_DEF_FWSTRUCT_htsmutex_s
#define HTS_DEF_FWSTRUCT_htsmutex_s
typedef struct htsmutex_s htsmutex_s, *htsmutex;
#endif

/* Hashtables */
#ifndef HTS_DEF_FWSTRUCT_struct_coucal
#define HTS_DEF_FWSTRUCT_struct_coucal
typedef struct struct_coucal struct_coucal, *coucal;
#endif

/* Mirror runtime state (mutable engine state, not user options). */
#ifndef HTS_DEF_FWSTRUCT_htsoptstate
#define HTS_DEF_FWSTRUCT_htsoptstate
typedef struct htsoptstate htsoptstate;
#endif
struct htsoptstate {
  htsmutex lock; /**< guards this state block */
  /* */
  int stop; /**< set to request the mirror to stop */
  int exit_xh;
  int back_add_stats;
  /* */
  int mimehtml_created; /**< MIME/MHTML output already started */
  String mimemid;       /**< MIME multipart boundary id */
  FILE *mimefp;         /**< MIME/MHTML output file */
  int delayedId;        /**< counter for delayed-type-check ids */
  /* */
  filenote_strc strc; /**< filenote() listing state */
  /* Per-call function contexts (thread-local scratch, avoids globals) */
  htscallbacks callbacks;   /**< user callback chain head */
  concat_strc concat;       /**< concat() rotating buffers */
  usercommand_strc usercmd; /**< pending user shell command */
  fspc_strc fspc;           /**< error/warning/info counters */
  char *userhttptype;
  int verif_backblue_done; /**< backblue.gif/fade.gif already emitted */
  int verif_external_status;
  coucal dns_cache; /**< DNS resolution cache: hostname -> t_dnscache record */
  int dns_cache_nthreads; /**< number of in-flight DNS resolver threads */
  /* HTML parsing state */
  char _hts_errmsg[HTS_CDLMAXSIZE + 256]; /**< last engine error message */
  int _hts_in_html_parsing;
  int _hts_in_html_done;
  int _hts_in_html_poll;
  int _hts_setpause;
  int _hts_in_mirror; /**< nonzero while a mirror is running */
  char **_hts_addurl; /**< extra URLs to inject at runtime */
  int _hts_cancel;
  htsoptstatecancel *cancel; /**< list of URLs flagged for cancellation */
  char HTbuff[2048];
  unsigned int debug_state;
  unsigned int tmpnameid; /**< counter for temporary file names */
  int is_ended;           /**< mirror has finished */
};

/* Library handles */
#ifndef HTS_DEF_FWSTRUCT_htslibhandles
#define HTS_DEF_FWSTRUCT_htslibhandles
typedef struct htslibhandles htslibhandles;
#endif
#ifndef HTS_DEF_FWSTRUCT_htslibhandle
#define HTS_DEF_FWSTRUCT_htslibhandle
typedef struct htslibhandle htslibhandle;
#endif
struct htslibhandle {
  char *moduleName; /**< name of a loaded external module */
  void *handle;     /**< dlopen() handle for it */
};

struct htslibhandles {
  int count;             /**< number of loaded module handles */
  htslibhandle *handles; /**< array of loaded module handles */
};

/* Javascript parser flags */
typedef enum htsparsejava_flags {
  HTSPARSE_NONE = 0,          // don't parse
  HTSPARSE_DEFAULT = 1,       // parse default (all)
  HTSPARSE_NO_CLASS = 2,      // don't parse .java
  HTSPARSE_NO_JAVASCRIPT = 4, // don't parse .js
  HTSPARSE_NO_AGGRESSIVE = 8  // don't aggressively parse .js or .java
} htsparsejava_flags;

/* Link-rewriting style for saved pages (opt->urlmode). */
#ifndef HTS_DEF_DEFSTRUCT_hts_urlmode
#define HTS_DEF_DEFSTRUCT_hts_urlmode

typedef enum hts_urlmode {
  HTS_URLMODE_ABSOLUTE = 0, /**< absolute URL (http://host/path) everywhere */
  HTS_URLMODE_ABSOLUTE_FILE = 1, /**< legacy file: form, unused */
  HTS_URLMODE_RELATIVE = 2,      /**< relative link (default) */
  HTS_URLMODE_ABSOLUTE_URI = 3,  /**< absolute URI from root (/path) */
  HTS_URLMODE_KEEP_ORIGINAL = 4, /**< keep the original link, do not rewrite */
  HTS_URLMODE_TRANSPARENT_PROXY = 5 /**< transparent-proxy URL */
} hts_urlmode;
#endif

/* Cache policy for updates and retries (opt->cache). */
#ifndef HTS_DEF_DEFSTRUCT_hts_cachemode
#define HTS_DEF_DEFSTRUCT_hts_cachemode

typedef enum hts_cachemode {
  HTS_CACHE_NONE = 0,       /**< no cache */
  HTS_CACHE_PRIORITY = 1,   /**< cache takes priority over the network */
  HTS_CACHE_TEST_UPDATE = 2 /**< check for update before reuse (default) */
} hts_cachemode;
#endif

/* Interactive wizard level (opt->wizard). */
#ifndef HTS_DEF_DEFSTRUCT_hts_wizard
#define HTS_DEF_DEFSTRUCT_hts_wizard

typedef enum hts_wizard {
  HTS_WIZARD_NONE = 0, /**< no wizard */
  HTS_WIZARD_ASK = 1,  /**< wizard asks questions */
  HTS_WIZARD_AUTO = 2  /**< wizard runs without asking */
} hts_wizard;
#endif

/* robots.txt / meta-robots obedience level (opt->robots). */
#ifndef HTS_DEF_DEFSTRUCT_hts_robots
#define HTS_DEF_DEFSTRUCT_hts_robots

typedef enum hts_robots {
  HTS_ROBOTS_NEVER = 0,        /**< ignore robots rules */
  HTS_ROBOTS_SOMETIMES = 1,    /**< partial obedience (default) */
  HTS_ROBOTS_ALWAYS = 2,       /**< obey robots rules */
  HTS_ROBOTS_ALWAYS_STRICT = 3 /**< obey even strict rules */
} hts_robots;
#endif

/* What to fetch (opt->getmode bitmask). */
typedef enum hts_getmode {
  HTS_GETMODE_HTML = 1 << 0,      /**< save HTML files */
  HTS_GETMODE_NONHTML = 1 << 1,   /**< save non-HTML files */
  HTS_GETMODE_HTML_FIRST = 1 << 2 /**< fetch HTML first, then the other files */
} hts_getmode;

/* Allowed directions in the directory tree (opt->seeker bitmask). */
typedef enum hts_seeker {
  HTS_SEEKER_DOWN = 1 << 0, /**< may descend into subdirectories */
  HTS_SEEKER_UP = 1 << 1    /**< may ascend to parent directories */
} hts_seeker;

/* opt->travel: link-following scope in the low byte, flags OR'd in above it. */
typedef enum hts_travel_scope {
  HTS_TRAVEL_SAME_ADDRESS = 0, /**< stay on the same address (host) */
  HTS_TRAVEL_SAME_DOMAIN = 1,  /**< stay on the same principal domain */
  HTS_TRAVEL_SAME_TLD = 2,     /**< stay on the same TLD (e.g. .com) */
  HTS_TRAVEL_EVERYWHERE = 7,   /**< follow links anywhere on the web */
  HTS_TRAVEL_TEST_ALL = 1 << 8 /**< also test forbidden URLs (-t) */
} hts_travel_scope;

/* Mask selecting the scope value out of opt->travel. */
#define HTS_TRAVEL_SCOPE_MASK 0xff

/* Text progress display detail (opt->verbosedisplay). */
typedef enum hts_verbosedisplay {
  HTS_VERBOSE_NONE = 0,   /**< no animated progress display (default) */
  HTS_VERBOSE_SIMPLE = 1, /**< minimal single-line progress */
  HTS_VERBOSE_FULL = 2    /**< full animated progress */
} hts_verbosedisplay;

/* Delayed file-type resolution policy (opt->savename_delayed). */
typedef enum hts_savename_delayed {
  HTS_SAVENAME_DELAYED_NONE = 0, /**< resolve the type immediately */
  HTS_SAVENAME_DELAYED_SOFT = 1, /**< delay the type check when unknown */
  HTS_SAVENAME_DELAYED_HARD = 2  /**< always delay the type check (default) */
} hts_savename_delayed;

/* Saved-name length layout (opt->savename_83). */
typedef enum hts_savename_83 {
  HTS_SAVENAME_83_LONG = 0,   /**< long file names (default) */
  HTS_SAVENAME_83_DOS = 1,    /**< DOS 8.3 names (ISO9660 level 1) */
  HTS_SAVENAME_83_ISO9660 = 2 /**< ISO9660 level 2 names (up to 31 chars) */
} hts_savename_83;

/* Host-banning triggers (opt->hostcontrol bitmask). */
typedef enum hts_hostcontrol {
  HTS_HOSTCONTROL_BAN_TIMEOUT = 1 << 0, /**< ban a timing-out host */
  HTS_HOSTCONTROL_BAN_SLOW = 1 << 1     /**< ban a too-slow host */
} hts_hostcontrol;

#ifndef HTS_DEF_FWSTRUCT_lien_buffers
#define HTS_DEF_FWSTRUCT_lien_buffers
typedef struct lien_buffers lien_buffers;
#endif

/*
 * Per-mirror options and state block. This is the central HTTrack parameters
 * structure: created by hts_create_opt(), it carries every tunable option for
 * one mirror and embeds the live engine state, and is then consumed by
 * hts_main2().
 *
 * Callers normally configure it through the command-line argv vector (the
 * option parser), not by writing fields directly. The only fields real
 * consumers poke directly are 'log' and 'errlog' (set either to NULL to
 * silence logging).
 */
#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif
struct httrackp {
  size_t size_httrackp; /**< size of this structure (version/ABI guard) */
  /* */
  hts_wizard wizard; /**< interactive wizard level (none/ask/auto) */
  hts_boolean flush; /**< fflush() log files after each write */
  int travel;        /**< link-following scope (same domain, etc.) */
  int seeker;        /**< allowed direction: go up and/or down the tree */
  int depth;         /**< maximum recursion depth (-rN) */
  int extdepth;      /**< maximum recursion depth outside the start domain */
  hts_urlmode
      urlmode; /**< saved-link rewriting style (relative, absolute, etc.) */
  hts_boolean no_type_change; // do not change file type according to MIME
  hts_log_type debug;         /**< debug logging level */
  int getmode;                /**< what to fetch (HTML, images, ...) bitmask */
  FILE *log;                  /**< informational log stream; NULL mutes it */
  FILE *errlog;               /**< error log stream; NULL mutes it */
  LLint maxsite;              /**< max total bytes for the whole mirror */
  LLint maxfile_nonhtml;      /**< max bytes per non-HTML file */
  LLint maxfile_html;         /**< max bytes per HTML file */
  int maxsoc;                 /**< max simultaneous sockets (-cN) */
  LLint fragment;             /**< split site after this many bytes */
  hts_tristate
      nearlink; /**< also fetch images/data adjacent to a page but off-site */
  hts_boolean makeindex;  /**< build a top-level index.html */
  hts_boolean kindex;     /**< build a keyword index */
  hts_tristate delete_old; /**< delete locally obsolete files after update */
  int timeout;            /**< connection timeout in seconds */
  int rateout;            /**< minimum transfer rate (bytes/s) before abort */
  int maxtime;            /**< max total mirror duration in seconds */
  int maxrate;            /**< max transfer rate cap (bytes/s) */
  float maxconn;          /**< max connections per second */
  int waittime;           /**< scheduled start time (wall-clock seconds) */
  hts_cachemode cache;    /**< cache generation mode */
  hts_boolean shell; /**< driven by a shell over stdin/stdout pipes */
  t_proxy proxy;     /**< proxy configuration */
  hts_savename_83
      savename_83;   /**< saved-name length layout (long/DOS/ISO9660) */
  int savename_type; /**< saved-name layout (original tree, flat, ...) */
  String
      savename_userdef; /**< user-defined name template (e.g. %h%p/%n%q.%t) */
  hts_savename_delayed savename_delayed; /**< delayed type-check policy */
  hts_boolean
      delayed_cached;   // delayed type check can be cached to speedup updates
  hts_boolean mimehtml; /**< produce a single MIME/MHTML archive */
  hts_boolean user_agent_send; /**< send a User-Agent header */
  String user_agent;           /**< User-Agent value (e.g. httrack/1.0) */
  String referer;              /**< Referer value to send */
  String from;                 /**< From value to send */
  String path_log;             /**< directory for cache and logs */
  String path_html;            /**< output directory for the mirror */
  String path_html_utf8; /**< output directory for the mirror, UTF-8 form */
  String path_bin;       /**< directory for HTML templates */
  int retry;             /**< extra retries on a failed transfer */
  hts_boolean makestat;  /**< maintain a transfer-statistics log */
  hts_boolean maketrack; /**< maintain an operations-statistics log */
  int parsejava;         /**< Java/JS parsing mode; see htsparsejava_flags */
  int hostcontrol; /**< ban slow/timing-out hosts; see hts_hostcontrol bits */
  hts_tristate errpage; /**< generate an error page on 404 and similar */
  hts_boolean
      check_type; /**< probe unknown-type links (cgi/asp/dir) and follow moves
                   */
  hts_boolean all_in_cache;      /**< keep all retrieved data in the cache */
  hts_robots robots;             /**< robots.txt handling level */
  hts_tristate external;         /**< render external links as error pages */
  hts_boolean passprivacy;       /**< strip passwords from external links */
  hts_boolean includequery;      /**< include the query string in saved names */
  hts_boolean mirror_first_page; /**< only mirror the links of the first page */
  String sys_com;                /**< system command to run */
  hts_boolean sys_com_exec;      /**< actually execute sys_com */
  hts_boolean accept_cookie;     /**< accept and send cookies */
  t_cookie *cookie;              /**< cookie store */
  hts_boolean http10;            /**< force HTTP/1.0 */
  hts_boolean nokeepalive;       /**< disable keep-alive */
  hts_boolean nocompression;     /**< disable content compression */
  hts_boolean sizehack;          /**< treat same-size response as "updated" */
  hts_boolean urlhack;           // force "url normalization" to avoid loops
  hts_boolean tolerant;          /**< accept an incorrect Content-Length */
  hts_tristate
      parseall; /**< parse aggressively, including unknown tags with links */
  hts_boolean parsedebug; /**< parser debug mode */
  hts_boolean norecatch;  /**< do not re-fetch files the user deleted locally */
  hts_verbosedisplay verbosedisplay; /**< animated text progress display */
  String footer; /**< footer/info line injected into pages */
  int maxcache;  /**< in-memory cache backing limit (bytes) */
  hts_boolean ftp_proxy;    /**< use the HTTP proxy for FTP too */
  String filelist;          /**< file listing URLs to include */
  String urllist;           /**< file listing filters to include */
  htsfilters filters;       /**< filter pointers (+/-pattern rules) */
  hash_struct *hash;        // hash structure
  lien_url **liens;         // links
  int lien_tot;             // top index of "links" heap (always out-of-range)
  lien_buffers *liensbuf;   // links buffers
  robots_wizard *robotsptr; // robots ptr
  String lang_iso;          /**< Accept-Language value (en, fr, ...) */
  String accept;            // Accept:
  String headers;           // Additional headers
  String mimedefs;          // ext1=mimetype1\next2=mimetype2..
  String mod_blacklist;     /**< blacklisted modules */
  hts_boolean convert_utf8; // filenames UTF-8 conversion (3.46)
  //
  int maxlink;   /**< max number of links */
  int maxfilter; /**< max number of filters */
  //
  const char *exec; /**< path of the running executable */
  //
  hts_boolean quiet;                 /**< suppress non-wizard questions */
  hts_boolean keyboard;              /**< poll stdin for keyboard input */
  hts_boolean bypass_limits;         // bypass built-in limits
  hts_boolean background_on_suspend; // background process on suspend signal
  //
  hts_boolean is_update; /**< this run is an update (show "File updated...") */
  hts_boolean dir_topindex; /**< rebuild the top index afterwards */
  //
  // callbacks
  t_hts_htmlcheck_callbacks
      *callbacks_fun; /**< user HTML/parsing callback table */
  // store library handles
  htslibhandles libHandles; /**< loaded external module handles */
  //
  htsoptstate state; /**< embedded live engine state */
  String strip_query; /**< query keys to drop when deduping URLs (-strip-query);
                           appended at the tail to keep field offsets stable */
  hts_boolean
      no_www_dedup; /**< with urlhack, keep www.host distinct from host */
  hts_boolean no_slash_dedup; /**< with urlhack, keep redundant // in paths */
  hts_boolean no_query_dedup; /**< with urlhack, keep query-argument order */
  String cookies_file;        /**< extra Netscape cookies.txt to preload
                                 (--cookies-file) */
  int pause_min_ms; /**< inter-file pause lower bound, ms (0=off, #185) */
  int pause_max_ms; /**< inter-file pause upper bound, ms */
};

/* Running statistics for a mirror. */
#ifndef HTS_DEF_FWSTRUCT_hts_stat_struct
#define HTS_DEF_FWSTRUCT_hts_stat_struct
typedef struct hts_stat_struct hts_stat_struct;
#endif
struct hts_stat_struct {
  LLint HTS_TOTAL_RECV; /**< total bytes received from the network */
  LLint stat_bytes;     /**< total bytes written to disk */
  TStamp stat_timestart; /**< mirror start time */
  //
  LLint total_packed;    /**< compressed bytes received (on the wire) */
  LLint total_unpacked;  /**< bytes after decompression */
  int total_packedfiles; /**< number of compressed files */
  //
  TStamp
      istat_timestart[2]; /**< window start times for the instantaneous rate */
  LLint istat_bytes[2];   /**< window byte counts for the instantaneous rate */
  TStamp
      istat_reference01; /**< reference timestamp handed from window #0 to #1 */
  int istat_idlasttimer; /**< id of the timer that last produced a stat */
  //
  int stat_files;         /**< number of files written */
  int stat_updated_files; /**< number of files updated */
  int stat_background;    /**< number of files written in the background */
  //
  int stat_nrequests;    /**< number of requests issued on sockets */
  int stat_sockid;       /**< total number of sockets ever allocated */
  int stat_nsocket;      /**< current number of open sockets */
  int stat_errors;       /**< number of errors */
  int stat_errors_front; /**< errors at the very first level */
  int stat_warnings;     /**< number of warnings */
  int stat_infos;        /**< number of info messages */
  int nbk;               /**< background-anticipated files now completed */
  LLint nb;              /**< bytes currently being transferred (estimate) */
  //
  LLint rate; /**< current transfer rate */
  //
  TStamp last_connect; /**< time of the last connect() call */
  TStamp last_request; /**< time of the last request issued */
};

/* Extra per-request parameters (mirrors httrackp request options). */
#ifndef HTS_DEF_FWSTRUCT_htsrequest_proxy
#define HTS_DEF_FWSTRUCT_htsrequest_proxy
typedef struct htsrequest_proxy htsrequest_proxy;
#endif
struct htsrequest_proxy {
  int active;           /**< nonzero if a proxy is used for this request */
  const char *name;     /**< proxy host name */
  int port;             /**< proxy port */
  const char *bindhost; /**< local address to bind the outgoing socket to */
};

#ifndef HTS_DEF_FWSTRUCT_htsrequest
#define HTS_DEF_FWSTRUCT_htsrequest
typedef struct htsrequest htsrequest;
#endif
struct htsrequest {
  short int user_agent_send; /**< send a User-Agent header */
  short int http11; /**< sign the request as HTTP/1.1 rather than HTTP/1.0 */
  short int nokeepalive;   /**< disable keep-alive */
  short int range_used;    /**< a Range header is in use */
  short int nocompression; /**< disable compression */
  short int flush_garbage; // recycled
  const char *user_agent;  /**< User-Agent value */
  const char *referer;     /**< Referer value */
  const char *from;        /**< From value */
  const char *lang_iso;    /**< Accept-Language value */
  const char *accept;      /**< Accept value */
  const char *headers;     /**< extra request headers */
  htsrequest_proxy proxy;  /**< proxy for this request */
};

/* Result of a connection / header fetch. */
#ifndef HTS_DEF_FWSTRUCT_htsblk
#define HTS_DEF_FWSTRUCT_htsblk
typedef struct htsblk htsblk;
#endif
struct htsblk {
  int statuscode; /**< HTTP status code; -1=error, 200=OK, ... (RFC1945) */
  short int notmodified; /**< page/file was not modified (not transferred) */
  short int is_write;    /**< output goes to disk (out) vs memory (adr) */
  short int is_chunk;    /**< chunked transfer encoding */
  short int compressed;  /**< body is compressed */
  short int empty;       /**< body is empty */
  short int keep_alive;  /**< connection is keep-alive */
  short int keep_alive_trailers; /**< keep-alive with trailers extension */
  int keep_alive_t;              /**< keep-alive timeout (seconds) */
  int keep_alive_max;            /**< keep-alive max number of requests */
  char *adr;                     /**< in-memory body buffer; NULL if empty */
  char *headers;                 /**< received headers, if any */
  FILE *out;                     /**< destination file when is_write=1 */
  LLint size;                    /**< body size */
  char msg[80];                  /**< failure message ("" if none) */
  char contenttype[HTS_MIMETYPE_SIZE];     // content-type (e.g. "text/html")
  char charset[HTS_MIMETYPE_SIZE];         // charset (e.g. "iso-8859-1")
  char contentencoding[HTS_MIMETYPE_SIZE]; // content-encoding (e.g. "gzip")
  char *location;    /**< resolved Location target, if any */
  LLint totalsize;   /**< total size to download (-1=unknown) */
  short int is_file; /**< 1 if a file descriptor rather than a socket */
  T_SOC soc;         /**< socket id */
  SOCaddr address;   /**< peer IP address */
  int address_size;  // IP address structure length (unused internally)
  FILE *fp;          /**< file handle for file:// */
#if HTS_USEOPENSSL
  short int ssl; /**< nonzero if this is an SSL connection (https) */
  // BIO* ssl_soc;          // SSL structure
  SSL *ssl_con; /**< SSL connection structure */
#endif
  char lastmodified[64]; /**< Last-Modified value */
  char etag[256];        /**< ETag value */
  char cdispo[256];      /**< Content-Disposition filename (truncated) */
  LLint crange;          /**< Content-Range length */
  LLint crange_start;    /**< Content-Range start offset */
  LLint crange_end;      /**< Content-Range end offset */
  int debugid;           /**< connection debug id */
  /* */
  htsrequest req; /**< parameters used for the request */
  /*char digest[32+2];   // md5 digest generated by the engine ("" if none) */
};

/* A single link in the crawl. */
#ifndef HTS_DEF_FWSTRUCT_lien_url
#define HTS_DEF_FWSTRUCT_lien_url
typedef struct lien_url lien_url;
#endif
struct lien_url {
  char *adr;        /**< host/address part of the URL */
  char *fil;        /**< remote file path */
  char *sav;        /**< local save name (with any path) */
  char *cod;        /**< codebase path for a Java class, if any */
  char *former_adr; /**< original address before a move; may be NULL */
  char *former_fil; /**< original remote file before a move; may be NULL */

  int premier;      /**< index of the first link that seeded this domain */
  int precedent;    /**< index of the link that referenced this one */
  int depth;        /**< remaining allowed depth; >0 strong, 0 weak */
  int pass2;        /**< second-pass marker; -1 means handled in background */
  char link_import; /**< imported after a move; skip the usual up/down rules */
  int retry;    /**< remaining retries */
  int testmode; /**< test only: send just a HEAD */
};

/* A file being fetched in the background. */
#ifndef HTS_DEF_FWSTRUCT_lien_back
#define HTS_DEF_FWSTRUCT_lien_back
typedef struct lien_back lien_back;
#endif
struct lien_back {
#if DEBUG_CHECKINT
  char magic;
#endif
  char url_adr[HTS_URLMAXSIZE * 2];     /**< host/address part of the URL */
  char url_fil[HTS_URLMAXSIZE * 2];     /**< remote file path */
  char url_sav[HTS_URLMAXSIZE * 2];     /**< local save name (with any path) */
  char referer_adr[HTS_URLMAXSIZE * 2]; /**< referer page host/address */
  char referer_fil[HTS_URLMAXSIZE * 2]; /**< referer page file */
  char
      location_buffer[HTS_URLMAXSIZE * 2]; /**< Location on a move (302, ...) */
  char *tmpfile; /**< temporary save name (compressed) */
  char tmpfile_buffer[HTS_URLMAXSIZE * 2]; /**< storage for tmpfile */
  char send_too[1024];    /**< data to send together with the header */
  int status;             /**< -1=unused, 0=ready, >0=operation in progress */
  int locked;             /**< locked (reserved) */
  int testmode;           /**< test mode */
  int timeout;            /**< timeout in seconds (0=none) */
  TStamp timeout_refresh; /**< last activity time, for timeout tracking */
  int rateout;            /**< minimum tolerated rate in bytes/s (0=none) */
  TStamp rateout_time;    /**< start time for the rate window */
  LLint maxfile_nonhtml;  /**< max bytes for a non-HTML file */
  LLint maxfile_html;     /**< max bytes for an HTML file */
  htsblk r;               /**< per-object result block */
  int is_update;          /**< update mode */
  int head_request;       /**< this is a HEAD request */
  LLint range_req_size;   /**< Range request size used */
  TStamp ka_time_start;   /**< keep-alive refresh start time */
  //
  int http11;       /**< sign the request as HTTP/1.1 rather than HTTP/1.0 */
  int is_chunk;     /**< chunked transfer */
  char *chunk_adr;  /**< buffer for the chunk being loaded */
  LLint chunk_size; /**< size of the chunk being loaded */
  LLint chunk_blocksize; /**< data size declared by the chunk */
  LLint compressed_size; /**< compressed size (stats only) */
  char info[256]; /**< status text, e.g. for FTP */
  int stop_ftp;   /**< stop flag for FTP */
  int finalized;  /**< finalized (memory optimization) */
  int early_add;  /**< was added before the link heap saw it */
#if DEBUG_CHECKINT
  char magic2;
#endif
};

#ifdef __cplusplus
}
#endif

#endif
