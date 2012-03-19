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
/* File: htsparse.h parser                                      */
/*       html/javascript/css parser                             */
/*       and other parser routines                              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */


typedef struct htsmoduleStructExtended {
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

} htsmoduleStructExtended;


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
                     char* former_adr, char* former_fil, 
                     int* forbidden_url);


/* Context state */

#define ENGINE_LOAD_CONTEXT_BASE() \
  lien_url** liens = (lien_url**) str->liens; \
  httrackp* opt = (httrackp*) str->opt; \
  struct_back* sback = (struct_back*) str->sback; \
  lien_back* const back = sback->lnk; \
  const int back_max = sback->count; \
  cache_back* cache = (cache_back*) str->cache; \
  hash_struct* hashptr = (hash_struct*) str->hashptr; \
  int numero_passe = str->numero_passe; \
  int add_tab_alloc = str->add_tab_alloc; \
  /* */ \
  int lien_tot = * ( (int*) (str->lien_tot_) ); \
  int ptr = * ( (int*) (str->ptr_) ); \
  int lien_size = * ( (int*) (str->lien_size_) ); \
  char* lien_buffer = * ( (char**) (str->lien_buffer_) )

#define ENGINE_SAVE_CONTEXT_BASE() \
  /* Apply changes */ \
  * ( (int*) (str->lien_tot_) ) = lien_tot; \
  * ( (int*) (str->ptr_) ) = ptr; \
  * ( (int*) (str->lien_size_) ) = lien_size; \
  * ( (char**) (str->lien_buffer_) ) = lien_buffer

#define WAIT_FOR_AVAILABLE_SOCKET() do { \
  int prev = _hts_in_html_parsing; \
  while(back_pluggable_sockets_strict(sback, opt) <= 0) { \
    _hts_in_html_parsing = 6; \
    /* Wait .. */ \
    back_wait(sback,opt,cache,0); \
    /* Transfer rate */ \
    engine_stats(); \
    /* Refresh various stats */ \
    HTS_STAT.stat_nsocket=back_nsoc(sback); \
    HTS_STAT.stat_errors=fspc(NULL,"error"); \
    HTS_STAT.stat_warnings=fspc(NULL,"warning"); \
    HTS_STAT.stat_infos=fspc(NULL,"info"); \
    HTS_STAT.nbk=backlinks_done(sback,liens,lien_tot,ptr); \
    HTS_STAT.nb=back_transfered(HTS_STAT.stat_bytes,sback); \
    /* Check */ \
    if (!hts_htmlcheck_loop(sback->lnk, sback->count, -1,ptr,lien_tot,(int) (time_local()-HTS_STAT.stat_timestart),&HTS_STAT)) { \
      return -1; \
    } \
  } \
  _hts_in_html_parsing = prev; \
} while(0)

#endif
