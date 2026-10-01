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

/** @file htsopt.h
    The httrackp options block: every tunable of one mirror, the live engine
    state embedded beside it, and the enumerations its fields take. */

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

/** A growable string, owned by the String and released by StringFree(). Read
    it with StringBuff() and never write these fields yourself. htsstrings.h
    defines the same struct and carries the field contract, so it is the one to
    read. */
struct String {
  char *buffer_;
  size_t length_;
  size_t capacity_;
};
#endif

/* Defines */
/** Size in bytes of a concat()/fconcat() scratch buffer. */
#define CATBUFF_SIZE (STRING_SIZE * 2 * 2)

/** Base size in bytes of the fixed path buffers below. */
#define STRING_SIZE 2048

/** Proxy the engine sends its requests through (-P). */
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

/** The +/- filter rules in force during a mirror. */
#ifndef HTS_DEF_FWSTRUCT_htsfilters
#define HTS_DEF_FWSTRUCT_htsfilters
typedef struct htsfilters htsfilters;
#endif
struct htsfilters {
  char ***filters; /**< pointer to the +/-pattern filter array */
  int *filptr;     /**< pointer to the current filter count */
};

/** Called once when the engine shuts down. Its result is ignored. */
typedef int (*htscallbacksfncptr)(void);

typedef struct htscallbacks htscallbacks;

/** One module's exit callback. The head entry is embedded in htsoptstate, so
    the engine frees every entry but that one. */
struct htscallbacks {
  void *moduleHandle;         /**< module to unload once exitFnc has run */
  htscallbacksfncptr exitFnc; /**< called once, before the module is unloaded */
  htscallbacks *next;         /**< next entry, NULL-terminated */
};

/** Where the engine lists the files it has written (see filenote()). */
#ifndef HTS_DEF_FWSTRUCT_filenote_strc
#define HTS_DEF_FWSTRUCT_filenote_strc
typedef struct filenote_strc filenote_strc;
#endif
struct filenote_strc {
  FILE *lst; /**< open list file each saved name is appended to, or NULL */
  char path[STRING_SIZE * 2]; /**< root stripped from the listed names */
};

#ifndef HTS_DEF_FWSTRUCT_concat_strc
#define HTS_DEF_FWSTRUCT_concat_strc
typedef struct concat_strc concat_strc;
#endif
struct concat_strc {
  int index;                          /**< buffer handed out last */
  char buff[16][STRING_SIZE * 2 * 2]; /**< handed out in turn, so only the
                                           last 16 results stay valid */
};

/** Caller-owned scratch for int2bytes() and its variants: they format into
    it and return a pointer inside it, so it must outlive the result. */
#ifndef HTS_DEF_FWSTRUCT_strc_int2bytes2
#define HTS_DEF_FWSTRUCT_strc_int2bytes2
typedef struct strc_int2bytes2 strc_int2bytes2;
#endif
struct strc_int2bytes2 {
  char catbuff[CATBUFF_SIZE]; /**< what int2bytes() and int2bytessec() return */
  char buff1[256];            /**< the number */
  char buff2[32];             /**< the unit, and int2char()'s whole result */
  char *buffadr[2];           /**< {buff1, buff2}, what int2bytes2() returns */
};

/** The command the engine runs after each saved file (-V). */
#ifndef HTS_DEF_FWSTRUCT_usercommand_strc
#define HTS_DEF_FWSTRUCT_usercommand_strc
typedef struct usercommand_strc usercommand_strc;
#endif
struct usercommand_strc {
  int exe;        /**< nonzero once a non-empty command is armed */
  char cmd[2048]; /**< the command, where $0 stands for the file name */
};

#ifndef HTS_DEF_FWSTRUCT_fspc_strc
#define HTS_DEF_FWSTRUCT_fspc_strc
typedef struct fspc_strc fspc_strc;
#endif
struct fspc_strc {
  int error;   /**< errors and panics logged */
  int warning; /**< warnings logged */
  int info;    /**< info and notice messages logged */
};

/* lien_url */
#ifndef HTS_DEF_FWSTRUCT_lien_url
#define HTS_DEF_FWSTRUCT_lien_url
typedef struct lien_url lien_url;
#endif

#ifndef HTS_DEF_DEFSTRUCT_hts_log_type
#define HTS_DEF_DEFSTRUCT_hts_log_type

/** Log levels, most severe first. A message is written when its level is at
    most opt->debug. */
typedef enum hts_log_type {
  LOG_PANIC,         /**< the engine cannot go on */
  LOG_ERROR,         /**< an operation failed */
  LOG_WARNING,       /**< something looks wrong, but the mirror goes on */
  LOG_NOTICE,        /**< normal but notable event (the default level) */
  LOG_INFO,          /**< progress detail (-z) */
  LOG_DEBUG,         /**< engine internals (-Z) */
  LOG_TRACE,         /**< every step (a second -Z) */
  LOG_ERRNO = 1 << 8 /**< OR it into a level to append strerror(errno) */
} hts_log_type;
#endif

/** One URL the caller asked the mirror to drop. */
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

/** Live state of one running mirror, embedded in httrackp. */
#ifndef HTS_DEF_FWSTRUCT_htsoptstate
#define HTS_DEF_FWSTRUCT_htsoptstate
typedef struct htsoptstate htsoptstate;
#endif
struct htsoptstate {
  htsmutex lock; /**< guards this state block */
  /* */
  /** set to request the mirror to stop; volatile: polled without a lock */
  volatile int stop;
  /** why the mirror ended early: 1 asked for, 2 rolled back, -1 engine fatal
      (the only one hts_main2() reports); not the process exit status.
      volatile: signal handlers write it and the mirror loop polls it */
  volatile int exit_xh;
  int back_add_stats; /**< bumped each time a file is queued for transfer */
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
  char *userhttptype;       /**< unused */
  int verif_backblue_done; /**< backblue.gif/fade.gif already emitted */
  int verif_external_status; /**< one bit per external notice already written */
  coucal dns_cache; /**< DNS resolution cache: hostname -> t_dnscache record */
  int dns_cache_nthreads; /**< number of in-flight DNS resolver threads */
  /* HTML parsing state */
  char _hts_errmsg[HTS_CDLMAXSIZE + 256]; /**< last engine error message */
  /** Which engine phase is running, or 0 for none. hts_is_parsing() and
      hts_is_testing() report it. */
  int _hts_in_html_parsing;
  int _hts_in_html_done; /**< progress of the current parse, in percent */
  int _hts_in_html_poll; /**< a caller asked for a display refresh */
  int _hts_setpause;     /**< nonzero holds the transfers, see hts_setpause() */
  int _hts_in_mirror; /**< nonzero while a mirror is running */
  char **_hts_addurl; /**< extra URLs to inject at runtime */
  int _hts_cancel;    /**< 1 cancels the parse, 2 cancels the link test */
  htsoptstatecancel *cancel; /**< list of URLs flagged for cancellation */
  char HTbuff[2048];         /**< text of the question put to the user */
  unsigned int debug_state;  /**< one bit per debug warning already logged */
  unsigned int tmpnameid; /**< counter for temporary file names */
  int is_ended;           /**< mirror has finished */
  void *warc; /**< WARC writer (warc_writer*), or NULL, or the WARC_DISABLED
                   sentinel (htswarc.c) */
};

/** One module the engine has loaded, either from -%W (--callback) or from the
    set it preloads itself. */
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

/** Script-parsing switches OR'd into opt->parsejava (-jN). */
typedef enum htsparsejava_flags {
  HTSPARSE_NONE = 0,     /**< -j0, which only stops the parser modules */
  HTSPARSE_DEFAULT = 1,  /**< the default, and all the flags below cleared */
  HTSPARSE_NO_CLASS = 2, /**< run no external parser module */
  HTSPARSE_NO_JAVASCRIPT = 4, /**< scan no script for links */
  HTSPARSE_NO_AGGRESSIVE = 8  /**< cancel the -%P raw scan */
} htsparsejava_flags;

/** How a saved page's links are rewritten (opt->urlmode, -KN). */
#ifndef HTS_DEF_DEFSTRUCT_hts_urlmode
#define HTS_DEF_DEFSTRUCT_hts_urlmode

typedef enum hts_urlmode {
  HTS_URLMODE_ABSOLUTE = 0, /**< absolute URL (http://host/path), bare -K */
  HTS_URLMODE_ABSOLUTE_FILE = 1, /**< no engine path handles this value */
  HTS_URLMODE_RELATIVE = 2,      /**< relative link (the default, -K0) */
  HTS_URLMODE_ABSOLUTE_URI = 3,  /**< absolute URI from the root, /path (-K3) */
  HTS_URLMODE_KEEP_ORIGINAL = 4, /**< keep the original link as it is (-K4) */
  HTS_URLMODE_TRANSPARENT_PROXY = 5 /**< transparent-proxy URL (-K5) */
} hts_urlmode;
#endif

/** Cache policy for updates and retries (opt->cache, -CN). */
#ifndef HTS_DEF_DEFSTRUCT_hts_cachemode
#define HTS_DEF_DEFSTRUCT_hts_cachemode

typedef enum hts_cachemode {
  HTS_CACHE_NONE = 0,       /**< no cache (-C0) */
  HTS_CACHE_PRIORITY = 1,   /**< read the cache first (the default, -C1) */
  HTS_CACHE_TEST_UPDATE = 2 /**< ask the server for an update first (-C2) */
} hts_cachemode;
#endif

/** Interactive wizard level (opt->wizard). */
#ifndef HTS_DEF_DEFSTRUCT_hts_wizard
#define HTS_DEF_DEFSTRUCT_hts_wizard

typedef enum hts_wizard {
  HTS_WIZARD_NONE = 0, /**< no wizard */
  HTS_WIZARD_ASK = 1,  /**< the wizard asks questions (-W) */
  HTS_WIZARD_AUTO = 2  /**< the wizard decides alone (the default, -w) */
} hts_wizard;
#endif

/** robots.txt and meta-robots obedience level (opt->robots, -sN). */
#ifndef HTS_DEF_DEFSTRUCT_hts_robots
#define HTS_DEF_DEFSTRUCT_hts_robots

typedef enum hts_robots {
  HTS_ROBOTS_NEVER = 0,        /**< ignore robots rules (-s0) */
  HTS_ROBOTS_SOMETIMES = 1,    /**< partial obedience (bare -s, or -s1) */
  HTS_ROBOTS_ALWAYS = 2,       /**< obey robots rules (the default, -s2) */
  HTS_ROBOTS_ALWAYS_STRICT = 3 /**< obey even the strict rules (-s3) */
} hts_robots;
#endif

/** What to fetch (opt->getmode bitmask, set whole by -pN). */
typedef enum hts_getmode {
  HTS_GETMODE_HTML = 1 << 0,      /**< save HTML files */
  HTS_GETMODE_NONHTML = 1 << 1,   /**< save non-HTML files */
  HTS_GETMODE_HTML_FIRST = 1 << 2 /**< fetch the HTML before the rest */
} hts_getmode;

/** Allowed directions in the directory tree (opt->seeker bitmask): -S stays
    put, -D goes down, -U goes up, -B does both. */
typedef enum hts_seeker {
  HTS_SEEKER_DOWN = 1 << 0, /**< may descend into subdirectories */
  HTS_SEEKER_UP = 1 << 1    /**< may ascend to parent directories */
} hts_seeker;

/** opt->travel: link-following scope in the low byte, flags OR'd in above it.
    -a, -d, -l and -e each set a scope, and -t adds HTS_TRAVEL_TEST_ALL. */
typedef enum hts_travel_scope {
  HTS_TRAVEL_SAME_ADDRESS = 0, /**< same host, the default (-a) */
  HTS_TRAVEL_SAME_DOMAIN = 1,  /**< same principal domain (-d) */
  HTS_TRAVEL_SAME_TLD = 2,     /**< same TLD, .com for example (-l) */
  HTS_TRAVEL_EVERYWHERE = 7,   /**< anywhere on the web (-e) */
  HTS_TRAVEL_TEST_ALL = 1 << 8 /**< also test forbidden URLs (-t) */
} hts_travel_scope;

/** Mask selecting the scope value out of opt->travel. */
#define HTS_TRAVEL_SCOPE_MASK 0xff

/** Text progress display detail (opt->verbosedisplay, -%vN). */
typedef enum hts_verbosedisplay {
  HTS_VERBOSE_NONE = 0,   /**< no progress display (-%v0) */
  HTS_VERBOSE_SIMPLE = 1, /**< one progress line (-%v1) */
  HTS_VERBOSE_FULL = 2    /**< full animation (bare -%v, or -%v2) */
} hts_verbosedisplay;

/** Delayed file-type resolution policy (opt->savename_delayed, -%N). */
typedef enum hts_savename_delayed {
  HTS_SAVENAME_DELAYED_NONE = 0, /**< resolve the type at once (-%N0) */
  HTS_SAVENAME_DELAYED_SOFT = 1, /**< delay it for an unknown type (-%N1) */
  HTS_SAVENAME_DELAYED_HARD = 2  /**< always delay it (the default, -%N2) */
} hts_savename_delayed;

/** Saved-name length layout (opt->savename_83, -LN). */
typedef enum hts_savename_83 {
  HTS_SAVENAME_83_LONG = 0,   /**< long file names (the default, -L1) */
  HTS_SAVENAME_83_DOS = 1,    /**< DOS 8.3 names, ISO9660 level 1 (-L0) */
  HTS_SAVENAME_83_ISO9660 = 2 /**< ISO9660 level 2 names, 31 chars (-L2) */
} hts_savename_83;

/** Host-banning triggers (opt->hostcontrol bitmask, -HN). */
typedef enum hts_hostcontrol {
  HTS_HOSTCONTROL_BAN_TIMEOUT = 1 << 0, /**< ban a timing-out host (-H1) */
  HTS_HOSTCONTROL_BAN_SLOW = 1 << 1     /**< ban a too-slow host (-H2) */
} hts_hostcontrol;

#ifndef HTS_DEF_FWSTRUCT_lien_buffers
#define HTS_DEF_FWSTRUCT_lien_buffers
typedef struct lien_buffers lien_buffers;
#endif

/** Per-mirror options and live state: hts_create_opt() builds one and
    hts_main2() runs it. Callers normally configure it through the command-line
    argv vector rather than by writing fields, and the only fields real
    consumers set themselves are log and errlog. An hts_tristate option left at
    HTS_DEFAULT is unspecified, so copy_htsopt() leaves such a field alone. */
#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif
struct httrackp {
  /** Size hts_create_opt() gave this block. A consumer must refuse to run when
      its own sizeof(httrackp) is bigger, because its extra fields then sit past
      the end of the block. */
  size_t size_httrackp;
  /* */
  hts_wizard wizard; /**< interactive wizard level (-W asks, -w does not) */
  hts_boolean flush; /**< flush the log files after each line (-#f) */
  int travel;        /**< how far a link may lead, from hts_travel_scope,
                          plus HTS_TRAVEL_TEST_ALL (-a, -d, -l, -e, -t) */
  int seeker;        /**< directions allowed in the tree, from hts_seeker
                          (-S none, -D down, -U up, -B both) */
  int depth;         /**< maximum recursion depth (-rN) */
  int extdepth;      /**< recursion depth outside the start domain (-%eN) */
  hts_urlmode urlmode; /**< how a saved page's links are rewritten (-KN) */
  hts_boolean no_type_change; /**< keep the saved name's own extension, not the
                                   MIME type's (-%t) */
  hts_log_type debug;         /**< most verbose level still logged (-z, -Z) */
  int getmode;                /**< what to fetch, from hts_getmode (-pN) */
  FILE *log;                  /**< informational log stream, NULL to mute it */
  FILE *errlog;               /**< error log stream, NULL to mute it */
  LLint maxsite;              /**< byte budget for the mirror, -1 none (-MN) */
  LLint maxfile_nonhtml;      /**< max bytes per non-HTML file, -1 none (-mN) */
  LLint maxfile_html;         /**< max bytes per HTML file, -1 none (-mN,N2) */
  int maxsoc;                 /**< max simultaneous sockets (-cN) */
  LLint fragment;             /**< pause after this many bytes, until the
                                   hts-paused.lock file is deleted (-GN) */
  hts_tristate nearlink;      /**< also get off-site files a page needs (-n) */
  hts_boolean makeindex;      /**< build a top-level index.html (-I) */
  hts_boolean kindex;         /**< build a searchable keyword index (-%I) */
  hts_tristate delete_old;    /**< delete obsolete local files (-X) */
  int timeout;                /**< connection timeout in seconds (-TN) */
  int rateout;                /**< give up below this rate, bytes/s (-JN) */
  int maxtime;         /**< mirror time limit in seconds, -1 none (-EN) */
  int maxrate;         /**< transfer rate cap in bytes/s (-AN) */
  float maxconn;       /**< new connections per second (-%cN) */
  int waittime;        /**< start the mirror at this time of day, in
                            seconds since local midnight (<=0 now, -#uN) */
  hts_cachemode cache; /**< cache policy for updates (-CN) */
  hts_boolean shell;   /**< driven by a shell on stdin/stdout (-#S) */
  t_proxy proxy;       /**< proxy configuration (-P, -%b) */
  hts_savename_83 savename_83; /**< saved-name length layout (-LN) */
  /** Saved-name layout preset (-N), or -1 for the savename_userdef template.
      `% 100` picks the tree: 0 site structure, 1 splits off images/, 2 splits
      off both images/ and html/, 4 and 5 split by extension, 99 gives random
      names, and every other value keeps the file name alone. An odd hundreds
      digit names the top directory after the host rather than "web", and an
      odd thousands digit drops that top directory. Site structure has no
      "web", so it reads no hundreds digit beyond the literal 100, which drops
      the host directory. An extensionless name is forced to ".html" for every
      preset but -1, and only while savename_delayed is not
      HTS_SAVENAME_DELAYED_HARD, which is the default, so a default run forces
      nothing. */
  int savename_type;
  String savename_userdef; /**< name template, e.g. %h%p/%n%q.%t (-N) */
  hts_savename_delayed savename_delayed; /**< delayed type-check policy (-%N) */
  hts_boolean delayed_cached;  /**< on an update, take the delayed type check
                                    from the cache (-%D) */
  hts_boolean mimehtml;        /**< produce a single MIME/MHTML archive (-%M) */
  hts_boolean user_agent_send; /**< send a User-Agent header (-F) */
  String user_agent;           /**< User-Agent value (-F) */
  String referer;              /**< Referer value to send (-%R) */
  String from;                 /**< From value to send (-%E) */
  String path_log;             /**< directory for the cache and the logs (-O) */
  String path_html;            /**< output directory for the mirror (-O) */
  String path_html_utf8; /**< output directory for the mirror, UTF-8 form */
  String path_bin;       /**< directory holding the HTML templates */
  int retry;             /**< extra retries on a failed transfer (-RN) */
  hts_boolean makestat;  /**< maintain a transfer-statistics log (-#Z) */
  hts_boolean maketrack; /**< maintain an operations-statistics log (-#T) */
  int parsejava;         /**< script parsing, from htsparsejava_flags (-jN) */
  int hostcontrol;       /**< when to ban a host, from hts_hostcontrol (-HN) */
  hts_tristate errpage;  /**< save the server's own error pages, 404 and such
                              (-oN) */
  hts_boolean check_type;   /**< test a link of unknown type, a cgi for example
                                 (-uN) */
  hts_boolean all_in_cache; /**< keep every file in the cache too (-k) */
  hts_robots robots;        /**< robots.txt obedience level (-sN) */
  hts_tristate external;    /**< show external links as error pages (-x) */
  hts_boolean passprivacy;  /**< no passwords in external links (-%x) */
  hts_boolean includequery; /**< saved names keep the query string (-%q) */
  hts_boolean mirror_first_page; /**< mirror only the first page's links (-Y) */
  String sys_com;                /**< command run after each saved file (-V) */
  hts_boolean sys_com_exec;      /**< run sys_com (-V "" clears it) */
  hts_boolean accept_cookie;     /**< accept and send cookies (-bN) */
  t_cookie *cookie;              /**< cookie store */
  hts_boolean http10;            /**< force HTTP/1.0 requests (-%h) */
  hts_boolean nokeepalive;       /**< do not use keep-alive (-%k0) */
  hts_boolean nocompression;     /**< do not ask for compression (-%z) */
  hts_boolean sizehack;          /**< same size means unchanged (-%s) */
  hts_boolean urlhack;           /**< fold duplicate URLs together (-%u) */
  hts_boolean tolerant;          /**< accept a bogus Content-Length (-%B) */
  hts_tristate parseall;  /**< parse every link, even in an unknown tag (-%P) */
  hts_boolean parsedebug; /**< parser debug mode (-#d) */
  hts_boolean norecatch;  /**< do not re-fetch locally deleted files (-%n) */
  hts_verbosedisplay verbosedisplay; /**< progress display level (-%vN) */
  String footer;            /**< footer line injected into saved pages (-%F) */
  int maxcache;             /**< max bytes of transfers held in memory */
  hts_boolean ftp_proxy;    /**< use the HTTP proxy for FTP too (-%f) */
  String filelist;          /**< file holding extra URLs, one per line (-%L) */
  String urllist;           /**< never filled */
  htsfilters filters;       /**< the +/- filter rules in force */
  hash_struct *hash;        /**< URL lookup tables, live while a mirror runs */
  lien_url **liens;         /**< the links, NULL-terminated */
  int lien_tot;             /**< number of entries in liens[] */
  lien_buffers *liensbuf;   /**< backing store for liens[], engine-owned */
  robots_wizard *robotsptr; /**< robots rules, live while a mirror runs */
  String lang_iso;          /**< Accept-Language value (-%l) */
  String accept;            /**< Accept value (-%a) */
  String headers;           /**< extra request headers, CRLF-ended (-%X) */
  String mimedefs;          /**< "ext=type" MIME rules, one per line (-%A) */
  String mod_blacklist;     /**< modules that must not load (-%w) */
  hts_boolean convert_utf8; /**< convert saved file names to UTF-8 (-%T) */
  //
  int maxlink;   /**< max number of links, 0 for no limit (-#LN) */
  int maxfilter; /**< max number of filters (-#FN) */
  //
  const char *exec; /**< unused, always "" */
  //
  hts_boolean quiet;                 /**< never ask the user a question (-q) */
  hts_boolean keyboard;              /**< poll stdin for keyboard input (-#K) */
  hts_boolean bypass_limits;         /**< lift the built-in bandwidth and
                                          connection caps (-%!) */
  hts_boolean background_on_suspend; /**< background on a suspend signal (-y) */
  //
  hts_boolean is_update;    /**< this run updates a previous mirror */
  hts_boolean dir_topindex; /**< rebuild the project-folder top index (-%i) */
  //
  // callbacks
  t_hts_htmlcheck_callbacks
      *callbacks_fun; /**< user HTML/parsing callback table */
  // store library handles
  htslibhandles libHandles; /**< loaded external module handles */
  //
  /* Live state, not options: copy_htsopt must leave it alone. */
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
  String why_url;   /**< URL to diagnose (--why): print the deciding filter rule
                         and exit without crawling */
  String warc_file; /**< WARC output: WARC_AUTONAME for --warc, or the
                         --warc-file basename (appended at the tail: ABI) */
  LLint warc_max_size;  /**< --warc-max-size: rotate the archive past this many
                             bytes (<=0: single file). Tail: ABI */
  hts_boolean warc_cdx; /**< --warc-cdx: write a sorted CDXJ index next to the
                             archive. Tail: ABI */
  hts_boolean warc_wacz; /**< --wacz: package archive+index+pages as a WACZ zip
                              (implies --warc + --warc-cdx). Tail: ABI */
  hts_boolean changes;   /**< --changes: report what this crawl changed against
                              the previous mirror. Tail: ABI */
  void *changes_state;   /**< live change-report accumulator (hts_changes*),
                              engine-owned. Tail: ABI */
  hts_boolean single_file; /**< --single-file: once the mirror is done, rewrite
                                each saved page with its assets inlined as
                                data: URIs. Tail: ABI */
  LLint single_file_max_size; /**< --single-file-max-size: per-asset cap in
                                   bytes; a bigger asset stays a link.
                                   Tail: ABI */
  hts_boolean sitemap; /**< --sitemap: probe the start host's robots.txt for
                            Sitemap: lines, else /sitemap.xml. Tail: ABI */
  String sitemap_url;  /**< --sitemap-url: sitemap to ingest. Tail: ABI */
  /* Live state, not an option: copy_htsopt must leave it alone. It sits here
     rather than in htsoptstate because that struct is embedded by value, so
     growing it would shift every httrackp field declared after it. */
  void *sitemap_state; /**< hts_sitemap_state*, or NULL. Tail: ABI */
  void *singlefile_state; /**< hts_singlefile_state*, or NULL. Tail: ABI */
  String host_alias;  /**< --host-alias: '\n'-separated "alias[,alias...]=host"
                           rules folding the hostnames of one site onto a single
                           canonical host. Tail: ABI */
  int wizard_filters; /**< count of filters the wizard has inserted, held at the
                           low indices of the array. Live state, so copy_htsopt
                           must leave it alone. Tail: ABI */
  hts_boolean links_unqueued; /**< a page gave up before parsing, so its links
                                   were never queued and the update purge would
                                   treat them as gone. Live state, so
                                   copy_htsopt must leave it alone. Tail: ABI */
  hts_boolean abort_left_partial; /**< a teardown cut a body mid-transfer, so
                                       the partial it left needs its
                                       hts-cache/ref kept. Live state, so
                                       copy_htsopt must leave it alone.
                                       Tail: ABI */
  hts_tristate mirror_completed;  /**< httpmirror()'s verdict, read back by
                                       hts_mirror_completed(). HTS_DEFAULT
                                       until a mirror starts. Live state, so
                                       copy_htsopt must leave it alone.
                                       Tail: ABI */
  int transport_failures;         /**< links given up on a failed transfer,
                                       published as stat_transport_failures.
                                       Live state, so copy_htsopt must leave it
                                       alone. Tail: ABI */
  hts_boolean upper_links_refused; /**< a link was refused for sitting above
                                        the start directory, reported once at
                                        the end so -B is discoverable. Live
                                        state, so copy_htsopt must leave it
                                        alone. Tail: ABI */
  int max_retry_after; /**< longest Retry-After obeyed, in seconds. 0 retries
                            with no wait (--max-retry-after). Tail: ABI */
  hts_tristate mptcp;  /**< open connections with Multipath TCP (--mptcp).
                            HTS_DEFAULT: on where the kernel recovers from a
                            blackholed SYN by itself. Tail: ABI */
  int mptcp_connections; /**< HTTP connections that negotiated MPTCP. FTP opens
                              its sockets on a worker thread and is left out.
                              Live state, so copy_htsopt must leave it alone.
                              Tail: ABI */
  int mptcp_fallbacks;   /**< HTTP connections that asked for MPTCP and were
                              given plain TCP. Live state, so copy_htsopt must
                              leave it alone. Tail: ABI */
};

/** Running statistics for a mirror. */
#ifndef HTS_DEF_FWSTRUCT_hts_stat_struct
#define HTS_DEF_FWSTRUCT_hts_stat_struct
typedef struct hts_stat_struct hts_stat_struct;
#endif
struct hts_stat_struct {
  LLint HTS_TOTAL_RECV; /**< total bytes received from the network, published
                             here by engine_stats() and hts_get_stats() */
  LLint stat_bytes;     /**< total bytes written to disk */
  TStamp stat_timestart; /**< mirror start time */
  //
  LLint total_packed;    /**< compressed bytes received (on the wire) */
  LLint total_unpacked;  /**< bytes after decompression */
  int total_packedfiles; /**< number of compressed files */
  //
  TStamp istat_timestart[2]; /**< window start times, on mtime_monotonic() */
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
  //
  int stat_transport_failures; /**< links given up because the transfer failed,
                                    counted as it happens rather than from the
                                    log lines stat_errors reads, so an answered
                                    error is out and -Q leaves it alone */
  int stat_mptcp_connections;  /**< HTTP connections that negotiated Multipath
                                    TCP, published from opt->mptcp_connections */
  int stat_mptcp_fallbacks;    /**< HTTP connections that asked for Multipath
                                    TCP and were given plain TCP */
};

/** The option fields one request needs, copied out of httrackp. */
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
  short int flush_garbage; /**< set, and read by nothing */
  const char *user_agent;  /**< User-Agent value */
  const char *referer;     /**< Referer value */
  const char *from;        /**< From value */
  const char *lang_iso;    /**< Accept-Language value */
  const char *accept;      /**< Accept value */
  const char *headers;     /**< extra request headers */
  htsrequest_proxy proxy;  /**< proxy for this request */
};

/** Result of one transfer: its response headers, plus its body or output
    file. */
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
  char contenttype[HTS_MIMETYPE_SIZE];     /**< Content-Type ("text/html") */
  char charset[HTS_MIMETYPE_SIZE];         /**< charset ("iso-8859-1") */
  char contentencoding[HTS_MIMETYPE_SIZE]; /**< Content-Encoding ("gzip") */
  char *location;    /**< resolved Location target, HTS_LOCATION_SIZE bytes */
  LLint totalsize;   /**< total size to download (-1=unknown) */
  short int is_file; /**< 1 if a file descriptor rather than a socket */
  T_SOC soc;         /**< socket id */
  SOCaddr address;   /**< peer IP address */
  int address_size;  /**< byte length of address, set but unused here */
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
  /** Restart-whole signal: a resume this response rejected (unusable 206) must
      retry with no Range, else a surviving partial/temp-ref loops (#581). */
  hts_boolean refetch_wholefile;
  char *warc_reqhdr; /**< stashed raw request header block for WARC (or NULL) */
  char *
      warc_resphdr; /**< stashed raw response header block for WARC (or NULL) */
  int warc_truncated; /**< WARC-Truncated reason for a cap-truncated body
                           (WARC_TRUNC_*, 0=none). Tail: ABI */
  char *warc_rawpath; /**< verbatim WARC: spooled compressed body path, or NULL
                           (owns the file; unlinked on free). Tail: ABI */
  LLint warc_rawsize; /**< byte length of warc_rawpath. Tail: ABI */
  /** notmodified came from an engine hack, not a server 304 (#839).
      Tail: ABI */
  hts_boolean warc_forced_notmodified;
  /** Retry-After in seconds, uncapped. -1 when the header named nothing
      usable, which is what tells a retryable 503 from a fatal one.
      Tail: ABI */
  int retry_after;
  /*char digest[32+2];   // md5 digest generated by the engine ("" if none) */
};

/** A single link in the crawl. */
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
  hts_boolean
      refetch_whole; /**< force a whole-file GET, ignoring any partial/temp-ref
                   resume (#581); doubles as the latch spending this link's one
                   free restart, so copy it wherever retry is copied (#1052) */
};

/** A file being fetched in the background. */
#ifndef HTS_DEF_FWSTRUCT_lien_back
#define HTS_DEF_FWSTRUCT_lien_back
typedef struct lien_back lien_back;
#endif
struct lien_back {
#if DEBUG_CHECKINT
  char magic; /**< leading guard byte, must stay 0 */
#endif
  char url_adr[HTS_URLMAXSIZE * 2];     /**< host/address part of the URL */
  char url_fil[HTS_URLMAXSIZE * 2];     /**< remote file path */
  char url_sav[HTS_URLMAXSIZE * 2];     /**< local save name (with any path) */
  char referer_adr[HTS_URLMAXSIZE * 2]; /**< referer page host/address */
  char referer_fil[HTS_URLMAXSIZE * 2]; /**< referer page file */
  char location_buffer[HTS_LOCATION_SIZE]; /**< Location on a move (302, ...) */
  char *tmpfile; /**< temporary save name (compressed) */
  char tmpfile_buffer[HTS_URLMAXSIZE * 2]; /**< storage for tmpfile */
  char send_too[1024];    /**< data to send together with the header */
  int status;             /**< -1=unused, 0=ready, >0=operation in progress */
  int locked; /**< 0 free to move, 1 locked in memory, 2 pinned (may write) */
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
  volatile int stop_ftp; /**< stop flag for FTP, polled without a lock */
  int finalized;  /**< finalized (memory optimization) */
  int early_add;  /**< was added before the link heap saw it */
#if DEBUG_CHECKINT
  char magic2; /**< trailing guard byte, must stay 0 */
#endif
};

#ifdef __cplusplus
}
#endif

#endif
