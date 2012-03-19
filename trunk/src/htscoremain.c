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
/* File: httrack.c subroutines:                                 */
/*       main routine (first called)                            */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htscoremain.h"

#include "htsglobal.h"
#include "htscore.h"
#include "htsdefines.h"
#include "htsalias.h"
#include "htswrap.h"
#include <ctype.h>
#if HTS_WIN
#else
#ifndef HTS_DO_NOT_USE_UID
/* setuid */
#include <pwd.h>
#include <unistd.h>
#endif
#endif

extern int exit_xh;          // sortir prématurément

/* Resolver */
extern int IPV6_resolver;


// Add a command in the argc/argv
#define cmdl_add(token,argc,argv,buff,ptr) \
  argv[argc]=(buff+ptr); \
  strcpy(argv[argc],token); \
  ptr += (strlen(argv[argc])+2); \
  argc++

// Insert a command in the argc/argv
#define cmdl_ins(token,argc,argv,buff,ptr) \
  { \
  int i; \
  for(i=argc;i>0;i--)\
  argv[i]=argv[i-1];\
  } \
  argv[0]=(buff+ptr); \
  strcpy(argv[0],token); \
  ptr += (strlen(argv[0])+2); \
  argc++

#define htsmain_free() do { if (url != NULL) { free(url); } } while(0)

// Main, récupère les paramètres et appelle le robot
#if HTS_ANALYSTE
int hts_main(int argc, char **argv) {
#else
int main(int argc, char **argv) {
#endif
  char* x_argv[999];      // Patch pour argv et argc: en cas de récupération de ligne de commande
  char* x_argvblk=NULL;   // (reprise ou update)
  int   x_ptr=0;          // offset
  /*
  char* x_argv2[999];     // Patch pour config
  char* x_argvblk2=NULL;
  */
  //
  int argv_url=-1;           // ==0 : utiliser cache et doit.log
  char* argv_firsturl=NULL;  // utilisé pour nommage par défaut
  char* url = NULL;          // URLS séparées par un espace
  //char url[65536];         // URLS séparées par un espace
  // the parametres
  httrackp httrack;
  int httrack_logmode=3;   // ONE log file
  int recuperer=0;       // récupérer un plantage (n'arrive jamais, à supprimer)
#if HTS_WIN
#if HTS_ANALYSTE!=2
  WORD   wVersionRequested; /* requested version WinSock API */ 
  WSADATA wsadata;        /* Windows Sockets API data */
#endif
#else
#ifndef HTS_DO_NOT_USE_UID
  int switch_uid=-1,switch_gid=-1;      /* setuid/setgid */
#endif
  int switch_chroot=0;                  /* chroot ? */
#endif
  //
  url = malloc(65536);
  if (url == NULL) {
    HTS_PANIC_PRINTF("* memory exhausted");
    htsmain_free();
    return -1;
  }
  url[0]='\0';
  //

#if HTS_ANALYSTE
  // custom wrappers
  hts_htmlcheck_init         = (t_hts_htmlcheck_init)           htswrap_read("init");
  hts_htmlcheck_uninit       = (t_hts_htmlcheck_uninit)         htswrap_read("free");
  hts_htmlcheck_start        = (t_hts_htmlcheck_start)          htswrap_read("start");
  hts_htmlcheck_end          = (t_hts_htmlcheck_end)            htswrap_read("end");
  hts_htmlcheck_chopt        = (t_hts_htmlcheck_chopt)          htswrap_read("change-options");
  hts_htmlcheck              = (t_hts_htmlcheck)                htswrap_read("check-html");
  hts_htmlcheck_query        = (t_hts_htmlcheck_query)          htswrap_read("query");
  hts_htmlcheck_query2       = (t_hts_htmlcheck_query2)         htswrap_read("query2");
  hts_htmlcheck_query3       = (t_hts_htmlcheck_query3)         htswrap_read("query3");
  hts_htmlcheck_loop         = (t_hts_htmlcheck_loop)           htswrap_read("loop");
  hts_htmlcheck_check        = (t_hts_htmlcheck_check)          htswrap_read("check-link");
  hts_htmlcheck_pause        = (t_hts_htmlcheck_pause)          htswrap_read("pause");
  hts_htmlcheck_filesave     = (t_hts_htmlcheck_filesave)       htswrap_read("save-file");
  hts_htmlcheck_linkdetected = (t_hts_htmlcheck_linkdetected)   htswrap_read("link-detected");
  hts_htmlcheck_xfrstatus    = (t_hts_htmlcheck_xfrstatus)      htswrap_read("transfer-status");
  hts_htmlcheck_savename     = (t_hts_htmlcheck_savename)       htswrap_read("save-name");
#endif

  // options par défaut
  memset(&httrack, 0, sizeof(httrackp));
  httrack.wizard=2;   // wizard automatique
  httrack.quiet=0;     // questions
  //  
  httrack.travel=0;   // même adresse
  httrack.depth=9999; // mirror total par défaut
  httrack.extdepth=0; // mais pas à l'extérieur
  httrack.seeker=1;   // down 
  httrack.urlmode=2;  // relatif par défaut
  httrack.debug=0;    // pas de débug en plus
  httrack.getmode=3;  // linear scan
  httrack.maxsite=-1; // taille max site (aucune)
  httrack.maxfile_nonhtml=-1; // taille max fichier non html
  httrack.maxfile_html=-1;    // idem pour html
  httrack.maxsoc=8;     // nbre socket max
  httrack.fragment=-1;  // pas de fragmentation
  httrack.nearlink=0;   // ne pas prendre les liens non-html "adjacents"
  httrack.makeindex=1;  // faire un index
  httrack.kindex=0;     // index 'keyword'
  httrack.delete_old=1; // effacer anciens fichiers
  httrack.makestat=0;  // pas de fichier de stats
  httrack.maketrack=0; // ni de tracking
  httrack.timeout=120; // timeout par défaut (2 minutes)
  httrack.cache=1;     // cache prioritaire
  httrack.shell=0;     // pas de shell par defaut
  httrack.proxy.active=0;    // pas de proxy
  httrack.user_agent_send=1; // envoyer un user-agent
  strcpy(httrack.user_agent,"Mozilla/4.5 (compatible; HTTrack 3.0x; Windows 98)");
  httrack.savename_83=0;     // noms longs par défaut
  httrack.savename_type=0;   // avec structure originale
  httrack.parsejava=1;       // parser classes
  httrack.hostcontrol=0;     // PAS de control host pour timeout et traffic jammer
  httrack.retry=2;           // 2 retry par défaut
  httrack.errpage=1;         // copier ou générer une page d'erreur en cas d'erreur (404 etc.)
  httrack.check_type=1;      // vérifier type si inconnu (cgi,asp..) SAUF / considéré comme html
  httrack.all_in_cache=0;    // ne pas tout stocker en cache
  httrack.robots=2;          // traiter les robots.txt
  httrack.external=0;        // liens externes normaux
  httrack.passprivacy=0;     // mots de passe dans les fichiers
  httrack.includequery=1;    // include query-string par défaut
  httrack.mirror_first_page=0;  // pas mode mirror links
  httrack.accept_cookie=1;   // gérer les cookies
  httrack.cookie=NULL;
  httrack.http10=0;          // laisser http/1.1
  httrack.nocompression=0;   // pas de compression
  httrack.tolerant=0;        // ne pas accepter content-length incorrect
  httrack.parseall=1;        // tout parser (tags inconnus, par exemple)
  httrack.norecatch=0;       // ne pas reprendre les fichiers effacés par l'utilisateur
  httrack.verbosedisplay=0;  // pas d'animation texte
  strcpy(httrack.footer,HTS_DEFAULT_FOOTER);
  httrack.ftp_proxy=1;       // proxy http pour ftp
  strcpy(httrack.filelist,"");
  strcpy(httrack.lang_iso,"en, *");
  strcpy(httrack.mimedefs,"\n"); // aucun filtre mime (\n IMPORTANT)
  //
  httrack.log=stdout;
  httrack.errlog=stderr;
  httrack.flush=1;           // flush sur les fichiers log
  httrack.aff_progress=0;
  httrack.keyboard=0;
  //
  strcpy(httrack.path_html,"");
  strcpy(httrack.path_log,"");
  strcpy(httrack.path_bin,"");
  //
  httrack.maxlink=100000;    // 100,000 liens max par défaut (400Kb)
  httrack.maxfilter=200;     // 200 filtres max par défaut
  httrack.maxcache=1048576*32;  // a peu près 32Mo en cache max -- OPTION NON PARAMETRABLE POUR L'INSTANT --
  //httrack.maxcache_anticipate=256;  // maximum de liens à anticiper
  httrack.maxtime=-1;        // temps max en secondes
  httrack.maxrate=-1;        // pas de taux maxi
  httrack.maxconn=10;        // nombre connexions/s
  httrack.waittime=-1;       // wait until.. hh*3600+mm*60+ss
  //
  httrack.exec=argv[0];
  httrack.is_update=0;      // not an update (yet)
  httrack.dir_topindex=0;   // do not built top index (yet)
  //
  httrack.state.stop=0;     // stopper
  //
  _DEBUG_HEAD=0;            // pas de debuggage en têtes
  
#if HTS_WIN
#if HTS_ANALYSTE!=2
  {
    int stat;
    wVersionRequested = 0x0101;
    stat = WSAStartup( wVersionRequested, &wsadata );
    if (stat != 0) {
      HTS_PANIC_PRINTF("Winsock not found!\n");
      htsmain_free();
      return -1;
    } else if (LOBYTE(wsadata.wVersion) != 1  && HIBYTE(wsadata.wVersion) != 1) {
      HTS_PANIC_PRINTF("WINSOCK.DLL does not support version 1.1\n");
      WSACleanup();
      htsmain_free();
      return -1;
    }
  }
#endif
#endif

  /* Init root dir */
  hts_rootdir(argv[0]);

#if HTS_WIN
#else
  /* Terminal is a tty, may ask questions and display funny information */
  if (isatty(1)) {
    httrack.quiet=0;
    httrack.verbosedisplay=1;
  }
  /* Not a tty, no stdin input or funny output! */
  else {
    httrack.quiet=1;
    httrack.verbosedisplay=0;
  }
#endif

  /* First test: if -#R then only launch ftp */
  if (argc > 2) {
    if (strcmp(argv[1],"-#R")==0) {
      if (argc==6) {
        lien_back r;
        char* path;
        FILE* fp;
        strcpy(r.url_adr,argv[2]);
        strcpy(r.url_fil,argv[3]);
        strcpy(r.url_sav,argv[4]);
        path=argv[5];
        r.status=1000;
        run_launch_ftp(&r);
        fp=fopen(fconv(path),"wb");
        if (fp) {
          fprintf(fp,"%d %s",r.r.statuscode,r.r.msg);
          fclose(fp); fp=NULL;
          rename(fconv(path),fconcat(path,".ok"));
        } else remove(fconv(path));
      } else {
        printf("htsftp error, wrong parameter number (%d)\n",argc);
      }
      exit(0);   // pas _exit()
    }
  }

  // ok, non ftp, continuer


  // Binary program path?
#ifndef HTS_HTTRACKDIR
  {
    char* path=fslash(argv[0]);
    char* a;
    if ((a=strrchr(path,'/'))) {
      httrack.path_bin[0]='\0';
      strncat(httrack.path_bin,argv[0],(int) a - (int) path);
    }
  }
#else
  strcpy(httrack.path_bin,HTS_HTTRACKDIR);
#endif


  /* filter CR, LF, TAB.. */
  {
    int na;
    for(na=1;na<argc;na++) {
      char* a;
      while( (a=strchr(argv[na],'\x0d')) ) *a=' ';
      while( (a=strchr(argv[na],'\x0a')) ) *a=' ';
      while( (a=strchr(argv[na],9)) )      *a=' ';
      /* equivalent to "empty parameter" */
      if ((strcmp(argv[na],HTS_NOPARAM)==0) || (strcmp(argv[na],HTS_NOPARAM2)==0))        // (none)
        strcpy(argv[na],"\"\"");
      if (strncmp(argv[na],"-&",2)==0)
        argv[na][1]='%';
    }
  }



  /* create x_argvblk buffer for transformed command line */
  {
    int current_size=0;
    int size;
    int na;
    for(na=0;na<argc;na++)
      current_size += (strlen(argv[na]) + 1);
    if ((size=fsize("config"))>0)
      current_size += size;
    x_argvblk=(char*) malloct(current_size+32768);
    if (x_argvblk == NULL) {
      HTS_PANIC_PRINTF("Error, not enough memory");
      htsmain_free();
      return -1;
    }
    x_argvblk[0]='\0';
    x_ptr=0;
  }

  /* Create new argc/argv, replace alias, count URLs, treat -h, -q, -i */
  {
    char _tmp_argv[2][HTS_CDLMAXSIZE];
    char* tmp_argv[2];
    char tmp_error[HTS_CDLMAXSIZE];
    int tmp_argc;
    int x_argc=0;
    int na;
    tmp_argv[0]=_tmp_argv[0];
    tmp_argv[1]=_tmp_argv[1];
    //
    argv_url=0;       /* pour comptage */
    //
    cmdl_add(argv[0],x_argc,x_argv,x_argvblk,x_ptr);
    na=1;             /* commencer après nom_prg */
    while(na<argc) {
      int result=1;
      tmp_argv[0][0]=tmp_argv[1][0]='\0';

      /* Vérifier argv[] non vide */
      if (strnotempty(argv[na])) {
        
        /* Vérifier Commande (alias) */
        result=optalias_check(argc,(const char * const *)argv,na,
          &tmp_argc,(char**)tmp_argv,tmp_error);
        if (!result) {
          HTS_PANIC_PRINTF(tmp_error);
          htsmain_free();
          return -1;
        }
        
        /* Copier */
        cmdl_add(tmp_argv[0],x_argc,x_argv,x_argvblk,x_ptr);
        if (tmp_argc > 1) {
          cmdl_add(tmp_argv[1],x_argc,x_argv,x_argvblk,x_ptr);
        }
        
        /* Compter URLs et détecter -i,-q.. */
        if (tmp_argc == 1) {           /* pas -P & co */
          if (!cmdl_opt(tmp_argv[0])) {   /* pas -c0 & co */
            if (argv_url<0) argv_url=0;   // -1==force -> 1=one url already detected, wipe all previous options
            //if (argv_url>=0) {
            argv_url++;
            if (!argv_firsturl)
              argv_firsturl=x_argv[x_argc-1];
            //}
          } else {
            if (strcmp(tmp_argv[0],"-h")==0) {
              help(argv[0],!httrack.quiet);
              htsmain_free();
              return 0;
            } else {
              if (strncmp(tmp_argv[0],"--",2)) {   /* pas */
                if ((strchr(tmp_argv[0],'q')!=NULL))
                  httrack.quiet=1;    // ne pas poser de questions! (nohup par exemple)
                if ((strchr(tmp_argv[0],'i')!=NULL)) {  // doit.log!
                  argv_url=-1;        /* forcer */
                  httrack.quiet=1;
                }
              }
            }
          }
        } else if (tmp_argc == 2) {
          if ((strcmp(tmp_argv[0],"-%L")==0)) {  // liste d'URLs
            if (argv_url<0) argv_url=0;   // -1==force -> 1=one url already detected, wipe all previous options
            //if (argv_url>=0)
            argv_url++;        /* forcer */
          }
        }
      }

      na+=result;
    }
    if (argv_url<0)
      argv_url=0;

    /* Nouveaux argc et argv */
    argv=x_argv;
    argc=x_argc;
  }




  // Ici on ajoute les arguments de config
/*
  if (fexist("config")) {    // configuration        
    x_argvblk2=(char*) calloct(32768,1);

    if (x_argvblk2!=NULL) {
      FILE* fp;
      int x_argc2;
          
      //strcpy(x_argvblk2,"httrack ");
      fp=fopen("config","rb");
      if (fp) {
        linput(fp,x_argvblk2+strlen(x_argvblk2),32000);
        fclose(fp); fp=NULL;
    
        // calculer arguments selon derniers arguments
        x_argv2[0]=argv[0];
        x_argc2=1;
        {
          char* p=x_argvblk2;
          do {
            x_argv2[x_argc2++]=p;
            p=strchr(p,' ');
            if (p) {
              *p=0;    // octet nul (tableau)
              p++;
            }            
          } while(p!=NULL);
        }
        // recopier arguments actuels (pointeurs uniquement)
        {
         int na;
          for(na=1;na<argc;na++) {
            x_argv2[x_argc2++]=argv[na];
          }
        }
        argc=x_argc2;      // nouvel argc
        argv=x_argv2;      // nouvel argv
      }
    }
  }
*/


  // Option O and includerc
  { 
    int loops=0;
    while (loops<2) {
      char* com;
      int na;
      
      for(na=1;na<argc;na++) {
        
        if (argv[na][0]=='"') {
          char tempo[HTS_CDLMAXSIZE];
          strcpy(tempo,argv[na]+1);
          if (tempo[strlen(tempo)-1]!='"') {
            char s[HTS_CDLMAXSIZE];
            sprintf(s,"Missing quote in %s",argv[na]);
            HTS_PANIC_PRINTF(s);
            htsmain_free();
            return -1;
          }
          tempo[strlen(tempo)-1]='\0';
          strcpy(argv[na],tempo);
        }
        
        if (cmdl_opt(argv[na])) { // option
          com=argv[na]+1;
          
          while(*com) {
            switch(*com) {
            case 'O':    // output path
              if ((na+1>=argc) || (argv[na+1][0]=='-')) {
                HTS_PANIC_PRINTF("Option O needs to be followed by a blank space, and a path (or path,path)");
                printf("Example: -O /binary/\n");
                printf("Example: -O /binary/,/log/\n");
                htsmain_free();
                return -1;
              } else {
                char* a;
                na++;
                strcpy(httrack.path_html,"");
                strcpy(httrack.path_log,"");
                a=strstr(argv[na],"\",\"");  // rechercher en premier, au cas ou -O "c:\pipo,test","c:\test"
                if (!a)
                  a=strchr(argv[na],',');  // 2 path
                else
                  a++;  // position ,
                if (a) {
                  strncat(httrack.path_html,argv[na],(int) (a-argv[na]));
                  strcat(httrack.path_log,a+1);
                } else {
                  strcpy(httrack.path_log,argv[na]);
                  strcpy(httrack.path_html,argv[na]);
                }
                // Eliminer les cas comme -O "C:\mirror\"
                if (httrack.path_log[0]=='"') {  // Guillemets
                  char tmp[256];
                  strcpy(tmp,httrack.path_log+1);
                  if (tmp[strlen(tmp)-1]=='"')
                    tmp[strlen(tmp)-1]='\0';
                  strcpy(httrack.path_log,tmp);
                }
                if (httrack.path_html[0]=='"') {
                  char tmp[256];
                  strcpy(tmp,httrack.path_html+1);
                  if (tmp[strlen(tmp)-1]=='"')
                    tmp[strlen(tmp)-1]='\0';
                  strcpy(httrack.path_html,tmp);
                }
                check_path(httrack.path_log,argv_firsturl);
                if (check_path(httrack.path_html,argv_firsturl)) {
                  httrack.dir_topindex=1;     // rebuilt top index
                }
                
                //printf("-->%s\n%s\n",httrack.path_html,httrack.path_log);
                
              }
              break;
            }  // switch
            com++;    
          }  // while
          
        }  // arg
        
      }  // for
     
         /* if doit.log exists, or if new URL(s) defined, 
      then DO NOT load standard config files */
      /* (config files are added in doit.log) */
#if DEBUG_STEPS
      printf("Loading httrackrc/doit.log\n");
#endif
      /* recreate a doit.log (no old doit.log or new URLs (and parameters)) */
      if ((strnotempty(httrack.path_log)) || (strnotempty(httrack.path_html)))
        loops++;      // do not loop once again and do not include rc file (O option exists)
      else {
        if ( (!fexist(fconcat(httrack.path_log,"hts-cache/doit.log"))) || (argv_url>0) ) {
          if (!optinclude_file(fconcat(httrack.path_log,HTS_HTTRACKRC),&argc,argv,x_argvblk,&x_ptr))
            if (!optinclude_file(HTS_HTTRACKRC,&argc,argv,x_argvblk,&x_ptr)) {
              if (!optinclude_file(fconcat(hts_gethome(),"/"HTS_HTTRACKRC),&argc,argv,x_argvblk,&x_ptr)) {
#ifdef HTS_HTTRACKCNF
                optinclude_file(HTS_HTTRACKCNF,&argc,argv,x_argvblk,&x_ptr);
#endif
              }
            }
        } else
          loops++;      // do not loop once again
      }

      loops++;
   } // while

  }  // traiter -O


  
  /* load doit.log and insert in current command line */
  if ( fexist(fconcat(httrack.path_log,"hts-cache/doit.log")) && (argv_url<=0) ) {
    FILE* fp=fopen(fconcat(httrack.path_log,"hts-cache/doit.log"),"rb");
    if (fp) {
      int insert_after=1;     /* insérer après nom au début */
      //
      char buff[8192];
      char *p,*lastp;
      linput(fp,buff,8000);
      fclose(fp); fp=NULL;
      p=buff;
      do {
        int insert_after_argc;
        // read next
        lastp=p;
        if (p) {
          p=next_token(p,1);
          if (p) {
            *p=0;    // null
            p++;
          }
        }

        /* Insert parameters BUT so that they can be in the same order */
        if (lastp) {
          if (strnotempty(lastp)) {
            insert_after_argc=argc-insert_after;
            cmdl_ins(lastp,insert_after_argc,(argv+insert_after),x_argvblk,x_ptr);
            argc=insert_after_argc+insert_after;
            insert_after++;
          }
        }
      } while(lastp!=NULL);
      //fclose(fp);
    }
  }


  // Existence d'un cache - pas de new mais un old.. renommer
#if DEBUG_STEPS
  printf("Checking cache\n");
#endif
  if ( (!fexist(fconcat(httrack.path_log,"hts-cache/new.dat"))) || (!fexist(fconcat(httrack.path_log,"hts-cache/new.ndx"))) ) {
    if ( (fexist(fconcat(httrack.path_log,"hts-cache/old.dat"))) && (fexist(fconcat(httrack.path_log,"hts-cache/old.ndx"))) ) {
      remove(fconcat(httrack.path_log,"hts-cache/new.dat"));
      remove(fconcat(httrack.path_log,"hts-cache/new.ndx"));
      //remove(fconcat(httrack.path_log,"hts-cache/new.lst"));
      rename(fconcat(httrack.path_log,"hts-cache/old.dat"),fconcat(httrack.path_log,"hts-cache/new.dat"));
      rename(fconcat(httrack.path_log,"hts-cache/old.ndx"),fconcat(httrack.path_log,"hts-cache/new.ndx"));
      //rename(fconcat(httrack.path_log,"hts-cache/old.lst"),fconcat(httrack.path_log,"hts-cache/new.lst"));
    }
  }

  /* Interrupted mirror detected */
  if (!httrack.quiet) {
    if (fexist(fconcat(httrack.path_log,"hts-in_progress.lock"))) {
      /* Old cache */
      if ( (fexist(fconcat(httrack.path_log,"hts-cache/old.dat"))) && (fexist(fconcat(httrack.path_log,"hts-cache/old.ndx"))) ) {
        if (httrack.log != NULL) {
          fprintf(httrack.log,"Warning!\n");
          fprintf(httrack.log,"An aborted mirror has been detected!\nThe current temporary cache is required for any update operation and only contains data downloaded during the last aborted session.\nThe former cache might contain more complete information; if you do not want to lose that information, you have to restore it and delete the current cache.\nThis can easily be done here by erasing the hts-cache/new.* files\n");
          fprintf(httrack.log,"Please restart HTTrack with --continue (-iC1) option to override this message!\n");
        }
        exit(0);
      }
    }
  }
    
  // remplacer "macros" comme --spider
  // permet de lancer httrack sans a avoir à se rappeler de syntaxes comme p0C0I0Qc32 ..
#if DEBUG_STEPS
  printf("Checking last macros\n");
#endif
  {
    int i;
    for(i=0;i<argc;i++) {
#if DEBUG_STEPS
      printf("Checking #%d:\n",argv[i]);
      printf("%s\n",argv[i]);
#endif
      if (argv[i][0]=='-') {
        if (argv[i][1]=='-') {  // --xxx
          if ((strfield2(argv[i]+2,"clean")) || (strfield2(argv[i]+2,"tide"))) {  // nettoyer
            strcpy(argv[i]+1,"");
            if (fexist(fconcat(httrack.path_log,"hts-log.txt")))
              remove(fconcat(httrack.path_log,"hts-log.txt"));
            if (fexist(fconcat(httrack.path_log,"hts-err.txt")))
              remove(fconcat(httrack.path_log,"hts-err.txt"));
            if (fexist(fconcat(httrack.path_html,"index.html")))
              remove(fconcat(httrack.path_html,"index.html"));
            if (fexist(fconcat(httrack.path_log,"hts-cache/new.dat")))
              remove(fconcat(httrack.path_log,"hts-cache/new.dat"));
            if (fexist(fconcat(httrack.path_log,"hts-cache/new.ndx")))
              remove(fconcat(httrack.path_log,"hts-cache/new.ndx"));
            if (fexist(fconcat(httrack.path_log,"hts-cache/old.dat")))
              remove(fconcat(httrack.path_log,"hts-cache/old.dat"));
            if (fexist(fconcat(httrack.path_log,"hts-cache/old.ndx")))
              remove(fconcat(httrack.path_log,"hts-cache/old.ndx"));
            if (fexist(fconcat(httrack.path_log,"hts-cache/new.lst")))
              remove(fconcat(httrack.path_log,"hts-cache/new.lst"));
            if (fexist(fconcat(httrack.path_log,"hts-cache/old.lst")))
              remove(fconcat(httrack.path_log,"hts-cache/old.lst"));
            if (fexist(fconcat(httrack.path_log,"hts-cache/new.txt")))
              remove(fconcat(httrack.path_log,"hts-cache/new.txt"));
            if (fexist(fconcat(httrack.path_log,"hts-cache/old.txt")))
              remove(fconcat(httrack.path_log,"hts-cache/old.txt"));
            if (fexist(fconcat(httrack.path_log,"hts-cache/doit.log")))
              remove(fconcat(httrack.path_log,"hts-cache/doit.log"));
            if (fexist(fconcat(httrack.path_log,"hts-in_progress.lock")))
              remove(fconcat(httrack.path_log,"hts-in_progress.lock"));
            rmdir(fconcat(httrack.path_log,"hts-cache"));
            //
          } else if (strfield2(argv[i]+2,"catchurl")) {      // capture d'URL via proxy temporaire!
            argv_url=1;     // forcer a passer les parametres
            strcpy(argv[i]+1,"#P");
            //
          } else if (strfield2(argv[i]+2,"updatehttrack")) {
#ifdef _WIN32
            char s[HTS_CDLMAXSIZE];
            sprintf(s,"%s not available in this version",argv[i]);
            HTS_PANIC_PRINTF(s);
            htsmain_free();
            return -1;
#else
#if 0
            char _args[8][256];
            char *args[8];
            
            printf("Cheking for updates...\n");
            strcpy(_args[0],argv[0]);
            strcpy(_args[1],"--get");
            sprintf(_args[2],HTS_UPDATE_WEBSITE,HTS_PLATFORM,"");
            strcpy(_args[3],"--quickinfo");
            args[0]=_args[0];
            args[1]=_args[1];
            args[2]=_args[2];
            args[3]=_args[3];
            args[4]=NULL;
            if (execvp(args[0],args)==-1) {
            }
#endif
#endif
          }
          //
          else {
            char s[HTS_CDLMAXSIZE];
            sprintf(s,"%s not recognized",argv[i]);
            HTS_PANIC_PRINTF(s);
            htsmain_free();
            return -1;
          }

        } 
      }
    }
  }

  // Compter urls/jokers
  /*
  if (argv_url<=0) { 
    int na;
    argv_url=0;
    for(na=1;na<argc;na++) {
      if ( (strcmp(argv[na],"-P")==0) || (strcmp(argv[na],"-N")==0) || (strcmp(argv[na],"-F")==0) || (strcmp(argv[na],"-O")==0) || (strcmp(argv[na],"-V")==0) ) {
        na++;    // sauter nom de proxy
      } else if (!cmdl_opt(argv[na])) { 
        argv_url++;   // un de plus       
      } else if (strcmp(argv[na],"-h")==0) {
        help(argv[0],!httrack.quiet);
        htsmain_free();
        return 0;
      } else {
        if ((strchr(argv[na],'q')!=NULL))
          httrack.quiet=1;    // ne pas poser de questions! (nohup par exemple)
        if ((strchr(argv[na],'i')!=NULL)) {  // doit.log!
          argv_url=0;
          na=argc;
        }
      }
    }
  }  
  */

  // Ici on ajoute les arguments qui ont été appelés avant au cas où on récupère une session
  // Exemple: httrack www.truc.fr -L0 puis ^C puis httrack sans URL : ajouter URL précédente
  /*
  if (argv_url==0) {
    //if ((fexist(fconcat(httrack.path_log,"hts-cache/new.dat"))) && (fexist(fconcat(httrack.path_log,"hts-cache/new.ndx")))) {  // il existe déja un cache précédent.. renommer
    if (fexist(fconcat(httrack.path_log,"hts-cache/doit.log"))) {    // un cache est présent
      
      x_argvblk=(char*) calloct(32768,1);
      
      if (x_argvblk!=NULL) {
        FILE* fp;
        int x_argc;
        
        //strcpy(x_argvblk,"httrack ");
        fp=fopen(fconcat(httrack.path_log,"hts-cache/doit.log"),"rb");
        if (fp) {
          linput(fp,x_argvblk+strlen(x_argvblk),8192);
          fclose(fp); fp=NULL;
        }
        
        // calculer arguments selon derniers arguments
        x_argv[0]=argv[0];
        x_argc=1;
        {
          char* p=x_argvblk;
          do {
            x_argv[x_argc++]=p;
            //p=strstr(p," ");
            // exemple de chaine: "echo \"test\"" c:\a "\$0"
            p=next_token(p,1);    // prochain token
            if (p) {
              *p=0;    // octet nul (tableau)
              p++;
            }            
          } while(p!=NULL);
        }
        // recopier arguments actuels (pointeurs uniquement)
        {
          int na;
          for(na=1;na<argc;na++) {
            if (strcmp(argv[na],"-O") != 0)    // SAUF le path!
              x_argv[x_argc++]=argv[na];
            else
              na++;
          }
        }
        argc=x_argc;      // nouvel argc
        argv=x_argv;      // nouvel argv
      }
      
      
    }
    //}
  }
  */
  
  // Vérifier quiet
  /*
  { 
    int na;    
    for(na=1;na<argc;na++) {
      if (!cmdl_opt(argv[na])) { 
        if ((strcmp(argv[na],"-P")==0) || (strcmp(argv[na],"-N")==0) || (strcmp(argv[na],"-F")==0) || (strcmp(argv[na],"-O")==0) || (strcmp(argv[na],"-V")==0))
          na++;    // sauter nom de proxy
      } else {
        if ((strchr(argv[na],'q')!=NULL) || (strchr(argv[na],'i')!=NULL))
          httrack.quiet=1;    // ne pas poser de questions! (nohup par exemple)
      }
    }
  }
  */

  // Pas d'URL
#if DEBUG_STEPS
  printf("Checking URLs\n");
#endif
  if (argv_url==0) {
    // Présence d'un cache, que faire?..
    if ((fexist(fconcat(httrack.path_log,"hts-cache/new.dat"))) && (fexist(fconcat(httrack.path_log,"hts-cache/new.ndx")))) {  // il existe déja un cache précédent.. renommer
      if (fexist(fconcat(httrack.path_log,"hts-cache/doit.log"))) {    // un cache est présent
        if (x_argvblk!=NULL) {
          int m;        
          // établir mode - mode cache: 1 (cache valide) 2 (cache à vérifier)
          if (fexist(fconcat(httrack.path_log,"hts-in_progress.lock"))) {    // cache prioritaire
            m=1;
            recuperer=1;
          } else {
            m=2;
          }
          httrack.cache=m;
          
          if (httrack.quiet==0) {  // sinon on continue automatiquement
            HT_REQUEST_START;
            HT_PRINT("A cache (hts-cache/) has been found in the directory ");
            HT_PRINT(httrack.path_log);
            HT_PRINT(LF);
            if (m==1) {
              HT_PRINT("That means that a transfer has been aborted"LF);
              HT_PRINT("OK to Continue ");
            } else {
              HT_PRINT("That means you can update faster the remote site(s)"LF);
              HT_PRINT("OK to Update ");
            }
            HT_PRINT("httrack "); HT_PRINT(x_argvblk); HT_PRINT("?"LF);
            HT_REQUEST_END;
            if (!ask_continue()) { 
              htsmain_free();
              return 0;
            }
          }
          
        } else {
          HTS_PANIC_PRINTF("Error, not enough memory");
          htsmain_free();
          return -1;
        }
      } else { // log existe pas
        HTS_PANIC_PRINTF("A cache has been found, but no command line");
        printf("Please launch httrack with proper parameters to reuse the cache\n");
        htsmain_free();
        return -1;
      }
      
    } else {    // aucune URL définie et pas de cache
#if HTS_ANALYSTE!=2
      if (httrack.quiet) {
#endif
        help(argv[0],!httrack.quiet);
        htsmain_free();
        return -1;
#if HTS_ANALYSTE!=2
      } else {
        help_wizard(&httrack);
        htsmain_free();
        return -1;
      }
#endif
      htsmain_free();
      return 0;
    }
  } else {   // plus de 2 paramètres
    // un fichier log existe?
    if (fexist(fconcat(httrack.path_log,"hts-in_progress.lock"))) {  // fichier lock?
      //char s[32];
      
      httrack.cache=1;    // cache prioritaire
      if (httrack.quiet==0) {
        if ((fexist(fconcat(httrack.path_log,"hts-cache/new.dat"))) && (fexist(fconcat(httrack.path_log,"hts-cache/new.ndx")))) {  // il existe déja un cache précédent.. renommer
          HT_REQUEST_START;
          HT_PRINT("There is a lock-file in the directory ");
          HT_PRINT(httrack.path_log);
          HT_PRINT(LF"That means that a mirror has not been terminated"LF);
          HT_PRINT("Be sure you call httrack with proper parameters"LF);
          HT_PRINT("(The cache allows you to restart faster the transfer)"LF);
          HT_REQUEST_END;
          if (!ask_continue()) {
            htsmain_free();
            return 0;
          }
        }
      }
    } else if (fexist(fconcat(httrack.path_html,"index.html"))) {
      //char s[32];
      httrack.cache=2;  // cache vient après test de validité
      if (httrack.quiet==0) {
        if ((fexist(fconcat(httrack.path_log,"hts-cache/new.dat"))) && (fexist(fconcat(httrack.path_log,"hts-cache/new.ndx")))) {  // il existe déja un cache précédent.. renommer
          HT_REQUEST_START;
          HT_PRINT("There is an index.html and a hts-cache folder in the directory ");
          HT_PRINT(httrack.path_log);
          HT_PRINT(LF"A site may have been mirrored here, that could mean that you want to update it"LF);
          HT_PRINT("Be sure parameters are ok"LF);
          HT_REQUEST_END;
          if (!ask_continue()) {
            htsmain_free();
            return 0;
          }
        } else {
          HT_REQUEST_START;
          HT_PRINT("There is an index.html in the directory ");
          HT_PRINT(httrack.path_log);
          HT_PRINT(" but no cache"LF);
          HT_PRINT("There is an index.html in the directory, but no cache"LF);
          HT_PRINT("A site may have been mirrored here, and erased.."LF);
          HT_PRINT("Be sure parameters are ok"LF);
          HT_REQUEST_END;
          if (!ask_continue()) {
            htsmain_free();
            return 0;
          }
        }
      }
    }
  }
  
  
  // Treat parameters
  // Traiter les paramètres
#if DEBUG_STEPS
  printf("Analyze parameters\n");
#endif
  { 
    char* com;
    int na;
    
    for(na=1;na<argc;na++) {

      if (argv[na][0]=='"') {
        char tempo[HTS_CDLMAXSIZE];
        strcpy(tempo,argv[na]+1);
        if (tempo[strlen(tempo)-1]!='"') {
          char s[HTS_CDLMAXSIZE];
          sprintf(s,"Missing quote in %s",argv[na]);
          HTS_PANIC_PRINTF(s);
          htsmain_free();
          return -1;
        }
        tempo[strlen(tempo)-1]='\0';
        strcpy(argv[na],tempo);
      }

      if (cmdl_opt(argv[na])) { // option
        com=argv[na]+1;
        
        while(*com) {
          switch(*com) {
          case ' ': case 9: case '-': case '\0': break;
            //
          case 'h': 
            help(argv[0],0); 
            htsmain_free();
            return 0;   // déja fait normalement
            //
          case 'g':    // récupérer un (ou plusieurs) fichiers isolés
            httrack.wizard=2;             // le wizard on peut plus s'en passer..
            //httrack.wizard=0;             // pas de wizard
            httrack.cache=0;              // ni de cache
            httrack.makeindex=0;          // ni d'index
            httrack_logmode=1;            // erreurs à l'écran
            httrack.savename_type=1003;   // mettre dans le répertoire courant
            httrack.depth=0;              // ne pas explorer la page
            httrack.accept_cookie=0;      // pas de cookies
            break;
          case 'w': httrack.wizard=2;    // wizard 'soft' (ne pose pas de questions)
            httrack.travel=0;
            httrack.seeker=1;
            break;
          case 'W': httrack.wizard=1;    // Wizard-Help (pose des questions)
            httrack.travel=0;
            httrack.seeker=1;
            break;
          case 'r':                      // n'est plus le recurse get bestial mais wizard itou!
            if (isdigit((unsigned char)*(com+1))) {
              sscanf(com+1,"%d",&httrack.depth);
              while(isdigit((unsigned char)*(com+1))) com++;
            } else httrack.depth=3;
            break;
/*
          case 'r': httrack.wizard=0;
            if (isdigit((unsigned char)*(com+1))) {
              sscanf(com+1,"%d",&httrack.depth);
              while(isdigit((unsigned char)*(com+1))) com++;
            } else httrack.depth=3;
            break;
*/
            //
            // note: les tests httrack.depth sont pour éviter de faire
            // un miroir du web (:-O) accidentelement ;-)
          case 'a': /*if (httrack.depth==9999) httrack.depth=3;*/
            httrack.travel=0+(httrack.travel&256); break;
          case 'd': /*if (httrack.depth==9999) httrack.depth=3;*/
            httrack.travel=1+(httrack.travel&256); break;
          case 'l': /*if (httrack.depth==9999) httrack.depth=3;*/
            httrack.travel=2+(httrack.travel&256); break;
          case 'e': /*if (httrack.depth==9999) httrack.depth=3;*/
            httrack.travel=7+(httrack.travel&256); break;
          case 't': httrack.travel|=256; break;
          case 'n': httrack.nearlink=1; break;
          case 'x': httrack.external=1; break;
            //
          case 'U': httrack.seeker=2; break;
          case 'D': httrack.seeker=1; break;
          case 'S': httrack.seeker=0; break;
          case 'B': httrack.seeker=3; break;
            //
          case 'Y': httrack.mirror_first_page=1; break;
            //
          case 'q': case 'i': httrack.quiet=1; break;
            //
          case 'Q': httrack_logmode=0; break;
          case 'v': httrack_logmode=1; break;
          case 'f': httrack_logmode=2; if (*(com+1)=='2') httrack_logmode=3; while(isdigit((unsigned char)*(com+1))) com++; break;
            //
          //case 'A': httrack.urlmode=1; break;
          //case 'R': httrack.urlmode=2; break;
          case 'K': httrack.urlmode=0; 
            if (isdigit((unsigned char)*(com+1))) {
              sscanf(com+1,"%d",&httrack.urlmode);
              if (httrack.urlmode == 0) {  // in fact K0 ==> K2
                                           // and K ==> K0
                httrack.urlmode=2;
              }
              while(isdigit((unsigned char)*(com+1))) com++; 
            }
            //if (*(com+1)=='0') { httrack.urlmode=2; com++; } break;
            //
          case 'c':
            if (isdigit((unsigned char)*(com+1))) {
              sscanf(com+1,"%d",&httrack.maxsoc);
              while(isdigit((unsigned char)*(com+1))) com++;
              httrack.maxsoc=max(httrack.maxsoc,1);     // FORCER A 1
            } else httrack.maxsoc=8;
            
            break;
            //
          case 'p': sscanf(com+1,"%d",&httrack.getmode); while(isdigit((unsigned char)*(com+1))) com++; break;
            //        
          case 'G': sscanf(com+1,LLintP,&httrack.fragment); while(isdigit((unsigned char)*(com+1))) com++; break;
          case 'M': sscanf(com+1,LLintP,&httrack.maxsite); while(isdigit((unsigned char)*(com+1))) com++; break;
          case 'm': sscanf(com+1,LLintP,&httrack.maxfile_nonhtml); while(isdigit((unsigned char)*(com+1))) com++; 
            if (*(com+1)==',') {
              com++;
              sscanf(com+1,LLintP,&httrack.maxfile_html); while(isdigit((unsigned char)*(com+1))) com++;
            } else httrack.maxfile_html=-1;
            break;
            //
          case 'T': sscanf(com+1,"%d",&httrack.timeout); while(isdigit((unsigned char)*(com+1))) com++; break;
          case 'J': sscanf(com+1,"%d",&httrack.rateout); while(isdigit((unsigned char)*(com+1))) com++; break;
          case 'R': sscanf(com+1,"%d",&httrack.retry); while(isdigit((unsigned char)*(com+1))) com++; break;
          case 'E': sscanf(com+1,"%d",&httrack.maxtime); while(isdigit((unsigned char)*(com+1))) com++; break;
          case 'H': sscanf(com+1,"%d",&httrack.hostcontrol); while(isdigit((unsigned char)*(com+1))) com++; break;
          case 'A': sscanf(com+1,"%d",&httrack.maxrate); while(isdigit((unsigned char)*(com+1))) com++; break;

          case 'j': httrack.parsejava=1; if (*(com+1)=='0') { httrack.parsejava=0; com++; } break;
            //
          case 'I': httrack.makeindex=1; if (*(com+1)=='0') { httrack.makeindex=0; com++; } break;
            //
          case 'X': httrack.delete_old=1; if (*(com+1)=='0') { httrack.delete_old=0; com++; } break;
            //
          case 'b': sscanf(com+1,"%d",&httrack.accept_cookie); while(isdigit((unsigned char)*(com+1))) com++; break;
            //
          case 'N':
            if (strcmp(argv[na],"-N")==0) {    // Tout seul
              if ((na+1>=argc) || (argv[na+1][0]=='-')) {  // erreur
                HTS_PANIC_PRINTF("Option N needs a number, or needs to be followed by a blank space, and a string");
                printf("Example: -N4\n");
                htsmain_free();
                return -1;
              } else {
                na++;
                if (strlen(argv[na])>=127) {
                  HTS_PANIC_PRINTF("Userdef structure string too long");
                  htsmain_free();
                  return -1;
                }
                strcpy(httrack.savename_userdef,argv[na]);
                if (strnotempty(httrack.savename_userdef))
                  httrack.savename_type = -1;    // userdef!
                else
                  httrack.savename_type = 0;    // -N "" : par défaut
              }
            } else {
              sscanf(com+1,"%d",&httrack.savename_type); while(isdigit((unsigned char)*(com+1))) com++;
            }
            break;
          case 'L': 
            {
              sscanf(com+1,"%d",&httrack.savename_83); 
              switch(httrack.savename_83) {
              case 0:
                httrack.savename_83=1;
                break;
              case 1:
                httrack.savename_83=0;
                break;
              default:
                httrack.savename_83=2;
                break;
              }
              while(isdigit((unsigned char)*(com+1))) com++; 
            }
            break;
          case 's': 
            if (isdigit((unsigned char)*(com+1))) {
              sscanf(com+1,"%d",&httrack.robots);
              while(isdigit((unsigned char)*(com+1))) com++;
            } else httrack.robots=1;
#if DEBUG_ROBOTS
            printf("robots.txt mode set to %d\n",httrack.robots);
#endif
            break;
          case 'o': sscanf(com+1,"%d",&httrack.errpage); while(isdigit((unsigned char)*(com+1))) com++; break;
          case 'u': sscanf(com+1,"%d",&httrack.check_type); while(isdigit((unsigned char)*(com+1))) com++; break;
            //
          case 'C': 
            if (isdigit((unsigned char)*(com+1))) {
              sscanf(com+1,"%d",&httrack.cache);
              while(isdigit((unsigned char)*(com+1))) com++;
            } else httrack.cache=1;
            break;
          case 'k': httrack.all_in_cache=1; break;
            //
          case 'z': httrack.debug=1; break;  // petit debug
          case 'Z': httrack.debug=2; break;  // GROS debug
            //
          case '&': case '%': {    // deuxième jeu d'options
            com++;
            switch(*com) {
            case 'x': httrack.passprivacy=1; if (*(com+1)=='0') { httrack.passprivacy=0; com++; } break;   // No passwords in html files
            case 'q': httrack.includequery=1; if (*(com+1)=='0') { httrack.includequery=0; com++; } break;   // No passwords in html files
            case 'I': httrack.kindex=1; if (isdigit((unsigned char)*(com+1))) { sscanf(com+1,"%d",&httrack.kindex); while(isdigit((unsigned char)*(com+1))) com++; }
              break;    // Keyword Index
            case 'c': sscanf(com+1,"%d",&httrack.maxconn); while(isdigit((unsigned char)*(com+1))) com++; break;
            case 'e': sscanf(com+1,"%d",&httrack.extdepth); while(isdigit((unsigned char)*(com+1))) com++; break;
            case 'B': httrack.tolerant=1; if (*(com+1)=='0') { httrack.tolerant=0; com++; } break;   // HTTP/1.0 notamment
            case 'h': httrack.http10=1; if (*(com+1)=='0') { httrack.http10=0; com++; } break;   // HTTP/1.0
            case 'z': httrack.nocompression=1; if (*(com+1)=='0') { httrack.nocompression=0; com++; } break;   // pas de compression
            case 'f': httrack.ftp_proxy=1; if (*(com+1)=='0') { httrack.ftp_proxy=0; com++; } break;   // proxy http pour ftp
            case 'P': httrack.parseall=1; if (*(com+1)=='0') { httrack.parseall=0; com++; } break;   // tout parser
            case 'n': httrack.norecatch=1; if (*(com+1)=='0') { httrack.norecatch=0; com++; } break;   // ne pas reprendre fichiers effacés localement
            case 's': httrack.sizehack=1; if (*(com+1)=='0') { httrack.sizehack=0; com++; } break;   // hack sur content-length
            case 'v': httrack.verbosedisplay=2; if (isdigit((unsigned char)*(com+1))) { sscanf(com+1,"%d",&httrack.verbosedisplay); while(isdigit((unsigned char)*(com+1))) com++; } break;

            // preserve: no footer, original links
            case 'p':
              httrack.footer[0]='\0';
              httrack.urlmode=4;
              break;
            case 'L':    // URL list
              if ((na+1>=argc) || (argv[na+1][0]=='-')) {
                HTS_PANIC_PRINTF("Option %L needs to be followed by a blank space, and a text filename");
                printf("Example: -%%L \"mylist.txt\"\n");
                htsmain_free();
                return -1;
              } else{
                na++;
                if (strlen(argv[na])>=254) {
                  HTS_PANIC_PRINTF("File list string too long");
                  htsmain_free();
                  return -1;
                }
                strcpy(httrack.filelist,argv[na]);
              }
              break;
            case 'A':    // assume
              if ((na+1>=argc) || (argv[na+1][0]=='-')) {
                HTS_PANIC_PRINTF("Option %A needs to be followed by a blank space, and a filesystemtype=mimetype/mimesubtype parameters");
                printf("Example: -%%A php3=text/html,asp=text/html\n");
                htsmain_free();
                return -1;
              } else{
                char* a;
                na++;
                if ( (strlen(argv[na]) + strlen(httrack.mimedefs) + 4) >= sizeof(httrack.mimedefs)) {
                  HTS_PANIC_PRINTF("Mime definition string too long");
                  htsmain_free();
                  return -1;
                }
                // --assume standard
                if (strcmp(argv[na],"standard") == 0) {
                  strcpy(httrack.mimedefs,"\n");
                  strcat(httrack.mimedefs,HTS_ASSUME_STANDARD);
                  strcat(httrack.mimedefs,"\n");
                } else {
                  strcat(httrack.mimedefs,argv[na]);
                  strcat(httrack.mimedefs,"\n");
                }
                a=httrack.mimedefs;
                while(*a) {
                  switch(*a) {
                  case ',': case ' ': case '\r': case ';': case '\t':
                    *a='\n';
                    break;
                  }
                  a++;
                }
              }
              break;
              //
            case 'l': 
              if ((na+1>=argc) || (argv[na+1][0]=='-')) {
                HTS_PANIC_PRINTF("Option %l needs to be followed by a blank space, and an ISO language code");
                printf("Example: -%%l \"en\"\n");
                htsmain_free();
                return -1;
              } else{
                na++;
                if (strlen(argv[na])>=62) {
                  HTS_PANIC_PRINTF("Lang list string too long");
                  htsmain_free();
                  return -1;
                }
                strcpy(httrack.lang_iso,argv[na]);
              }
              break;
              //
            case 'F':     // footer id
              if ((na+1>=argc) || (argv[na+1][0]=='-')) {
                HTS_PANIC_PRINTF("Option %F needs to be followed by a blank space, and a footer string");
                printf("Example: -%%F \"<!-- Mirrored from %%s by HTTrack Website Copier/"HTTRACK_AFF_VERSION" "HTTRACK_AFF_AUTHORS", %%s -->\"\n");
                htsmain_free();
                return -1;
              } else{
                na++;
                if (strlen(argv[na])>=254) {
                  HTS_PANIC_PRINTF("Footer string too long");
                  htsmain_free();
                  return -1;
                }
                strcpy(httrack.footer,argv[na]);
              }
              break;
            case 'H':                 // debug headers
              _DEBUG_HEAD=1;
              break;
            case 'O':
#if HTS_WIN
              printf("Warning option -%%O has no effect in this system (chroot)\n");
#else
              switch_chroot=1;
#endif
              break;
            case 'U':                 // setuid
              if ((na+1>=argc) || (argv[na+1][0]=='-')) {
                HTS_PANIC_PRINTF("Option %U needs to be followed by a blank space, and a username");
                printf("Example: -%%U smith\n");
                htsmain_free();
                return -1;
              } else {
                na++;
#if HTS_WIN
                printf("Warning option -%%U has no effect on this system (setuid)\n");
#else
#ifndef HTS_DO_NOT_USE_UID
                /* Change the user id and gid */
                {
                  struct passwd* userdef=getpwnam((const char*)argv[na]);
                  if (userdef) {    /* we'll have to switch the user id */
                    switch_gid=userdef->pw_gid;
                    switch_uid=userdef->pw_uid;
                  }
                }
#else
                printf("Warning option -%%U has no effect with this compiled version (setuid)\n");
#endif
#endif
              }
              break;
              
            default: {
              char s[HTS_CDLMAXSIZE];
              sprintf(s,"invalid option %%%c\n",*com);
              HTS_PANIC_PRINTF(s);
              htsmain_free();
              return -1;
                     }
              break;
              
            }
                    }
            break;
            //
          case '@': {    // troisième jeu d'options
            com++;
            switch(*com) {
            case 'i': 
#if HTS_INET6==0
              printf("Warning, option @i has no effect (v6 routines not compiled)\n");
#else 
              {
                int res=0;
                if (isdigit((unsigned char)*(com+1))) {
                  sscanf(com+1,"%d",&res); while(isdigit((unsigned char)*(com+1))) com++; 
                }
                switch(res) {
                case 1:
                case 4:
                  IPV6_resolver=1;
                  break;
                case 2:
                case 6:
                  IPV6_resolver=2;
                  break;
                case 0:
                  IPV6_resolver=0;
                  break;
                default:
                  printf("Unknown flag @i%d\n", res);
                  htsmain_free();
                  return -1;
                  break;
                }
              }
#endif
              break;
              
                default: {
                  char s[HTS_CDLMAXSIZE];
                  sprintf(s,"invalid option %%%c\n",*com);
                  HTS_PANIC_PRINTF(s);
                  htsmain_free();
                  return -1;
                         }
                  break;
                  
                  //case 's': httrack.sslengine=1; if (isdigit((unsigned char)*(com+1))) { sscanf(com+1,"%d",&httrack.sslengine); while(isdigit((unsigned char)*(com+1))) com++; } break;
            }
                    }
            break;

            //
          case '#':  { // non documenté (appel de l'interface)
            com++;
            switch(*com) {
            case 'f': httrack.flush=1; break;
            case 'h':
              printf("HTTrack version "HTTRACK_VERSION"\n");
              exit(1);
              break;
            case 'p': httrack.aff_progress=1; break;
            case 'S': httrack.shell=1; break;  // stdin sur un shell
            case 'K': httrack.keyboard=1; break;  // vérifier stdin
              //
            case 'L': sscanf(com+1,"%d",&httrack.maxlink); while(isdigit((unsigned char)*(com+1))) com++; break;
            case 'F': sscanf(com+1,"%d",&httrack.maxfilter); while(isdigit((unsigned char)*(com+1))) com++; break;
            case 'Z': httrack.makestat=1; break;
            case 'T': httrack.maketrack=1; break;
            case 'u': sscanf(com+1,"%d",&httrack.waittime); while(isdigit((unsigned char)*(com+1))) com++; break;

            case 'R':    // ohh ftp, catch->ftpget
              HTS_PANIC_PRINTF("Unexpected internal error with -#R command");
              htsmain_free();
              return -1;        
              break;
            case 'P': {     // catchurl
              help_catchurl(httrack.path_log);
              htsmain_free();
              return 0;
                      }
              break;
          
            case '0':   /* test #0 : filters */
              if (na+2>=argc) {
                HTS_PANIC_PRINTF("Option #0 needs to be followed by a filter string and a string");
                printf("Example: '-#0' '*.gif' 'foo.gif'\n");
                htsmain_free();
                return -1;
              } else {
                if (strjoker(argv[na+2],argv[na+1],NULL,NULL))
                  printf("%s does match %s\n",argv[na+2],argv[na+1]);
                else
                  printf("%s does NOT match %s\n",argv[na+2],argv[na+1]);
                htsmain_free();
                return 0;
              }
              break;
            case '!':
              if (na+1>=argc) {
                HTS_PANIC_PRINTF("Option #! needs to be followed by a commandline");
                printf("Example: '-#!' 'echo hello'\n");
                htsmain_free();
                return -1;
              } else {
                system(argv[na+1]);
              }
              break;

            default: printf("Internal option %c not recognized\n",*com); break;
            }
                     }
            break; 
          case 'O':    // output path
            na++;     // sauter, déja traité
            break;
          case 'P':    // proxy
            if ((na+1>=argc) || (argv[na+1][0]=='-')) {
              HTS_PANIC_PRINTF("Option P needs to be followed by a blank space, and a proxy proxy:port or user:id@proxy:port");
              printf("Example: -P proxy.myhost.com:8080\n");
              htsmain_free();
              return -1;
            } else {
              char* a;
              na++;
              httrack.proxy.active=1;
              // Rechercher MAIS en partant de la fin à cause de user:pass@proxy:port
              a = argv[na] + strlen(argv[na]) -1;
              // a=strstr(argv[na],":");  // port
              while( (a > argv[na]) && (*a != ':') && (*a != '@') ) a--;
              if (*a == ':') {  // un port est présent, <proxy>:port
                sscanf(a+1,"%d",&httrack.proxy.port);
                httrack.proxy.name[0]='\0';
                strncat(httrack.proxy.name,argv[na],(int) (a - argv[na]));
              } else {  // <proxy>
                httrack.proxy.port=8080;
                strcpy(httrack.proxy.name,argv[na]);
              }
            }
            break;
          case 'F':    // user-agent field
            if ((na+1>=argc) || (argv[na+1][0]=='-')) {
              HTS_PANIC_PRINTF("Option F needs to be followed by a blank space, and a user-agent name");
              printf("Example: -F \"my_user_agent/1.0\"\n");
              htsmain_free();
              return -1;
            } else{
              na++;
              if (strlen(argv[na])>=126) {
                HTS_PANIC_PRINTF("User-agent length too long");
                htsmain_free();
                return -1;
              }
              strcpy(httrack.user_agent,argv[na]);
              if (strnotempty(httrack.user_agent))
                httrack.user_agent_send=1;
              else
                httrack.user_agent_send=0;    // -F "" désactive l'option
            }
            break;
            //
          case 'V':    // execute command
            if ((na+1>=argc) || (argv[na+1][0]=='-')) {
              HTS_PANIC_PRINTF("Option V needs to be followed by a system-command string");
              printf("Example: -V \"tar uvf some.tar \\$0\"\n");
              htsmain_free();
              return -1;
            } else{
              na++;
              if (strlen(argv[na])>=2048) {
                HTS_PANIC_PRINTF("System-command length too long");
                htsmain_free();
                return -1;
              }
              strcpy(httrack.sys_com,argv[na]);
              if (strnotempty(httrack.sys_com))
                httrack.sys_com_exec=1;
              else
                httrack.sys_com_exec=0;    // -V "" désactive l'option
            }
            break;
            //
          default: {
            char s[HTS_CDLMAXSIZE];
            sprintf(s,"invalid option %c\n",*com);
            HTS_PANIC_PRINTF(s);
            htsmain_free();
            return -1;
                   }
            break;
          }  // switch
          com++;    
        }  // while
        
      }  else {  // URL/filters
        char tempo[1024];       
        if (strnotempty(url)) strcat(url," ");  // espace de séparation
        strcpy(tempo,unescape_http_unharm(argv[na],1));
        escape_spc_url(tempo);
        strcat(url,tempo);
      }  // if argv=- etc. 
      
    }  // for
  }
  
#if BDEBUG==3  
  printf("URLs/filters=%s\n",url);
#endif

#if DEBUG_STEPS
  printf("Analyzing parameters done\n");
#endif


#if HTS_WIN
#else
#ifndef HTS_DO_NOT_USE_UID
  /* Chroot - xxc */
  if (switch_chroot) {
    uid_t userid=getuid();
    //struct passwd* userdef=getpwuid(userid);
    //if (userdef) {
    if (!userid) {
      //if (strcmp(userdef->pw_name,"root")==0) {
      char rpath[1024];
      //printf("html=%s log=%s\n",httrack.path_html,httrack.path_log);    // xxc
      if ((httrack.path_html[0]) && (httrack.path_log[0])) {
        char *a=httrack.path_html,*b=httrack.path_log,*c=NULL,*d=NULL;
        c=a; d=b;
        while ((*a) && (*a == *b)) {
          if (*a=='/') { c=a; d=b; }
          a++;
          b++;
        }

        rpath[0]='\0';
        if (c != httrack.path_html) {
          if (httrack.path_html[0]!='/')
            strcat(rpath,"./");
          strncat(rpath,httrack.path_html,(int) (c - httrack.path_html));
        }
        {
          char tmp[1024];
          strcpy(tmp,c); strcpy(httrack.path_html,tmp);
          strcpy(tmp,d); strcpy(httrack.path_log,tmp);
        }
      } else {
        strcpy(rpath,"./");
        strcpy(httrack.path_html,"/");
        strcpy(httrack.path_log,"/");
      }
      if (rpath[0]) {
        printf("[changing root path to %s (path_data=%s,path_log=%s)]\n",rpath,httrack.path_html,httrack.path_log);
        if (chroot(rpath)) {
          printf("ERROR! Can not chroot to %s!\n",rpath);
          exit(0);
        }
        if (chdir("/")) {     /* new root */
          printf("ERROR! Can not chdir to %s!\n",rpath);
          exit(0);
        }
      } else
        printf("WARNING: chroot not possible with these paths\n");
    }
    //}
  }

  /* Setuid */
  if (switch_uid>=0) {
    printf("[setting user/group to %d/%d]\n",switch_uid,switch_gid);
    if (setgid(switch_gid))
      printf("WARNING! Can not setgid to %d!\n",switch_gid);
    if (setuid(switch_uid))
      printf("WARNING! Can not setuid to %d!\n",switch_uid);
  }

  /* Final check */
  {
    uid_t userid=getuid();
    if (!userid) {              /* running as r00t */
      printf("WARNING! You are running this program as root!\n");
      printf("It might be a good idea to use the -%%U option to change the userid:\n");
      printf("Example: -%%U smith\n\n");
    }
  }
#endif
#endif
  
  //printf("WARNING! This is *only* a beta-release of HTTrack\n");
  io_flush;
  
#if DEBUG_STEPS
  printf("Cache & log settings\n");
#endif
  
  // on utilise le cache..
  // en cas de présence des deux versions, garder la version la plus avancée,
  // cad la version contenant le plus de fichiers  
  if (httrack.cache) {
    if (fexist(fconcat(httrack.path_log,"hts-in_progress.lock"))) {   // problemes..
      if (fexist(fconcat(httrack.path_log,"hts-cache/new.dat")) && fexist(fconcat(httrack.path_log,"hts-cache/new.ndx"))) { 
        if (fexist(fconcat(httrack.path_log,"hts-cache/old.dat")) && fexist(fconcat(httrack.path_log,"hts-cache/old.ndx"))) {
          // switcher si new<32Ko et old>65Ko (tailles arbitraires) ?
          // ce cas est peut être une erreur ou un crash d'un miroir ancien, prendre
          // alors l'ancien cache
          if (fsize(fconcat(httrack.path_log,"hts-cache/new.dat"))<32768) {
            if (fsize(fconcat(httrack.path_log,"hts-cache/old.dat"))>65536) {
              if (fsize(fconcat(httrack.path_log,"hts-cache/old.dat")) > fsize(fconcat(httrack.path_log,"hts-cache/new.dat"))) {
                remove(fconcat(httrack.path_log,"hts-cache/new.dat"));
                remove(fconcat(httrack.path_log,"hts-cache/new.ndx"));
                rename(fconcat(httrack.path_log,"hts-cache/old.dat"),fconcat(httrack.path_log,"hts-cache/new.dat"));
                rename(fconcat(httrack.path_log,"hts-cache/old.ndx"),fconcat(httrack.path_log,"hts-cache/new.ndx"));  
                //} else {  // ne rien faire
                //  remove("hts-cache/old.dat");
                //  remove("hts-cache/old.ndx");
              }
            }
          }
        }
      }
    }
  }

  // Débuggage des en têtes
  if (_DEBUG_HEAD) {
    ioinfo=fopen(fconcat(httrack.path_log,"hts-ioinfo.txt"),"wb");
  }
  
  {
    char n_lock[256];
    // on peut pas avoir un affichage ET un fichier log
    // ca sera pour la version 2
    if (httrack_logmode==1) {
      httrack.log=stdout;
      httrack.errlog=stderr;
    } else if (httrack_logmode>=2) {
      // deux fichiers log
      structcheck(httrack.path_log);
      if (fexist(fconcat(httrack.path_log,"hts-log.txt")))
        remove(fconcat(httrack.path_log,"hts-log.txt"));
      if (fexist(fconcat(httrack.path_log,"hts-err.txt")))
        remove(fconcat(httrack.path_log,"hts-err.txt"));

      httrack.log=fopen(fconcat(httrack.path_log,"hts-log.txt"),"w");
      if (httrack_logmode==2)
        httrack.errlog=fopen(fconcat(httrack.path_log,"hts-err.txt"),"w");
      else
        httrack.errlog=httrack.log;
      if (httrack.log==NULL) {
        char s[HTS_CDLMAXSIZE];
        sprintf(s,"Unable to create log file %s",fconcat(httrack.path_log,"hts-log.txt"));
        HTS_PANIC_PRINTF(s);
        htsmain_free();
        return -1;
      } else if (httrack.errlog==NULL) {
        char s[HTS_CDLMAXSIZE];
        sprintf(s,"Unable to create log file %s",fconcat(httrack.path_log,"hts-err.txt"));
        HTS_PANIC_PRINTF(s);
        htsmain_free();
        return -1;
      }

    } else {
      httrack.log=NULL;
      httrack.errlog=NULL;
    }
    
    // un petit lock-file pour indiquer un miroir en cours, ainsi qu'un éventuel fichier log
    {
      FILE* fp=NULL;
      //int n=0;
      char t[256];
      time_local_rfc822(t);    // faut bien que ca serve quelque part l'heure RFC1945 arf'
      
      /* readme for information purpose */
      {
        FILE* fp=fopen(fconcat(httrack.path_log,"hts-cache/readme.txt"),"wb");
        if (fp) {
          fprintf(fp,"What's in this folder?"LF);
          fprintf(fp,""LF);
          fprintf(fp,"This folder (hts-cache) has been generated by WinHTTrack "HTTRACK_VERSION""LF);
          fprintf(fp,"and is used for updating this website."LF);
          fprintf(fp,"(The HTML website structure is stored here to allow fast updates)"LF""LF);
          fprintf(fp,"DO NOT delete this folder unless you do not want to update the mirror in the future!!"LF);
          fprintf(fp,"(you can safely delete old.dat, old.ndx and old.lst files, however)"LF);
          fprintf(fp,""LF);
          fprintf(fp,HTS_LOG_SECURITY_WARNING);
          fclose(fp);
        }
      }

      sprintf(n_lock,fconcat(httrack.path_log,"hts-in_progress.lock"));
      //sprintf(n_lock,fconcat(httrack.path_log,"hts-in_progress.lock"),n);
      /*do {
        if (!n)
          sprintf(n_lock,fconcat(httrack.path_log,"hts-in_progress.lock"),n);
        else
          sprintf(n_lock,fconcat(httrack.path_log,"hts-in_progress%d.lock"),n);
        n++;
      } while((fexist(n_lock)) && httrack.quiet);      
      if (fexist(n_lock)) {
        if (!recuperer) {
          remove(n_lock);
        }
      }*/

      // vérifier existence de la structure
      structcheck(httrack.path_html);
      structcheck(httrack.path_log);
     
      // reprise/update
      if (httrack.cache) {
        FILE* fp;
        int i;
#if HTS_WIN
        mkdir(fconcat(httrack.path_log,"hts-cache"));
#else
        mkdir(fconcat(httrack.path_log,"hts-cache"),HTS_PROTECT_FOLDER);
#endif
        fp=fopen(fconcat(httrack.path_log,"hts-cache/doit.log"),"wb");
        if (fp) {
          for(i=0+1;i<argc;i++) {
            if ( ((strchr(argv[i],' ')!=NULL) || (strchr(argv[i],'"')!=NULL) || (strchr(argv[i],'\\')!=NULL)) && (argv[i][0]!='"')  ) {
              int j;
              fprintf(fp,"\"");
              for(j=0;j<(int) strlen(argv[i]);j++) {
                if (argv[i][j]==34)
                  fprintf(fp,"\\\"");
                else if (argv[i][j]=='\\')
                  fprintf(fp,"\\\\");
                else
                  fprintf(fp,"%c",argv[i][j]);
              }
              fprintf(fp,"\"");
            } else if (strnotempty(argv[i])==0) {   // ""
              fprintf(fp,"\"\"");
            } else {   // non critique
              fprintf(fp,"%s",argv[i]);
            }
            if (i<argc-1)
              fprintf(fp," ");
          }
          fprintf(fp,LF);
          fprintf(fp,"File generated automatically on %s, do NOT edit"LF,t);
          fprintf(fp,LF);
          fprintf(fp,"To update a mirror, just launch httrack without any parameters"LF);
          fprintf(fp,"The existing cache will be used (and modified)"LF);
          fprintf(fp,"To have other options, retype all parameters and launch HTTrack"LF);
          fprintf(fp,"To continue an interrupted mirror, just launch httrack without any parameters"LF);
          fprintf(fp,LF);
          fclose(fp); fp=NULL;
        //} else if (httrack.debug>1) {
        //  printf("! FileOpen error, \"%s\"\n",strerror(errno));
        }
      }
      
      // petit message dans le lock
      if ( (fp=fopen(n_lock,"wb"))!=NULL) {
        int i;
        fprintf(fp,"Mirror in progress since %s .. please wait!"LF,t);
        for(i=0;i<argc;i++) {
          if (strchr(argv[i],' ')==NULL)
            fprintf(fp,"%s ",argv[i]);
          else    // entre ""
            fprintf(fp,"\"%s\" ",argv[i]);
        }
        fprintf(fp,LF);
        fprintf(fp, "To pause the engine: create an empty file named 'hts-stop.lock'"LF);
        fclose(fp); fp=NULL;
      }
      
      // fichier log        
      if (httrack.log)     {
        int i;
        fprintf(httrack.log,"HTTrack"HTTRACK_VERSION" launched on %s at %s"LF,t,url);
        fprintf(httrack.log,"(");
        for(i=0;i<argc;i++) {
          if ((strchr(argv[i],' ')==NULL) || (strchr(argv[i],'\"')))
            fprintf(httrack.log,"%s ",argv[i]);
          else    // entre "" (si espace(s) et pas déja de ")
            fprintf(httrack.log,"\"%s\" ",argv[i]);
        }
        fprintf(httrack.log,")"LF);
        fprintf(httrack.log,LF);
        fprintf(httrack.log,"Information, Warnings and Errors reported for this mirror:"LF);
        fprintf(httrack.log,HTS_LOG_SECURITY_WARNING );
        fprintf(httrack.log,LF);
      }

      if (httrack_logmode)     {
        printf("Mirror launched on %s by HTTrack Website Copier/"HTTRACK_VERSION" "HTTRACK_AFF_AUTHORS""LF,t);
        if (httrack.wizard==0) {
          printf("mirroring %s with %d levels, %d sockets,t=%d,s=%d,logm=%d,lnk=%d,mdg=%d\n",url,httrack.depth,httrack.maxsoc,httrack.travel,httrack.seeker,httrack_logmode,httrack.urlmode,httrack.getmode);
        } else {    // the magic wizard
          printf("mirroring %s with the wizard help..\n",url);
        }
      }
    }
    
    io_flush;

  /* Info for wrappers */
  if ( (httrack.debug>0) && (httrack.log!=NULL) ) {
    fspc(httrack.log,"info"); fprintf(httrack.log,"engine: init"LF);
  }
#if HTS_ANALYSTE
  hts_htmlcheck_init();
#endif

  // détourner SIGHUP etc.
#if HTS_WIN
  signal( SIGINT  , sig_ask    );   // ^C
  signal( SIGTERM , sig_finish );   // kill <process>
#else
  signal( SIGHUP  , sig_back   );   // close window
  signal( SIGTSTP , sig_back   );   // ^Z
  signal( SIGTERM , sig_finish );   // kill <process>
  signal( SIGINT  , sig_ask    );   // ^C
  signal( SIGPIPE , sig_brpipe );   // broken pipe (write into non-opened socket)
/*
deprecated - see SIGCHLD
#ifndef HTS_DO_NOT_SIGCLD
  signal( SIGCLD  , sig_ignore );   // child change status
#endif
*/
  signal( SIGCHLD , sig_ignore );   // child change status
#endif
#if DEBUG_STEPS
  printf("Launching the mirror\n");
#endif
  

    // Lancement du miroir
    // ------------------------------------------------------------
    if (httpmirror(url, &httrack)==0) {
      printf("Error during operation (see log file), site has not been successfully mirrored\n");
    } else {
      if  (httrack.shell) {
        HTT_REQUEST_START;
        HT_PRINT("TRANSFER DONE"LF);
        HTT_REQUEST_END
      } else {
        printf("Done.\n");
      }
    }
    // ------------------------------------------------------------

    //
    // Build top index
    if (httrack.dir_topindex) {
      char rpath[1024*2];
      char* a;
      strcpy(rpath,httrack.path_html);
      if (rpath[0]) {
        if (rpath[strlen(rpath)-1]=='/')
          rpath[strlen(rpath)-1]='\0';
      }
      a=strrchr(rpath,'/');
      if (a) {
        *a='\0';
        hts_buildtopindex(rpath,httrack.path_bin);
        if (httrack.log) {
          fspc(httrack.log,"info"); fprintf(httrack.log,"Top index rebuilt (done)"LF);
        }
      }
    }

    if (exit_xh ==1) {
      if (httrack.log) {
        fprintf(httrack.log,"* * MIRROR ABORTED! * *\nThe current temporary cache is required for any update operation and only contains data downloaded during the present aborted session.\nThe former cache might contain more complete information; if you do not want to lose that information, you have to restore it and delete the current cache.\nThis can easily be done here by erasing the hts-cache/new.* files]\n");
      }
    }

  /* Info for wrappers */
  if ( (httrack.debug>0) && (httrack.log!=NULL) ) {
    fspc(httrack.log,"info"); fprintf(httrack.log,"engine: free"LF);
  }
#if HTS_ANALYSTE
  hts_htmlcheck_uninit();
#endif
    
    if (httrack_logmode!=1) {
      if (httrack.errlog == httrack.log) httrack.errlog=NULL;
      if (httrack.log) { fclose(httrack.log); httrack.log=NULL; }
      if (httrack.errlog) { fclose(httrack.errlog); httrack.errlog=NULL; }
    }  
    
    // Débuggage des en têtes
    if (_DEBUG_HEAD) {
      if (ioinfo) {
        fclose(ioinfo);
      }
    }
    
    // supprimer lock
    remove(n_lock);
  }
  
  if (x_argvblk)
    freet(x_argvblk);
 
#if HTS_WIN
#if HTS_ANALYSTE!=2
//  WSACleanup();    // ** non en cas de thread tjs présent!..
#endif
#endif
#if HTS_TRACE_MALLOC
  hts_freeall();
#endif

  printf("Thanks for using HTTrack!\n");
  io_flush;
  htsmain_free();
  return 0;    // OK
}


// main() subroutines

// vérifier chemin path
int check_path(char* s,char* defaultname) {
  int i;
  int return_value=0;

  // Replace name: ~/mywebsites/# -> /home/foo/mywebsites/#
  expand_home(s);
  for(i=0;i<(int) strlen(s);i++)    // conversion \ -> /
    if (s[i]=='\\')
      s[i]='/';
  
  // remove ending /
  if (strnotempty(s))
  if (s[strlen(s)-1]=='/')
    s[strlen(s)-1]='\0';

   // Replace name: /home/foo/mywebsites/# -> /home/foo/mywebsites/wonderfulsite
  if (strnotempty(s)) {
    if (s[(i=strlen(s))-1]=='#') {
      if (strnotempty((defaultname?defaultname:""))) {
        char tempo[HTS_URLMAXSIZE*2];
        char* a=strchr(defaultname,'#');      // we never know..
        if (a) *a='\0';
        tempo[0]='\0';
        strncat(tempo,s,i-1);
        strcat(tempo,defaultname);
        strcpy(s,tempo);
      } else
        s[0]='\0';            // Clear path (no name/default url given)
      return_value=1;     // expanded
    }
  }

  // ending /
  if (strnotempty(s))
  if (s[strlen(s)-1]!='/')    // ajouter slash à la fin
    strcat(s,"/");

  return return_value;
}

// détermine si l'argument est une option
int cmdl_opt(char* s) {
  if (s[0]=='-') {  // c'est peut être une option
    if (strchr(s,'.')!=NULL)
      return 0;    // sans doute un -www.truc.fr (note: -www n'est pas compris)
    else if (strchr(s,'/')!=NULL)
      return 0;    // idem, -*cgi-bin/
    else if (strchr(s,'*')!=NULL)
      return 0;    // joker, idem
    else
      return 1;
  } else return 0;
}

