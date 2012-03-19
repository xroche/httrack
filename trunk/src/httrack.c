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
/* File: htsshow.c console progress info                        */
/* Only used on Linux version                                   */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#if HTS_WIN
#else
#ifndef Sleep
#define Sleep(a) { if (((a)*1000)%1000000) usleep(((a)*1000)%1000000); if (((a)*1000)/1000000) sleep(((a)*1000)/1000000); }
#endif
#endif

#include "htsglobal.h"
#include "httrack.h"

// htswrap_add
#include "htswrap.h"

#if HTS_ANALYSTE_CONSOLE

/* specific definitions */
#include "htsbase.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include "Winsock.h"
#endif
/* END specific definitions */

// ISO VT100/220 definitions
#define VT_COL_TEXT_BLACK    "30"
#define VT_COL_TEXT_RED      "31"
#define VT_COL_TEXT_GREEN    "32"
#define VT_COL_TEXT_YELLOW   "33"
#define VT_COL_TEXT_BLUE     "34"
#define VT_COL_TEXT_MAGENTA  "35"
#define VT_COL_TEXT_CYAN     "36"
#define VT_COL_TEXT_WHITE    "37"
#define VT_COL_BACK_BLACK    "40"
#define VT_COL_BACK_RED      "41"
#define VT_COL_BACK_GREEN    "42"
#define VT_COL_BACK_YELLOW   "43"
#define VT_COL_BACK_BLUE     "44"
#define VT_COL_BACK_MAGENTA  "45"
#define VT_COL_BACK_CYAN     "46"
#define VT_COL_BACK_WHITE    "47"
//
#define VT_GOTOXY(X,Y)  "\33["Y";"X"f"
#define VT_COLOR(C)     "\33["C"m"
#define VT_RESET        "\33[m"
#define VT_REVERSE      "\33[7m"
#define VT_UNREVERSE    "\33[27m"
#define VT_BOLD         "\33[1m"
#define VT_UNBOLD       "\33[22m"
#define VT_BLINK        "\33[5m"
#define VT_UNBLINK      "\33[25m"
//
#define VT_CLREOL       "\33[K"
#define VT_CLRSOL       "\33[1K"
#define VT_CLRLIN       "\33[2K"
#define VT_CLREOS       "\33[J"
#define VT_CLRSOS       "\33[1J"
#define VT_CLRSCR       "\33[2J"
//
#define csi(X)          printf(s_csi( X ));
void vt_clear(void) {
  printf("%s%s%s",VT_RESET,VT_CLRSCR,VT_GOTOXY("1","0"));
}
void vt_home(void) {
  printf("%s%s",VT_RESET,VT_GOTOXY("1","0"));
}
//


/*
#define STYLE_STATVALUES VT_COLOR(VT_COL_TEXT_BLACK)
#define STYLE_STATTEXT   VT_COLOR(VT_COL_TEXT_BLUE)   
*/
#define STYLE_STATVALUES VT_BOLD
#define STYLE_STATTEXT   VT_UNBOLD
#define STYLE_STATRESET  VT_UNBOLD
#define NStatsBuffer     14
#define MAX_LEN_INPROGRESS 40

static int use_show;


int main(int argc, char **argv) {
  hts_init();

  /*
  hts_htmlcheck_init         = (t_hts_htmlcheck_init)           htswrap_read("init");
Log: "engine: init"

  hts_htmlcheck_uninit       = (t_hts_htmlcheck_uninit)         htswrap_read("free");
Log: "engine: free"

  hts_htmlcheck_start        = (t_hts_htmlcheck_start)          htswrap_read("start");
Log: "engine: start"

  hts_htmlcheck_end          = (t_hts_htmlcheck_end)            htswrap_read("end");
Log: "engine: end"

  hts_htmlcheck_chopt        = (t_hts_htmlcheck_chopt)          htswrap_read("change-options");
Log: "engine: change-options"

  hts_htmlcheck              = (t_hts_htmlcheck)                htswrap_read("check-html");
Log: "check-html: <url>"

  hts_htmlcheck_query        = (t_hts_htmlcheck_query)          htswrap_read("query");
  hts_htmlcheck_query2       = (t_hts_htmlcheck_query2)         htswrap_read("query2");
  hts_htmlcheck_query3       = (t_hts_htmlcheck_query3)         htswrap_read("query3");
  hts_htmlcheck_loop         = (t_hts_htmlcheck_loop)           htswrap_read("loop");
  hts_htmlcheck_check        = (t_hts_htmlcheck_check)          htswrap_read("check-link");
Log: none

  hts_htmlcheck_pause        = (t_hts_htmlcheck_pause)          htswrap_read("pause");
Log: "pause: <lockfile>"

  hts_htmlcheck_filesave     = (t_hts_htmlcheck_filesave)       htswrap_read("save-file");
  hts_htmlcheck_linkdetected = (t_hts_htmlcheck_linkdetected)   htswrap_read("link-detected");
Log: none

  hts_htmlcheck_xfrstatus    = (t_hts_htmlcheck_xfrstatus)      htswrap_read("transfer-status");
Log: 
    "engine: transfer-status: link updated: <url> -> <file>"
  | "engine: transfer-status: link added: <url> -> <file>"
  | "engine: transfer-status: link recorded: <url> -> <file>"
  | "engine: transfer-status: link link error (<errno>, '<err_msg>'): <url>"
  hts_htmlcheck_savename     = (t_hts_htmlcheck_savename  )     htswrap_read("save-name");
Log: 
    "engine: save-name: local name: <url> -> <file>"
*/
  
  htswrap_add("init",htsshow_init);
  htswrap_add("free",htsshow_uninit);
  htswrap_add("start",htsshow_start);
  htswrap_add("change-options",htsshow_chopt);
  htswrap_add("end",htsshow_end);
  htswrap_add("check-html",htsshow_checkhtml);
  htswrap_add("loop",htsshow_loop);
  htswrap_add("query",htsshow_query);
  htswrap_add("query2",htsshow_query2);
  htswrap_add("query3",htsshow_query3);
  htswrap_add("check-link",htsshow_check);
  htswrap_add("pause",htsshow_pause);
  htswrap_add("save-file",htsshow_filesave);
  htswrap_add("link-detected",htsshow_linkdetected);
  htswrap_add("transfer-status",htsshow_xfrstatus);
  htswrap_add("save-name",htsshow_savename);

  return hts_main(argc,argv);
}


/* CALLBACK FUNCTIONS */

/* Initialize the Winsock */
void __cdecl htsshow_init(void) {
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

}
void __cdecl htsshow_uninit(void) {
#ifdef _WIN32
  WSACleanup();
#endif
}
int __cdecl htsshow_start(httrackp* opt) {
  use_show=0;
  if (opt->verbosedisplay==2) {
    use_show=1;
    vt_clear();
  }
  return 1; 
}
int __cdecl htsshow_chopt(httrackp* opt) {
  return __cdecl htsshow_start(opt);
}
int  __cdecl htsshow_end(void) { 
  return 1; 
}
int __cdecl htsshow_checkhtml(char* html,int len,char* url_adresse,char* url_fichier) {
  return 1;
}
int __cdecl htsshow_loop(lien_back* back,int back_max,int back_index,int lien_n,int lien_tot,int stat_time, hts_stat_struct* stats) {    // appelé à chaque boucle de HTTrack
  static TStamp prev_mytime=0; /* ok */
  static t_InpInfo SInfo; /* ok */
  //
  TStamp mytime;
  long int rate=0;
  char st[256];
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

  if (!use_show)
    return 1;

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
  if (SInfo.stat_back>=0) SInfo.stat_back=nbk;
  if (stat_written>=0) SInfo.stat_written=stat_written;
  if (stat_updated>=0) SInfo.stat_updated=stat_updated;
  if (stat_errors>=0)  SInfo.stat_errors=stat_errors;
  if (stat_warnings>=0)  SInfo.stat_warnings=stat_warnings;
  if (stat_infos>=0)  SInfo.stat_infos=stat_infos;


  if ( ((mytime - prev_mytime)>100) || ((mytime - prev_mytime)<0) ) {
    prev_mytime=mytime;

    
    st[0]='\0';
    qsec2str(st,stat_time);
    vt_home();
    printf(
    VT_GOTOXY("1","1")
    VT_CLREOL
                           STYLE_STATTEXT   "Bytes saved:"
                           STYLE_STATVALUES " \t%s"
                           "\t"
    VT_CLREOL
    VT_GOTOXY("40","1")
                           STYLE_STATTEXT   "Links scanned:"
                           STYLE_STATVALUES " \t%d/%d (+%d)"
    VT_CLREOL"\n"VT_CLREOL
    VT_GOTOXY("1","2")
                           STYLE_STATTEXT   "Time:"
                                            " \t"
                           STYLE_STATVALUES "%s"
                           "\t"
    VT_CLREOL
    VT_GOTOXY("40","2")
                           STYLE_STATTEXT   "Files written:"
                                            " \t"
                           STYLE_STATVALUES "%d"
    VT_CLREOL"\n"VT_CLREOL
    VT_GOTOXY("1","3")
                           STYLE_STATTEXT   "Transfer rate:"
                                            " \t"
                           STYLE_STATVALUES "%s (%s)"
                           "\t"
    VT_CLREOL
    VT_GOTOXY("40","3")
                           STYLE_STATTEXT   "Files updated:"
                                            " \t"
                           STYLE_STATVALUES "%d"
    VT_CLREOL"\n"VT_CLREOL
    VT_GOTOXY("1","4")
                           STYLE_STATTEXT   "Active connections:"
                                            " \t"
                           STYLE_STATVALUES "%d"
                           "\t"
    VT_CLREOL
    VT_GOTOXY("40","4")
                           STYLE_STATTEXT   "Errors:"
                           STYLE_STATVALUES " \t"
                           STYLE_STATVALUES "%d"
    VT_CLREOL"\n"
    STYLE_STATRESET
    ,
      /* */
      (char*)int2bytes(SInfo.stat_bytes),
      (int)lien_n,(int)SInfo.lien_tot,(int)nbk,
      (char*)st,
      (int)SInfo.stat_written,
      (char*)int2bytessec(SInfo.irate),(char*)int2bytessec(SInfo.rate),
      (int)SInfo.stat_updated,
      (int)SInfo.stat_nsocket,
      (int)SInfo.stat_errors
      /* */
      );

    
    // parcourir registre des liens
    if (back_index>=0) {  // seulement si index passé
      int j,k;
      int index=0;
      int ok=0;         // idem
      int l;            // idem
      //
      t_StatsBuffer StatsBuffer[NStatsBuffer];
      
      {
        int i;
        for(i=0;i<NStatsBuffer;i++) {
          strcpy(StatsBuffer[i].state,"");
          strcpy(StatsBuffer[i].name,"");
          strcpy(StatsBuffer[i].file,"");
          strcpy(StatsBuffer[i].url_sav,"");
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
                  strcpy(StatsBuffer[index].state,"receive"); ok=1;
                }
                break;
              case 1:
                if (back[i].status==99) {
                  strcpy(StatsBuffer[index].state,"request"); ok=1;
                }
                else if (back[i].status==100) {
                  strcpy(StatsBuffer[index].state,"connect"); ok=1;
                }
                else if (back[i].status==101) {
                  strcpy(StatsBuffer[index].state,"search"); ok=1;
                }
                else if (back[i].status==1000) {    // ohh le beau ftp
                  sprintf(StatsBuffer[index].state,"ftp: %s",back[i].info); ok=1;
                }
                break;
              default:
                if (back[i].status==0) {  // prêt
                  if ((back[i].r.statuscode==200)) {
                    strcpy(StatsBuffer[index].state,"ready"); ok=1;
                  }
                  else if ((back[i].r.statuscode>=100) && (back[i].r.statuscode<=599)) {
                    char tempo[256]; tempo[0]='\0';
                    infostatuscode(tempo,back[i].r.statuscode);
                    strcpy(StatsBuffer[index].state,tempo); ok=1;
                  }
                  else {
                    strcpy(StatsBuffer[index].state,"error"); ok=1;
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
                strcpy(StatsBuffer[index].url_sav,back[i].url_sav);   // pour cancel
                if (strcmp(back[i].url_adr,"file://"))
                  strcat(s,back[i].url_adr);
                else
                  strcat(s,"localhost");
                if (back[i].url_fil[0]!='/')
                  strcat(s,"/");
                strcat(s,back[i].url_fil);
                
                StatsBuffer[index].file[0]='\0';
                {
                  char* a=strrchr(s,'/');
                  if (a) {
                    strncat(StatsBuffer[index].file,a,200);
                    *a='\0';
                  }
                }
                
                if ((l=strlen(s))<MAX_LEN_INPROGRESS)
                  strcpy(StatsBuffer[index].name,s);
                else {
                  // couper
                  StatsBuffer[index].name[0]='\0';
                  strncat(StatsBuffer[index].name,s,MAX_LEN_INPROGRESS/2-2);
                  strcat(StatsBuffer[index].name,"...");
                  strcat(StatsBuffer[index].name,s+l-MAX_LEN_INPROGRESS/2+2);
                }
                               
                if (back[i].r.totalsize>0) {  // taille prédéfinie
                  StatsBuffer[index].sizetot=back[i].r.totalsize;
                  StatsBuffer[index].size=back[i].r.size;
                } else {  // pas de taille prédéfinie
                  if (back[i].status==0) {  // prêt
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

      /* LF */
      printf("%s\n",VT_CLREOL);

      /* Display current job */
      {
        int parsing=0;
        printf("Current job: ");
        if (!(parsing=hts_is_parsing(-1)))
          printf("receiving files");
        else {
          switch(hts_is_testing()) {
          case 0:
            printf("parsing HTML file (%d%%)",parsing);
            break;
          case 1:
            printf("parsing HTML file: testing links (%d%%)",parsing);
            break;
          case 2:
            printf("purging files");
            break;
          }
        }
        printf("%s\n",VT_CLREOL);
      }
      
      /* Display background jobs */
      {
        int i;
        for(i=0;i<NStatsBuffer;i++) {
          if (strnotempty(StatsBuffer[i].state)) {
            printf(VT_CLREOL" %s - \t%s%s \t%s / \t%s",
              StatsBuffer[i].state,
              StatsBuffer[i].name,
              StatsBuffer[i].file,
              int2bytes(StatsBuffer[i].size),
              int2bytes(StatsBuffer[i].sizetot)
              );
          }
          printf("%s\n",VT_CLREOL);
        }
      }


    }

  }



  return 1;
}
char* __cdecl htsshow_query(char* question) {
  static char s[12]=""; /* ok */
  printf("%s\nPress <Y><Enter> to confirm, <N><Enter> to abort\n",question);
  io_flush; linput(stdin,s,4);
  return s;
}
char* __cdecl htsshow_query2(char* question) {
  static char s[12]=""; /* ok */
  printf("%s\nPress <Y><Enter> to confirm, <N><Enter> to abort\n",question);
  io_flush; linput(stdin,s,4);
  return s;
}
char* __cdecl htsshow_query3(char* question) {
  static char line[256]; /* ok */
  do {
    io_flush; linput(stdin,line,206);
  } while(!strnotempty(line));
  printf("ok..\n");
  return line;
}
int __cdecl htsshow_check(char* adr,char* fil,int status) {
  return -1;
}
void __cdecl htsshow_pause(char* lockfile) {
  while (fexist(lockfile)) {
    Sleep(1000);
  }
}
void __cdecl htsshow_filesave(char* file) {
}
int __cdecl htsshow_linkdetected(char* link) {
  return 1;
}
int __cdecl htsshow_xfrstatus(lien_back* back) {
  return 1;
}
int __cdecl htsshow_savename(char* adr_complete,char* fil_complete,char* referer_adr,char* referer_fil,char* save) {
  return 1;
}


#endif
