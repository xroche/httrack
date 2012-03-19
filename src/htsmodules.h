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
/* File: htsmodules.h subroutines:                              */
/*       external modules (parsers)                             */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_MODULES
#define HTS_MODULES

/* Function type to add links inside the module 
  link : link to add (absolute or relative)
  str : structure defined below
  Returns 1 if the link was added, 0 if not
*/
typedef struct htsmoduleStruct htsmoduleStruct;
typedef int (* t_htsAddLink)(htsmoduleStruct* str, char* link);

/* Structure passed to the module */
struct htsmoduleStruct {
  /* Read-only elements */
  char* filename;                 /* filename (C:\My Web Sites\...) */
  int   size;                     /* size of filename (should be > 0) */
  char* mime;                     /* MIME type of the object */
  char* url_host;                 /* incoming hostname (www.foo.com) */
  char* url_file;                 /* incoming filename (/bar/bar.gny) */
  
  /* Write-only */
  char* err_msg;                  /* if an error occured, the error message (max. 1KB) */
  
  /* Read/Write */
  int relativeToHtmlLink;         /* set this to 1 if all urls you pass to addLink
                                  are in fact relative to the html file where your
                                  module was originally */
  
  /* Callbacks */
  t_htsAddLink addLink;           /* call this function when links are 
                                  being detected. it if not your responsability to decide
                                  if the engine will keep them, or not. */

  /* Optional */
  char* localLink;                /* if non null, the engine will write there the local
                                  relative filename of the link added by addLink(), or
                                  the absolute path if the link was refused by the wizard */
  int localLinkSize;              /* size of the optionnal buffer */
  
  /* User-defined */
  void* userdef;                  /* can be used by callback routines
                                  */

  /* ---- ---- ---- */

  /* Internal use - please don't touch */
  void* liens;
  void* opt;
  void* back;
  int back_max;
  void* cache;
  void* hashptr;
  int numero_passe;
  int add_tab_alloc;
  /* */
  int* lien_tot_;
  int* ptr_;
  int* lien_size_;
  char** lien_buffer_;
  /* Internal use - please don't touch */

};

extern void htspe_init(void);
extern int hts_parse_externals(htsmoduleStruct* str);
extern void* getFunctionPtr(char* file, char* fncname);

extern int gz_is_available;
extern int swf_is_available;
extern int SSL_is_available;
extern int V6_is_available;
extern char WHAT_is_available[64];

#endif
