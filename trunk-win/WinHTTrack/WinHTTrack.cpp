// WinHTTrack.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "WinHTTrack.h"

#include "Shell.h"

#include "wid1.h"
#include "maintab.h"

#include "MainFrm.h"
#include "splitter.h"
#include "about.h"

#include "WinHTTrackDoc.h"
#include "WinHTTrackView.h"

#include "inprogress.h"

// KB955045 (http://support.microsoft.com/kb/955045)
// To execute an application using this function on earlier versions of Windows
// (Windows 2000, Windows NT, and Windows Me/98/95), then it is mandatary to #include Ws2tcpip.h
// and also Wspiapi.h. When the Wspiapi.h header file is included, the 'getaddrinfo' function is
// #defined to the 'WspiapiGetAddrInfo' inline function in Wspiapi.h. 
#include <ws2tcpip.h>
#include <Wspiapi.h>
#ifndef getaddrinfo
#error getaddrinfo "should be defined"
#define getaddrinfo WspiapiGetAddrInfo
#endif

/* HTS - HTTRACK */
extern "C" {
  #include "HTTrackInterface.h"
  //#include "htsbase.h"
  //#include "htsglobal.h"
  //#include "htsthread.h"
};
#include <Ws2tcpip.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//
#include "DialogContainer.h"
#include "InfoUrl.h"

// PAS de domodal a l'exterieur!!!!!
#include "wizard.h"
#include "wizard2.h"
#include "WizLinks.h"
extern char WIZ_question[1000];
extern char WIZ_reponse[1000];


extern Wid1* dialog1;
extern CMainTab* maintab;
extern Cinprogress* inprogress;
extern CShellApp* CShellApp_app;
extern int termine;
extern int termine_requested;
extern int soft_term_requested;
extern int shell_terminated;
extern CInfoUrl* _Cinprogress_inst;
extern int LibRasUse;

/*extern "C" {
  char* hts_rootdir(char* file);
};*/


// rmdir
#include <direct.h>

// linput
/*extern "C" {
  void linput(FILE* fp,char* s,int max);
  void linput_trim(FILE* fp,char* s,int max);
  void linput_cpp(FILE* fp,char* s,int max);
};*/

/* WinHTTrack refresh Mutex */
HANDLE WhttMutex;

/* Location */
char* WhttLocation="";


// HTTrack main vars
HWND App_Main_HWND;
CSplitterFrame* this_CSplitterFrame=NULL;
HICON httrack_icon;
// Helper
LaunchHelp* HtsHelper=NULL;

// dirtreeview
#include "DirTreeView.h"
extern CDirTreeView* this_DirTreeView;

// New Project
#include "NewProj.h"
extern CNewProj* dialog0;


// InfoEnd
#include "infoend.h"
extern Cinfoend* this_Cinfoend;

// Pointeur sur nous
CWinHTTrackApp* this_app=NULL;

// fexist
extern "C" int fexist(const char*);

// Debugging
#include <dbghelp.h>

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackApp

BEGIN_MESSAGE_MAP(CWinHTTrackApp, CWinApp)
	//{{AFX_MSG_MAP(CWinHTTrackApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
	ON_COMMAND(ID_FILE_SAVE_AS, OnFileSaveAs)
	ON_COMMAND(ID_FILE_MRU_FILE1, OnFileMruFile1)
	//}}AFX_MSG_MAP
  ON_COMMAND(wm_ViewRestart,OnViewRestart)
  ON_COMMAND(wm_WizRequest1,OnWizRequest1)
  ON_COMMAND(wm_WizRequest2,OnWizRequest2)
  ON_COMMAND(wm_WizRequest3,OnWizRequest3)
	// Standard file based document commands
	//ON_COMMAND(ID_FILE_WIZARD, OnWizard)
	ON_COMMAND(ID_FILE_NEW, OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_FILE_DELETE_PROJ, OnFileDelete)
	ON_COMMAND(ID_FILE_BROWSE_SIT, OnBrowseWebsites)
  ON_COMMAND(IDC_langprefs,Onipabout)
  ON_COMMAND(ID_ABOUT,Onipabout)
  ON_COMMAND(ID_UPDATE,OnUpdate)
	ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
	ON_COMMAND(ID_HELP,OnHelpInfo2)
	//ON_COMMAND(ID_CONTEXT_HELP,OnContextHelp)
	ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
  // Forward to inprogress
  ON_BN_CLICKED(ID_LOAD_OPTIONS,FwOnLoadprofile)
  ON_BN_CLICKED(ID_FILE_SAVE_OPTIONS_AS,FwOnSaveprofile)
	ON_BN_CLICKED(ID_LoadDefaultOptions, FwOnLoaddefault)
	ON_BN_CLICKED(ID_SaveDefaultOptions, FwOnSavedefault)
	ON_BN_CLICKED(ID_ClearDefaultOptions,FwOnResetdefault)
  //
  ON_BN_CLICKED(ID_WINDOW_HIDE,FwOnhide)
  //
	ON_BN_CLICKED(ID_OPTIONS_MODIFY,FwOnModifyOpt)
	ON_BN_CLICKED(ID_FILE_PAUSE,FwOnPause)
	ON_BN_CLICKED(ID_LOG_VIEWLOG,FwOniplogLog)
	ON_BN_CLICKED(ID_LOG_VIEWERRORLOG,FwOniplogErr)
	ON_BN_CLICKED(ID_LOG_VIEWTRANSFERS,FwOnViewTransfers)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackApp construction

CWinHTTrackApp::CWinHTTrackApp()
{
  // HTTrack inits
  CreateMutex(NULL, FALSE, "WinHTTrack_RUN");
  HtsHelper = new LaunchHelp();
}

CWinHTTrackApp::~CWinHTTrackApp()
{
  DeleteTabs();
  delete HtsHelper;
  HtsHelper=NULL;
	if (global_opt != NULL)
	{
		hts_free_opt(global_opt);
		global_opt = NULL;
	}
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CWinHTTrackApp object

CWinHTTrackApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackApp initialization

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

static CRITICAL_SECTION DbgHelpLock;

static void PrintStackInit() {
  InitializeCriticalSection(&DbgHelpLock);
}

static BOOL PrintStack(char *const print_buffer, 
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
  EnterCriticalSection( &DbgHelpLock );
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
  LeaveCriticalSection( &DbgHelpLock );

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

void InitCBErrMsgEx(const char* msg, const char* file, int line, const char *trace) {
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

void InitCBErrMsg(const char* msg, const char* file, int line) {
  static char buffer[2048];
  char *trace = NULL;
  if (PrintStack(buffer, sizeof(buffer))) {
    trace = buffer;
  }
  InitCBErrMsgEx(msg, file, line, trace);
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
      InitCBErrMsg("Top-level exception caught", "unknown", 0);
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

void InitCBErr() {
  PrintStackInit();
  GlobalExceptionHandlerInit();
  hts_set_error_callback(InitCBErrMsg);

  //InitCBErrMsg("lol", "trololo.c", 1234);
}

int Eval_Exception( void );

int Eval_Exception ( int n_except )
{
    AfxMessageBox("error");

    return 0;
}

#include "mmsystem.h"

BOOL CWinHTTrackApp::InitInstance()
{
  /* No error messageboxes */
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOALIGNMENTFAULTEXCEPT | SEM_NOGPFAULTERRORBOX);

  /* Inits */
  InitCBErr();
  hts_init();

  WhttMutex = CreateMutex(NULL,FALSE,NULL);

  // Change the registry key under which our settings are stored.
  // TODO: You should modify this string to be something appropriate
  // such as the name of your company or organization.
  SetRegistryKey("WinHTTrack Website Copier");
  LANG_INIT();    // petite init langue
  
  /* INDISPENSABLE pour le drag&drop! */
  InitCommonControls();
  if (!AfxOleInit())
  {
	  AfxMessageBox(LANG(LANG_F1));
	  return FALSE;
  }
  AfxEnableControlContainer();
  
  // Pointeur sur CShellApp
  CShellApp_app=&app;
  this_app=this;
  _Cinprogress_inst=NULL;
  LibRasUse=0;

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

  httrack_icon=AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	LoadStdProfileSettings();  // Load standard INI file options (including MRU)

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.

  // DOC //
	CMultiDocTemplate* pDocTemplate;
	pDocTemplate = new CMultiDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CWinHTTrackDoc),
		RUNTIME_CLASS(CSplitterFrame),       // main SDI frame window
		RUNTIME_CLASS(CView)); 
	AddDocTemplate(pDocTemplate);

  /*
	CMDIFrameWnd* pMainFrame = new CMDIFrameWnd;
	if (!pMainFrame->LoadFrame(IDR_MAINFRAME))
		return FALSE;
  */

	// create main window
	CMainFrame* pMainFrame = new CMainFrame;
	if (!pMainFrame->LoadFrame(IDR_MAINFRAME))
		return FALSE;
	m_pMainWnd = pMainFrame;
	int nCmdShow = m_nCmdShow;


  // Also in this example, there is only one menubar shared between
	//  all the views.  The automatic menu enabling support of MFC
	//  will disable the menu items that don't apply based on the
	//  currently active view.  The one MenuBar is used for all
	//  document types, including when there are no open documents.

  // enable file manager drag/drop and DDE Execute open
	pMainFrame->DragAcceptFiles();

  // Now finally show the main menu
	//pMainFrame->ShowWindow(m_nCmdShow);
	//pMainFrame->UpdateWindow();
	m_pMainWnd = pMainFrame;

  // command line arguments are ignored, create a new (empty) document
	//OnFileNew();
  // DOC //

  // Parse command line for standard shell commands, DDE, file open
  CCommandLineInfo cmdInfo;
  ParseCommandLine(cmdInfo);

  TCHAR ModulePath[MAX_PATH + 1];
  ModulePath[0] = '\0';
  ::GetModuleFileName(NULL, ModulePath, sizeof(ModulePath)/sizeof(TCHAR) - 1);
  hts_rootdir(ModulePath);

  // Restore position
	((CMainFrame*)m_pMainWnd)->InitialShowWindow(nCmdShow);
	pMainFrame->UpdateWindow();

	// Dispatch commands specified on the command line
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

  // Init Winsock
  WSockInit();

	// The one and only window has been initialized, so show and update it.
	//m_pMainWnd->ShowWindow(SW_SHOW);
	//m_pMainWnd->UpdateWindow();

  /*CWinApp* app=AfxGetApp();
  POSITION pos;
  pos=app->GetFirstDocTemplatePosition();
  CDocTemplate* templ = app->GetNextDocTemplate(pos);
  pos=templ->GetFirstDocPosition();
  CDocument* doc  = templ->GetNextDoc(pos);

  CRuntimeClass* pRuntimeClass = RUNTIME_CLASS( CTest );
  CObject* pObject = pRuntimeClass->CreateObject();
  ASSERT( pObject->IsKindOf( RUNTIME_CLASS( CTest ) ) );
  
  doc->AddView((CView*) pObject);
  */

  {
    // enable file manager drag/drop and DDE Execute open
    EnableShellOpen();
    RegisterShellFileTypes();

    CWinApp* pApp = AfxGetApp();

    // register "New File" handler
    if (pApp->GetProfileInt("Interface","SetupRun",0) != 1
      || pApp->GetProfileInt("Interface","SetupHasRegistered",0) == 1) {
        HKEY phkResult;
        DWORD creResult;
      if (RegCreateKeyEx(HKEY_CLASSES_ROOT,".whtt",0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&phkResult,&creResult)==ERROR_SUCCESS) {
        RegCloseKey(phkResult);
        if (RegCreateKeyEx(HKEY_CLASSES_ROOT,".whtt\\ShellNew",0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&phkResult,&creResult)==ERROR_SUCCESS) {
          char voidbuff='\0';
          RegSetValueEx(phkResult,"NullFile",0,REG_SZ,(LPBYTE)&voidbuff,1);
          RegCloseKey(phkResult);
        }
      }   
    }

    // Infos la 1ere fois!
    if (pApp->GetProfileInt("Interface","FirstRun",0) != 3) {
      pApp->WriteProfileInt("Interface","FirstRun",3);

      Cabout about;
      about.DoModal();
      
      // Default proxy? Check is the current IP looks local or not.
      BOOL isPublic = FALSE;
      char hostname[256];
      if (gethostname(hostname, sizeof(hostname) - 1) == 0) {
        struct addrinfo* res = NULL;
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        if (getaddrinfo(hostname, NULL, &hints, &res) == 0) {
          if (res->ai_addr != NULL && res->ai_addrlen > 0) {
            if (res->ai_family == AF_INET) {
              sockaddr_in *const si = (sockaddr_in*) res->ai_addr;
              const unsigned char *const ipv4 = (unsigned char*) &si->sin_addr;
              isPublic = ! (
                ipv4[0] == 10  /* 10/8 */
                || (ipv4[0] == 192 && ipv4[1] == 168)  /* 192.168/16 */
                || (ipv4[0] == 172 && ipv4[1] >= 16 && ipv4[1] <= 31)  /* 172.16/12 */
                );
            } else if (res->ai_family == AF_INET6) {  /* assume no more proxy */
              isPublic = TRUE;
            }
          }
        }
        if (res) {
          freeaddrinfo(res);
        }
      }
      if (!isPublic && maintab) {
        maintab->DefineDefaultProxy();
        if (maintab->DoModal()!=IDCANCEL) {
          // Default proxy values
          CString strSection       = "OptionsValues";
          MyWriteProfileString("",strSection, "Proxy",maintab->m_option10.m_proxy);
          MyWriteProfileString("",strSection, "Port",maintab->m_option10.m_port);
        }
        maintab->UnDefineDefaultProxy();
      }
    }
  }
  

#ifdef HTTRACK_AFF_WARNING
#ifndef _DEBUG
  AfxMessageBox("--WARNING--\r\n"HTTRACK_AFF_WARNING);
#endif
#endif

  return TRUE;
}


BOOL CWinHTTrackApp::WSockInit() {
  // Initialiser WINSOCK
  WORD   wVersionRequested; /* requested version WinSock API */ 
  WSADATA wsadata;        /* Windows Sockets API data */
  {
    int stat;
    wVersionRequested = 0x0101;
    stat = WSAStartup( wVersionRequested, &wsadata );
    if (stat != 0) {
      //HTS_PANIC_PRINTF("Winsock not found!\n");
    } else if (LOBYTE(wsadata.wVersion) != 1  && HIBYTE(wsadata.wVersion) != 1) {
      //HTS_PANIC_PRINTF("WINSOCK.DLL does not support version 1.1\n");
      WSACleanup();
    }
  }
  // Fin Initialiser WINSOCK
  return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

/*
class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
		// No message handlers
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
*/

// App command to run the dialog
void CWinHTTrackApp::OnAppAbout()
{
  Cabout about;
  about.DoModal();
//	CAboutDlg aboutDlg;
//	aboutDlg.DoModal();
}


/* Request: Set new view in the splitter window (when clicking on finished, for example) */
/*extern "C" {
  int hts_resetvar(void);
}*/

void CWinHTTrackApp::OnViewRestart() {
  //CloseAllDocuments(FALSE);
  //OnFileNew();

  /* Free library */
  WHTT_LOCK();
  //hts_resetvar();
  WHTT_UNLOCK();

  this_CSplitterFrame->SetNewView(0,1,RUNTIME_CLASS(CDialogContainer));
}

void CWinHTTrackApp::OnWizRequest1() {
  wizard diawiz;
  diawiz.m_question=WIZ_question;
  diawiz.DoModal();
  strcpybuff(WIZ_reponse,diawiz.m_reponse);
}

void CWinHTTrackApp::OnWizRequest2() {
  wizard2 diawiz2;
  diawiz2.m_question=WIZ_question;
  if (diawiz2.DoModal()==IDOK)
  strcpybuff(WIZ_reponse,"YES");
  else
    strcpybuff(WIZ_reponse,"NO");
}

void CWinHTTrackApp::OnWizRequest3() {
  WizLinks diawiz3;
  diawiz3.m_url=WIZ_question;
  if (diawiz3.DoModal()==IDskipall)
    strcpybuff(WIZ_reponse,"*");
  else
    switch(diawiz3.m_lnk) {
    case 0:
      strcpybuff(WIZ_reponse,"0");
      break;
    case 1:
      strcpybuff(WIZ_reponse,"1");
      break;
    case 2:
      strcpybuff(WIZ_reponse,"2");
      break;
    case 3:
      strcpybuff(WIZ_reponse,"4");
      break;
    case 4:
      strcpybuff(WIZ_reponse,"5");
      break;
    case 5:
      strcpybuff(WIZ_reponse,"6");
      break;
    default:
      strcpybuff(WIZ_reponse,"");
      break;
  }
}


//


/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackApp message handlers

// Ne fait pas partie de la classe
/*
UINT RunBackEngine( LPVOID pP ) {
  static int running=0;
  if (running)
    return 0;
  running=1;
  {
    CWinApp* app=AfxGetApp();
    POSITION pos;
    pos=app->GetFirstDocTemplatePosition();
    CDocTemplate* templ = app->GetNextDocTemplate(pos);
    pos=templ->GetFirstDocPosition();
    CDocument* doc  = templ->GetNextDoc(pos);
    pos=doc->GetFirstViewPosition();
    CView*     view = doc->GetNextView(pos);
    App_Main_HWND=view->m_hWnd;
  }
  //
  CShellApp app;
  app.InitInstance();
  running=0;
  return 0;
}
*/

/*
void CWinHTTrackApp::OnWizard() {
  //this_CSplitterFrame->SetNewView(0,1,RUNTIME_CLASS(CDialogContainer));
}
*/

afx_msg void CWinHTTrackApp::OnFileNew( ) {
  OpenDocumentFile("");
}

afx_msg void CWinHTTrackApp::OnFileOpen( ) {
  this->CWinApp::OnFileOpen();
}

void CWinHTTrackApp::OnFileSave() {
}

void CWinHTTrackApp::OnFileSaveAs() 
{
	// TODO: Add your command handler code here
	
}

void CWinHTTrackApp::OnFileDelete()
{
  static char szFilter[256];
  strcpybuff(szFilter,"WinHTTrack Website Copier Project (*.whtt)|*.whtt||");
  CFileDialog* dial = new CFileDialog(true,"whtt",NULL,OFN_HIDEREADONLY,szFilter);
  if (dial->DoModal() == IDOK) {
    CString st=dial->GetPathName();
    if (fexist((char*) LPCTSTR(st))) {
      int pos=st.ReverseFind('.');
      CString dir=st.Left(pos)+"\\";
      char msg[1000];
      sprintf(msg,"%s\r\n%s",LANG_DELETECONF,dir);
      if (AfxMessageBox(msg,MB_OKCANCEL)==IDOK) {
        if (remove(st)) {
          AfxMessageBox("Error deleting "+st);
        } else {
          RmDir(dir);
        }
      }
    } else
      AfxMessageBox(LANG(LANG_G26 /*"File not found!","Fichier introuvable!"*/));
  }
  delete dial;
}

void CWinHTTrackApp::OnBrowseWebsites()
{
  CString st=dialog0->GetBasePath();

  if (st.GetLength()<=1) {
    CString strSection       = "DefaultValues";    
    CWinApp* pApp = AfxGetApp();
    st = pApp->GetProfileString(strSection, "BasePath");
    st += "\\";
  }

  st+="index.html";
  ShellExecute(NULL,"open",st,"","",SW_RESTORE);	
}

BOOL CWinHTTrackApp::RmDir(CString srcpath) {
  CWaitCursor wait;

  if (srcpath.GetLength()==0)
    return FALSE;
  CString path=srcpath;
  WIN32_FIND_DATA find;
  if (path.Right(1)!="\\")
    path+="\\";  
  HANDLE h = FindFirstFile(path+"*.*",&find);
  if (h != INVALID_HANDLE_VALUE) {
    do {
      if (!(find.dwFileAttributes  & FILE_ATTRIBUTE_SYSTEM ))
      if (strcmp(find.cFileName,".."))
      if (strcmp(find.cFileName,"."))
        if (!(find.dwFileAttributes  & FILE_ATTRIBUTE_DIRECTORY )) {
          if (remove(path+find.cFileName)) {
            AfxMessageBox("Error deleting "+path+find.cFileName);
            return FALSE;
          }
        } else {
          if (!RmDir(path+find.cFileName))
            return FALSE;
        }
    } while(FindNextFile(h,&find));
    FindClose(h);
  }
  if (rmdir(srcpath)) {
    AfxMessageBox("Error deleting "+srcpath);
    return FALSE;
  }
  return TRUE;
}


void CWinHTTrackApp::OnFileMruFile1() 
{
	// TODO: Add your command handler code here
	
}

void CWinHTTrackApp::Onipabout() 
{
  Cabout about;
  about.DoModal();
}

void CWinHTTrackApp::OnUpdate() 
{
  CString st;
  st.Format(HTS_UPDATE_WEBSITE,0,LANGUAGE_NAME);
  HtsHelper->Help(st);
}

// Appel aide
void CWinHTTrackApp::OnHelpInfo2() {
  (void) OnHelpInfo(NULL);
}

BOOL CWinHTTrackApp::OnHelpInfo(HELPINFO* dummy) 
{
  HtsHelper->Help("step2.html");
  return true;
}

// Forwards

void CWinHTTrackApp::FwOnhide() {
  if (this_CSplitterFrame)
    this_CSplitterFrame->Onhide();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}

void CWinHTTrackApp::FwOnLoadprofile() {
  if ((dialog1!=NULL) && (maintab!=NULL))
    dialog1->OnLoadprofile();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}
void CWinHTTrackApp::FwOnSaveprofile() {
  if ((dialog1!=NULL) && (maintab!=NULL))
    dialog1->OnSaveprofile();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}
void CWinHTTrackApp::FwOnLoaddefault() {
  if ((dialog1!=NULL) && (maintab!=NULL))
    dialog1->OnLoaddefault();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}
void CWinHTTrackApp::FwOnSavedefault() {
  if ((dialog1!=NULL) && (maintab!=NULL))
    dialog1->OnSavedefault();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}
void CWinHTTrackApp::FwOnResetdefault() {
  if ((dialog1!=NULL) && (maintab!=NULL))
    dialog1->OnResetdefault();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}

//

void CWinHTTrackApp::FwOnModifyOpt() {
  if ((inprogress!=NULL) && (maintab!=NULL))
    inprogress->OnModifyOpt();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}

void CWinHTTrackApp::FwOnPause() {
  if ((inprogress!=NULL) && (maintab!=NULL))
    inprogress->OnPause();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}

void CWinHTTrackApp::FwOniplogLog() {
  if ((inprogress!=NULL) && (maintab!=NULL))
    inprogress->OniplogLog();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}

void CWinHTTrackApp::FwOniplogErr() {
  if ((inprogress!=NULL) && (maintab!=NULL))
    inprogress->OniplogErr();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}

void CWinHTTrackApp::FwOnViewTransfers() {
  if ((inprogress!=NULL) && (maintab!=NULL))
    inprogress->OnViewTransfers();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}

CDocument* CWinHTTrackApp::OpenDocumentFile( LPCTSTR lpszFileName)
{
  // Eviter deux fenêtres (un seul document)
  // Le CMultui..->CSingleDoc.. est trop complexe à changer (à cause du splitter-wnd)
  int count=1;

  { /* Check if a document exists, and if exists if empty or not, and if name is different */
    POSITION pos;
    pos=GetFirstDocTemplatePosition();
    if (pos) {
      CDocTemplate* tmpl=GetNextDocTemplate(pos);
      if (tmpl) {
        pos=tmpl->GetFirstDocPosition();
        if (pos) {
          CDocument* doc  = tmpl->GetNextDoc(pos);
          if (doc) {
            if (dialog0->GetName().GetLength()==0) {
              CloseAllDocuments(FALSE);
              count=0;        /* No documents */
            } else {
              if (dialog0->GetPath0()+".whtt" == LPCSTR(lpszFileName))
                return NULL;
            }
          }
        } else
          count=0;          /* No documents */
      }
    }
  }

  // Ouvrir nouvelle instance
  if (count) {
    char cmdl[2048];
    TCHAR ModulePath[MAX_PATH + 1];
    ModulePath[0] = '\0';
    ::GetModuleFileName(NULL, ModulePath, sizeof(ModulePath)/sizeof(TCHAR) - 1);
    CString name = ModulePath;
    strcpybuff(cmdl,"\"");
    strcatbuff(cmdl,lpszFileName);
    strcatbuff(cmdl,"\"");
    ShellExecute(NULL,"open",name,cmdl,"",SW_RESTORE);
    return NULL;
  }

  // Ouvrir nouveau?
  //if (count)
  //  return;       // ne rien faire, car limité à 1 document
  //count++;

  /* Ouvrir */
  /*
  CWinApp* app=AfxGetApp();
  POSITION pos;
  pos=app->GetFirstDocTemplatePosition();
  CDocTemplate* templ = app->GetNextDocTemplate(pos);
  pos=templ->GetFirstDocPosition();
  if (pos) {
    CDocument* doc  = templ->GetNextDoc(pos);
    if (doc)
      if (!doc->SaveModified())
        return NULL;
  }
  CloseAllDocuments(FALSE);
  */
  if (strlen(lpszFileName))
    return CWinApp::OpenDocumentFile(lpszFileName);
  else
    CWinApp::OnFileNew();
  return NULL;
}

void CWinHTTrackApp::NewTabs() {
  DeleteTabs();
  m_tab0 = new CFirstInfo();
  m_tab1 = new CNewProj();
  m_tab2 = new Wid1();
  m_tab3 = new Ctrans();
  m_tabprogress = new Cinprogress();
  m_tabend = new Cinfoend();
}

void CWinHTTrackApp::DeleteTabs() {
  if (m_tab0)
  if (m_tab0->GetSafeHwnd())       /* a déja été détruit par CWinApp */
    delete m_tab0;
  if (m_tab1)
  if (m_tab1->GetSafeHwnd())
    delete m_tab1;
  if (m_tab2)
  if (m_tab2->GetSafeHwnd())
    delete m_tab2;
  if (m_tab3)
  if (m_tab3->GetSafeHwnd())
    delete m_tab3;
  if (m_tabprogress)
  if (m_tabprogress->GetSafeHwnd())
    delete m_tabprogress;
  if (m_tabend)
  if (m_tabend->GetSafeHwnd())
    delete m_tabend;

  m_tab0=NULL;
  m_tab1=NULL;
  m_tab2=NULL;
  m_tab3=NULL;
  m_tabprogress=NULL;
  m_tabend=NULL;
}

int CWinHTTrackApp::ExitInstance() 
{
  LANG_DELETE();

  /* Uninitialize */
  htsthread_wait();
  hts_uninit();

  return CWinApp::ExitInstance();
}
