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
    Loadable-parser (external module) interface. The engine hands one downloaded
    object to a module as a htsmoduleStruct, and the module reports the links it
    found by calling the addLink callback that structure carries. */

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

#ifndef HTS_DEF_FWSTRUCT_htsmoduleStruct
#define HTS_DEF_FWSTRUCT_htsmoduleStruct
typedef struct htsmoduleStruct htsmoduleStruct;
#endif
/** Reports one link the module found. @p str must be the structure the engine
    passed to the module, and the call is only valid while that module call is
    still running. The engine copies @p link, absolute or relative, and keeps no
    pointer into it.
    @return nonzero once the engine has queued the link, and 0 when its filters
    refused it, when it is HTS_URLMAXSIZE bytes or longer, or when the engine
    ran out of room. */
typedef int (*t_htsAddLink)(htsmoduleStruct *str, char *link);

/** Everything a parser module gets for one downloaded object. Each field says
    whether the engine fills it in before the call or the module writes it back.
    The engine owns every buffer here except localLink. */
struct htsmoduleStruct {
  /* Filled in by the engine before the module is called */
  const char *filename; /**< local file the engine saved the object to */
  int size;             /**< object body size in bytes, truncated to int */
  const char *mime;     /**< MIME type the server declared */
  const char *url_host; /**< source host, carrying a "https://" or "ftp://"
                             prefix when the scheme is not plain HTTP */
  const char *url_file; /**< remote path of the object (/bar/bar.gny) */

  /* Written back by the module */
  const char *wrapper_name; /**< name the module reports itself under, put in
                                 the log and matched against -%w. The engine
                                 presets it, so store a string that outlives
                                 the call. */
  char *err_msg;            /**< 1KB engine buffer the module writes its error
                                 message into */

  /* Read/Write */
  int relativeToHtmlLink; /**< set to 1 when the links passed to addLink are
                               relative to the HTML page that referenced this
                               object rather than to the object itself */

  /* Callbacks */
  t_htsAddLink addLink; /**< engine entry point the module calls for each link
                             it finds */

  /* Optional */
  char *localLink;   /**< buffer the module supplies and owns. addLink writes
                          an accepted link's local relative file name there, or
                          a refused link's absolute URL, and leaves it alone if
                          it is too small. */
  int localLinkSize; /**< bytes available in localLink */

  /* User-defined */
  void *userdef; /**< free for the module to use. The engine never reads it. */

  httrackp *opt; /**< options of the mirror in progress, free to read */

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

/** Entry-point types for a module's init, exit and plug-in hooks. Nothing in
    the engine calls them. A plug-in is installed through hts_plug() and
    hts_unplug() instead, declared in htsdefines.h. */
typedef int (*t_htsWrapperInit)(char *fn, char *args);

typedef int (*t_htsWrapperExit)(void);

typedef int (*t_htsWrapperPlugInit)(char *args);

/* Engine-internal declarations */
#ifdef HTS_INTERNAL_BYTECODE

#include "htsglobal.h"

/** Capabilities string of the build ("-noV6", "-nossl", ...) followed by
    "+name" for each module loaded into @p opt. The returned pointer is a
    scratch buffer inside @p opt, so never free it, and read it before the next
    call overwrites it. */
HTSEXT_API const char *hts_get_version_info(httrackp *opt);

/** Capabilities string of the build, without the loaded-module list. The
    engine owns it and it lives as long as the process, but it stays empty until
    htspe_init() has run. */
HTSEXT_API const char *hts_is_available(void);

/** Prepares the module subsystem, and must run before hts_is_available() or
    before any module is loaded. Calling it again does nothing. On Windows it
    also drops the current directory from the DLL search path
    (CVE-2010-5252). */
extern void htspe_init(void);

/** Tear-down counterpart of htspe_init(). */
extern void htspe_uninit(void);

/** Offers the object described by @p str to the loaded parser modules.
    @return 1 when a module parsed it, 0 when a module claimed it but failed and
    left the reason in @p str's err_msg, and -1 when no module claimed it or the
    one that did is blacklisted by -%w. */
extern int hts_parse_externals(htsmoduleStruct *str);

/** Nonzero when the library was built with IPv6 support. It holds the
    HTS_INET6 value the library itself was compiled with. */
extern int V6_is_available;
#endif

#ifdef __cplusplus
}
#endif

#endif
