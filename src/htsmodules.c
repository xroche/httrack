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
/* File: htsmodules.c subroutines:                              */
/*       external modules (parsers)                             */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htsglobal.h"
#include "htsmodules.h"
#include "htsopt.h"
#include "htsbasenet.h"
#include "htslib.h"

extern int fspc(httrackp *opt,FILE* fp,const char* type);

#ifndef _WIN32
#if HTS_DLOPEN
#include <dlfcn.h>
#endif
#endif

/* >>> Put all modules definitions here */
#include "htszlib.h"
#include "htsbase.h"
/* <<< */

/* >>> Put all modules variables here */

int gz_is_available = 0;
#if 0
t_gzopen gzopen = NULL;
t_gzread gzread = NULL;
t_gzclose gzclose = NULL;
#endif

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

static char WHAT_is_available[64]="";
/* <<< */

HTSEXT_API const char* hts_get_version_info(httrackp *opt) {
  size_t size;
  int i;
  strcpy(opt->state.HTbuff, WHAT_is_available);
  size = strlen(opt->state.HTbuff);
  for(i = 0 ; i < opt->libHandles.count ; i++) {
    const char *name = opt->libHandles.handles[i].moduleName;
    if (name != NULL) {
      size_t nsize = strlen(name) + sizeof("+");
      size += nsize;
      if (size + 1 >= sizeof(opt->state.HTbuff))
        break;
      strcat(opt->state.HTbuff, "+");
      strcat(opt->state.HTbuff, name);
    }
  }
  return opt->state.HTbuff;
}

/* memory checks */
HTSEXT_API htsErrorCallback htsCallbackErr = NULL;
HTSEXT_API int htsMemoryFastXfr = 1;    /* fast xfr by default */
void abortLog__fnc(char* msg, char* file, int line);
void abortLog__fnc(char* msg, char* file, int line) {
  FILE* fp = fopen("CRASH.TXT", "wb");
  if (!fp) fp = fopen("/tmp/CRASH.TXT", "wb");
  if (!fp) fp = fopen("C:\\CRASH.TXT", "wb");
  if (!fp) fp = fopen("CRASH.TXT", "wb");
  if (fp) {
    fprintf(fp, "HTTrack " HTTRACK_VERSIONID " closed at '%s', line %d\r\n", file, line);
    fprintf(fp, "Reason:\r\n%s\r\n", msg);
    fflush(fp);
    fclose(fp);
  }
}
HTSEXT_API t_abortLog abortLog__ = abortLog__fnc;    /* avoid VC++ inlining */

static void htspe_log(htsmoduleStruct* str, const char* msg);

int hts_parse_externals(htsmoduleStruct* str) {
  str->wrapper_name = "wrapper-lib";

  /* External callback */
  if (RUN_CALLBACK1(str->opt, detect, str)) {
    if (str->wrapper_name == NULL)
      str->wrapper_name = "wrapper-lib";
    /* Blacklisted */
    if (multipleStringMatch(str->wrapper_name, StringBuff(str->opt->mod_blacklist))) {
      return -1;
    } else {
      htspe_log(str, str->wrapper_name);
      return RUN_CALLBACK1(str->opt, parse, str);
    }
  }

  /* Not detected */
  return -1;
}

//static void addCallback(htscallbacks* chain, void* moduleHandle, htscallbacksfncptr exitFnc) {
//  while(chain->next != NULL) {
//    chain = chain->next;
//  }
//  chain->next = calloct(1, sizeof(htscallbacks));
//  assertf(chain->next != NULL);
//  chain = chain->next;
//  memset(chain, 0, sizeof(*chain));
//  chain->exitFnc = exitFnc;
//  chain->moduleHandle = moduleHandle;
//}

void clearCallbacks(htscallbacks* chain_);
void clearCallbacks(htscallbacks* chain_) {
  htscallbacks* chain;
  chain = chain_;
  while(chain != NULL) {
    if (chain->exitFnc != NULL) {
      (void) chain->exitFnc();      /* result ignored */
      chain->exitFnc = NULL;
    }
    chain = chain->next;
  }
  chain = chain_;
  while(chain != NULL) {
    if (chain->moduleHandle != NULL) {
#ifdef _WIN32
      FreeLibrary(chain->moduleHandle);
#else
      dlclose(chain->moduleHandle);
#endif
    }
    chain = chain->next;
  }
  chain = chain_->next;     // Don't free the block #0
  while(chain != NULL) {
    htscallbacks* nextchain = chain->next;
    freet(chain);
    chain = nextchain;
  }
  chain_->next = NULL;    // Empty
}

void* openFunctionLib(const char* file_) {
  void* handle;
  char *file = malloct(strlen(file_) + 32);
  strcpy(file, file_);
#ifdef _WIN32
  handle = LoadLibraryA(file);
  if (handle == NULL) {
    sprintf(file, "%s.dll", file_);
    handle = LoadLibraryA(file);
  }
#else
  handle = dlopen(file, RTLD_LAZY);
  if (handle == NULL) {
    sprintf(file, "lib%s.so", file_);
    handle = dlopen(file, RTLD_LAZY);
  }
#endif
  freet(file);
  return handle;
}

void closeFunctionLib(void* handle) {
#ifdef _WIN32
  FreeLibrary(handle);
#else
  dlclose(handle);
#endif
}

void* getFunctionPtr(void* handle, const char* fncname_) {
  if (handle) {
    void* userfunction = NULL;
    char *fncname = strdupt(fncname_);

    /* Strip optional comma */
    char *comma;
    if ((comma = strchr(fncname, ',')) != NULL) {   /* empty arg */
      *comma++ = '\0';
    }
    
    /* the function itself */
    userfunction = (void*) DynamicGet(handle, (char*)fncname);

    freet(fncname);

    return userfunction;
  }
  return NULL;
}

void* ssl_handle = NULL;
#ifdef _WIN32
void* ssl_handle_2 = NULL;
#endif
void htspe_init(void) {
  static int initOk = 0;
  if (!initOk) {
    initOk = 1;
    
    /* Zlib is now statically linked */
    gz_is_available = 1;

    /* OpenSSL */
#if HTS_DLOPEN
    {
      void* handle;
#ifdef _WIN32
      handle = LoadLibraryA((char*)"ssleay32");
#else
      /* We are compatible with 0.9.6/7/8/8b and potentially above */
      static const char *const libs[] = {
        "libssl.so.1.0",
        "libssl.so.1",
        "libssl.so.1.0.0",
        /* */
        "libssl.so.0",
        "libssl.so.0.9",
        "libssl.so.0.9.8p",
        "libssl.so.0.9.8o",
        "libssl.so.0.9.8n",
        "libssl.so.0.9.8m",
        "libssl.so.0.9.8l",
        "libssl.so.0.9.8k", /* (Debarshi Ray) */
        "libssl.so.0.9.8j", /* (Debarshi Ray) */
        "libssl.so.0.9.8g", /* Added 8g release too (Debarshi Ray) */
        "libssl.so.0.9.8b",
        "libssl.so.0.9.8",
        "libssl.so.0.9.7",
        "libssl.so.0.9.6",
        "libssl.so",        /* Try harder with devel link */
        NULL
      };
      int i;
      for(i = 0, handle = NULL ; handle == NULL && libs[i] != NULL ; i++) {
        handle = dlopen(libs[i], RTLD_LAZY);
      }
#endif
      ssl_handle = handle;
      if (handle != NULL) {
        SSL_shutdown = (t_SSL_shutdown) DynamicGet(handle, (char*)"SSL_shutdown");
        SSL_free = (t_SSL_free) DynamicGet(handle, (char*)"SSL_free");
        SSL_new = (t_SSL_new) DynamicGet(handle, (char*)"SSL_new");
        SSL_clear = (t_SSL_clear) DynamicGet(handle, (char*)"SSL_clear");
        SSL_set_fd = (t_SSL_set_fd) DynamicGet(handle, (char*)"SSL_set_fd");
        SSL_set_connect_state = (t_SSL_set_connect_state) DynamicGet(handle, (char*)"SSL_set_connect_state");
        SSL_connect = (t_SSL_connect) DynamicGet(handle, (char*)"SSL_connect");
        SSL_get_error = (t_SSL_get_error) DynamicGet(handle, (char*)"SSL_get_error");
        SSL_write = (t_SSL_write) DynamicGet(handle, (char*)"SSL_write");
        SSL_read = (t_SSL_read) DynamicGet(handle, (char*)"SSL_read");
        SSL_library_init = (t_SSL_library_init) DynamicGet(handle, (char*)"SSL_library_init");
        ERR_load_SSL_strings = (t_ERR_load_SSL_strings) DynamicGet(handle, (char*)"ERR_load_SSL_strings");
        SSLv23_client_method = (t_SSLv23_client_method) DynamicGet(handle, (char*)"SSLv23_client_method");
        SSL_CTX_new  = (t_SSL_CTX_new) DynamicGet(handle, (char*)"SSL_CTX_new");
        SSL_load_error_strings = (t_SSL_load_error_strings) DynamicGet(handle, (char*)"SSL_load_error_strings");
        SSL_CTX_ctrl = (t_SSL_CTX_ctrl) DynamicGet(handle, (char*)"SSL_CTX_ctrl");
#ifdef _WIN32
        handle = LoadLibraryA((char*)"libeay32");
        ssl_handle_2 = handle;
#endif
        ERR_load_crypto_strings = (t_ERR_load_crypto_strings) DynamicGet(handle, (char*)"ERR_load_crypto_strings");
        ERR_error_string = (t_ERR_error_string) DynamicGet(handle, (char*)"ERR_error_string");

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
    
    /* Options availability */
    sprintf(WHAT_is_available, "%s%s%s",
      V6_is_available ? "" : "-noV6",
      gz_is_available ? "" : "-nozip",
      SSL_is_available ? "" : "-nossl");
  }
}

void htspe_uninit(void) {
#ifdef _WIN32
  CloseHandle(ssl_handle);
  CloseHandle(ssl_handle_2);
  ssl_handle = NULL;
  ssl_handle_2 = NULL;
#else
  dlclose(ssl_handle);
  ssl_handle = NULL;
#endif
}

static void htspe_log(htsmoduleStruct* str, const char* msg) {
  const char* savename = str->filename;
  httrackp* opt = (httrackp*) str->opt;
  if ((opt->debug>1) && (opt->log!=NULL)) {
    HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"(External module): parsing %s using module %s"LF, 
      savename, msg);
  }
}

HTSEXT_API const char* hts_is_available(void) {
  return WHAT_is_available;
}
