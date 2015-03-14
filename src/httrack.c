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
/* File: htsshow.c console progress info                        */
/* Only used on Linux & FreeBSD versions                        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef _WIN32
#ifndef Sleep
#define Sleep(a) { if (((a)*1000)%1000000) usleep(((a)*1000)%1000000); if (((a)*1000)/1000000) sleep(((a)*1000)/1000000); }
#endif
#endif

#include "httrack-library.h"

#include "htsglobal.h"
#include "htsbase.h"
#include "htsopt.h"
#include "htsdefines.h"
#include "httrack.h"
#include "htslib.h"

/* Static definitions */
static int fexist(const char *s);
static int linput(FILE * fp, char *s, int max);

// htswrap_add
#include "htswrap.h"

/* specific definitions */
//#include "htsbase.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
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
#if (defined(__linux) && defined(HAVE_EXECINFO_H))
#include <execinfo.h>
#define USES_BACKTRACE
#endif
/* END specific definitions */

static void __cdecl htsshow_init(t_hts_callbackarg * carg);
static void __cdecl htsshow_uninit(t_hts_callbackarg * carg);
static int __cdecl htsshow_start(t_hts_callbackarg * carg, httrackp * opt);
static int __cdecl htsshow_chopt(t_hts_callbackarg * carg, httrackp * opt);
static int __cdecl htsshow_end(t_hts_callbackarg * carg, httrackp * opt);
static int __cdecl htsshow_preprocesshtml(t_hts_callbackarg * carg,
                                          httrackp * opt, char **html, int *len,
                                          const char *url_address,
                                          const char *url_file);
static int __cdecl htsshow_postprocesshtml(t_hts_callbackarg * carg,
                                           httrackp * opt, char **html,
                                           int *len, const char *url_address,
                                           const char *url_file);
static int __cdecl htsshow_checkhtml(t_hts_callbackarg * carg, httrackp * opt,
                                     char *html, int len,
                                     const char *url_address,
                                     const char *url_file);
static int __cdecl htsshow_loop(t_hts_callbackarg * carg, httrackp * opt,
                                lien_back * back, int back_max, int back_index,
                                int lien_n, int lien_tot, int stat_time,
                                hts_stat_struct * stats);
static const char *__cdecl htsshow_query(t_hts_callbackarg * carg,
                                         httrackp * opt, const char *question);
static const char *__cdecl htsshow_query2(t_hts_callbackarg * carg,
                                          httrackp * opt, const char *question);
static const char *__cdecl htsshow_query3(t_hts_callbackarg * carg,
                                          httrackp * opt, const char *question);
static int __cdecl htsshow_check(t_hts_callbackarg * carg, httrackp * opt,
                                 const char *adr, const char *fil, int status);
static int __cdecl htsshow_check_mime(t_hts_callbackarg * carg, httrackp * opt,
                                      const char *adr, const char *fil,
                                      const char *mime, int status);
static void __cdecl htsshow_pause(t_hts_callbackarg * carg, httrackp * opt,
                                  const char *lockfile);
static void __cdecl htsshow_filesave(t_hts_callbackarg * carg, httrackp * opt,
                                     const char *file);
static void __cdecl htsshow_filesave2(t_hts_callbackarg * carg, httrackp * opt,
                                      const char *adr, const char *fil,
                                      const char *save, int is_new,
                                      int is_modified, int not_updated);
static int __cdecl htsshow_linkdetected(t_hts_callbackarg * carg,
                                        httrackp * opt, char *link);
static int __cdecl htsshow_linkdetected2(t_hts_callbackarg * carg,
                                         httrackp * opt, char *link,
                                         const char *start_tag);
static int __cdecl htsshow_xfrstatus(t_hts_callbackarg * carg, httrackp * opt,
                                     lien_back * back);
static int __cdecl htsshow_savename(t_hts_callbackarg * carg, httrackp * opt,
                                    const char *adr_complete,
                                    const char *fil_complete,
                                    const char *referer_adr,
                                    const char *referer_fil, char *save);
static int __cdecl htsshow_sendheader(t_hts_callbackarg * carg, httrackp * opt,
                                      char *buff, const char *adr,
                                      const char *fil, const char *referer_adr,
                                      const char *referer_fil,
                                      htsblk * outgoing);
static int __cdecl htsshow_receiveheader(t_hts_callbackarg * carg,
                                         httrackp * opt, char *buff,
                                         const char *adr, const char *fil,
                                         const char *referer_adr,
                                         const char *referer_fil,
                                         htsblk * incoming);

static void vt_clear(void);
static void vt_home(void);

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
static void vt_clear(void) {
  printf("%s%s%s", VT_RESET, VT_CLRSCR, VT_GOTOXY("1", "0"));
}
static void vt_home(void) {
  printf("%s%s", VT_RESET, VT_GOTOXY("1", "0"));
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
static httrackp *global_opt = NULL;

static void signal_handlers(void);

int main(int argc, char **argv) {
  int ret = 0;
  httrackp *opt;

#ifdef _WIN32
  {
    WORD wVersionRequested;     // requested version WinSock API
    WSADATA wsadata;            // Windows Sockets API data
    int stat;

    wVersionRequested = 0x0101;
    stat = WSAStartup(wVersionRequested, &wsadata);
    if (stat != 0) {
      printf("Winsock not found!\n");
      return;
    } else if (LOBYTE(wsadata.wVersion) != 1 && HIBYTE(wsadata.wVersion) != 1) {
      printf("WINSOCK.DLL does not support version 1.1\n");
      WSACleanup();
      return;
    }
  }
#endif

  signal_handlers();
  hts_init();

  // Check version compatibility
  if (hts_sizeof_opt() != sizeof(httrackp)) {
    fprintf(stderr,
      "incompatible current httrack library version %s, expected version %s",
      hts_version(), HTTRACK_VERSIONID);
    abortLog("incompatible httrack library version, please update both httrack and its library");
  }

  opt = global_opt = hts_create_opt();
  assert(opt->size_httrackp == sizeof(httrackp));

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
  if (ret) {
    fprintf(stderr, "* %s\n", hts_errmsg(opt));
  }
  global_opt = NULL;
  hts_free_opt(opt);
  htsthread_wait();             /* wait for pending threads */
  hts_uninit();

#ifdef _WIN32
  WSACleanup();
#endif

  return ret;
}

/* CALLBACK FUNCTIONS */

/* Initialize the Winsock */
static void __cdecl htsshow_init(t_hts_callbackarg * carg) {
}
static void __cdecl htsshow_uninit(t_hts_callbackarg * carg) {
}
static int __cdecl htsshow_start(t_hts_callbackarg * carg, httrackp * opt) {
  use_show = 0;
  if (opt->verbosedisplay == 2) {
    use_show = 1;
    vt_clear();
  }
  return 1;
}
static int __cdecl htsshow_chopt(t_hts_callbackarg * carg, httrackp * opt) {
  return htsshow_start(carg, opt);
}
static int __cdecl htsshow_end(t_hts_callbackarg * carg, httrackp * opt) {
  return 1;
}
static int __cdecl htsshow_preprocesshtml(t_hts_callbackarg * carg,
                                          httrackp * opt, char **html, int *len,
                                          const char *url_address,
                                          const char *url_file) {
  return 1;
}
static int __cdecl htsshow_postprocesshtml(t_hts_callbackarg * carg,
                                           httrackp * opt, char **html,
                                           int *len, const char *url_address,
                                           const char *url_file) {
  return 1;
}
static int __cdecl htsshow_checkhtml(t_hts_callbackarg * carg, httrackp * opt,
                                     char *html, int len,
                                     const char *url_address,
                                     const char *url_file) {
  return 1;
}
static int __cdecl htsshow_loop(t_hts_callbackarg * carg, httrackp * opt, lien_back * back, int back_max, int back_index, int lien_n, int lien_tot, int stat_time, hts_stat_struct * stats) {   // appelé à chaque boucle de HTTrack
  static TStamp prev_mytime = 0;        /* ok */
  static t_InpInfo SInfo;       /* ok */

  //
  TStamp mytime;
  long int rate = 0;
  char st[256];

  //
  int stat_written = -1;
  int stat_updated = -1;
  int stat_errors = -1;
  int stat_warnings = -1;
  int stat_infos = -1;
  int nbk = -1;
  int stat_nsocket = -1;
  LLint stat_bytes = -1;
  LLint stat_bytes_recv = -1;
  int irate = -1;

  if (stats) {
    stat_written = stats->stat_files;
    stat_updated = stats->stat_updated_files;
    stat_errors = stats->stat_errors;
    stat_warnings = stats->stat_warnings;
    stat_infos = stats->stat_infos;
    nbk = stats->nbk;
    stat_nsocket = stats->stat_nsocket;
    irate = (int) stats->rate;
    stat_bytes = stats->nb;
    stat_bytes_recv = stats->HTS_TOTAL_RECV;
  }

  if (!use_show)
    return 1;

  mytime = mtime_local();
  if ((stat_time > 0) && (stat_bytes_recv > 0))
    rate = (int) (stat_bytes_recv / stat_time);
  else
    rate = 0;                   // pas d'infos

  /* Infos */
  if (stat_bytes >= 0)
    SInfo.stat_bytes = stat_bytes;      // bytes
  if (stat_time >= 0)
    SInfo.stat_time = stat_time;        // time
  if (lien_tot >= 0)
    SInfo.lien_tot = lien_tot;  // nb liens
  if (lien_n >= 0)
    SInfo.lien_n = lien_n;      // scanned
  SInfo.stat_nsocket = stat_nsocket;    // socks
  if (rate > 0)
    SInfo.rate = rate;          // rate
  if (irate >= 0)
    SInfo.irate = irate;        // irate
  if (SInfo.irate < 0)
    SInfo.irate = SInfo.rate;
  if (nbk >= 0)
    SInfo.stat_back = nbk;
  if (stat_written >= 0)
    SInfo.stat_written = stat_written;
  if (stat_updated >= 0)
    SInfo.stat_updated = stat_updated;
  if (stat_errors >= 0)
    SInfo.stat_errors = stat_errors;
  if (stat_warnings >= 0)
    SInfo.stat_warnings = stat_warnings;
  if (stat_infos >= 0)
    SInfo.stat_infos = stat_infos;

  if (((mytime - prev_mytime) > 100) || ((mytime - prev_mytime) < 0)) {
    strc_int2bytes2 strc, strc2, strc3;

    prev_mytime = mytime;

    st[0] = '\0';
    qsec2str(st, stat_time);
    vt_home();
    printf(VT_GOTOXY("1", "1")
           VT_CLREOL STYLE_STATTEXT "Bytes saved:" STYLE_STATVALUES " \t%s" "\t"
           VT_CLREOL VT_GOTOXY("40", "1")
           STYLE_STATTEXT "Links scanned:" STYLE_STATVALUES " \t%d/%d (+%d)"
           VT_CLREOL "\n" VT_CLREOL VT_GOTOXY("1", "2")
           STYLE_STATTEXT "Time:" " \t" STYLE_STATVALUES "%s" "\t" VT_CLREOL
           VT_GOTOXY("40", "2")
           STYLE_STATTEXT "Files written:" " \t" STYLE_STATVALUES "%d" VT_CLREOL
           "\n" VT_CLREOL VT_GOTOXY("1", "3")
           STYLE_STATTEXT "Transfer rate:" " \t" STYLE_STATVALUES "%s (%s)" "\t"
           VT_CLREOL VT_GOTOXY("40", "3")
           STYLE_STATTEXT "Files updated:" " \t" STYLE_STATVALUES "%d" VT_CLREOL
           "\n" VT_CLREOL VT_GOTOXY("1", "4")
           STYLE_STATTEXT "Active connections:" " \t" STYLE_STATVALUES "%d" "\t"
           VT_CLREOL VT_GOTOXY("40", "4")
           STYLE_STATTEXT "Errors:" STYLE_STATVALUES " \t" STYLE_STATVALUES "%d"
           VT_CLREOL "\n" STYLE_STATRESET,
           /* */
           (char *) int2bytes(&strc, SInfo.stat_bytes), (int) lien_n,
           (int) SInfo.lien_tot, (int) nbk, (char *) st,
           (int) SInfo.stat_written, (char *) int2bytessec(&strc2, SInfo.irate),
           (char *) int2bytessec(&strc3, SInfo.rate), (int) SInfo.stat_updated,
           (int) SInfo.stat_nsocket, (int) SInfo.stat_errors
           /* */
      );

    // parcourir registre des liens
    if (back_index >= 0) {      // seulement si index passé
      int j, k;
      int index = 0;
      int ok = 0;               // idem
      int l;                    // idem

      //
      t_StatsBuffer StatsBuffer[NStatsBuffer];

      {
        int i;

        for(i = 0; i < NStatsBuffer; i++) {
          strcpybuff(StatsBuffer[i].state, "");
          strcpybuff(StatsBuffer[i].name, "");
          strcpybuff(StatsBuffer[i].file, "");
          strcpybuff(StatsBuffer[i].url_sav, "");
          StatsBuffer[i].back = 0;
          StatsBuffer[i].size = 0;
          StatsBuffer[i].sizetot = 0;
        }
      }
      for(k = 0; k < 2; k++) {  // 0: lien en cours 1: autres liens
        for(j = 0; (j < 3) && (index < NStatsBuffer); j++) {    // passe de priorité
          int _i;

          for(_i = 0 + k; (_i < max(back_max * k, 1)) && (index < NStatsBuffer); _i++) {        // no lien
            int i = (back_index + _i) % back_max;       // commencer par le "premier" (l'actuel)

            if (back[i].status >= 0) {  // signifie "lien actif"
              // int ok=0;  // OPTI
              ok = 0;
              switch (j) {
              case 0:          // prioritaire
                if ((back[i].status > 0) && (back[i].status < 99)) {
                  strcpybuff(StatsBuffer[index].state, "receive");
                  ok = 1;
                }
                break;
              case 1:
                if (back[i].status == STATUS_WAIT_HEADERS) {
                  strcpybuff(StatsBuffer[index].state, "request");
                  ok = 1;
                } else if (back[i].status == STATUS_CONNECTING) {
                  strcpybuff(StatsBuffer[index].state, "connect");
                  ok = 1;
                } else if (back[i].status == STATUS_WAIT_DNS) {
                  strcpybuff(StatsBuffer[index].state, "search");
                  ok = 1;
                } else if (back[i].status == STATUS_FTP_TRANSFER) {     // ohh le beau ftp
                  sprintf(StatsBuffer[index].state, "ftp: %s", back[i].info);
                  ok = 1;
                }
                break;
              default:
                if (back[i].status == STATUS_READY) {   // prêt
                  if (back[i].r.statuscode == 200) {
                    strcpybuff(StatsBuffer[index].state, "ready");
                    ok = 1;
                  } else if (back[i].r.statuscode >= 100
                             && back[i].r.statuscode <= 599) {
                    char tempo[256];

                    tempo[0] = '\0';
                    infostatuscode(tempo, back[i].r.statuscode);
                    strcpybuff(StatsBuffer[index].state, tempo);
                    ok = 1;
                  } else {
                    strcpybuff(StatsBuffer[index].state, "error");
                    ok = 1;
                  }
                }
                break;
              }

              if (ok) {
                char BIGSTK s[HTS_URLMAXSIZE * 2];

                //
                StatsBuffer[index].back = i;    // index pour + d'infos
                //
                s[0] = '\0';
                strcpybuff(StatsBuffer[index].url_sav, back[i].url_sav);        // pour cancel
                if (strcmp(back[i].url_adr, "file://"))
                  strcatbuff(s, back[i].url_adr);
                else
                  strcatbuff(s, "localhost");
                if (back[i].url_fil[0] != '/')
                  strcatbuff(s, "/");
                strcatbuff(s, back[i].url_fil);

                StatsBuffer[index].file[0] = '\0';
                {
                  char *a = strrchr(s, '/');

                  if (a) {
                    strncatbuff(StatsBuffer[index].file, a, 200);
                    *a = '\0';
                  }
                }

                if ((l = (int) strlen(s)) < MAX_LEN_INPROGRESS)
                  strcpybuff(StatsBuffer[index].name, s);
                else {
                  // couper
                  StatsBuffer[index].name[0] = '\0';
                  strncatbuff(StatsBuffer[index].name, s,
                              MAX_LEN_INPROGRESS / 2 - 2);
                  strcatbuff(StatsBuffer[index].name, "...");
                  strcatbuff(StatsBuffer[index].name,
                             s + l - MAX_LEN_INPROGRESS / 2 + 2);
                }

                if (back[i].r.totalsize >= 0) { // taille prédéfinie
                  StatsBuffer[index].sizetot = back[i].r.totalsize;
                  StatsBuffer[index].size = back[i].r.size;
                } else {        // pas de taille prédéfinie
                  if (back[i].status == STATUS_READY) { // prêt
                    StatsBuffer[index].sizetot = back[i].r.size;
                    StatsBuffer[index].size = back[i].r.size;
                  } else {
                    StatsBuffer[index].sizetot = 8192;
                    StatsBuffer[index].size = (back[i].r.size % 8192);
                  }
                }
                index++;
              }
            }
          }
        }
      }

      /* LF */
      printf("%s\n", VT_CLREOL);

      /* Display current job */
      {
        int parsing = 0;

        printf("Current job: ");
        if (!(parsing = hts_is_parsing(opt, -1)))
          printf("receiving files");
        else {
          switch (hts_is_testing(opt)) {
          case 0:
            printf("parsing HTML file (%d%%)", parsing);
            break;
          case 1:
            printf("parsing HTML file: testing links (%d%%)", parsing);
            break;
          case 2:
            printf("purging files");
            break;
          case 3:
            printf("loading cache");
            break;
          case 4:
            printf("waiting (scheduler)");
            break;
          case 5:
            printf("waiting (throttle)");
            break;
          }
        }
        printf("%s\n", VT_CLREOL);
      }

      /* Display background jobs */
      {
        int i;

        for(i = 0; i < NStatsBuffer; i++) {
          if (strnotempty(StatsBuffer[i].state)) {
            printf(VT_CLREOL " %s - \t%s%s \t%s / \t%s", StatsBuffer[i].state,
                   StatsBuffer[i].name, StatsBuffer[i].file, int2bytes(&strc,
                                                                       StatsBuffer
                                                                       [i].
                                                                       size),
                   int2bytes(&strc2, StatsBuffer[i].sizetot)
              );
          }
          printf("%s\n", VT_CLREOL);
        }
      }

    }

  }

  return 1;
}
static const char *__cdecl htsshow_query(t_hts_callbackarg * carg,
                                         httrackp * opt, const char *question) {
  static char s[12] = "";       /* ok */

  printf("%s\nPress <Y><Enter> to confirm, <N><Enter> to abort\n", question);
  io_flush;
  linput(stdin, s, 4);
  return s;
}
static const char *__cdecl htsshow_query2(t_hts_callbackarg * carg,
                                          httrackp * opt,
                                          const char *question) {
  static char s[12] = "";       /* ok */

  printf("%s\nPress <Y><Enter> to confirm, <N><Enter> to abort\n", question);
  io_flush;
  linput(stdin, s, 4);
  return s;
}
static const char *__cdecl htsshow_query3(t_hts_callbackarg * carg,
                                          httrackp * opt,
                                          const char *question) {
  static char line[256];        /* ok */

  printf("\n" "A link, %s, is located beyond this mirror scope.\n"
         "What should I do? (type in the choice + enter)\n\n"
         "* Ignore all further links and do not ask any more questions\n"
         "0 Ignore this link (default if empty entry)\n"
         "1 Ignore directory and lower structures\n" "2 Ignore all domain\n"
         "\n" "4 Get only this page/link, but not links inside this page\n"
         "5 Mirror this link (useful)\n"
         "6 Mirror all links located on the same domain as this link\n" "\n",
         question);
  do {
    printf(">> ");
    io_flush;
    linput(stdin, line, 200);
  } while(!strnotempty(line));
  printf("ok..\n");
  return line;
}
static int __cdecl htsshow_check(t_hts_callbackarg * carg, httrackp * opt,
                                 const char *adr, const char *fil, int status) {
  return -1;
}
static int __cdecl htsshow_check_mime(t_hts_callbackarg * carg, httrackp * opt,
                                      const char *adr, const char *fil,
                                      const char *mime, int status) {
  return -1;
}
static void __cdecl htsshow_pause(t_hts_callbackarg * carg, httrackp * opt,
                                  const char *lockfile) {
  while(fexist(lockfile)) {
    Sleep(1000);
  }
}
static void __cdecl htsshow_filesave(t_hts_callbackarg * carg, httrackp * opt,
                                     const char *file) {
}
static void __cdecl htsshow_filesave2(t_hts_callbackarg * carg, httrackp * opt,
                                      const char *adr, const char *fil,
                                      const char *save, int is_new,
                                      int is_modified, int not_updated) {
}
static int __cdecl htsshow_linkdetected(t_hts_callbackarg * carg,
                                        httrackp * opt, char *link) {
  return 1;
}
static int __cdecl htsshow_linkdetected2(t_hts_callbackarg * carg,
                                         httrackp * opt, char *link,
                                         const char *start_tag) {
  return 1;
}
static int __cdecl htsshow_xfrstatus(t_hts_callbackarg * carg, httrackp * opt,
                                     lien_back * back) {
  return 1;
}
static int __cdecl htsshow_savename(t_hts_callbackarg * carg, httrackp * opt,
                                    const char *adr_complete,
                                    const char *fil_complete,
                                    const char *referer_adr,
                                    const char *referer_fil, char *save) {
  return 1;
}
static int __cdecl htsshow_sendheader(t_hts_callbackarg * carg, httrackp * opt,
                                      char *buff, const char *adr,
                                      const char *fil, const char *referer_adr,
                                      const char *referer_fil,
                                      htsblk * outgoing) {
  return 1;
}
static int __cdecl htsshow_receiveheader(t_hts_callbackarg * carg,
                                         httrackp * opt, char *buff,
                                         const char *adr, const char *fil,
                                         const char *referer_adr,
                                         const char *referer_fil,
                                         htsblk * incoming) {
  return 1;
}

/* *** Various functions *** */

static int fexist(const char *s) {
  struct stat st;

  memset(&st, 0, sizeof(st));
  if (stat(s, &st) == 0) {
    if (S_ISREG(st.st_mode)) {
      return 1;
    }
  }
  return 0;
}

static int linput(FILE * fp, char *s, int max) {
  int c;
  int j = 0;

  do {
    c = fgetc(fp);
    if (c != EOF) {
      switch (c) {
      case 13:
        break;                  // sauter CR
      case 10:
        c = -1;
        break;
      case 9:
      case 12:
        break;                  // sauter ces caractères
      default:
        s[j++] = (char) c;
        break;
      }
    }
  } while((c != -1) && (c != EOF) && (j < (max - 1)));
  s[j] = '\0';
  return j;
}

// routines de détournement de SIGHUP & co (Unix)
//
static void sig_ignore(int code) {      // ignorer signal
}
static void sig_term(int code) {        // quitter brutalement
  fprintf(stderr, "\nProgram terminated (signal %d)\n", code);
  exit(0);
}
static void sig_finish(int code) {      // finir et quitter
  signal(code, sig_term);       // quitter si encore
  if (global_opt != NULL) {
    global_opt->state.exit_xh = 1;
  }
  fprintf(stderr, "\nExit requested to engine (signal %d)\n", code);
}

#ifdef _WIN32
#if 0
static void sig_ask(int code) { // demander
  char s[256];

  signal(code, sig_term);       // quitter si encore
  printf("\nQuit program/Interrupt/Cancel? (Q/I/C) ");
  fflush(stdout);
  scanf("%s", s);
  if ((s[0] == 'y') || (s[0] == 'Y') || (s[0] == 'o') || (s[0] == 'O')
      || (s[0] == 'q') || (s[0] == 'Q'))
    exit(0);                    // quitter
  else if ((s[0] == 'i') || (s[0] == 'I')) {
    if (global_opt != NULL) {
      // ask for stop
      global_opt->state.stop = 1;
    }
  }
  signal(code, sig_ask);        // remettre signal
}
#endif
#else
static void sig_doback(int blind);
static void sig_back(int code) {        // ignorer et mettre en backing 
  if (global_opt != NULL && !global_opt->background_on_suspend) {
    signal(SIGTSTP, SIG_DFL);   // ^Z
    printf("\nInterrupting the program.\n");
    fflush(stdout);
    kill(getpid(), SIGTSTP);
  } else {
    // Background the process.
    signal(code, sig_ignore);
    sig_doback(0);
  }
}

#if 0
static void sig_ask(int code) { // demander
  char s[256];

  signal(code, sig_term);       // quitter si encore
  printf
    ("\nQuit program/Interrupt/Background/bLind background/Cancel? (Q/I/B/L/C) ");
  fflush(stdout);
  scanf("%s", s);
  if ((s[0] == 'y') || (s[0] == 'Y') || (s[0] == 'o') || (s[0] == 'O')
      || (s[0] == 'q') || (s[0] == 'Q'))
    exit(0);                    // quitter
  else if ((s[0] == 'b') || (s[0] == 'B') || (s[0] == 'a') || (s[0] == 'A'))
    sig_doback(0);              // arrière plan
  else if ((s[0] == 'l') || (s[0] == 'L'))
    sig_doback(1);              // arrière plan
  else if ((s[0] == 'i') || (s[0] == 'I')) {
    if (global_opt != NULL) {
      // ask for stop
      printf("finishing pending transfers.. please wait\n");
      global_opt->state.stop = 1;
    }
    signal(code, sig_ask);      // remettre signal
  } else {
    printf("cancel..\n");
    signal(code, sig_ask);      // remettre signal
  }
}
#endif

static void sig_brpipe(int code) {      // treat if necessary
  signal(code, sig_brpipe);
}
static void sig_doback(int blind) {     // mettre en backing 
  int out = -1;

  //
  printf("\nMoving into background to complete the mirror...\n");
  fflush(stdout);

  if (global_opt != NULL) {
    // suppress logging and asking lousy questions
    global_opt->quiet = 1;
    global_opt->verbosedisplay = 0;
  }

  if (!blind)
    out = open("hts-nohup.out", O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
  if (out == -1)
    out = open("/dev/null", O_WRONLY, S_IRUSR | S_IWUSR);
  dup2(out, 0);
  dup2(out, 1);
  dup2(out, 2);
  //
  switch (fork()) {
  case 0:
    break;
  case -1:
    fprintf(stderr, "Error: can not fork process\n");
    break;
  default:                     // pere
    _exit(0);
    break;
  }
}
#endif

#undef FD_ERR
#define FD_ERR 2

static void print_backtrace(void) {
#ifdef USES_BACKTRACE
  void *stack[256];
  const int size = backtrace(stack, sizeof(stack)/sizeof(stack[0]));
  if (size != 0) {
    backtrace_symbols_fd(stack, size, FD_ERR);
  }
#else
  const char msg[] = "No stack trace available on this OS :(\n";
  if (write(FD_ERR, msg, sizeof(msg) - 1) != sizeof(msg) - 1) {
    /* sorry GCC */
  }
#endif
}

static size_t print_num(char *buffer, int num) {
  size_t i, j;
  if (num < 0) {
    *(buffer++) = '-';
    num = -num;
  }
  for(i = 0 ; num != 0 || i == 0 ; i++, num /= 10) {
    buffer[i] = '0' + ( num % 10 );
  }
  for(j = 0 ; j < i ; j++) {
    const char c = buffer[i - j - 1];
    buffer[i - j - 1] = buffer[j];
    buffer[j] = c;
  }
  buffer[i] = '\0';
  return i;
}

static void sig_fatal(int code) {
  const char msg[] = "\nCaught signal ";
  const char msgreport[] =
    "\nPlease report the problem at http://forum.httrack.com\n";
  char buffer[256];
  size_t size;

  signal(code, SIG_DFL);
  signal(SIGABRT, SIG_DFL);

  memcpy(buffer, msg, sizeof(msg) - 1);
  size = sizeof(msg) - 1;
  size += print_num(&buffer[size], code);
  buffer[size++] = '\n';
  (void) (write(FD_ERR, buffer, size) == size);
  print_backtrace();
  (void) (write(FD_ERR, msgreport, sizeof(msgreport) - 1)
    == sizeof(msgreport) - 1);
  abort();
}

#undef FD_ERR

static void sig_leave(int code) {
  if (global_opt != NULL && global_opt->state._hts_in_mirror) {
    signal(code, sig_term);     // quitter si encore
    printf("\n** Finishing pending transfers.. press again ^C to quit.\n");
    if (global_opt != NULL) {
      // ask for stop
      hts_log_print(global_opt, LOG_ERROR, "Exit requested by shell or user");
      global_opt->state.stop = 1;
    }
  } else {
    sig_term(code);
  }
}

static void signal_handlers(void) {
#ifdef _WIN32
#if 0                           /* BUG366763 */
  signal(SIGINT, sig_ask);      // ^C
#else
  signal(SIGINT, sig_leave);    // ^C
#endif
  signal(SIGTERM, sig_finish);  // kill <process>
#else
#if 0                           /* BUG366763 */
  signal(SIGHUP, sig_back);     // close window
#endif
  signal(SIGTSTP, sig_back);    // ^Z
  signal(SIGTERM, sig_finish);  // kill <process>
#if 0                           /* BUG366763 */
  signal(SIGINT, sig_ask);      // ^C
#else
  signal(SIGINT, sig_leave);    // ^C
#endif
  signal(SIGPIPE, sig_brpipe);  // broken pipe (write into non-opened socket)
  signal(SIGCHLD, sig_ignore);  // child change status
#endif
#ifdef SIGABRT
  signal(SIGABRT, sig_fatal);    // abort
#endif
#ifdef SIGBUS
  signal(SIGBUS, sig_fatal);    // bus error
#endif
#ifdef SIGILL
  signal(SIGILL, sig_fatal);    // illegal instruction
#endif
#ifdef SIGSEGV
  signal(SIGSEGV, sig_fatal);   // segmentation violation
#endif
#ifdef SIGSTKFLT
  signal(SIGSTKFLT, sig_fatal); // stack fault
#endif
}

// fin routines de détournement de SIGHUP & co
