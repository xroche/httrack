/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2014 Xavier Roche and other contributors

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
/* File: WinHTTrack subroutines:                                */
/*       Crash reporting                                        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "stdafx.h"

// Debugging
#include <windows.h>
#include <dbghelp.h>
#include <Objbase.h>
#include <Shlobj.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "htsglobal.h"

static BOOL ShowFile(const CHAR *const filename) {
  //Load Shell helper
  const HRESULT hr = CoInitialize(NULL);
  HMODULE shell = LoadLibraryA("Shell32");
  if (shell == NULL)
    return FALSE;

  //Find functions
  union {
    FARPROC ptr[3];
    struct {
      PIDLIST_ABSOLUTE (STDAPICALLTYPE*ILCreateFromPath)(PCTSTR);
      HRESULT (STDAPICALLTYPE*SHOpenFolderAndSelectItems)(PCIDLIST_ABSOLUTE, UINT, PCUITEMID_CHILD_ARRAY, DWORD);
      void (STDAPICALLTYPE*ILFree)(PIDLIST_RELATIVE);
    } fun;
  } shfun = {
    GetProcAddress(shell, "ILCreateFromPathA"),
    GetProcAddress(shell, "SHOpenFolderAndSelectItems"),
    GetProcAddress(shell, "ILFree")
  };

  if (shfun.ptr[0] == NULL || shfun.ptr[1] == NULL || shfun.ptr[2] == NULL)
    return FALSE;

  //Courtesy of flashk
  //(http://stackoverflow.com/questions/9355/programatically-select-multiple-files-in-windows-explorer

  //Item to be selected
  PIDLIST_ABSOLUTE file = shfun.fun.ILCreateFromPath(filename);

  //Perform selection
  const HRESULT success = shfun.fun.SHOpenFolderAndSelectItems(file, 0, NULL, 0);

  //Free resources
  shfun.fun.ILFree(file);

  // Free shell32
  FreeLibrary(shell);
  if (SUCCEEDED(hr))
    CoUninitialize();

  return SUCCEEDED(success);
}

static BOOL PrintStack_(char *const print_buffer, 
                        const size_t print_buffer_size) {
  HMODULE kernel32 = LoadLibraryA("Kernel32");
  if (kernel32 == NULL)
    return FALSE;
  union {
    FARPROC ptr;
    VOID (WINAPI *RtlCaptureContext)(PCONTEXT);
  } rtl = { GetProcAddress(kernel32, "RtlCaptureContext") };
  if (rtl.ptr == NULL)
    return FALSE;

  HMODULE dbghelp = LoadLibraryA("dbghelp");
  if (dbghelp == NULL)
    return FALSE;
  union {
    FARPROC ptr[9];
    struct {
      BOOL (WINAPI * SymInitialize)(HANDLE, PCTSTR, BOOL);
      BOOL (WINAPI * StackWalk64)(DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID, PREAD_PROCESS_MEMORY_ROUTINE64, PFUNCTION_TABLE_ACCESS_ROUTINE64, PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64);
      PVOID (WINAPI * SymFunctionTableAccess64)(HANDLE, DWORD64);
      DWORD64 (WINAPI * SymGetModuleBase64)(HANDLE, DWORD64);
      BOOL (WINAPI * SymFromAddr)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
      DWORD (WINAPI * UnDecorateSymbolName)(PCTSTR, PTSTR, DWORD, DWORD);
      BOOL (WINAPI * EnumerateLoadedModules64)(HANDLE, PENUMLOADED_MODULES_CALLBACK64, PVOID);
      BOOL (WINAPI * SymGetLineFromAddr64)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);
      BOOL (WINAPI * SymGetModuleInfo64)(HANDLE, DWORD64, PIMAGEHLP_MODULE64);
    } fun;
  } sym = {
    GetProcAddress(dbghelp, "SymInitialize"),
    GetProcAddress(dbghelp, "StackWalk64"),
    GetProcAddress(dbghelp, "SymFunctionTableAccess64"),
    GetProcAddress(dbghelp, "SymGetModuleBase64"),
    GetProcAddress(dbghelp, "SymFromAddr"),
    GetProcAddress(dbghelp, "UnDecorateSymbolName"),
    GetProcAddress(dbghelp, "EnumerateLoadedModules64"),
    GetProcAddress(dbghelp, "SymGetLineFromAddr64"),
    GetProcAddress(dbghelp, "SymGetModuleInfo64")
  };
  if (sym.ptr[0] == NULL)
    return FALSE;

  // Initialize dbghelp
  const HANDLE hProcess = GetCurrentProcess();
  sym.fun.SymInitialize(hProcess, NULL, TRUE);

  // Inspired by <http://jpassing.com/2008/03/12/walking-the-stack-of-the-current-thread/>
  DWORD MachineType;
  CONTEXT Context;
  STACKFRAME64 StackFrame;

  rtl.RtlCaptureContext( &Context );

 //
  // Set up stack frame.
  //
  ZeroMemory( &StackFrame, sizeof( STACKFRAME64 ) );
#ifdef _M_IX86
  MachineType                 = IMAGE_FILE_MACHINE_I386;
  StackFrame.AddrPC.Offset    = Context.Eip;
  StackFrame.AddrPC.Mode      = AddrModeFlat;
  StackFrame.AddrFrame.Offset = Context.Ebp;
  StackFrame.AddrFrame.Mode   = AddrModeFlat;
  StackFrame.AddrStack.Offset = Context.Esp;
  StackFrame.AddrStack.Mode   = AddrModeFlat;
#elif _M_X64
  MachineType                 = IMAGE_FILE_MACHINE_AMD64;
  StackFrame.AddrPC.Offset    = Context.Rip;
  StackFrame.AddrPC.Mode      = AddrModeFlat;
  StackFrame.AddrFrame.Offset = Context.Rsp;
  StackFrame.AddrFrame.Mode   = AddrModeFlat;
  StackFrame.AddrStack.Offset = Context.Rsp;
  StackFrame.AddrStack.Mode   = AddrModeFlat;
#elif _M_IA64
  MachineType                 = IMAGE_FILE_MACHINE_IA64;
  StackFrame.AddrPC.Offset    = Context.StIIP;
  StackFrame.AddrPC.Mode      = AddrModeFlat;
  StackFrame.AddrFrame.Offset = Context.IntSp;
  StackFrame.AddrFrame.Mode   = AddrModeFlat;
  StackFrame.AddrBStore.Offset= Context.RsBSP;
  StackFrame.AddrBStore.Mode  = AddrModeFlat;
  StackFrame.AddrStack.Offset = Context.IntSp;
  StackFrame.AddrStack.Mode   = AddrModeFlat;
#else
  #error "Unsupported platform"
#endif

  DWORD64 Stack[256];
  SIZE_T StackCount = 0;

  //
  // Dbghelp is is singlethreaded, so acquire a lock.
  //
  // Note that the code assumes that 
  // SymInitialize( GetCurrentProcess(), NULL, TRUE ) has 
  // already been called.
  //
  while(StackCount < sizeof(Stack) / sizeof(Stack[0])
    && sym.fun.StackWalk64 != NULL
    && sym.fun.StackWalk64(MachineType,
                           GetCurrentProcess(),
                           GetCurrentThread(),
                           &StackFrame,
                           MachineType == IMAGE_FILE_MACHINE_I386 
                           ? NULL
                           : &Context,
                           NULL,
                           sym.fun.SymFunctionTableAccess64,
                           sym.fun.SymGetModuleBase64,
                           NULL)
    && StackFrame.AddrPC.Offset != 0
    && StackFrame.AddrReturn.Offset != 0
    && StackFrame.AddrPC.Offset != StackFrame.AddrReturn.Offset
    )
  {
    Stack[StackCount++] = StackFrame.AddrPC.Offset;
  }

  // Now print information
  PSYMBOL_INFO pSymbol = (PSYMBOL_INFO) calloc(sizeof(*pSymbol) + MAX_SYM_NAME + 1, 1);
  pSymbol->MaxNameLen = MAX_SYM_NAME;
  pSymbol->SizeOfStruct = sizeof(*pSymbol);

  IMAGEHLP_LINE64 pIHLine;
  ZeroMemory(&pIHLine, sizeof(pIHLine));
  pIHLine.SizeOfStruct = sizeof(pIHLine);

  IMAGEHLP_MODULE64 pIHModule;
  ZeroMemory(&pIHModule, sizeof(pIHModule));
  pIHModule.SizeOfStruct = sizeof(pIHModule);

  CHAR *undecoratedName = (CHAR*) malloc(MAX_SYM_NAME);

  size_t print_buffer_offs = 0;
  for(SIZE_T i = 0 ; i < StackCount ; i++) {
    const char *function = "unknown";
    const char *file = "";
    const char *module = "";
    int line = 0;

    const DWORD64 dwAddr = Stack[i];

    if (sym.fun.SymGetModuleInfo64 != NULL
      && sym.fun.SymGetModuleInfo64(hProcess, dwAddr, &pIHModule)) {
      module = pIHModule.ModuleName;
    }

    DWORD64 displacement;
    if (sym.fun.SymFromAddr != NULL
      && sym.fun.SymFromAddr(hProcess, dwAddr, &displacement, pSymbol)) {
      if (sym.fun.UnDecorateSymbolName(pSymbol->Name, undecoratedName, MAX_SYM_NAME, UNDNAME_NAME_ONLY)) {
        function = undecoratedName;
      } else {
        function = pSymbol->Name;
      }
    }

    DWORD wdisplacement = (DWORD) displacement;
    if (sym.fun.SymGetLineFromAddr64 != NULL
      && sym.fun.SymGetLineFromAddr64(hProcess, dwAddr, &wdisplacement, &pIHLine)) {
      if (pIHLine.FileName != NULL) {
        file = pIHLine.FileName;
        line = (int) pIHLine.LineNumber;
      }
    }

    char lines[32];
    sprintf(lines, "%d", line);

    if (print_buffer_size - print_buffer_offs 
      < strlen(function) + strlen(file) + strlen(lines) + strlen(module) + 8)
      break;

#undef ADD_STR
#define ADD_STR(STR) do { \
    strcpy(&print_buffer[print_buffer_offs], STR); \
    print_buffer_offs += strlen(&print_buffer[print_buffer_offs]); \
    } while(0)
    ADD_STR(function);
    if (file != NULL && file[0] != '\0') {
      ADD_STR(" (");
      ADD_STR(file);
      ADD_STR(":");
      ADD_STR(lines);
      ADD_STR(")");
    }
    if (module != NULL && module[0] != '\0') {
      ADD_STR(" [");
      ADD_STR(module);
      ADD_STR("]");
    }
    ADD_STR("\r\n");
#undef ADD_STR
  }

  free(pSymbol);
  free(undecoratedName);

  FreeLibrary(kernel32);
  FreeLibrary(dbghelp);

  return TRUE;
}

static CRITICAL_SECTION DbgHelpLock;

static void PrintStackInit() {
  InitializeCriticalSection(&DbgHelpLock);
}

static BOOL PrintStack(char *const print_buffer, 
                       const size_t print_buffer_size) {
  EnterCriticalSection( &DbgHelpLock );
  BOOL ret = PrintStack_(print_buffer, print_buffer_size);
  LeaveCriticalSection( &DbgHelpLock );
  return ret;
}

void CrashReportReportEx(const char* msg, const char* file, int line, const char *trace) {
  // Produce audit file
  const size_t filename_max = 32;
  CHAR path[MAX_PATH + 1 + filename_max];
  if (GetTempPath(sizeof(path) - filename_max, path) != 0) {
    FILE *fp;
    strcat(path, "CRASH.TXT");
    if ((fp = fopen(path, "wb")) != NULL) {
      fprintf(fp, "HTTrack " HTTRACK_VERSIONID " closed at '%s', line %d\r\n",
        file, line);
      fprintf(fp, "Reason: %s\r\n\r\n", msg);
      if (trace != NULL) {
        fprintf(fp, "Stack trace:\r\n%s", trace);
      }
      fflush(fp);
      fclose(fp);
    }
    (void) ShowFile(path);
  } else {
    strcpy(path, "[unable to save]");
  }

  // Display message box
  CString st;
  st.Format("A fatal error has occured\r\n%s"
    "\r\nin %s:%d\r\n"
    "Please report the problem at http://forum.httrack.com\r\n"
    "using the %s file generated\r\n"
    "Thank you!", msg, file, line, path);
  AfxMessageBox(st, MB_OK|MB_APPLMODAL|MB_SYSTEMMODAL|MB_ICONSTOP);
}

void CrashReportReport(const char* msg, const char* file, int line) {
  static char buffer[2048];
  char *trace = NULL;
  if (PrintStack(buffer, sizeof(buffer))) {
    trace = buffer;
  }
  CrashReportReportEx(msg, file, line, trace);
}

static LONG WINAPI GlobalExceptionHandler(PEXCEPTION_POINTERS pExceptPtrs) {
  switch(pExceptPtrs->ExceptionRecord->ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_STACK_OVERFLOW:
      CrashReportReport("Top-level exception caught", "unknown", 0);
      return EXCEPTION_CONTINUE_SEARCH;
      break;
    default:
      return EXCEPTION_CONTINUE_SEARCH;
      break;
  }
}

static BOOL GlobalExceptionHandlerInit() {
  HMODULE kernel32 = LoadLibraryA("Kernel32");
  if (kernel32 == NULL)
    return FALSE;
  union {
    FARPROC ptr;
    LPTOP_LEVEL_EXCEPTION_FILTER (WINAPI * SetUnhandledExceptionFilter)(LPTOP_LEVEL_EXCEPTION_FILTER);
  } se = { GetProcAddress(kernel32, "SetUnhandledExceptionFilter") };
  if (se.ptr == NULL)
    return FALSE;
  se.SetUnhandledExceptionFilter(GlobalExceptionHandler);
  return TRUE;
}

void CrashReportInit() {
  PrintStackInit();
  GlobalExceptionHandlerInit();
}
