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
/* File: httrack.c subroutines:                                 */
/*       basic authentication: password storage                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */



#ifndef HTSBAUTH_DEFH
#define HTSBAUTH_DEFH 

// robots wizard
#ifndef HTS_DEF_FWSTRUCT_bauth_chain
#define HTS_DEF_FWSTRUCT_bauth_chain
typedef struct bauth_chain bauth_chain;
#endif
struct bauth_chain {
  char prefix[1024];          /* www.foo.com/secure/ */
  char auth[1024];            /* base-64 encoded user:pass */
  struct bauth_chain* next;   /* next element */
};


// buffer pour les cookies et authentification
#ifndef HTS_DEF_FWSTRUCT_t_cookie
#define HTS_DEF_FWSTRUCT_t_cookie
typedef struct t_cookie t_cookie;
#endif
struct t_cookie {
  int max_len;
  char data[32768];
  bauth_chain auth;
};


/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

// cookies
int cookie_add(t_cookie* cookie,char* cook_name,char* cook_value,char* domain,char* path);
int cookie_del(t_cookie* cookie,char* cook_name,char* domain,char* path);
int cookie_load(t_cookie* cookie, const char* path, const char* name);
int cookie_save(t_cookie* cookie,char* name);
void cookie_insert(char* s,char* ins);
void cookie_delete(char* s,int pos);
char* cookie_get(char *buffer, char* cookie_base,int param);
char* cookie_find(char* s,char* cook_name,char* domain,char* path);
char* cookie_nextfield(char* a);

// basic auth
int bauth_add(t_cookie* cookie,char* adr,char* fil,char* auth);
char* bauth_check(t_cookie* cookie,char* adr,char* fil);
char* bauth_prefix(char *buffer, char* adr,char* fil);

#endif

#endif
