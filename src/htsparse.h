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


typedef struct {
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


