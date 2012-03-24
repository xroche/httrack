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
/* File: webhttrack.c routines                                  */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <ctype.h>
#ifndef _WIN32
#include <signal.h>
#endif
// htswrap_add
#include "htsglobal.h"
#include "htsbasenet.h"
#include "htswrap.h"
#include "httrack-library.h"
#include "htsdefines.h"

/* Threads */
#include "htsthread.h"

/* External modules */
#include "htsinthash.c"
#include "htsmd5.c"
#include "md5.c"

#include "htsserver.h"
#include "htsweb.h"

#if USE_BEGINTHREAD==0
#error fatal: no threads support
#endif

#ifdef _WIN32
#ifndef __cplusplus
// DOS
#include <process.h>    /* _beginthread, _endthread */
#endif
#else
#endif

static htsmutex refreshMutex = HTSMUTEX_INIT;

static int help_server(char* dest_path, int defaultPort);
extern int commandRunning;
extern int commandEnd;
extern int commandReturn;
extern int commandEndRequested;
extern char* commandReturnMsg;
extern char* commandReturnCmdl;

static void htsweb_sig_brpipe( int code ) {
  /* ignore */
}

int main(int argc, char* argv[])
{
  int i;
  int ret = 0;
  int defaultPort = 0;
  printf("Initialzing the server..\n");

#ifdef _WIN32
  {
    WORD   wVersionRequested;   // requested version WinSock API
    WSADATA wsadata;            // Windows Sockets API data
    int stat;
    wVersionRequested = 0x0101;
    stat = WSAStartup( wVersionRequested, &wsadata );
    if (stat != 0) {
      fprintf(stderr, "Winsock not found!\n");
      return -1;
    } else if (LOBYTE(wsadata.wVersion) != 1  && HIBYTE(wsadata.wVersion) != 1) {
      fprintf(stderr, "WINSOCK.DLL does not support version 1.1\n");
      WSACleanup();
      return -1;
    }
  }
#endif

  if (argc < 2 || (argc % 2) != 0) {
    fprintf(stderr, "** Warning: use the webhttrack frontend if available\n");
    fprintf(stderr, "usage: %s [--port <port>] <path-to-html-root-dir> [key value [key value]..]\n", argv[0]);
    fprintf(stderr, "example: %s /usr/share/httrack/\n", argv[0]);
    return 1;
  }

  /* init and launch */
  hts_init();
  htslang_init();

  /* set general keys */
#ifdef HTS_ETCPATH
  smallserver_setkey("ETCPATH", HTS_ETCPATH);
#endif
#ifdef HTS_BINPATH
  smallserver_setkey("BINPATH", HTS_BINPATH);
#endif
#ifdef HTS_LIBPATH
  smallserver_setkey("LIBPATH", HTS_LIBPATH);
#endif
#ifdef HTS_PREFIX
  smallserver_setkey("PREFIX", HTS_PREFIX);
#endif
#ifdef HTS_HTTRACKCNF
  smallserver_setkey("HTTRACKCNF", HTS_HTTRACKCNF);
#endif
#ifdef HTS_HTTRACKDIR
  smallserver_setkey("HTTRACKDIR", HTS_HTTRACKDIR);
#endif
#ifdef HTS_INET6
  smallserver_setkey("INET6", "1");
#endif
#ifdef HTS_USEOPENSSL
  smallserver_setkey("USEOPENSSL", "1");
#endif
#ifdef HTS_DLOPEN
  smallserver_setkey("DLOPEN", "1");
#endif
#ifdef HTS_USESWF
  smallserver_setkey("USESWF", "1");
#endif
#ifdef HTS_USEZLIB
  smallserver_setkey("USEZLIB", "1");
#endif
#ifdef _WIN32
  smallserver_setkey("WIN32", "1");
#endif
  smallserver_setkey("HTTRACK_VERSION", HTTRACK_VERSION);
  smallserver_setkey("HTTRACK_VERSIONID", HTTRACK_VERSIONID);
  smallserver_setkey("HTTRACK_AFF_VERSION", HTTRACK_AFF_VERSION);
  {
    char tmp[32];
    sprintf(tmp, "%d", -1);
    smallserver_setkey("HTS_PLATFORM", tmp);
  }
  smallserver_setkey("HTTRACK_WEB", HTTRACK_WEB); 

  /* protected session-id */
  {
    char buff[1024];
    char digest[32 + 2];
    srand((unsigned int)time(NULL));
    sprintf(buff, "%d-%d", (int)time(NULL), (int)rand());
    domd5mem(buff,strlen(buff),digest,1);
    smallserver_setkey("sid", digest);
    smallserver_setkey("_sid", digest);
  }

  /* set commandline keys */
	for(i = 2 ; i < argc ; i += 2) {
		if (strcmp(argv[i], "--port") == 0) {
			if (sscanf(argv[i + 1], "%d", &defaultPort) != 1 || defaultPort < 0 || defaultPort >= 65535 ) {
				fprintf(stderr, "couldn't set the port number to %s\n", argv[i + 1]);
				return -1;
			}
		} else {
			smallserver_setkey(argv[i], argv[i + 1]);
		}
  }

  /* sigpipe */
#ifndef _WIN32
  signal( SIGPIPE , htsweb_sig_brpipe );   // broken pipe (write into non-opened socket)
#endif

  /* launch */
  ret = help_server(argv[1], defaultPort);

  htsthread_wait();
  hts_uninit();

#ifdef _WIN32
  WSACleanup();
#endif

  return ret;
}

static int webhttrack_runmain(httrackp *opt, int argc, char** argv);
static void back_launch_cmd( void* pP ) {
  char* cmd = (char*) pP;
  char** argv = (char**) malloct(1024 * sizeof(char*));
  int argc = 0;
  int i = 0;
  int g = 0;
	//
  httrackp *opt;

  /* copy commandline */
  if (commandReturnCmdl)
    free(commandReturnCmdl);
  commandReturnCmdl = strdup(cmd);

  /* split */
  argv[0]="webhttrack";
  argv[1]=cmd;
  argc++;    
  i = 0;
  while(cmd[i]) {
    if (cmd[i] == '\t' || cmd[i] == '\r' || cmd[i] == '\n') {
      cmd[i] = ' ';
    }
    i++;
  }
  i = 0;
  while(cmd[i])  {
    if(cmd[i]=='\"') g=!g;
    if(cmd[i]==' ') {
      if(!g){
        cmd[i]='\0';
        argv[argc++]=cmd+i+1;
      }
    }  
    i++;
  }

	/* init */
	hts_init();
  global_opt = opt = hts_create_opt();

  /* run */
  commandReturn = webhttrack_runmain(opt, argc, argv);
  if (commandReturn) {
    if (commandReturnMsg)
      free(commandReturnMsg);
    commandReturnMsg = strdup(hts_errmsg(opt));
  }

	/* free */
	global_opt = NULL;
	hts_free_opt(opt);
  hts_uninit();

  /* okay */
  commandRunning = 0;

  /* finished */
  commandEnd = 1;

  /* free */
  free(cmd);
  freet(argv);
  return ;
}

void webhttrack_main(char* cmd) {
  commandRunning = 1;
  hts_newthread(back_launch_cmd, (void*) strdup(cmd));
}

void webhttrack_lock(void) {
  hts_mutexlock(&refreshMutex);
}

void webhttrack_release(void) {
  hts_mutexrelease(&refreshMutex);
}

static int webhttrack_runmain(httrackp *opt, int argc, char** argv) {
	int ret;

  CHAIN_FUNCTION(opt, init, htsshow_init, NULL);
  CHAIN_FUNCTION(opt, uninit, htsshow_uninit, NULL);
  CHAIN_FUNCTION(opt, start, htsshow_start, NULL);
  CHAIN_FUNCTION(opt, end, htsshow_end, NULL);
  CHAIN_FUNCTION(opt, chopt, htsshow_chopt, NULL);
  CHAIN_FUNCTION(opt, preprocess, htsshow_preprocesshtml, NULL);
  CHAIN_FUNCTION(opt, postprocess, htsshow_postprocesshtml, NULL);
  CHAIN_FUNCTION(opt, check_html, htsshow_checkhtml, NULL);
  CHAIN_FUNCTION(opt, query, htsshow_query, NULL);
  CHAIN_FUNCTION(opt, query2, htsshow_query2, NULL);
  CHAIN_FUNCTION(opt, query3, htsshow_query3, NULL);
  CHAIN_FUNCTION(opt, loop, htsshow_loop, NULL);
  CHAIN_FUNCTION(opt, check_link, htsshow_check, NULL);
  CHAIN_FUNCTION(opt, check_mime, htsshow_check_mime, NULL);
  CHAIN_FUNCTION(opt, pause, htsshow_pause, NULL);
  CHAIN_FUNCTION(opt, filesave, htsshow_filesave, NULL);
  CHAIN_FUNCTION(opt, filesave2, htsshow_filesave2, NULL);
  CHAIN_FUNCTION(opt, linkdetected, htsshow_linkdetected, NULL);
  CHAIN_FUNCTION(opt, linkdetected2, htsshow_linkdetected2, NULL);
  CHAIN_FUNCTION(opt, xfrstatus, htsshow_xfrstatus, NULL);
  CHAIN_FUNCTION(opt, savename, htsshow_savename, NULL);
  CHAIN_FUNCTION(opt, sendhead, htsshow_sendheader, NULL);
  CHAIN_FUNCTION(opt, receivehead, htsshow_receiveheader, NULL);

	ret = hts_main2(argc, argv, opt);
  htsthread_wait_n(1);
	
	return ret;
}

static int help_server(char* dest_path, int defaultPort) {
  int returncode = 0;
  char adr_prox[HTS_URLMAXSIZE*2];
  int port_prox;
  T_SOC soc=smallserver_init_std(&port_prox, adr_prox, defaultPort);
  if (soc!=INVALID_SOCKET) {
    char url[HTS_URLMAXSIZE*2];
    char method[32];
    char data[32768];
    url[0]=method[0]=data[0]='\0';
    //
    printf("Okay, temporary server installed.\nThe URL is:\n");
    printf("URL=http://%s:%d/\n", adr_prox, port_prox);
#ifndef _WIN32
    {
      pid_t pid = getpid();
      printf("PID=%d\n", (int)pid);
    }
#endif
    fflush(stdout);
    fflush(stderr);
    //
    if (!smallserver(soc,url,method,data,dest_path)) {
      int last_errno = errno;
      fprintf(stderr, "Unable to create the server: %s\n", strerror(last_errno));
#ifdef _WIN32
      closesocket(soc);
#else
      close(soc);
#endif
      printf("Done\n");
      returncode = 1;
    } else {
      returncode = 0;
    }
  } else {
    fprintf(stderr, "Unable to initialize a temporary server (no remaining port)\n");
    returncode = 1;
  }
  printf("EXITED\n");
  fflush(stdout);
  fflush(stderr);
  return returncode;
}


/* CALLBACK FUNCTIONS */

/* Initialize the Winsock */
void __cdecl htsshow_init(t_hts_callbackarg *carg) {
}
void __cdecl htsshow_uninit(t_hts_callbackarg *carg) {
}
int __cdecl htsshow_start(t_hts_callbackarg *carg, httrackp* opt) {
  return 1; 
}
int __cdecl htsshow_chopt(t_hts_callbackarg *carg, httrackp* opt) {
  return htsshow_start(carg, opt);
}
int  __cdecl htsshow_end(t_hts_callbackarg *carg, httrackp* opt) { 
  return 1; 
}
int __cdecl htsshow_preprocesshtml(t_hts_callbackarg *carg, httrackp *opt, char** html,int* len,const char* url_address,const char* url_file) {
  return 1;
}
int __cdecl htsshow_postprocesshtml(t_hts_callbackarg *carg, httrackp *opt, char** html,int* len,const char* url_address,const char* url_file) {
  return 1;
}
int __cdecl htsshow_checkhtml(t_hts_callbackarg *carg, httrackp *opt, char* html,int len,const char* url_address,const char* url_file) {
  return 1;
}
int __cdecl htsshow_loop(t_hts_callbackarg *carg, httrackp *opt, lien_back* back,int back_max,int back_index,int lien_n,int lien_tot,int stat_time, hts_stat_struct* stats) {    // appelé à chaque boucle de HTTrack
  static TStamp prev_mytime=0; /* ok */
  static t_InpInfo SInfo; /* ok */
  //
  TStamp mytime;
  long int rate=0;
  //
  int stat_written=-1;
  int stat_updated=-1;
  int stat_errors=-1;
  int stat_warnings=-1;
  int stat_infos=-1;
  int nbk=-1;
  LLint nb=-1;
  int stat_nsocket=-1;
  LLint stat_bytes=-1;
  LLint stat_bytes_recv=-1;
  int irate=-1;
  //
  char st[256];

  /* Exit now */
  if (commandEndRequested == 2)
    return 0;

  /* Lock */
  webhttrack_lock();

  if (stats) {
    stat_written=stats->stat_files;
    stat_updated=stats->stat_updated_files;
    stat_errors=stats->stat_errors;
    stat_warnings=stats->stat_warnings;
    stat_infos=stats->stat_infos;
    nbk=stats->nbk;
    stat_nsocket=stats->stat_nsocket;
    irate=(int)stats->rate;
    nb=stats->nb;
    stat_bytes=stats->nb;
    stat_bytes_recv=stats->HTS_TOTAL_RECV;
  }
  
  mytime=mtime_local();
  if ((stat_time>0) && (stat_bytes_recv>0))
    rate=(int)(stat_bytes_recv/stat_time);
  else
    rate=0;    // pas d'infos
  
  /* Infos */
  if (stat_bytes>=0) SInfo.stat_bytes=stat_bytes;      // bytes
  if (stat_time>=0) SInfo.stat_time=stat_time;         // time
  if (lien_tot>=0) SInfo.lien_tot=lien_tot; // nb liens
  if (lien_n>=0) SInfo.lien_n=lien_n;       // scanned
  SInfo.stat_nsocket=stat_nsocket;          // socks
  if (rate>0)  SInfo.rate=rate;                // rate
  if (irate>=0) SInfo.irate=irate;             // irate
  if (SInfo.irate<0) SInfo.irate=SInfo.rate;
  if (nbk>=0) SInfo.stat_back=nbk;
  if (stat_written>=0) SInfo.stat_written=stat_written;
  if (stat_updated>=0) SInfo.stat_updated=stat_updated;
  if (stat_errors>=0)  SInfo.stat_errors=stat_errors;
  if (stat_warnings>=0)  SInfo.stat_warnings=stat_warnings;
  if (stat_infos>=0)  SInfo.stat_infos=stat_infos;
  
  
  st[0]='\0';
  qsec2str(st,stat_time);
  
  /* Set keys */
  smallserver_setkeyint("info.stat_bytes", SInfo.stat_bytes);
  smallserver_setkeyint("info.stat_time", SInfo.stat_time);
  smallserver_setkeyint("info.lien_tot", SInfo.lien_tot);
  smallserver_setkeyint("info.lien_n", SInfo.lien_n);
  smallserver_setkeyint("info.stat_nsocket", SInfo.stat_nsocket);
  smallserver_setkeyint("info.rate", SInfo.rate);
  smallserver_setkeyint("info.irate", SInfo.irate);
  smallserver_setkeyint("info.stat_back", SInfo.stat_back);
  smallserver_setkeyint("info.stat_written", SInfo.stat_written);
  smallserver_setkeyint("info.stat_updated", SInfo.stat_updated);
  smallserver_setkeyint("info.stat_errors", SInfo.stat_errors);
  smallserver_setkeyint("info.stat_warnings", SInfo.stat_warnings);
  smallserver_setkeyint("info.stat_infos", SInfo.stat_infos);
  /* */
  smallserver_setkey("info.stat_time_str", st);
  
  if ( ((mytime - prev_mytime)>100) || ((mytime - prev_mytime)<0) ) {
    prev_mytime=mytime;
    
    
    // parcourir registre des liens
    if (back_index>=0 && back_max > 0) {  // seulement si index passé
      int j,k;
      int index=0;
      int ok=0;         // idem
      int l;            // idem
      //
      t_StatsBuffer StatsBuffer[NStatsBuffer];
      
      {
        int i;
        for(i=0;i<NStatsBuffer;i++) {
          strcpybuff(StatsBuffer[i].state,"");
          strcpybuff(StatsBuffer[i].name,"");
          strcpybuff(StatsBuffer[i].file,"");
          strcpybuff(StatsBuffer[i].url_sav,"");
          StatsBuffer[i].back=0;
          StatsBuffer[i].size=0;
          StatsBuffer[i].sizetot=0;
        }
      }
      for(k=0;k<2;k++) {    // 0: lien en cours 1: autres liens
        for(j=0;(j<3) && (index<NStatsBuffer);j++) {  // passe de priorité
          int _i;
          for(_i=0+k;(_i< max(back_max*k,1) ) && (index<NStatsBuffer);_i++) {  // no lien
            int i=(back_index+_i)%back_max;    // commencer par le "premier" (l'actuel)
            if (back[i].status>=0) {     // signifie "lien actif"
              // int ok=0;  // OPTI
              ok=0;
              switch(j) {
              case 0:     // prioritaire
                if ((back[i].status>0) && (back[i].status<99)) {
                  strcpybuff(StatsBuffer[index].state,"receive"); ok=1;
                }
                break;
              case 1:
                if (back[i].status==STATUS_WAIT_HEADERS) {
                  strcpybuff(StatsBuffer[index].state,"request"); ok=1;
                }
                else if (back[i].status==STATUS_CONNECTING) {
									strcpybuff(StatsBuffer[index].state,"connect"); ok=1;
								}
								else if (back[i].status==STATUS_WAIT_DNS) {
									strcpybuff(StatsBuffer[index].state,"search"); ok=1;
								}
								else if (back[i].status==STATUS_FTP_TRANSFER) {    // ohh le beau ftp
									char proto[] = "ftp";
									if (back[i].url_adr[0]) {
										char* ep = strchr(back[i].url_adr, ':');
										char* eps = strchr(back[i].url_adr, '/');
										int count;
										if (ep != NULL && ep < eps && (count = (int) (ep - back[i].url_adr) ) < 4) {
											proto[0] = '\0';
											strncat(proto, back[i].url_adr, count);
										}
									}
									sprintf(StatsBuffer[index].state,"%s: %s",proto,back[i].info); ok=1;
								}
								break;
							default:
								if (back[i].status==STATUS_READY) {  // prêt
                  if ((back[i].r.statuscode==HTTP_OK)) {
                    strcpybuff(StatsBuffer[index].state,"ready"); ok=1;
                  }
                  else if ((back[i].r.statuscode>=100) && (back[i].r.statuscode<=599)) {
                    char tempo[256]; tempo[0]='\0';
                    infostatuscode(tempo,back[i].r.statuscode);
                    strcpybuff(StatsBuffer[index].state,tempo); ok=1;
                  }
                  else {
                    strcpybuff(StatsBuffer[index].state,"error"); ok=1;
                  }
                }
                break;
              }
              
              if (ok) {
                char s[HTS_URLMAXSIZE*2];
                //
                StatsBuffer[index].back=i;        // index pour + d'infos
                //
                s[0]='\0';
                strcpybuff(StatsBuffer[index].url_sav,back[i].url_sav);   // pour cancel
                if (strcmp(back[i].url_adr,"file://"))
                  strcatbuff(s,back[i].url_adr);
                else
                  strcatbuff(s,"localhost");
                if (back[i].url_fil[0]!='/')
                  strcatbuff(s,"/");
                strcatbuff(s,back[i].url_fil);
                
                StatsBuffer[index].file[0]='\0';
                {
                  char* a=strrchr(s,'/');
                  if (a) {
                    strncatbuff(StatsBuffer[index].file,a,200);
                    *a='\0';
                  }
                }
                
                if ((l = (int) strlen(s))<MAX_LEN_INPROGRESS)
                  strcpybuff(StatsBuffer[index].name,s);
                else {
                  // couper
                  StatsBuffer[index].name[0]='\0';
                  strncatbuff(StatsBuffer[index].name,s,MAX_LEN_INPROGRESS/2-2);
                  strcatbuff(StatsBuffer[index].name,"...");
                  strcatbuff(StatsBuffer[index].name,s+l-MAX_LEN_INPROGRESS/2+2);
                }
                
                if (back[i].r.totalsize>0) {  // taille prédéfinie
                  StatsBuffer[index].sizetot=back[i].r.totalsize;
                  StatsBuffer[index].size=back[i].r.size;
                } else {  // pas de taille prédéfinie
                  if (back[i].status==STATUS_READY) {  // prêt
                    StatsBuffer[index].sizetot=back[i].r.size;
                    StatsBuffer[index].size=back[i].r.size;
                  } else {
                    StatsBuffer[index].sizetot=8192;
                    StatsBuffer[index].size=(back[i].r.size % 8192);
                  }
                }
                index++;
              }
            }
          }
        }
      }

      /* Display current job */
      {
        int parsing=0;
        if (commandEndRequested)
          smallserver_setkey("info.currentjob", "finishing pending transfers - Select [Cancel] to stop now!");
        else if (!(parsing=hts_is_parsing(opt, -1)))
          smallserver_setkey("info.currentjob", "receiving files");
        else {
          char tmp[1024];
          tmp[0] = '\0';
          switch(hts_is_testing(opt)) {
          case 0:
            sprintf(tmp, "parsing HTML file (%d%%)",parsing);
            break;
          case 1:
            sprintf(tmp, "parsing HTML file: testing links (%d%%)",parsing);
            break;
          case 2:
            sprintf(tmp, "purging files");
            break;
          case 3:
            sprintf(tmp, "loading cache");
            break;
          case 4:
            sprintf(tmp, "waiting (scheduler)");
            break;
          case 5:
            sprintf(tmp, "waiting (throttle)");
            break;
          }
          smallserver_setkey("info.currentjob", tmp);
        }
      }

      /* Display background jobs */
      {
        int i;
        for(i=0;i<NStatsBuffer;i++) {
          if (strnotempty(StatsBuffer[i].state)) {
						strc_int2bytes2 strc;
            smallserver_setkeyarr("info.state[", i, "]", StatsBuffer[i].state);
            smallserver_setkeyarr("info.name[", i, "]", StatsBuffer[i].name);
            smallserver_setkeyarr("info.file[", i, "]", StatsBuffer[i].file);
            smallserver_setkeyarr("info.size[", i, "]", int2bytes(&strc,StatsBuffer[i].size));
            smallserver_setkeyarr("info.sizetot[", i, "]", int2bytes(&strc,StatsBuffer[i].sizetot));
            smallserver_setkeyarr("info.url_adr[", i, "]", StatsBuffer[i].url_adr);
            smallserver_setkeyarr("info.url_fil[", i, "]", StatsBuffer[i].url_fil);
            smallserver_setkeyarr("info.url_sav[", i, "]", StatsBuffer[i].url_sav);
          }
        }
      }


    }   
      
  }
  
  /* UnLock */
  webhttrack_release();
  
  return 1;
}
const char* __cdecl htsshow_query(t_hts_callbackarg *carg, httrackp *opt, const char* question) {
  static char s[]=""; /* ok */
  return s;
}
const char* __cdecl htsshow_query2(t_hts_callbackarg *carg, httrackp *opt, const char* question) {
  static char s[]=""; /* ok */
  return s;
}
const char* __cdecl htsshow_query3(t_hts_callbackarg *carg, httrackp *opt, const char* question) {
  static char s[]=""; /* ok */
  return s;
}
int __cdecl htsshow_check(t_hts_callbackarg *carg, httrackp *opt, const char* adr,const char* fil,int status) {
  return -1;
}
int __cdecl htsshow_check_mime(t_hts_callbackarg *carg, httrackp *opt, const char* adr,const char* fil,const char* mime,int status) {
  return -1;
}
void __cdecl htsshow_pause(t_hts_callbackarg *carg, httrackp *opt, const char* lockfile) {
}
void __cdecl htsshow_filesave(t_hts_callbackarg *carg, httrackp *opt, const char* file) {
}
void  __cdecl htsshow_filesave2(t_hts_callbackarg *carg, httrackp *opt, const char* adr, const char* fil, const char* save, int is_new, int is_modified,int not_updated) {
}
int __cdecl htsshow_linkdetected(t_hts_callbackarg *carg, httrackp *opt, char* link) {
  return 1;
}
int __cdecl htsshow_linkdetected2(t_hts_callbackarg *carg, httrackp *opt, char* link, const char* start_tag) {
  return 1;
}
int __cdecl htsshow_xfrstatus(t_hts_callbackarg *carg, httrackp *opt, lien_back* back) {
  return 1;
}
int __cdecl htsshow_savename(t_hts_callbackarg *carg, httrackp *opt, const char* adr_complete,const char* fil_complete,const char* referer_adr,const char* referer_fil,char* save) {
  return 1;
}
int __cdecl htsshow_sendheader(t_hts_callbackarg *carg, httrackp *opt, char* buff, const char* adr, const char* fil, const char* referer_adr, const char* referer_fil, htsblk* outgoing) {
  return 1;
}
int __cdecl htsshow_receiveheader(t_hts_callbackarg *carg, httrackp *opt, char* buff, const char* adr, const char* fil, const char* referer_adr, const char* referer_fil, htsblk* incoming) {
  return 1;
}


