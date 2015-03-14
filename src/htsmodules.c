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

extern int fspc(httrackp * opt, FILE * fp, const char *type);

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

#if 0
t_gzopen gzopen = NULL;
t_gzread gzread = NULL;
t_gzclose gzclose = NULL;
#endif

int V6_is_available = HTS_INET6;

static char WHAT_is_available[64] = "";

/* <<< */

HTSEXT_API const char *hts_get_version_info(httrackp * opt) {
  size_t size;
  int i;

  strcpy(opt->state.HTbuff, WHAT_is_available);
  size = strlen(opt->state.HTbuff);
  for(i = 0; i < opt->libHandles.count; i++) {
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

static void htspe_log(htsmoduleStruct * str, const char *msg);

int hts_parse_externals(htsmoduleStruct * str) {
  str->wrapper_name = "wrapper-lib";

  /* External callback */
  if (RUN_CALLBACK1(str->opt, detect, str)) {
    if (str->wrapper_name == NULL)
      str->wrapper_name = "wrapper-lib";
    /* Blacklisted */
    if (multipleStringMatch
        (str->wrapper_name, StringBuff(str->opt->mod_blacklist))) {
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

void clearCallbacks(htscallbacks * chain_);
void clearCallbacks(htscallbacks * chain_) {
  htscallbacks *chain;

  chain = chain_;
  while(chain != NULL) {
    if (chain->exitFnc != NULL) {
      (void) chain->exitFnc();  /* result ignored */
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
  chain = chain_->next;         // Don't free the block #0
  while(chain != NULL) {
    htscallbacks *nextchain = chain->next;

    freet(chain);
    chain = nextchain;
  }
  chain_->next = NULL;          // Empty
}

void *openFunctionLib(const char *file_) {
  void *handle;
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

void closeFunctionLib(void *handle) {
#ifdef _WIN32
  FreeLibrary(handle);
#else
  dlclose(handle);
#endif
}

void *getFunctionPtr(void *handle, const char *fncname_) {
  if (handle) {
    void *userfunction = NULL;
    char *fncname = strdupt(fncname_);

    /* Strip optional comma */
    char *comma;

    if ((comma = strchr(fncname, ',')) != NULL) {       /* empty arg */
      *comma++ = '\0';
    }

    /* the function itself */
    userfunction = (void *) DynamicGet(handle, (char *) fncname);

    freet(fncname);

    return userfunction;
  }
  return NULL;
}

void htspe_init(void) {
  static int initOk = 0;

  if (!initOk) {
    initOk = 1;

    /* See CVE-2010-5252 */
#if (defined(_WIN32) && (!defined(_DEBUG)))
    {
      /* See KB 2389418
         "If this parameter is an empty string (""), the call removes the 
         current directory from the default DLL search order" */
      BOOL (WINAPI*const k32_SetDllDirectoryA)(LPCSTR) = 
        (BOOL (WINAPI *)(LPCSTR))
        GetProcAddress(GetModuleHandle("kernel32.dll"), "SetDllDirectoryA");
      if (k32_SetDllDirectoryA != NULL && !k32_SetDllDirectoryA("")) {
        /* Do no choke on NT or 98SE with KernelEx NT API (James Blough) */
        const DWORD dwVersion = GetVersion();
        const DWORD dwMajorVersion = (DWORD)(LOBYTE(LOWORD(dwVersion)));
        const DWORD dwMinorVersion = (DWORD)(HIBYTE(LOWORD(dwVersion)));
        if (dwMajorVersion >= 5) {
          assertf(!"SetDllDirectory failed");
        }
      }
    }
#endif

    /* Options availability */
    sprintf(WHAT_is_available, "%s%s%s", V6_is_available ? "" : "-noV6",
            "",
#if HTS_USEOPENSSL
            ""
#else
            "-nossl"
#endif
            );
  }
}

void htspe_uninit(void) {
}

static void htspe_log(htsmoduleStruct * str, const char *msg) {
  const char *savename = str->filename;
  httrackp *opt = (httrackp *) str->opt;

  hts_log_print(opt, LOG_DEBUG, "(External module): parsing %s using module %s",
                savename, msg);
}

HTSEXT_API const char *hts_is_available(void) {
  return WHAT_is_available;
}
