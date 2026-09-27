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
    Loadable-parser (external module) interface. The engine describes one
    downloaded object in a htsmoduleStruct, and the module reports the links it
    finds through that structure's addLink callback. */

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
    gave the module, and the call is valid only while that module call runs. The
    engine copies @p link, absolute or relative.
    @return nonzero when the engine accepted the link, and 0 when a filter
    refused it, when it is HTS_URLMAXSIZE bytes or longer, or when the engine
    had no room left for it. */
typedef int (*t_htsAddLink)(htsmoduleStruct *str, char *link);

/** Everything a parser module gets for one downloaded object. Each field names
    its writer, the engine before the call or the module during it. */
struct htsmoduleStruct {
  const char *filename; /**< engine: local file the object was saved to */
  int size;             /**< engine: body size in bytes, truncated to int */
  const char *mime;     /**< engine: MIME type, declared by the server or
                             guessed from the URL */
  const char *url_host; /**< engine: source host, with a scheme prefix
                             ("https://", "ftp://") unless plain HTTP */
  const char *url_file; /**< engine: remote path of the object (/bar/bar.gny) */

  const char *wrapper_name; /**< module: the name it reports itself under, used
                                 by the log and the -%w blacklist. The engine
                                 presets it, so store a lasting string. */
  char *err_msg;            /**< module: writes its error message into this 1KB
                                 engine buffer */
  int relativeToHtmlLink;   /**< module: 1 when the links it passes to addLink
                                 are relative to the HTML page that referenced
                                 this object, not to the object itself */
  t_htsAddLink addLink;     /**< engine: link collector the module calls */
  char *localLink;   /**< module: optional buffer addLink fills with an accepted
                          link's local relative name, or a refused link's
                          absolute URL, and leaves alone when too small */
  int localLinkSize; /**< module: bytes available in localLink */
  void *userdef;     /**< module: free for its own use */
  httrackp *opt;     /**< engine: options of the mirror in progress */

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
    the tree uses them, because a plug-in is installed through the hts_plug()
    and hts_unplug() entry points declared in htsdefines.h. */
typedef int (*t_htsWrapperInit)(char *fn, char *args);

typedef int (*t_htsWrapperExit)(void);

typedef int (*t_htsWrapperPlugInit)(char *args);

/* Engine-internal declarations */
#ifdef HTS_INTERNAL_BYTECODE

#include "htsglobal.h"

/** Capabilities string of the build ("-noV6", "-nossl", ...) followed by
    "+name" for each module loaded into @p opt. The result is a scratch buffer
    inside @p opt, so never free it and read it before the next call. */
HTSEXT_API const char *hts_get_version_info(httrackp *opt);

/** Capabilities string of the build, without the loaded-module list. The engine
    owns it for the life of the process, and it stays empty until htspe_init()
    has run. */
HTSEXT_API const char *hts_is_available(void);

/** Prepares the module subsystem, which must happen before hts_is_available()
    or any module load. Calling it again does nothing. */
extern void htspe_init(void);

/** Tear-down counterpart of htspe_init(). */
extern void htspe_uninit(void);

/** Offers the object described by @p str to the loaded parser modules.
    @return 1 when a module parsed it, 0 when the module that claimed it failed
    and left the reason in err_msg, and -1 when none claimed it or -%w
    blacklists the one that did. */
extern int hts_parse_externals(htsmoduleStruct *str);

/** The HTS_INET6 value the library itself was built with. */
extern int V6_is_available;
#endif

#ifdef __cplusplus
}
#endif

#endif
