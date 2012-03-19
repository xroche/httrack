/*
    HTTrack library example
    .c file

    Prerequisites:
      - install winhttrack
      - set the proper path in the project settings (especially for the httrack lib and dll)

    How to build: (callback.so or callback.dll)
      With GNU-GCC:
        gcc -I/usr/include/httrack -O -g3 -Wall -D_REENTRANT -o example example.c -lhttrack1
      With MS-Visual C++:
        cl -nologo -W3 -Zi -Zp4 -DWIN32 -Fe"mycallback.exe" callbacks-example.c wsock32.lib libhttrack.lib
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* Standard httrack module includes */
#include "httrack-library.h"
#include "htsopt.h"
#include "htsdefines.h"

/* Local definitions */
#include "example.h"

/*
 * Name:           main
 * Description:    main() function
 * Parameters:     None
 * Should return:  error status
*/
int main(void) {
  /* 
     First, ask for an URL 
     Note: For the test, option r2 (mirror max depth=1) and --testscan (no index, no cache, do not store, no log files)
  */
  char _argv[][256] = {"httrack_test" , "<URL>" , "-r3" , "--testscan" , ""  };
  char* argv[]      = {NULL           , NULL    , NULL  , NULL        , NULL};
  int         argc = 0;
  httrackp *opt;
	int ret;
  while(strlen(_argv[argc])) {
    argv[argc]=_argv[argc];
    argc++;
  }
  argv[argc]=NULL;
  printf("HTTrackLib test program\n");
  printf("Enter URL (example: www.foobar.com/index.html) :");
  scanf("%s",argv[1]);
  printf("Test: 1 depth\n");

	/* Initialize the library */
#ifdef _WIN32
  {
    WORD   wVersionRequested;   // requested version WinSock API
    WSADATA wsadata;            // Windows Sockets API data
    int stat;
    wVersionRequested = 0x0101;
    stat = WSAStartup( wVersionRequested, &wsadata );
    if (stat != 0) {
      printf("Winsock not found!\n");
      return;
    } else if (LOBYTE(wsadata.wVersion) != 1  && HIBYTE(wsadata.wVersion) != 1) {
      printf("WINSOCK.DLL does not support version 1.1\n");
      WSACleanup();
      return;
    }
  }
#endif
  hts_init();

	/* Create option settings and set callbacks (wrappers) */
	opt = hts_create_opt();

  CHAIN_FUNCTION(opt, init, httrack_wrapper_init, NULL);
  CHAIN_FUNCTION(opt, uninit, httrack_wrapper_uninit, NULL);
  CHAIN_FUNCTION(opt, start, httrack_wrapper_start, NULL);
  CHAIN_FUNCTION(opt, end, httrack_wrapper_end, NULL);
  CHAIN_FUNCTION(opt, chopt, httrack_wrapper_chopt, NULL);
  CHAIN_FUNCTION(opt, preprocess, httrack_wrapper_preprocesshtml, NULL);
  CHAIN_FUNCTION(opt, postprocess, httrack_wrapper_postprocesshtml, NULL);
  CHAIN_FUNCTION(opt, check_html, httrack_wrapper_checkhtml, NULL);
  CHAIN_FUNCTION(opt, query, httrack_wrapper_query, NULL);
  CHAIN_FUNCTION(opt, query2, httrack_wrapper_query2, NULL);
  CHAIN_FUNCTION(opt, query3, httrack_wrapper_query3, NULL);
  CHAIN_FUNCTION(opt, loop, httrack_wrapper_loop, NULL);
  CHAIN_FUNCTION(opt, check_link, httrack_wrapper_check, NULL);
  CHAIN_FUNCTION(opt, check_mime, httrack_wrapper_check_mime, NULL);
  CHAIN_FUNCTION(opt, pause, httrack_wrapper_pause, NULL);
  CHAIN_FUNCTION(opt, filesave, httrack_wrapper_filesave, NULL);
  CHAIN_FUNCTION(opt, filesave2, httrack_wrapper_filesave2, NULL);
  CHAIN_FUNCTION(opt, linkdetected, httrack_wrapper_linkdetected, NULL);
  CHAIN_FUNCTION(opt, linkdetected2, httrack_wrapper_linkdetected2, NULL);
  CHAIN_FUNCTION(opt, xfrstatus, httrack_wrapper_xfrstatus, NULL);
  CHAIN_FUNCTION(opt, savename, httrack_wrapper_savename, NULL);
  CHAIN_FUNCTION(opt, sendhead, httrack_wrapper_sendheader, NULL);
  CHAIN_FUNCTION(opt, receivehead, httrack_wrapper_receiveheader, NULL);

  /* Then, launch the mirror */
	ret = hts_main2(argc, argv, opt);

  /* Wait for a key */
  printf("\nPress ENTER key to exit\n");
  scanf("%s",argv[1]);

	/* Clear option state */
	hts_free_opt(opt);
	hts_uninit();
#ifdef _WIN32
  WSACleanup();
#endif

  /* That's all! */
  return 0;
}


/* CALLBACK FUNCTIONS */

/* Initialize the Winsock */
static void CDECL httrack_wrapper_init(t_hts_callbackarg *carg) {
  printf("Engine started\n");
}
static void CDECL httrack_wrapper_uninit(t_hts_callbackarg *carg) {
  printf("Engine exited\n");
}
static int CDECL httrack_wrapper_start(t_hts_callbackarg *carg, httrackp* opt) {
  printf("Start of mirror\n");
  return 1; 
}
static int CDECL httrack_wrapper_chopt(t_hts_callbackarg *carg, httrackp* opt) {
  return 1;
}
static int CDECL httrack_wrapper_end(t_hts_callbackarg *carg, httrackp* opt) { 
  printf("End of mirror\n");
  return 1; 
}
static int CDECL httrack_wrapper_checkhtml(t_hts_callbackarg *carg, httrackp *opt,  char* html,int len,const char* url_address,const char* url_file) {
  printf("Parsing html file: http://%s%s\n",url_address,url_file);
  return 1;
}
static int CDECL httrack_wrapper_loop(t_hts_callbackarg *carg, httrackp *opt,  void* _back,int back_max,int back_index,int lien_n,int lien_tot,int stat_time,hts_stat_struct* stats) {
  /* printf("..httrack_wrapper_loop called\n"); */
  return 1;
}
static const char* CDECL httrack_wrapper_query(t_hts_callbackarg *carg, httrackp *opt,  const char* question) {
  /* Answer is No */
  return "N";
}
static const char* CDECL httrack_wrapper_query2(t_hts_callbackarg *carg, httrackp *opt,  const char* question) {
  /* Answer is No */
  return "N";
}
static const char* CDECL httrack_wrapper_query3(t_hts_callbackarg *carg, httrackp *opt,  const char* question) {
  /* Answer is "" */
  return "";
}
static int CDECL httrack_wrapper_check(t_hts_callbackarg *carg, httrackp *opt,  const char* adr,const char* fil,int status) {
  printf("Link status tested: http://%s%s\n",adr,fil);
  return -1;
}
static void CDECL httrack_wrapper_pause(t_hts_callbackarg *carg, httrackp *opt,  const char* lockfile) {
  /* Wait until lockfile is removed.. */
}
static void CDECL httrack_wrapper_filesave(t_hts_callbackarg *carg, httrackp *opt,  const char* file) {
}
static int CDECL httrack_wrapper_linkdetected(t_hts_callbackarg *carg, httrackp *opt,  char* link) {
  printf("Link detected: %s\n",link);
  return 1;
}
static int CDECL httrack_wrapper_xfrstatus(t_hts_callbackarg *carg, httrackp *opt,  void* back) {
  return 1;
}
static int CDECL httrack_wrapper_preprocesshtml(t_hts_callbackarg *carg, httrackp *opt,  char** html,int* len,const char* url_address,const char* url_file) {
  return 1;
}
static int CDECL httrack_wrapper_postprocesshtml(t_hts_callbackarg *carg, httrackp *opt,  char** html,int* len,const char* url_address,const char* url_file) {
  return 1;
}
static int CDECL httrack_wrapper_check_mime(t_hts_callbackarg *carg, httrackp *opt,  const char* adr,const char* fil,const char* mime,int status) {
  return -1;
}
static void CDECL httrack_wrapper_filesave2(t_hts_callbackarg *carg, httrackp *opt,  const char* adr, const char* fil, const char* save, int is_new, int is_modified,int not_updated) {
}
static int CDECL httrack_wrapper_linkdetected2(t_hts_callbackarg *carg, httrackp *opt,  char* link, const char* start_tag) {
  return 1;
}
static int CDECL httrack_wrapper_savename(t_hts_callbackarg *carg, httrackp *opt,  const char* adr_complete,const char* fil_complete,const char* referer_adr,const char* referer_fil,char* save) {
  return 1;
}
static int CDECL httrack_wrapper_sendheader(t_hts_callbackarg *carg, httrackp *opt,  char* buff, const char* adr, const char* fil, const char* referer_adr, const char* referer_fil, htsblk* outgoing) {
  return 1;
}
static int CDECL httrack_wrapper_receiveheader(t_hts_callbackarg *carg, httrackp *opt,  char* buff, const char* adr, const char* fil, const char* referer_adr, const char* referer_fil, htsblk* incoming) {
  return 1;
}
