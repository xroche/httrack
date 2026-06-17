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
/* File: htsmodules.h subroutines:                              */
/*       external modules (parsers)                             */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/** @file htsmodules.h
    Loadable-parser (external module) interface. The engine hands a downloaded
    object to a module via htsmoduleStruct; the module reports discovered links
    back through the addLink callback. */

#ifndef HTS_MODULES
#define HTS_MODULES

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_lien_url
#define HTS_DEF_FWSTRUCT_lien_url
typedef struct lien_url lien_url;
#endif
#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif
#ifndef HTS_DEF_FWSTRUCT_struct_back
#define HTS_DEF_FWSTRUCT_struct_back
typedef struct struct_back struct_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_cache_back
#define HTS_DEF_FWSTRUCT_cache_back
typedef struct cache_back cache_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_hash_struct
#define HTS_DEF_FWSTRUCT_hash_struct
typedef struct hash_struct hash_struct;
#endif

/** Callback a module invokes to report a discovered link.
    str: the per-object context the module was called with.
    link: link to add (absolute or relative); the engine copies it.
    Returns 1 if the engine accepted/queued the link, 0 if it was rejected. */
#ifndef HTS_DEF_FWSTRUCT_htsmoduleStruct
#define HTS_DEF_FWSTRUCT_htsmoduleStruct
typedef struct htsmoduleStruct htsmoduleStruct;
#endif
typedef int (*t_htsAddLink) (htsmoduleStruct * str, char *link);

/** Per-object context passed to a parser module for one downloaded file.
    Field access classes are noted; engine owns all pointers unless stated. */
struct htsmoduleStruct {
  /* Read-only elements */
  const char *filename;         /* filename (C:\My Web Sites\...) */
  int size;                     /* size of filename (should be > 0) */
  const char *mime;             /* MIME type of the object */
  const char *url_host;         /* incoming hostname (www.foo.com) */
  const char *url_file;         /* incoming filename (/bar/bar.gny) */

  /* Write-only */
  const char *wrapper_name;     /* name of wrapper (static string) */
  char *err_msg;                /* if an error occurred, the error message (max. 1KB) */

  /* Read/Write */
  int relativeToHtmlLink;       /* set this to 1 if all urls you pass to addLink
                                   are in fact relative to the html file where your
                                   module was originally */

  /* Callbacks */
  t_htsAddLink addLink;         /* call this function when links are 
                                   being detected. it if not your responsability to decide
                                   if the engine will keep them, or not. */

  /* Optional */
  char *localLink;              /* if non null, the engine will write there the local
                                   relative filename of the link added by addLink(), or
                                   the absolute path if the link was refused by the wizard */
  int localLinkSize;            /* size of the optionnal buffer */

  /* User-defined */
  void *userdef;                /* can be used by callback routines
                                 */

  /* The parser httrackp structure (may be used) */
  httrackp *opt;

  /* Internal use - please don't touch */
  struct_back *sback;
  cache_back *cache;
  hash_struct *hashptr;
  int numero_passe;
  /* */
  int *ptr_;
  const char *page_charset_;
  /* Internal use - please don't touch */

};

#ifdef __cplusplus
extern "C" {
#endif

/** Module lifecycle hooks. Init/PlugInit return 1 on success, 0 on failure;
    Exit returns its own status (ignored by the engine). */
typedef int (*t_htsWrapperInit) (char *fn, char *args);

typedef int (*t_htsWrapperExit) (void);

typedef int (*t_htsWrapperPlugInit) (char *args);

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

/** Capabilities string ("-noV6", "-nossl", ...) followed by "+name" for each
    loaded module. Returned pointer aliases opt->state.HTbuff; do not free, and
    it is overwritten by the next call. */
HTSEXT_API const char *hts_get_version_info(httrackp * opt);

/** Static capabilities string set by htspe_init(); valid for the process
    lifetime, do not free. */
HTSEXT_API const char *hts_is_available(void);

/** Initialize the module subsystem (idempotent): builds the capabilities
    string and, on Windows, hardens the DLL search path. */
extern void htspe_init(void);

/** Tear-down counterpart of htspe_init(); currently a no-op. */
extern void htspe_uninit(void);

/** Run the external-parser callbacks for the object described by str.
    Returns the parse callback result (>=0) on a handled object, or -1 if no
    module claimed it or its wrapper_name is blacklisted. */
extern int hts_parse_externals(htsmoduleStruct * str);

/** Nonzero if IPv6 support was compiled in (== HTS_INET6). */
extern int V6_is_available;
#endif

#ifdef __cplusplus
}
#endif

#endif
