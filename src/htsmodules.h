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
/* File: htsmodules.h subroutines:                              */
/*       external modules (parsers)                             */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_MODULES
#define HTS_MODULES

/* Forware definitions */
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

/* Function type to add links inside the module 
  link : link to add (absolute or relative)
  str : structure defined below
  Returns 1 if the link was added, 0 if not
*/
#ifndef HTS_DEF_FWSTRUCT_htsmoduleStruct
#define HTS_DEF_FWSTRUCT_htsmoduleStruct
typedef struct htsmoduleStruct htsmoduleStruct;
#endif
typedef int (*t_htsAddLink) (htsmoduleStruct * str, char *link);

/* Structure passed to the module */
struct htsmoduleStruct {
  /* Read-only elements */
  const char *filename;         /* filename (C:\My Web Sites\...) */
  int size;                     /* size of filename (should be > 0) */
  const char *mime;             /* MIME type of the object */
  const char *url_host;         /* incoming hostname (www.foo.com) */
  const char *url_file;         /* incoming filename (/bar/bar.gny) */

  /* Write-only */
  const char *wrapper_name;     /* name of wrapper (static string) */
  char *err_msg;                /* if an error occured, the error message (max. 1KB) */

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

/* Used to wrap module initialization */
/* return 1 if init was ok */
typedef int (*t_htsWrapperInit) (char *fn, char *args);
typedef int (*t_htsWrapperExit) (void);
typedef int (*t_htsWrapperPlugInit) (char *args);

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE
HTSEXT_API const char *hts_get_version_info(httrackp * opt);
HTSEXT_API const char *hts_is_available(void);
extern void htspe_init(void);
extern void htspe_uninit(void);
extern int hts_parse_externals(htsmoduleStruct * str);

/*extern int swf_is_available;*/
extern int V6_is_available;
#endif

#ifdef __cplusplus
}
#endif

#endif
