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
/* File: htsmodules.c subroutines:                              */
/*       external modules (parsers)                             */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef _WIN32
#if HTS_DLOPEN
#include <dlfcn.h>
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "htsglobal.h"
#include "htsmodules.h"
#include "htsopt.h"
extern int fspc(FILE* fp,char* type);

/* >>> Put all modules definitions here */
#include "htszlib.h"
#include "htsbase.h"

typedef int (*t_hts_detect_swf)(htsmoduleStruct* str);
typedef int (*t_hts_parse_swf)(htsmoduleStruct* str);
/* <<< */

/* >>> Put all modules includes here */
#include "htsjava.h"
#if HTS_USESWF
#endif
/* <<< */

/* >>> Put all modules variables here */

int swf_is_available = 0;
t_hts_detect_swf hts_detect_swf = NULL;
t_hts_parse_swf hts_parse_swf = NULL;

int gz_is_available = 0;
t_gzopen gzopen = NULL;
t_gzread gzread = NULL;
t_gzclose gzclose = NULL;

int SSL_is_available = 0;
t_SSL_shutdown SSL_shutdown = NULL;
t_SSL_free SSL_free = NULL;
t_SSL_CTX_ctrl SSL_CTX_ctrl = NULL;
t_SSL_new SSL_new = NULL;
t_SSL_clear SSL_clear = NULL;
t_SSL_set_fd SSL_set_fd = NULL;
t_SSL_set_connect_state SSL_set_connect_state = NULL;
t_SSL_connect SSL_connect = NULL;
t_SSL_get_error SSL_get_error = NULL;
t_SSL_write SSL_write = NULL;
t_SSL_read SSL_read = NULL;
t_SSL_library_init SSL_library_init = NULL;
t_ERR_load_crypto_strings ERR_load_crypto_strings = NULL;
t_ERR_load_SSL_strings ERR_load_SSL_strings = NULL;
t_SSLv23_client_method SSLv23_client_method = NULL;
t_SSL_CTX_new SSL_CTX_new = NULL;
t_ERR_error_string ERR_error_string = NULL;
t_SSL_load_error_strings SSL_load_error_strings = NULL;

int V6_is_available = HTS_INET6;

char WHAT_is_available[64]="";
/* <<< */

/* memory checks */
HTSEXT_API htsErrorCallback htsCallbackErr = NULL;
HTSEXT_API int htsMemoryFastXfr = 1;    /* fast xfr by default */
void abortLog__fnc(char* msg, char* file, int line);
void abortLog__fnc(char* msg, char* file, int line) {
  FILE* fp = fopen("CRASH.TXT", "wb");
  if (!fp) fp = fopen("/tmp/CRASH.TXT", "wb");
  if (!fp) fp = fopen("C:\\CRASH.TXT", "wb");
  if (fp) {
    fprintf(fp, "HTTrack " HTTRACK_VERSIONID " closed at '%s', line %d\r\n", file, line);
    fprintf(fp, "Reason:\r\n%s\r\n", msg);
    fflush(fp);
    fclose(fp);
  }
}
HTSEXT_API t_abortLog abortLog__ = abortLog__fnc;    /* avoid VC++ inlining */

static void htspe_log(htsmoduleStruct* str, char* msg);

int hts_parse_externals(htsmoduleStruct* str) {
  /* >>> Put all module calls here */

  /* JAVA */
  if (hts_detect_java(str)) {
    htspe_log(str, "java-lib");
    return hts_parse_java(str);
  }

#if HTS_USESWF
  /* FLASH 
   (external module derivated from Macromedia(tm)'s classes) 
  */
  else if (swf_is_available && hts_detect_swf(str)) {
    htspe_log(str, "swf-lib");
    return hts_parse_swf(str);
  }
#endif

  /* <<< */
  
  /* Not detected */
  return -1;
}

/* NOTE: handled NOT closed */
void* getFunctionPtr(char* file_, char* fncname) {
  char file[1024];
  void* handle;
  void* userfunction = NULL;
  strcpybuff(file, file_);
#ifdef _WIN32
  handle = LoadLibrary(file);
  if (handle == NULL) {
    strcatbuff(file, ".dll");
    handle = LoadLibrary(file);
  }
#else
  handle = dlopen(file, RTLD_LAZY);
  if (handle == NULL) {
    strcatbuff(file, ".so");
    handle = dlopen(file, RTLD_LAZY);
  }
#endif
  if (handle) {
    userfunction = (void*) DynamicGet(handle, fncname);
    if (userfunction == NULL) {
#ifdef _WIN32
      FreeLibrary(handle);
#else
      dlclose(handle);
#endif
    }
  }
  return userfunction;
}

void htspe_init() {
  static int initOk = 0;
  if (!initOk) {
    initOk = 1;
    
    /* >>> Put all module initializations here */
    
    /* Zlib */
#if HTS_DLOPEN
    {
      void* handle;
#ifdef _WIN32
      handle = LoadLibrary("zlib");
#else
      handle = dlopen("libz.so.1", RTLD_LAZY);
#endif
      if (handle) {
        gzopen = (t_gzopen) DynamicGet(handle, "gzopen");
        gzread = (t_gzread) DynamicGet(handle, "gzread");
        gzclose = (t_gzclose) DynamicGet(handle, "gzclose");
        if (gzopen && gzread && gzclose) {
          gz_is_available = 1;
        }
      }
    }
#endif

    /* OpenSSL */
#if HTS_DLOPEN
    {
      void* handle;
#ifdef _WIN32
      handle = LoadLibrary("ssleay32");
#else
      /* We are compatible with 0.9.6/7 and potentially above */
      handle = dlopen("libssl.so.0.9.7", RTLD_LAZY);
      if (handle == NULL) {
        handle = dlopen("libssl.so.0.9.6", RTLD_LAZY);
      }
      if (handle == NULL) {
        /* Try harder */
        handle = dlopen("libssl.so.0", RTLD_LAZY);
      }
#endif
      if (handle) {
        SSL_shutdown = (t_SSL_shutdown) DynamicGet(handle, "SSL_shutdown");
        SSL_free = (t_SSL_free) DynamicGet(handle, "SSL_free");
        SSL_new = (t_SSL_new) DynamicGet(handle, "SSL_new");
        SSL_clear = (t_SSL_clear) DynamicGet(handle, "SSL_clear");
        SSL_set_fd = (t_SSL_set_fd) DynamicGet(handle, "SSL_set_fd");
        SSL_set_connect_state = (t_SSL_set_connect_state) DynamicGet(handle, "SSL_set_connect_state");
        SSL_connect = (t_SSL_connect) DynamicGet(handle, "SSL_connect");
        SSL_get_error = (t_SSL_get_error) DynamicGet(handle, "SSL_get_error");
        SSL_write = (t_SSL_write) DynamicGet(handle, "SSL_write");
        SSL_read = (t_SSL_read) DynamicGet(handle, "SSL_read");
        SSL_library_init = (t_SSL_library_init) DynamicGet(handle, "SSL_library_init");
        ERR_load_SSL_strings = (t_ERR_load_SSL_strings) DynamicGet(handle, "ERR_load_SSL_strings");
        SSLv23_client_method = (t_SSLv23_client_method) DynamicGet(handle, "SSLv23_client_method");
        SSL_CTX_new  = (t_SSL_CTX_new) DynamicGet(handle, "SSL_CTX_new");
        SSL_load_error_strings = (t_SSL_load_error_strings) DynamicGet(handle, "SSL_load_error_strings");
        SSL_CTX_ctrl = (t_SSL_CTX_ctrl) DynamicGet(handle, "SSL_CTX_ctrl");
#ifdef _WIN32
        handle = LoadLibrary("libeay32");
#endif
        ERR_load_crypto_strings = (t_ERR_load_crypto_strings) DynamicGet(handle, "ERR_load_crypto_strings");
        ERR_error_string = (t_ERR_error_string) DynamicGet(handle, "ERR_error_string");

        if (SSL_shutdown && SSL_free && SSL_CTX_ctrl && SSL_new && SSL_clear && 
          SSL_set_fd && SSL_set_connect_state && SSL_connect && SSL_get_error && SSL_write 
          && SSL_read && SSL_library_init && SSLv23_client_method && SSL_CTX_new
          && SSL_load_error_strings && ERR_error_string) {
          SSL_is_available = 1;
        }
      }
    }
#endif
    /* */
    
    /* 
    FLASH
    Load the library on-the-fly, if available
    If not, that's not a problem
    */
#if HTS_DLOPEN
    {
#ifdef _WIN32
      void* handle = LoadLibrary("htsswf");
#else
      void* handle = dlopen("libhtsswf.so.1", RTLD_LAZY);
#endif
      if (handle) {
        hts_detect_swf = (t_hts_detect_swf) DynamicGet(handle, "hts_detect_swf");
        hts_parse_swf = (t_hts_parse_swf) DynamicGet(handle, "hts_parse_swf");
        if (hts_detect_swf && hts_parse_swf) {
          swf_is_available = 1;
        }
      }
      // FreeLibrary(handle);
      // dlclose(handle);
    }
#endif

    /* <<< */

    /* Options availability */
    sprintf(WHAT_is_available, "%s%s%s%s",
      V6_is_available ? "" : "-noV6",
      gz_is_available ? "" : "-nozip",
      SSL_is_available ? "" : "-nossl",
      swf_is_available ? "+swf" : "");

    
  }
}

static void htspe_log(htsmoduleStruct* str, char* msg) {
  char* savename = str->filename;
  httrackp* opt = (httrackp*) str->opt;
  if ((opt->debug>1) && (opt->log!=NULL)) {
    fspc(opt->log,"debug"); fprintf(opt->log,"(External module): parsing %s using module %s"LF, 
      savename, msg);
  }
}

HTSEXT_API const char* hts_is_available(void) {
  return WHAT_is_available;
}
