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
/* File: htsparse.h parser                                      */
/*       html/javascript/css parser                             */
/*       and other parser routines                              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsglobal.h"

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_htsblk
#define HTS_DEF_FWSTRUCT_htsblk
typedef struct htsblk htsblk;
#endif
#ifndef HTS_DEF_FWSTRUCT_robots_wizard
#define HTS_DEF_FWSTRUCT_robots_wizard
typedef struct robots_wizard robots_wizard;
#endif
#ifndef HTS_DEF_FWSTRUCT_hash_struct
#define HTS_DEF_FWSTRUCT_hash_struct
typedef struct hash_struct hash_struct;
#endif

#ifndef HTS_DEF_FWSTRUCT_htsmoduleStructExtended
#define HTS_DEF_FWSTRUCT_htsmoduleStructExtended
typedef struct htsmoduleStructExtended htsmoduleStructExtended;
#endif
struct htsmoduleStructExtended {
  /* Main object */
  htsblk* r_;

  /* Error handling */
  int* error_;
  int* exit_xh_;
  int* store_errpage_;

  /* Structural */
  int* filptr_;
  char*** filters_;
  robots_wizard* robots_;
  hash_struct* hash_;
  int* lien_max_;

  /* Base & codebase */
  char* base;
  char* codebase;

  /* Index */
  int* makeindex_done_;
  FILE** makeindex_fp_;
  int* makeindex_links_;
  char* makeindex_firstlink_;

  /* Html templates */
  char *template_header_;
  char *template_body_;
  char *template_footer_;

  /* Specific to downloads */
  LLint* stat_fragment_;
  TStamp makestat_time;
  FILE* makestat_fp;
  LLint* makestat_total_;
  int* makestat_lnk_;
  FILE* maketrack_fp;

  /* Function-dependant */
  char* loc_;
  TStamp* last_info_shell_;
  int* info_shell_;

};


/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

/*
  Main parser, attempt to scan links inside the html/css/js file
  Parameters: The public module structure, and the private module variables
*/
int htsparse(htsmoduleStruct* str, htsmoduleStructExtended* stre);

/*
  Check for 301,302.. errors ("moved") and handle them; re-isuue requests, make
  rediretc file, handle filters considerations..
  Parameters: The public module structure, and the private module variables
  Returns 0 upon success
*/
int hts_mirror_check_moved(htsmoduleStruct* str, htsmoduleStructExtended* stre);

/*
  Process user intercations: pause, add link, delete link..
*/
void hts_mirror_process_user_interaction(htsmoduleStruct* str, htsmoduleStructExtended* stre);

/*
  Get the next file on the queue, waiting for it, handling other files in background..
  Parameters: The public module structure, and the private module variables
  Returns 0 upon success
*/
int hts_mirror_wait_for_next_file(htsmoduleStruct* str, htsmoduleStructExtended* stre);

/*
  Wair for (adr, fil, save) to be started, that is, 
  to be ready for naming, having its header MIME type
  If the final URL is to be forbidden, sets 'forbidden_url' to the corresponding value
*/
int hts_wait_delayed(htsmoduleStruct* str, 
                     char* adr, char* fil, char* save, 
                     char* parent_adr, char* parent_fil,
                     char* former_adr, char* former_fil, 
                     int* forbidden_url);


/* Context state */

#define ENGINE_DEFINE_CONTEXT_BASE() \
  lien_url** const liens HTS_UNUSED = (lien_url**) str->liens; \
  httrackp* const opt HTS_UNUSED = (httrackp*) str->opt; \
  struct_back* const sback HTS_UNUSED = (struct_back*) str->sback; \
  lien_back* const back HTS_UNUSED = sback->lnk; \
  const int back_max HTS_UNUSED = sback->count; \
  cache_back* const cache HTS_UNUSED = (cache_back*) str->cache; \
  hash_struct* const hashptr HTS_UNUSED = (hash_struct*) str->hashptr; \
  const int numero_passe HTS_UNUSED = str->numero_passe; \
  const int add_tab_alloc HTS_UNUSED = str->add_tab_alloc; \
  /* variable */ \
  int lien_tot = *str->lien_tot_; \
  int ptr = *str->ptr_; \
  size_t lien_size = *str->lien_size_; \
  char* lien_buffer = *str->lien_buffer_

#define ENGINE_SET_CONTEXT_BASE() \
  lien_tot = *str->lien_tot_; \
  ptr = *str->ptr_; \
  lien_size = *str->lien_size_; \
  lien_buffer = *str->lien_buffer_

#define ENGINE_LOAD_CONTEXT_BASE() \
  ENGINE_DEFINE_CONTEXT_BASE()

#define ENGINE_SAVE_CONTEXT_BASE() \
  /* Apply changes */ \
  * str->lien_tot_ = lien_tot; \
  * str->ptr_ = ptr; \
  * str->lien_size_ = lien_size; \
  * str->lien_buffer_ = lien_buffer

#define WAIT_FOR_AVAILABLE_SOCKET() do { \
  int prev = opt->state._hts_in_html_parsing; \
  while(back_pluggable_sockets_strict(sback, opt) <= 0) { \
    opt->state._hts_in_html_parsing = 6; \
    /* Wait .. */ \
    back_wait(sback,opt,cache,0); \
    /* Transfer rate */ \
    engine_stats(); \
    /* Refresh various stats */ \
    HTS_STAT.stat_nsocket=back_nsoc(sback); \
    HTS_STAT.stat_errors=fspc(opt,NULL,"error"); \
    HTS_STAT.stat_warnings=fspc(opt,NULL,"warning"); \
    HTS_STAT.stat_infos=fspc(opt,NULL,"info"); \
    HTS_STAT.nbk=backlinks_done(sback,liens,lien_tot,ptr); \
    HTS_STAT.nb=back_transfered(HTS_STAT.stat_bytes,sback); \
    /* Check */ \
    if (!RUN_CALLBACK7(opt, loop, sback->lnk, sback->count, -1,ptr,lien_tot,(int) (time_local()-HTS_STAT.stat_timestart),&HTS_STAT)) { \
      return -1; \
    } \
  } \
  opt->state._hts_in_html_parsing = prev; \
} while(0)

#endif
