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
/* File: Some defines for httrack.c and others                  */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

// Fichier librairie .h
#ifndef HTS_DEFINES_DEFH
#define HTS_DEFINES_DEFH

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_back
#define HTS_DEF_FWSTRUCT_lien_back
typedef struct lien_back lien_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_htsblk
#define HTS_DEF_FWSTRUCT_htsblk
typedef struct htsblk htsblk;
#endif
#ifndef HTS_DEF_FWSTRUCT_hts_stat_struct
#define HTS_DEF_FWSTRUCT_hts_stat_struct
typedef struct hts_stat_struct hts_stat_struct;
#endif
#ifndef HTS_DEF_FWSTRUCT_htsmoduleStruct
#define HTS_DEF_FWSTRUCT_htsmoduleStruct
typedef struct htsmoduleStruct htsmoduleStruct;
#endif
#ifndef HTS_DEF_FWSTRUCT_t_hts_callbackarg
#define HTS_DEF_FWSTRUCT_t_hts_callbackarg
typedef struct t_hts_callbackarg t_hts_callbackarg;
#endif
#ifndef HTS_DEF_FWSTRUCT_t_hts_callbackarg
#define HTS_DEF_FWSTRUCT_t_hts_callbackarg
typedef struct t_hts_callbackarg t_hts_callbackarg;
#endif

/* External callbacks */
#ifndef EXTERNAL_FUNCTION
#ifdef _WIN32
#define EXTERNAL_FUNCTION __declspec(dllexport)
#else
#define EXTERNAL_FUNCTION
#endif
#endif

/* --wrapper plug function prototype */

typedef int (*t_hts_plug) (httrackp * opt, const char *argv);
typedef int (*t_hts_unplug) (httrackp * opt);

/* htsopt function callbacks definitions */

typedef void (*t_hts_htmlcheck_init) (t_hts_callbackarg * carg);
typedef void (*t_hts_htmlcheck_uninit) (t_hts_callbackarg * carg);
typedef int (*t_hts_htmlcheck_start) (t_hts_callbackarg * carg, httrackp * opt);
typedef int (*t_hts_htmlcheck_end) (t_hts_callbackarg * carg, httrackp * opt);
typedef int (*t_hts_htmlcheck_chopt) (t_hts_callbackarg * carg, httrackp * opt);
typedef int (*t_hts_htmlcheck_process) (t_hts_callbackarg * carg,
                                        httrackp * opt, char **html, int *len,
                                        const char *url_adresse,
                                        const char *url_fichier);
typedef t_hts_htmlcheck_process t_hts_htmlcheck_preprocess;
typedef t_hts_htmlcheck_process t_hts_htmlcheck_postprocess;
typedef int (*t_hts_htmlcheck_check_html) (t_hts_callbackarg * carg,
                                           httrackp * opt, char *html, int len,
                                           const char *url_adresse,
                                           const char *url_fichier);
typedef const char *(*t_hts_htmlcheck_query) (t_hts_callbackarg * carg,
                                              httrackp * opt,
                                              const char *question);
typedef const char *(*t_hts_htmlcheck_query2) (t_hts_callbackarg * carg,
                                               httrackp * opt,
                                               const char *question);
typedef const char *(*t_hts_htmlcheck_query3) (t_hts_callbackarg * carg,
                                               httrackp * opt,
                                               const char *question);
typedef int (*t_hts_htmlcheck_loop) (t_hts_callbackarg * carg, httrackp * opt,
                                     lien_back * back, int back_max,
                                     int back_index, int lien_tot,
                                     int lien_ntot, int stat_time,
                                     hts_stat_struct * stats);
typedef int (*t_hts_htmlcheck_check_link) (t_hts_callbackarg * carg,
                                           httrackp * opt, const char *adr,
                                           const char *fil, int status);
typedef int (*t_hts_htmlcheck_check_mime) (t_hts_callbackarg * carg,
                                           httrackp * opt, const char *adr,
                                           const char *fil, const char *mime,
                                           int status);
typedef void (*t_hts_htmlcheck_pause) (t_hts_callbackarg * carg, httrackp * opt,
                                       const char *lockfile);
typedef void (*t_hts_htmlcheck_filesave) (t_hts_callbackarg * carg,
                                          httrackp * opt, const char *file);
typedef void (*t_hts_htmlcheck_filesave2) (t_hts_callbackarg * carg,
                                           httrackp * opt, const char *hostname,
                                           const char *filename,
                                           const char *localfile, int is_new,
                                           int is_modified, int not_updated);
typedef int (*t_hts_htmlcheck_linkdetected) (t_hts_callbackarg * carg,
                                             httrackp * opt, char *link);
typedef int (*t_hts_htmlcheck_linkdetected2) (t_hts_callbackarg * carg,
                                              httrackp * opt, char *link,
                                              const char *tag_start);
typedef int (*t_hts_htmlcheck_xfrstatus) (t_hts_callbackarg * carg,
                                          httrackp * opt, lien_back * back);
typedef int (*t_hts_htmlcheck_savename) (t_hts_callbackarg * carg,
                                         httrackp * opt,
                                         const char *adr_complete,
                                         const char *fil_complete,
                                         const char *referer_adr,
                                         const char *referer_fil, char *save);
typedef t_hts_htmlcheck_savename t_hts_htmlcheck_extsavename;
typedef int (*t_hts_htmlcheck_sendhead) (t_hts_callbackarg * carg,
                                         httrackp * opt, char *buff,
                                         const char *adr, const char *fil,
                                         const char *referer_adr,
                                         const char *referer_fil,
                                         htsblk * outgoing);
typedef int (*t_hts_htmlcheck_receivehead) (t_hts_callbackarg * carg,
                                            httrackp * opt, char *buff,
                                            const char *adr, const char *fil,
                                            const char *referer_adr,
                                            const char *referer_fil,
                                            htsblk * incoming);

/* External additional parsing module(s) */
typedef int (*t_hts_htmlcheck_detect) (t_hts_callbackarg * carg, httrackp * opt,
                                       htsmoduleStruct * str);
typedef int (*t_hts_htmlcheck_parse) (t_hts_callbackarg * carg, httrackp * opt,
                                      htsmoduleStruct * str);

/* Callbacks */
#ifndef HTS_DEF_FWSTRUCT_t_hts_htmlcheck_callbacks
#define HTS_DEF_FWSTRUCT_t_hts_htmlcheck_callbacks
typedef struct t_hts_htmlcheck_callbacks t_hts_htmlcheck_callbacks;
#endif

/* Callabck array */
#define DEFCALLBACK(NAME)         \
  struct NAME {                   \
    t_hts_htmlcheck_ ##NAME fun;  \
    t_hts_callbackarg *carg;      \
  } NAME

/* Callback items */
typedef void *t_hts_htmlcheck_t_hts_htmlcheck_callbacks_item;
typedef DEFCALLBACK(t_hts_htmlcheck_callbacks_item);

/* Linked list, which should be used for the 'arg' user-defined argument */
struct t_hts_callbackarg {
  /* User-defined agument for the called function */
  void *userdef;

  /* Previous function, if any (fun != NULL) */
  struct prev {
    void *fun;
    t_hts_callbackarg *carg;
  } prev;
};

/* Callback structure */
struct t_hts_htmlcheck_callbacks {
  /* v3.41 */
  DEFCALLBACK(init);
  DEFCALLBACK(uninit);
  DEFCALLBACK(start);
  DEFCALLBACK(end);
  DEFCALLBACK(chopt);
  DEFCALLBACK(preprocess);
  DEFCALLBACK(postprocess);
  DEFCALLBACK(check_html);
  DEFCALLBACK(query);
  DEFCALLBACK(query2);
  DEFCALLBACK(query3);
  DEFCALLBACK(loop);
  DEFCALLBACK(check_link);
  DEFCALLBACK(check_mime);
  DEFCALLBACK(pause);
  DEFCALLBACK(filesave);
  DEFCALLBACK(filesave2);
  DEFCALLBACK(linkdetected);
  DEFCALLBACK(linkdetected2);
  DEFCALLBACK(xfrstatus);
  DEFCALLBACK(savename);
  DEFCALLBACK(sendhead);
  DEFCALLBACK(receivehead);
  DEFCALLBACK(detect);
  DEFCALLBACK(parse);
  /* >3.41 */
  DEFCALLBACK(extsavename);
};

/* Library internal definitions */
#ifdef HTS_INTERNAL_BYTECODE

#ifndef HTS_DEF_FWSTRUCT_t_hts_callback_ref
#define HTS_DEF_FWSTRUCT_t_hts_callback_ref
typedef struct t_hts_callback_ref t_hts_callback_ref;
#endif
struct t_hts_callback_ref {
  const char *name;
  size_t offset;
};

#ifdef __cplusplus
extern "C" {
#endif

extern const t_hts_htmlcheck_callbacks default_callbacks;

#ifdef __cplusplus
}
#endif

#define HT_PRINT(A) strcatbuff(opt->state.HTbuff,A);
#define HT_REQUEST_START opt->state.HTbuff[0]='\0';
#define HT_REQUEST_END
#define HTT_REQUEST_START opt->state.HTbuff[0]='\0';
#define HTT_REQUEST_END
#define HTS_REQUEST_START opt->state.HTbuff[0]='\0';
#define HTS_REQUEST_END
#define HTS_PANIC_PRINTF(S) strcpybuff(opt->state._hts_errmsg,S);

#endif

#endif
