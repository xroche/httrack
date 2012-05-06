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
/* File: opt->c subroutines:                                 */
/*       main routine (first called)                            */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htscoremain.h"

#include "htsglobal.h"
#include "htscore.h"
#include "htsdefines.h"
#include "htsalias.h"
#include "htswrap.h"
#include "htsmodules.h"
#include "htszlib.h"
#include "htscharset.h"

#include <ctype.h>
#if USE_BEGINTHREAD
#ifdef _WIN32
#include <process.h>
#endif
#endif
#ifdef _WIN32
#else
#ifndef HTS_DO_NOT_USE_UID
/* setuid */
#include <pwd.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif
#endif

/* Resolver */
extern int IPV6_resolver;


// Add a command in the argc/argv
#define cmdl_add(token,argc,argv,buff,ptr) \
  argv[argc]=(buff+ptr); \
  strcpybuff(argv[argc],token); \
  ptr += (int) (strlen(argv[argc])+2); \
  argc++

// Insert a command in the argc/argv
#define cmdl_ins(token,argc,argv,buff,ptr) \
  { \
  int i; \
  for(i=argc;i>0;i--)\
  argv[i]=argv[i-1];\
  } \
  argv[0]=(buff+ptr); \
  strcpybuff(argv[0],token); \
  ptr += (int) (strlen(argv[0])+2); \
  argc++

#define htsmain_free() do { if (url != NULL) { free(url); } } while(0)

#define ensureUrlCapacity(url, urlsize, size) do { \
  if (urlsize < size || url == NULL) { \
    urlsize = size; \
    if (url == NULL) { \
      url = (char*) malloct(urlsize); \
      if (url != NULL) url[0]='\0'; \
    } else { \
      url = (char*) realloct(url, urlsize); \
    } \
    if (url == NULL) { \
      HTS_PANIC_PRINTF("* memory exhausted"); \
      htsmain_free(); \
      return -1; \
    } \
  } \
} while(0)

HTSEXT_API int hts_main(int argc, char **argv)
{
  httrackp *opt = hts_create_opt();
	int ret = hts_main2(argc, argv, opt);
	hts_free_opt(opt);
	return ret;
}

// Main, récupère les paramètres et appelle le robot
HTSEXT_API int hts_main2(int argc, char **argv, httrackp *opt) {
  char** x_argv=NULL;     // Patch pour argv et argc: en cas de récupération de ligne de commande
  char* x_argvblk=NULL;   // (reprise ou update)
  int   x_ptr=0;          // offset
  //
  int argv_url=-1;           // ==0 : utiliser cache et doit.log
  char* argv_firsturl=NULL;  // utilisé pour nommage par défaut
  char* url = NULL;          // URLS séparées par un espace
  int   url_sz = 65535;
  //char url[65536];         // URLS séparées par un espace
  // the parametres
  int httrack_logmode=3;   // ONE log file
  int recuperer=0;         // récupérer un plantage (n'arrive jamais, à supprimer)
#ifndef _WIN32
#ifndef HTS_DO_NOT_USE_UID
  int switch_uid=-1,switch_gid=-1;      /* setuid/setgid */
#endif
  int switch_chroot=0;                  /* chroot ? */
#endif
  //
  ensureUrlCapacity(url, url_sz, 65536);

	// Create options
	_DEBUG_HEAD=0;            // pas de debuggage en têtes

  /* command max-size check (3.43 ; 3.42-4) */
  {
    int i;
    for(i = 0 ; i < argc ; i++) {
      if (strlen(argv[i]) >= HTS_CDLMAXSIZE) {
        HTS_PANIC_PRINTF("argument too long");
        htsmain_free();
        return -1;
      }
    }
  }

  /* Init root dir */
  hts_rootdir(argv[0]);

#ifdef _WIN32
#else
  /* Terminal is a tty, may ask questions and display funny information */
  if (isatty(1)) {
    opt->quiet=0;
    opt->verbosedisplay=1;
  }
  /* Not a tty, no stdin input or funny output! */
  else {
    opt->quiet=1;
    opt->verbosedisplay=0;
  }
#endif

  // Binary program path?
#ifndef HTS_HTTRACKDIR
  {
  	char catbuff[CATBUFF_SIZE];
    char* path=fslash(catbuff,argv[0]);
    char* a;
    if ((a=strrchr(path,'/'))) {
      StringCopyN(opt->path_bin,argv[0],a - path);
    }
  }
#else
  StringCopy(opt->path_bin, HTS_HTTRACKDIR);
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
        strcpybuff(argv[na],"\"\"");
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
      current_size += (int) (strlen(argv[na]) + 1);
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

    /* Create argv */
    x_argv = (char**) malloct(sizeof(char*) * ( argc + 1024 ));
  }

  /* Create new argc/argv, replace alias, count URLs, treat -h, -q, -i */
  {
    char BIGSTK _tmp_argv[2][HTS_CDLMAXSIZE];
    char BIGSTK tmp_error[HTS_CDLMAXSIZE];
    char* tmp_argv[2];
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
              help(argv[0],!opt->quiet);
              htsmain_free();
              return 0;
            } else if (strcmp(tmp_argv[0],"-#h")==0) {
              printf("HTTrack version "HTTRACK_VERSION"%s\n", hts_get_version_info(opt));
              return 0;
            } else {
              if (strncmp(tmp_argv[0],"--",2)) {   /* pas */
                if ((strchr(tmp_argv[0],'q')!=NULL))
                  opt->quiet=1;    // ne pas poser de questions! (nohup par exemple)
                if ((strchr(tmp_argv[0],'i')!=NULL)) {  // doit.log!
                  argv_url=-1;        /* forcer */
                  opt->quiet=1;
                }
              } else if (strcmp(tmp_argv[0] + 2,"quiet") == 0) {
                opt->quiet=1;    // ne pas poser de questions! (nohup par exemple)
              } else if (strcmp(tmp_argv[0] + 2,"continue") == 0) {
                argv_url=-1;        /* forcer */
                opt->quiet=1;
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

  // Option O and includerc
  { 
    int loops=0;
    while (loops<2) {
      char* com;
      int na;
      
      for(na=1;na<argc;na++) {
        
        if (argv[na][0]=='"') {
          char BIGSTK tempo[HTS_CDLMAXSIZE];
          strcpybuff(tempo,argv[na]+1);
          if (tempo[strlen(tempo)-1]!='"') {
            char BIGSTK s[HTS_CDLMAXSIZE];
            sprintf(s,"Missing quote in %s",argv[na]);
            HTS_PANIC_PRINTF(s);
            htsmain_free();
            return -1;
          }
          tempo[strlen(tempo)-1]='\0';
          strcpybuff(argv[na],tempo);
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
								int i, j;
								int inQuote;
								String *path;
								int noDbl = 0;
								if (com[1] == '1') {			/* only 1 arg */
									com++;
									noDbl = 1;
								}
                na++;
								StringClear(opt->path_html);
                StringClear(opt->path_log);
								for(i = 0, j = 0, inQuote = 0, path = &opt->path_html ; argv[na][i] != 0 ; i++) {
									if (argv[na][i] == '"') {
										if (inQuote)
											inQuote = 0;
										else
											inQuote = 1;
									} else if (!inQuote && !noDbl && argv[na][i] == ',') {
                    //StringAddchar(path, '\0');
										j = 0;
										path = &opt->path_log;
									} else {
                    StringAddchar(*path, argv[na][i]);
										//path[j++] = argv[na][i];
									}
								}
								//path[j++] = '\0';
								if (StringLength(opt->path_log) == 0) {
									StringCopyS(opt->path_log, opt->path_html);
								}

                check_path(&opt->path_log, argv_firsturl);
                if (check_path(&opt->path_html, argv_firsturl)) {
                  opt->dir_topindex=1;     // rebuilt top index
                }
                
                //printf("-->%s\n%s\n",StringBuff(opt->path_html),StringBuff(opt->path_log));                
              }
              break;
            }  // switch
            com++;    
          }  // while
          
        }  // arg
        
      }  // for
     
      // Convert path to UTF-8
#ifdef _WIN32
      {
        char *const path = hts_convertStringSystemToUTF8(StringBuff(opt->path_html), (int) StringLength(opt->path_html));
        if (path != NULL) {
          StringCopy(opt->path_html_utf8, path);
          free(path);
        } else {
          StringCopyN(opt->path_html_utf8, StringBuff(opt->path_html), StringLength(opt->path_html));
        }
      }
#else
      // Assume UTF-8 filesystem.
      StringCopyN(opt->path_html_utf8, StringBuff(opt->path_html), StringLength(opt->path_html));
#endif

         /* if doit.log exists, or if new URL(s) defined, 
      then DO NOT load standard config files */
      /* (config files are added in doit.log) */
#if DEBUG_STEPS
      printf("Loading httrackrc/doit.log\n");
#endif
      /* recreate a doit.log (no old doit.log or new URLs (and parameters)) */
      if ((strnotempty(StringBuff(opt->path_log))) || (strnotempty(StringBuff(opt->path_html))))
        loops++;      // do not loop once again and do not include rc file (O option exists)
      else {
        if ( (!fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/doit.log"))) || (argv_url>0) ) {
          if (!optinclude_file(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),HTS_HTTRACKRC),&argc,argv,x_argvblk,&x_ptr))
            if (!optinclude_file(HTS_HTTRACKRC,&argc,argv,x_argvblk,&x_ptr)) {
              if (!optinclude_file(fconcat(OPT_GET_BUFF(opt), hts_gethome(),"/"HTS_HTTRACKRC),&argc,argv,x_argvblk,&x_ptr)) {
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
  if ( fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/doit.log")) && (argv_url<=0) ) {
    FILE* fp=fopen(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/doit.log"),"rb");
    if (fp) {
      int insert_after=1;     /* insérer après nom au début */
      //
      char BIGSTK buff[8192];
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
  if (!fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.zip"))) {
    if ( fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.zip")) ) {
      rename(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.zip"),fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.zip"));
    }
  } else if ( (!fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.dat"))) || (!fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.ndx"))) ) {
    if ( (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.dat"))) && (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.ndx"))) ) {
      remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.dat"));
      remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.ndx"));
      //remove(fconcat(StringBuff(opt->path_log),"hts-cache/new.lst"));
      rename(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.dat"),fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.dat"));
      rename(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.ndx"),fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.ndx"));
      //rename(fconcat(StringBuff(opt->path_log),"hts-cache/old.lst"),fconcat(StringBuff(opt->path_log),"hts-cache/new.lst"));
    }
  }

  /* Interrupted mirror detected */
  if (!opt->quiet) {
    if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-in_progress.lock"))) {
      /* Old cache */
      if ( (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.dat"))) && (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.ndx"))) ) {
        if (opt->log != NULL) {
          fprintf(opt->log,"Warning!\n");
          fprintf(opt->log,"An aborted mirror has been detected!\nThe current temporary cache is required for any update operation and only contains data downloaded during the last aborted session.\nThe former cache might contain more complete information; if you do not want to lose that information, you have to restore it and delete the current cache.\nThis can easily be done here by erasing the hts-cache/new.* files\n");
          fprintf(opt->log,"Please restart HTTrack with --continue (-iC1) option to override this message!\n");
        }
        return 0;
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
            strcpybuff(argv[i]+1,"");
            if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-log.txt")))
              remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-log.txt"));
            if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-err.txt")))
              remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-err.txt"));
            if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_html),"index.html")))
              remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_html),"index.html"));
            /* */
            if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.zip")))
              remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.zip"));
            if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.zip")))
              remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.zip"));
            if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.dat")))
              remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.dat"));
            if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.ndx")))
              remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.ndx"));
            if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.dat")))
              remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.dat"));
            if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.ndx")))
              remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.ndx"));
            if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.lst")))
              remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.lst"));
            if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.lst")))
              remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.lst"));
            if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.txt")))
              remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.txt"));
            if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.txt")))
              remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.txt"));
            if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/doit.log")))
              remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/doit.log"));
            if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-in_progress.lock")))
              remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-in_progress.lock"));
            rmdir(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache"));
            //
          } else if (strfield2(argv[i]+2,"catchurl")) {      // capture d'URL via proxy temporaire!
            argv_url=1;     // forcer a passer les parametres
            strcpybuff(argv[i]+1,"#P");
            //
          } else if (strfield2(argv[i]+2,"updatehttrack")) {
#ifdef _WIN32
            char s[HTS_CDLMAXSIZE + 256];
            sprintf(s,"%s not available in this version",argv[i]);
            HTS_PANIC_PRINTF(s);
            htsmain_free();
            return -1;
#else
#if 0
            char _args[8][256];
            char *args[8];
            
            printf("Cheking for updates...\n");
            strcpybuff(_args[0],argv[0]);
            strcpybuff(_args[1],"--get");
            sprintf(_args[2],HTS_UPDATE_WEBSITE,0,"");
            strcpybuff(_args[3],"--quickinfo");
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
            char s[HTS_CDLMAXSIZE + 256];
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
        help(argv[0],!opt->quiet);
        htsmain_free();
        return 0;
      } else {
        if ((strchr(argv[na],'q')!=NULL))
          opt->quiet=1;    // ne pas poser de questions! (nohup par exemple)
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
    //if ((fexist(fconcat(StringBuff(opt->path_log),"hts-cache/new.dat"))) && (fexist(fconcat(StringBuff(opt->path_log),"hts-cache/new.ndx")))) {  // il existe déja un cache précédent.. renommer
    if (fexist(fconcat(StringBuff(opt->path_log),"hts-cache/doit.log"))) {    // un cache est présent
      
      x_argvblk=(char*) calloct(32768,1);
      
      if (x_argvblk!=NULL) {
        FILE* fp;
        int x_argc;
        
        //strcpybuff(x_argvblk,"httrack ");
        fp=fopen(fconcat(StringBuff(opt->path_log),"hts-cache/doit.log"),"rb");
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
          opt->quiet=1;    // ne pas poser de questions! (nohup par exemple)
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
    if (
      ( fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.zip")) )
      ||
      ( fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.dat")) && fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.ndx")) )
      ) {  // il existe déja un cache précédent.. renommer
      if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/doit.log"))) {    // un cache est présent
        if (x_argvblk!=NULL) {
          int m;        
          // établir mode - mode cache: 1 (cache valide) 2 (cache à vérifier)
          if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-in_progress.lock"))) {    // cache prioritaire
            m=1;
            recuperer=1;
          } else {
            m=2;
          }
          opt->cache=m;
          
          if (opt->quiet==0) {  // sinon on continue automatiquement
            HT_REQUEST_START;
            HT_PRINT("A cache (hts-cache/) has been found in the directory ");
            HT_PRINT(StringBuff(opt->path_log));
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
            if (!ask_continue(opt)) { 
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
      if (opt->quiet) {
        help(argv[0],!opt->quiet);
        htsmain_free();
        return -1;
      } else {
        help_wizard(opt);
        htsmain_free();
        return -1;
      }
      htsmain_free();
      return 0;
    }
  } else {   // plus de 2 paramètres
    // un fichier log existe?
    if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-in_progress.lock"))) {  // fichier lock?
      //char s[32];
      
      opt->cache=1;    // cache prioritaire
      if (opt->quiet==0) {
        if (
          ( fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.zip")) )
          ||
          ( fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.dat")) && fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.ndx")) )
          ) {
          HT_REQUEST_START;
          HT_PRINT("There is a lock-file in the directory ");
          HT_PRINT(StringBuff(opt->path_log));
          HT_PRINT(LF"That means that a mirror has not been terminated"LF);
          HT_PRINT("Be sure you call httrack with proper parameters"LF);
          HT_PRINT("(The cache allows you to restart faster the transfer)"LF);
          HT_REQUEST_END;
          if (!ask_continue(opt)) {
            htsmain_free();
            return 0;
          }
        }
      }
    } else if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_html),"index.html"))) {
      //char s[32];
      opt->cache=2;  // cache vient après test de validité
      if (opt->quiet==0) {
        if (
          ( fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.zip")) )
          ||
          ( fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.dat")) && fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.ndx")) )
          ) {
          HT_REQUEST_START;
          HT_PRINT("There is an index.html and a hts-cache folder in the directory ");
          HT_PRINT(StringBuff(opt->path_log));
          HT_PRINT(LF"A site may have been mirrored here, that could mean that you want to update it"LF);
          HT_PRINT("Be sure parameters are ok"LF);
          HT_REQUEST_END;
          if (!ask_continue(opt)) {
            htsmain_free();
            return 0;
          }
        } else {
          HT_REQUEST_START;
          HT_PRINT("There is an index.html in the directory ");
          HT_PRINT(StringBuff(opt->path_log));
          HT_PRINT(" but no cache"LF);
          HT_PRINT("There is an index.html in the directory, but no cache"LF);
          HT_PRINT("A site may have been mirrored here, and erased.."LF);
          HT_PRINT("Be sure parameters are ok"LF);
          HT_REQUEST_END;
          if (!ask_continue(opt)) {
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
        char BIGSTK tempo[HTS_CDLMAXSIZE + 256];
        strcpybuff(tempo,argv[na]+1);
        if (tempo[strlen(tempo)-1]!='"') {
          char s[HTS_CDLMAXSIZE + 256];
          sprintf(s,"Missing quote in %s",argv[na]);
          HTS_PANIC_PRINTF(s);
          htsmain_free();
          return -1;
        }
        tempo[strlen(tempo)-1]='\0';
        strcpybuff(argv[na],tempo);
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
            opt->wizard=2;             // le wizard on peut plus s'en passer..
            //opt->wizard=0;             // pas de wizard
            opt->cache=0;              // ni de cache
            opt->makeindex=0;          // ni d'index
            httrack_logmode=1;            // erreurs à l'écran
            opt->savename_type=1003;   // mettre dans le répertoire courant
            opt->depth=0;              // ne pas explorer la page
            opt->accept_cookie=0;      // pas de cookies
            opt->robots=0;             // pas de robots
            break;
          case 'w': opt->wizard=2;    // wizard 'soft' (ne pose pas de questions)
            opt->travel=0;
            opt->seeker=1;
            break;
          case 'W': opt->wizard=1;    // Wizard-Help (pose des questions)
            opt->travel=0;
            opt->seeker=1;
            break;
          case 'r':                      // n'est plus le recurse get bestial mais wizard itou!
            if (isdigit((unsigned char)*(com+1))) {
              sscanf(com+1,"%d",&opt->depth);
              while(isdigit((unsigned char)*(com+1))) com++;
            } else opt->depth=3;
            break;
/*
          case 'r': opt->wizard=0;
            if (isdigit((unsigned char)*(com+1))) {
              sscanf(com+1,"%d",&opt->depth);
              while(isdigit((unsigned char)*(com+1))) com++;
            } else opt->depth=3;
            break;
*/
            //
            // note: les tests opt->depth sont pour éviter de faire
            // un miroir du web (:-O) accidentelement ;-)
          case 'a': /*if (opt->depth==9999) opt->depth=3;*/
            opt->travel=0+(opt->travel&256); break;
          case 'd': /*if (opt->depth==9999) opt->depth=3;*/
            opt->travel=1+(opt->travel&256); break;
          case 'l': /*if (opt->depth==9999) opt->depth=3;*/
            opt->travel=2+(opt->travel&256); break;
          case 'e': /*if (opt->depth==9999) opt->depth=3;*/
            opt->travel=7+(opt->travel&256); break;
          case 't': opt->travel|=256; break;
          case 'n': opt->nearlink=1; break;
          case 'x': opt->external=1; break;
            //
          case 'U': opt->seeker=2; break;
          case 'D': opt->seeker=1; break;
          case 'S': opt->seeker=0; break;
          case 'B': opt->seeker=3; break;
            //
          case 'Y': opt->mirror_first_page=1; break;
            //
          case 'q': case 'i': opt->quiet=1; break;
            //
          case 'Q': httrack_logmode=0; break;
          case 'v': httrack_logmode=1; break;
          case 'f': httrack_logmode=2; if (*(com+1)=='2') httrack_logmode=3; while(isdigit((unsigned char)*(com+1))) com++; break;
            //
          //case 'A': opt->urlmode=1; break;
          //case 'R': opt->urlmode=2; break;
          case 'K': opt->urlmode=0; 
            if (isdigit((unsigned char)*(com+1))) {
              sscanf(com+1,"%d",&opt->urlmode);
              if (opt->urlmode == 0) {  // in fact K0 ==> K2
                                           // and K ==> K0
                opt->urlmode=2;
              }
              while(isdigit((unsigned char)*(com+1))) com++; 
            }
            //if (*(com+1)=='0') { opt->urlmode=2; com++; } break;
            //
          case 'c':
            if (isdigit((unsigned char)*(com+1))) {
              sscanf(com+1,"%d",&opt->maxsoc);
              while(isdigit((unsigned char)*(com+1))) com++;
              opt->maxsoc=max(opt->maxsoc,1);     // FORCER A 1
            } else opt->maxsoc=4;
            
            break;
            //
          case 'p': sscanf(com+1,"%d",&opt->getmode); while(isdigit((unsigned char)*(com+1))) com++; break;
            //        
          case 'G': sscanf(com+1,LLintP,&opt->fragment); while(isdigit((unsigned char)*(com+1))) com++; break;
          case 'M': sscanf(com+1,LLintP,&opt->maxsite); while(isdigit((unsigned char)*(com+1))) com++; break;
          case 'm': sscanf(com+1,LLintP,&opt->maxfile_nonhtml); while(isdigit((unsigned char)*(com+1))) com++; 
            if (*(com+1)==',') {
              com++;
              sscanf(com+1,LLintP,&opt->maxfile_html); while(isdigit((unsigned char)*(com+1))) com++;
            } else opt->maxfile_html=-1;
            break;
            //
          case 'T': sscanf(com+1,"%d",&opt->timeout); while(isdigit((unsigned char)*(com+1))) com++; break;
          case 'J': sscanf(com+1,"%d",&opt->rateout); while(isdigit((unsigned char)*(com+1))) com++; break;
          case 'R': sscanf(com+1,"%d",&opt->retry); while(isdigit((unsigned char)*(com+1))) com++; break;
          case 'E': sscanf(com+1,"%d",&opt->maxtime); while(isdigit((unsigned char)*(com+1))) com++; break;
          case 'H': sscanf(com+1,"%d",&opt->hostcontrol); while(isdigit((unsigned char)*(com+1))) com++; break;
          case 'A': sscanf(com+1,"%d",&opt->maxrate); while(isdigit((unsigned char)*(com+1))) com++; break;

          case 'j':
            opt->parsejava = HTSPARSE_DEFAULT;
            if (isdigit((unsigned char)*(com+1))) {
              sscanf(com+1,"%d",&opt->parsejava);
              while(isdigit((unsigned char)*(com+1))) com++; 
            }
            break;
            //
          case 'I': opt->makeindex=1; if (*(com+1)=='0') { opt->makeindex=0; com++; } break;
            //
          case 'X': opt->delete_old=1; if (*(com+1)=='0') { opt->delete_old=0; com++; } break;
            //
          case 'y': opt->background_on_suspend=1; if (*(com+1)=='0') { opt->background_on_suspend=0; com++; } break;
            //
          case 'b': sscanf(com+1,"%d",&opt->accept_cookie); while(isdigit((unsigned char)*(com+1))) com++; break;
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
                StringCopy(opt->savename_userdef, argv[na]);
                if (StringLength(opt->savename_userdef) > 0)
                  opt->savename_type = -1;    // userdef!
                else
                  opt->savename_type = 0;    // -N "" : par défaut
              }
            } else {
              sscanf(com+1,"%d",&opt->savename_type); while(isdigit((unsigned char)*(com+1))) com++;
            }
            break;
          case 'L': 
            {
              sscanf(com+1,"%d",&opt->savename_83); 
              switch(opt->savename_83) {
              case 0:    // 8-3 (ISO9660 L1)
                opt->savename_83=1;
                break;
              case 1:
                opt->savename_83=0;
                break;
              default:    // 2 == ISO9660 (ISO9660 L2)
                opt->savename_83=2;
                break;
              }
              while(isdigit((unsigned char)*(com+1))) com++; 
            }
            break;
          case 's': 
            if (isdigit((unsigned char)*(com+1))) {
              sscanf(com+1,"%d",&opt->robots);
              while(isdigit((unsigned char)*(com+1))) com++;
            } else opt->robots=1;
#if DEBUG_ROBOTS
            printf("robots.txt mode set to %d\n",opt->robots);
#endif
            break;
          case 'o': sscanf(com+1,"%d",&opt->errpage); while(isdigit((unsigned char)*(com+1))) com++; break;
          case 'u': sscanf(com+1,"%d",&opt->check_type); while(isdigit((unsigned char)*(com+1))) com++; break;
            //
          case 'C': 
            if (isdigit((unsigned char)*(com+1))) {
              sscanf(com+1,"%d",&opt->cache);
              while(isdigit((unsigned char)*(com+1))) com++;
            } else opt->cache=1;
            break;
          case 'k': opt->all_in_cache=1; break;
            //
          case 'z': opt->debug=1; break;  // petit debug
          case 'Z': opt->debug=2; break;  // GROS debug
            //
          case '&': case '%': {    // deuxième jeu d'options
            com++;
            switch(*com) {
            case 'M': opt->mimehtml = 1; if (*(com+1)=='0') { opt->mimehtml=0; com++; } break;
            case 'k': opt->nokeepalive = 0; if (*(com+1)=='0') { opt->nokeepalive = 1; com++; } break;
            case 'x': opt->passprivacy=1; if (*(com+1)=='0') { opt->passprivacy=0; com++; } break;   // No passwords in html files
            case 'q': opt->includequery=1; if (*(com+1)=='0') { opt->includequery=0; com++; } break;   // No passwords in html files
            case 'I': opt->kindex=1; if (isdigit((unsigned char)*(com+1))) { sscanf(com+1,"%d",&opt->kindex); while(isdigit((unsigned char)*(com+1))) com++; }
              break;    // Keyword Index
            case 'c': sscanf(com+1,"%f",&opt->maxconn); while(isdigit((unsigned char)*(com+1)) || *(com+1) == '.') com++; break;
            case 'e': sscanf(com+1,"%d",&opt->extdepth); while(isdigit((unsigned char)*(com+1))) com++; break;
            case 'B': opt->tolerant=1; if (*(com+1)=='0') { opt->tolerant=0; com++; } break;   // HTTP/1.0 notamment
            case 'h': opt->http10=1; if (*(com+1)=='0') { opt->http10=0; com++; } break;   // HTTP/1.0
            case 'z': opt->nocompression=1; if (*(com+1)=='0') { opt->nocompression=0; com++; } break;   // pas de compression
            case 'f': opt->ftp_proxy=1; if (*(com+1)=='0') { opt->ftp_proxy=0; com++; } break;   // proxy http pour ftp
            case 'P': opt->parseall=1; if (*(com+1)=='0') { opt->parseall=0; com++; } break;   // tout parser
            case 'n': opt->norecatch=1; if (*(com+1)=='0') { opt->norecatch=0; com++; } break;   // ne pas reprendre fichiers effacés localement
            case 's': opt->sizehack=1; if (*(com+1)=='0') { opt->sizehack=0; com++; } break;   // hack sur content-length
            case 'u': opt->urlhack=1; if (*(com+1)=='0') { opt->urlhack=0; com++; } break;   // url hack
            case 'v': opt->verbosedisplay=2; if (isdigit((unsigned char)*(com+1))) { sscanf(com+1,"%d",&opt->verbosedisplay); while(isdigit((unsigned char)*(com+1))) com++; } break;
            case 'i': opt->dir_topindex = 1; if (*(com+1)=='0') { opt->dir_topindex=0; com++; } break;
            case 'N': opt->savename_delayed = 2; if (isdigit((unsigned char)*(com+1))) { sscanf(com+1,"%d",&opt->savename_delayed); while(isdigit((unsigned char)*(com+1))) com++; } break;
            case 'D': opt->delayed_cached=1; if (*(com+1)=='0') { opt->delayed_cached=0; com++; } break;   // url hack
            case 'T': opt->convert_utf8=1; if (*(com+1)=='0') { opt->convert_utf8=0; com++; } break;   // convert to utf-8
            case '!': opt->bypass_limits = 1; if (*(com+1)=='0') { opt->bypass_limits=0; com++; } break;
#if HTS_USEMMS
						case 'm': sscanf(com+1,"%d",&opt->mms_maxtime); while(isdigit((unsigned char)*(com+1))) com++; break;
#endif
            case 'w':   // disable specific plugin
              if ((na+1>=argc) || (argv[na+1][0]=='-')) {
                HTS_PANIC_PRINTF("Option %w needs to be followed by a blank space, and a module name");
                printf("Example: -%%w htsswf\n");
                htsmain_free();
                return -1;
              } else{
                na++;
                StringCat(opt->mod_blacklist, argv[na]);
                StringCat(opt->mod_blacklist, "\n");
              }
              break;

            // preserve: no footer, original links
            case 'p':
              StringClear(opt->footer);
              opt->urlmode=4;
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
                StringCopy(opt->filelist,argv[na]);
              }
              break;
            case 'b':  // bind
              if ((na+1>=argc) || (argv[na+1][0]=='-')) {
                HTS_PANIC_PRINTF("Option %b needs to be followed by a blank space, and a local hostname");
                printf("Example: -%%b \"ip4.localhost\"\n");
                htsmain_free();
                return -1;
              } else{
                na++;
                if (strlen(argv[na])>=254) {
                  HTS_PANIC_PRINTF("Hostname string too long");
                  htsmain_free();
                  return -1;
                }
                StringCopy(opt->proxy.bindhost, argv[na]);
              }
              break;
            case 'S':    // Scan Rules list
              if ((na+1>=argc) || (argv[na+1][0]=='-')) {
                HTS_PANIC_PRINTF("Option %S needs to be followed by a blank space, and a text filename");
                printf("Example: -%%S \"myfilterlist.txt\"\n");
                htsmain_free();
                return -1;
              } else{
                off_t fz;
                na++;
                fz = fsize(argv[na]);
                if (fz < 0) {
                  HTS_PANIC_PRINTF("File url list could not be opened");
                  htsmain_free();
                  return -1;
                } else {
                  FILE* fp = fopen(argv[na], "rb");
                  if (fp != NULL) {
                    int cl = (int) strlen(url);
                    ensureUrlCapacity(url, url_sz, cl + fz + 8192);
                    if (cl > 0) {  /* don't stick! (3.43) */
                      url[cl] = ' ';
                      cl++;
                    }
                    if (fread(url + cl, 1, fz, fp) != fz) {
                      HTS_PANIC_PRINTF("File url list could not be read");
                      htsmain_free();
                      return -1;
                    }
                    fclose(fp);
                    *(url + cl + fz) = '\0';
                  }
                }
              }
              break;
            case 'A':    // assume
              if ((na+1>=argc) || (argv[na+1][0]=='-')) {
                HTS_PANIC_PRINTF("Option %A needs to be followed by a blank space, and a filesystemtype=mimetype/mimesubtype parameters");
                printf("Example: -%%A php3=text/html,asp=text/html\n");
                htsmain_free();
                return -1;
              } else{
                na++;
                // --assume standard
                if (strcmp(argv[na], "standard") == 0) {
                  StringCopy(opt->mimedefs,"\n");
                  StringCat(opt->mimedefs,HTS_ASSUME_STANDARD);
                  StringCat(opt->mimedefs,"\n");
                } else {
                  char* a;
                  //char* b = StringBuff(opt->mimedefs) + StringLength(opt->mimedefs);
                  for(a = argv[na] ; *a != '\0' ; a++) {
                    if (*a == ';') {    /* next one */
                      StringAddchar(opt->mimedefs, '\n');
                      //*b++ = '\n';
                    } else if (*a == ',' || *a == '\n' || *a == '\r' || *a == '\t') {
                      StringAddchar(opt->mimedefs, ' ');
                      //*b++ = ' ';
                    } else {
                      StringAddchar(opt->mimedefs, *a);
                      //*b++ = *a;
                    }
                  }
                  StringAddchar(opt->mimedefs, '\n');
                  //*b++ = '\n';    /* next def */
                  //*b++ = '\0';
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
                StringCopy(opt->lang_iso,argv[na]);
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
                StringCopy(opt->footer,argv[na]);
              }
              break;
            case 'H':                 // debug headers
              _DEBUG_HEAD=1;
              break;
            case 'O':
#ifdef _WIN32
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
#ifdef _WIN32
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
              
            case 'W':       // Wrapper callback
              // --wrapper check-link=obj.so:check_link
              if ((na+1>=argc) || (argv[na+1][0]=='-')) {
                HTS_PANIC_PRINTF("Option %W needs to be followed by a blank space, and a <callback-name>=<myfile.so>:<function-name> field");
                printf("Example: -%%W check-link=checklink.so:check\n");
                htsmain_free();
                return -1;
              } else {
                char* pos;
                na++;
                for(pos = argv[na] ; *pos != '\0' && *pos != '=' && *pos != ',' && *pos != ':' ; pos++);
                /* httrack --wrapper callback[,foo] */
                if (*pos == 0 || *pos == ',' || *pos == ':') {
                  int ret;
                  char *moduleName;
                  if (*pos == ',' || *pos == ':') {
                    *pos = '\0';
                    moduleName = strdupt(argv[na]);
                    *pos = ',';   /* foce seperator to ',' */
                  } else {
                    moduleName = strdupt(argv[na]);
                  }
                  ret = plug_wrapper(opt, moduleName, argv[na]);
                  freet(moduleName);
                  if (ret == 0) {
                    char BIGSTK tmp[1024 * 2];
                    sprintf(tmp, "option %%W : unable to plug the module %s (returncode != 1)", argv[na]);
                    HTS_PANIC_PRINTF(tmp);
                    htsmain_free();
                    return -1;
                  } else if (ret == -1) {
                    char BIGSTK tmp[1024 * 2];
                    int last_errno = errno;
                    sprintf(tmp, "option %%W : unable to load the module %s: %s (check the library path ?)", argv[na], strerror(last_errno));
                    HTS_PANIC_PRINTF(tmp);
                    htsmain_free();
                    return -1;
                  }
                }
                /* Old style */
                /* httrack --wrapper save-name=callback:process,string */
                else if (*pos == '=') {
                  fprintf(stderr, "Syntax error in option %%W : the old (<3.41) API is no more supported!\n");
                  HTS_PANIC_PRINTF("Syntax error in option %W : the old (<3.41) API is no more supported!");
                  printf("Example: -%%W check-link=checklink.so:check\n");
                  htsmain_free();
                  return -1;
                } else {
                  HTS_PANIC_PRINTF("Syntax error in option %W : this function needs to be followed by a blank space, and a module name");
                  printf("Example: -%%W check-link=checklink.so:check\n");
                  htsmain_free();
                  return -1;
                }
              }
              break;
              
            case 'R':    // Referer
              if ((na+1>=argc) || (argv[na+1][0]=='-')) {
                HTS_PANIC_PRINTF("Option %R needs to be followed by a blank space, and a referer URL");
                printf("Example: -%%R \"http://www.example.com/\"\n");
                htsmain_free();
                return -1;
              } else{
                na++;
                if (strlen(argv[na])>=254) {
                  HTS_PANIC_PRINTF("Referer URL too long");
                  htsmain_free();
                  return -1;
                }
                StringCopy(opt->referer, argv[na]);
              }
              break;
            case 'E':    // From Email address
              if ((na+1>=argc) || (argv[na+1][0]=='-')) {
                HTS_PANIC_PRINTF("Option %E needs to be followed by a blank space, and an email");
                printf("Example: -%%E \"postmaster@example.com\"\n");
                htsmain_free();
                return -1;
              } else{
                na++;
                if (strlen(argv[na])>=254) {
                  HTS_PANIC_PRINTF("From email too long");
                  htsmain_free();
                  return -1;
                }
                StringCopy(opt->from, argv[na]);
              }
              break;

            default: {
              char s[HTS_CDLMAXSIZE + 256];
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
                  char s[HTS_CDLMAXSIZE + 256];
                  sprintf(s,"invalid option %%%c\n",*com);
                  HTS_PANIC_PRINTF(s);
                  htsmain_free();
                  return -1;
                         }
                  break;
                  
                  //case 's': opt->sslengine=1; if (isdigit((unsigned char)*(com+1))) { sscanf(com+1,"%d",&opt->sslengine); while(isdigit((unsigned char)*(com+1))) com++; } break;
            }
                    }
            break;
            
            //
          case '#':  { // non documenté
            com++;
            switch(*com) {
            case 'C':   // list cache files : httrack -#C '*spid*.gif' will attempt to find the matching file
              {
                int hasFilter = 0;
                int found = 0;
                char* filter=NULL;
                cache_back cache;
                inthash cache_hashtable=inthash_new(HTS_HASH_SIZE);
                int backupXFR = htsMemoryFastXfr;
                int sendb = 0;
                if (isdigit((unsigned char)*(com+1))) {
                  sscanf(com+1,"%d",&sendb);
                  while(isdigit((unsigned char)*(com+1))) com++;
                } else sendb=0;
                if (!((na+1>=argc) || (argv[na+1][0]=='-'))) {
                  na++;
                  hasFilter = 1;
                  filter=argv[na];
                }
                htsMemoryFastXfr = 1;               /* fast load */

                memset(&cache, 0, sizeof(cache_back));
                cache.type=1;       // cache?
                cache.log=stdout;     // log?
                cache.errlog=stderr;  // err log?
                cache.ptr_ant=cache.ptr_last=0;   // pointeur pour anticiper
                cache.hashtable=(void*)cache_hashtable;      /* copy backcache hash */
                cache.ro = 1;          /* read only */
                if (cache.hashtable) {
                  char BIGSTK adr[HTS_URLMAXSIZE*2];
                  char BIGSTK fil[HTS_URLMAXSIZE*2];
                  char BIGSTK url[HTS_URLMAXSIZE*2];
                  char linepos[256];
                  int  pos;
                  char* cacheNdx = readfile(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.ndx"));
                  cache_init(&cache,opt);            /* load cache */
                  if (cacheNdx != NULL) {
                    char firstline[256];
                    char* a = cacheNdx;
                    a+=cache_brstr(a, firstline);
                    a+=cache_brstr(a, firstline);
                    while ( a != NULL ) {
                      a=strchr(a+1,'\n');     /* start of line */
                      if (a) {
                        htsblk r;
                        /* */
                        a++;
                        /* read "host/file" */
                        a+=binput(a,adr,HTS_URLMAXSIZE);
                        a+=binput(a,fil,HTS_URLMAXSIZE);
                        url[0]='\0';
                        if (!link_has_authority(adr))
                          strcatbuff(url, "http://");
                        strcatbuff(url, adr);
                        strcatbuff(url, fil);
                        /* read position */
                        a+=binput(a,linepos,200);
                        sscanf(linepos,"%d",&pos);
                        if (!hasFilter
                          ||
                          (strjoker(url, filter, NULL, NULL) != NULL)
                          ) {
                          r = cache_read_ro(opt, &cache, adr, fil, "", NULL);    // lire entrée cache + data
                          if (r.statuscode != -1) {    // No errors
                            found++;
                            if (!hasFilter) {
                              fprintf(stdout, "%s%s%s\r\n", 
                                (link_has_authority(adr)) ? "" : "http://", 
                                adr, fil);
                            } else {
                              char msg[256], cdate[256];
                              char BIGSTK sav[HTS_URLMAXSIZE*2];
                              infostatuscode(msg, r.statuscode);
                              time_gmt_rfc822(cdate);

                              fprintf(stdout, "HTTP/1.1 %d %s\r\n",
                                r.statuscode,
                                r.msg[0] ? r.msg : msg
                                );
                              fprintf(stdout, "X-Host: %s\r\n", adr);
                              fprintf(stdout, "X-File: %s\r\n", fil);
                              fprintf(stdout, "X-URL: %s%s%s\r\n", 
                                (link_has_authority(adr)) ? "" : "http://", 
                                adr, fil);
                              if (url_savename(adr, fil, sav, /*former_adr*/NULL, /*former_fil*/NULL, /*referer_adr*/NULL, /*referer_fil*/NULL,
                                /*opt*/opt, /*liens*/NULL, /*lien_tot*/0, /*sback*/NULL, /*cache*/&cache, /*hash*/NULL, /*ptr*/0, /*numero_passe*/0, /*mime_type*/NULL)!=-1) {
                                if (fexist(sav)) {
                                  fprintf(stdout, "Content-location: %s\r\n", sav);
                                }
                              }
                              fprintf(stdout, "Date: %s\r\n", cdate);
                              fprintf(stdout, "Server: HTTrack Website Copier/"HTTRACK_VERSION"\r\n");
                              if (r.lastmodified[0]) {
                                fprintf(stdout, "Last-Modified: %s\r\n", r.lastmodified);
                              }
                              if (r.etag[0]) {
                                fprintf(stdout, "Etag: %s\r\n", r.etag);
                              }
                              if (r.totalsize >= 0) {
                                fprintf(stdout, "Content-Length: "LLintP"\r\n", r.totalsize);
                              }
                              fprintf(stdout, "X-Content-Length: "LLintP"\r\n", (r.size >= 0) ? r.size : (-r.size) );
                              if (r.contenttype >= 0) {
                                fprintf(stdout, "Content-Type: %s\r\n", r.contenttype);
                              }
                              if (r.cdispo[0]) {
                                fprintf(stdout, "Content-Disposition: %s\r\n", r.cdispo);
                              }
                              if (r.contentencoding[0]) {
                                fprintf(stdout, "Content-Encoding: %s\r\n", r.contentencoding);
                              }
                              if (r.is_chunk) {
                                fprintf(stdout, "Transfer-Encoding: chunked\r\n");
                              }
#if HTS_USEOPENSSL
                              if (r.ssl) {
                                fprintf(stdout, "X-SSL: yes\r\n");
                              }
#endif
                              if (r.is_write) {
                                fprintf(stdout, "X-Direct-To-Disk: yes\r\n");
                              }
                              if (r.compressed) {
                                fprintf(stdout, "X-Compressed: yes\r\n");
                              }
                              if (r.notmodified) {
                                fprintf(stdout, "X-Not-Modified: yes\r\n");
                              }
                              if (r.is_chunk) {
                                fprintf(stdout, "X-Chunked: yes\r\n");
                              }
                              fprintf(stdout, "\r\n");
                              /* Send the body */
                              if (sendb && r.adr) {
                                fprintf(stdout, "%s\r\n", r.adr);
                              }
                            }
                          }
                        }
                      }
                    }
                    freet(cacheNdx);
                  }
                }
                if (!found) {
                  fprintf(stderr, "No cache entry found%s%s%s\r\n",
                    (hasFilter)?" for '":"",
                    (hasFilter)?filter:"",
                    (hasFilter)?"'":""
                    );
                }
                htsMemoryFastXfr = backupXFR;
                return 0;
              }
              break;
            case 'E':     // extract cache
              if (!hts_extract_meta(StringBuff(opt->path_log))) {
                fprintf(stderr, "* error extracting meta-data\n");
                return 1;
              }
              fprintf(stderr, "* successfully extracted meta-data\n");
              return 0;
              break;
            case 'X': 
#ifndef STRDEBUG
              fprintf(stderr, "warning: no string debugging support built, option has no effect\n");
#endif
              htsMemoryFastXfr=1; 
              if (*(com+1)=='0') { htsMemoryFastXfr=0; com++; } 
              break;
            case 'R':
              {
                char* name;
                uLong repaired = 0;
                uLong repairedBytes = 0;
                if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.zip"))) {
                  name = fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.zip");
                } else if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.zip"))) {
                  name = fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.zip");
                } else {
                  fprintf(stderr, "* error: no cache found in %s\n", fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.zip"));
                  return 1;
                }
                fprintf(stderr, "Cache: trying to repair %s\n", name);
                if (unzRepair(name, 
                  fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/repair.zip"),
                  fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/repair.tmp"),
                  &repaired, &repairedBytes
                  ) == Z_OK) {
                  unlink(name);
                  rename(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/repair.zip"), name);
                  fprintf(stderr,"Cache: %d bytes successfully recovered in %d entries\n", (int) repairedBytes, (int) repaired);
                } else {
                  fprintf(stderr, "Cache: could not repair the cache\n");
                }
              }
              return 0;
              break;
            case '~': /* internal lib test */
              //Disabled because choke on GCC 4.3 (toni from links2linux.de)
              //{
              //  char thisIsATestYouShouldSeeAnError[12];
              //  const char *const bufferOverflowTest = "0123456789012345678901234567890123456789";
              //  strcpybuff(thisIsATestYouShouldSeeAnError, bufferOverflowTest);
              //  return 0;
              //}
              break;
            case 'f': opt->flush=1; break;
            case 'h':
              printf("HTTrack version "HTTRACK_VERSION"%s\n", hts_get_version_info(opt));
              return 0;
              break;
            case 'p': /* opt->aff_progress=1; deprecated */ break;
            case 'S': opt->shell=1; break;  // stdin sur un shell
            case 'K': opt->keyboard=1; break;  // vérifier stdin
              //
            case 'L': sscanf(com+1,"%d",&opt->maxlink); while(isdigit((unsigned char)*(com+1))) com++; break;
            case 'F': sscanf(com+1,"%d",&opt->maxfilter); while(isdigit((unsigned char)*(com+1))) com++; break;
            case 'Z': opt->makestat=1; break;
            case 'T': opt->maketrack=1; break;
            case 'u': sscanf(com+1,"%d",&opt->waittime); while(isdigit((unsigned char)*(com+1))) com++; break;

            /*case 'R':    // ohh ftp, catch->ftpget
              HTS_PANIC_PRINTF("Unexpected internal error with -#R command");
              htsmain_free();
              return -1;        
              break;
              */
            case 'P': {     // catchurl
              help_catchurl(StringBuff(opt->path_log));
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
            case '1':   /* test #1 : fil_simplifie */
              if (na+1>=argc) {
                HTS_PANIC_PRINTF("Option #1 needs to be followed by an URL");
                printf("Example: '-#1' ./foo/bar/../foobar\n");
                htsmain_free();
                return -1;
              } else {
                fil_simplifie(argv[na+1]);
                printf("simplified=%s\n", argv[na+1]);
                htsmain_free();
                return 0;
              }
              break;
            case '2':   // mimedefs
              if (na+1>=argc) {
                HTS_PANIC_PRINTF("Option #2 needs to be followed by an URL");
                printf("Example: '-#2' /foo/bar.php\n");
                htsmain_free();
                return -1;
              } else {
                char mime[256];
                // initialiser mimedefs
                //get_userhttptype(opt,1,opt->mimedefs,NULL);
                // check
                mime[0] = '\0';
                get_httptype(opt, mime, argv[na+1], 0);
                if (mime[0] != '\0') {
                  char ext[256];
                  printf("%s is '%s'\n", argv[na+1], mime);
                  ext[0] = '\0';
                  give_mimext(ext, mime);
                  if (ext[0]) {
                    printf("and its local type is '.%s'\n", ext);
                  }
                } else {
                  printf("%s is of an unknown MIME type\n", argv[na+1]);
                }
                htsmain_free();
                return 0;
              }
              break;
            case '3':   // charset tests: httrack #3 "iso-8859-1" "café"
              if (argc == 3) {
                char *s = hts_convertStringToUTF8(argv[2], strlen(argv[2]), argv[1]);
                if (s != NULL) {
                  printf(">> %s\n", s);
                  free(s);
                } else {
                  fprintf(stderr, "invalid string for charset %s\n", argv[1]);
                }
              } else {
                HTS_PANIC_PRINTF("Option #3 needs to be followed by a charset and a string");
              }
              htsmain_free();
              return 0;
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
            case 'd':
              opt->parsedebug = 1;
              break;

            /* autotest */
            case 't':     /* not yet implemented */
              fprintf(stderr, "** AUTOCHECK OK\n");
              return 0;
              break;

            default: printf("Internal option %c not recognized\n",*com); break;
            }
                     }
            break; 
          case 'O':    // output path
						while(isdigit(com[1])) {
							com++;
						}
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
              opt->proxy.active=1;
              // Rechercher MAIS en partant de la fin à cause de user:pass@proxy:port
              a = argv[na] + strlen(argv[na]) -1;
              // a=strstr(argv[na],":");  // port
              while( (a > argv[na]) && (*a != ':') && (*a != '@') ) a--;
              if (*a == ':') {  // un port est présent, <proxy>:port
                sscanf(a+1,"%d",&opt->proxy.port);
                StringCopyN(opt->proxy.name,argv[na],(int) (a - argv[na]));
              } else {  // <proxy>
                opt->proxy.port=8080;
                StringCopy(opt->proxy.name,argv[na]);
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
              StringCopy(opt->user_agent,argv[na]);
              if (StringNotEmpty(opt->user_agent))
                opt->user_agent_send=1;
              else
                opt->user_agent_send=0;    // -F "" désactive l'option
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
              StringCopy(opt->sys_com,argv[na]);
              if (StringNotEmpty(opt->sys_com))
                opt->sys_com_exec=1;
              else
                opt->sys_com_exec=0;    // -V "" désactive l'option
            }
            break;
            //
          default: {
            char s[HTS_CDLMAXSIZE + 256];
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
				char catbuff[CATBUFF_SIZE];
        char BIGSTK tempo[CATBUFF_SIZE];
		const int urlSize = (int) strlen(argv[na]);
		const int capa = (int) ( strlen(url) + urlSize + 32 );
		assertf(urlSize < HTS_URLMAXSIZE);
		if (urlSize < HTS_URLMAXSIZE) {
			ensureUrlCapacity(url, url_sz, capa);
			if (strnotempty(url)) strcatbuff(url," ");  // espace de séparation
			strcpybuff(tempo,unescape_http_unharm(catbuff,argv[na],1));
			escape_spc_url(tempo);
			strcatbuff(url,tempo);
		}
      }  // if argv=- etc. 
      
    }  // for
  }
  
#if BDEBUG==3  
  printf("URLs/filters=%s\n",url);
#endif

#if DEBUG_STEPS
  printf("Analyzing parameters done\n");
#endif


#ifdef _WIN32
#else
#ifndef HTS_DO_NOT_USE_UID
  /* Chroot - xxc */
  if (switch_chroot) {
    uid_t userid=getuid();
    //struct passwd* userdef=getpwuid(userid);
    //if (userdef) {
    if (!userid) {
      //if (strcmp(userdef->pw_name,"root")==0) {
      char BIGSTK rpath[1024];
      //printf("html=%s log=%s\n",StringBuff(opt->path_html),StringBuff(opt->path_log));    // xxc
      if ((StringBuff(opt->path_html)[0]) && (StringBuff(opt->path_log)[0])) {
        const char *a=StringBuff(opt->path_html),*b=StringBuff(opt->path_log),*c=NULL,*d=NULL;
        c=a; d=b;
        while ((*a) && (*a == *b)) {
          if (*a=='/') { c=a; d=b; }
          a++;
          b++;
        }

        rpath[0]='\0';
        if (c != StringBuff(opt->path_html)) {
          if (StringBuff(opt->path_html)[0]!='/')
            strcatbuff(rpath,"./");
          strncatbuff(rpath,StringBuff(opt->path_html),(int) (c - StringBuff(opt->path_html)));
        }
        StringCopyOverlapped(opt->path_html, c);
        StringCopyOverlapped(opt->path_log, d);
      } else {
        strcpybuff(rpath,"./");
        StringCopy(opt->path_html,"/");
        StringCopy(opt->path_log,"/");
      }
      if (rpath[0]) {
        printf("[changing root path to %s (path_data=%s,path_log=%s)]\n",rpath,StringBuff(opt->path_html),StringBuff(opt->path_log));
        if (chroot(rpath)) {
          printf("ERROR! Can not chroot to %s!\n",rpath);
          return -1;
        }
        if (chdir("/")) {     /* new root */
          printf("ERROR! Can not chdir to %s!\n",rpath);
          return -1;
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
  if (opt->cache) {
    if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-in_progress.lock"))) {   // problemes..
      if ( fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.dat")) ) { 
        if ( fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.zip")) ) {
          if (fsize(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.zip"))<32768) {
            if (fsize(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.zip"))>65536) {
              if (fsize(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.zip")) > fsize(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.zip"))) {
                remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.zip"));
                rename(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.zip"), fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.zip"));
              }
            }
          }
        }
      }
      else if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.dat")) && fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.ndx"))) { 
        if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.dat")) && fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.ndx"))) {
          // switcher si new<32Ko et old>65Ko (tailles arbitraires) ?
          // ce cas est peut être une erreur ou un crash d'un miroir ancien, prendre
          // alors l'ancien cache
          if (fsize(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.dat"))<32768) {
            if (fsize(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.dat"))>65536) {
              if (fsize(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.dat")) > fsize(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.dat"))) {
                remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.dat"));
                remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.ndx"));
                rename(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.dat"),fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.dat"));
                rename(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/old.ndx"),fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/new.ndx"));  
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
    ioinfo=fopen(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-ioinfo.txt"),"wb");
  }
  
  {
    char n_lock[256];
    // on peut pas avoir un affichage ET un fichier log
    // ca sera pour la version 2
    if (httrack_logmode==1) {
      opt->log=stdout;
      opt->errlog=stderr;
    } else if (httrack_logmode>=2) {
      // deux fichiers log
      structcheck(StringBuff(opt->path_log));
      if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-log.txt")))
        remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-log.txt"));
      if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-err.txt")))
        remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-err.txt"));

      /* Check FS directory structure created */
      structcheck(StringBuff(opt->path_log));

      opt->log=fopen(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-log.txt"),"w");
      if (httrack_logmode==2)
        opt->errlog=fopen(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-err.txt"),"w");
      else
        opt->errlog=opt->log;
      if (opt->log==NULL) {
        char s[HTS_CDLMAXSIZE + 256];
        sprintf(s,"Unable to create log file %s",fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-log.txt"));
        HTS_PANIC_PRINTF(s);
        htsmain_free();
        return -1;
      } else if (opt->errlog==NULL) {
        char s[HTS_CDLMAXSIZE + 256];
        sprintf(s,"Unable to create log file %s",fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-err.txt"));
        HTS_PANIC_PRINTF(s);
        htsmain_free();
        return -1;
      }

    } else {
      opt->log=NULL;
      opt->errlog=NULL;
    }
    
    // un petit lock-file pour indiquer un miroir en cours, ainsi qu'un éventuel fichier log
    {
      FILE* fp=NULL;
      //int n=0;
      char t[256];
      time_local_rfc822(t);    // faut bien que ca serve quelque part l'heure RFC1945 arf'
      
      /* readme for information purpose */
      {
        FILE* fp=fopen(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/readme.txt"),"wb");
        if (fp) {
          fprintf(fp,"What's in this folder?"LF);
          fprintf(fp,""LF);
          fprintf(fp,"This folder (hts-cache) has been generated by WinHTTrack "HTTRACK_VERSION"%s"LF, hts_get_version_info(opt));
          fprintf(fp,"and is used for updating this website."LF);
          fprintf(fp,"(The HTML website structure is stored here to allow fast updates)"LF""LF);
          fprintf(fp,"DO NOT delete this folder unless you do not want to update the mirror in the future!!"LF);
          fprintf(fp,"(you can safely delete old.zip and old.lst files, however)"LF);
          fprintf(fp,""LF);
          fprintf(fp,HTS_LOG_SECURITY_WARNING);
          fclose(fp);
        }
      }

      strcpy(n_lock,fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log), "hts-in_progress.lock"));
      //sprintf(n_lock,fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-in_progress.lock"),n);
      /*do {
        if (!n)
          sprintf(n_lock,fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-in_progress.lock"),n);
        else
          sprintf(n_lock,fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-in_progress%d.lock"),n);
        n++;
      } while((fexist(n_lock)) && opt->quiet);      
      if (fexist(n_lock)) {
        if (!recuperer) {
          remove(n_lock);
        }
      }*/

      // vérifier existence de la structure
      structcheck(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_html), "/"));
      structcheck(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log), "/"));
     
      // reprise/update
      if (opt->cache) {
        FILE* fp;
        int i;
#ifdef _WIN32
        mkdir(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache"));
#else
        mkdir(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache"),HTS_PROTECT_FOLDER);
#endif
        fp=fopen(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-cache/doit.log"),"wb");
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
        //} else if (opt->debug>1) {
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
#if USE_BEGINTHREAD
				fprintf(fp, "PID=%d\n", (int)getpid());
#ifndef _WIN32
				fprintf(fp, "UID=%d\n", (int)getuid());
				fprintf(fp, "GID=%d\n", (int)getuid());
#endif
				fprintf(fp, "START=%d\n", (int)time(NULL));
#endif
        fclose(fp); fp=NULL;
      }
      
      // fichier log        
      if (opt->log)     {
        int i;
        fprintf(opt->log,"HTTrack"HTTRACK_VERSION"%s launched on %s at %s"LF, 
          hts_get_version_info(opt),
          t, url);
        fprintf(opt->log,"(");
        for(i=0;i<argc;i++) {
#ifdef _WIN32
          char *carg = hts_convertStringSystemToUTF8(argv[i], (int) strlen(argv[i]));
          char *arg = carg != NULL ? carg : argv[i];
#else
          const char *arg = argv[i];
#endif
          if (strchr(arg, ' ') == NULL || strchr(arg, '\"') != NULL)
            fprintf(opt->log,"%s ", arg);
          else    // entre "" (si espace(s) et pas déja de ")
            fprintf(opt->log,"\"%s\" ", arg);
#ifdef _WIN32
          if (carg != NULL)
            free(carg);
#endif
        }
        fprintf(opt->log,")"LF);
        fprintf(opt->log,LF);
        fprintf(opt->log,"Information, Warnings and Errors reported for this mirror:"LF);
        fprintf(opt->log,HTS_LOG_SECURITY_WARNING );
        fprintf(opt->log,LF);
      }

      if (httrack_logmode) {
        printf("Mirror launched on %s by HTTrack Website Copier/"HTTRACK_VERSION"%s "HTTRACK_AFF_AUTHORS""LF,t,hts_get_version_info(opt));
        if (opt->wizard==0) {
          printf("mirroring %s with %d levels, %d sockets,t=%d,s=%d,logm=%d,lnk=%d,mdg=%d\n",url,opt->depth,opt->maxsoc,opt->travel,opt->seeker,httrack_logmode,opt->urlmode,opt->getmode);
        } else {    // the magic wizard
          printf("mirroring %s with the wizard help..\n",url);
        }
      }
    }
    
    io_flush;

    /* Enforce limits to avoid bandwith abuse. The bypass_limits should only be used by administrators and experts. */
    if (!opt->bypass_limits) {
      if (opt->maxsoc <= 0 || opt->maxsoc > 4) {
        opt->maxsoc = 4;
        if (opt->log != NULL) {
          HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"* security warning: maximum number of simultaneous connections limited to %d to avoid server overload"LF, (int)opt->maxsoc);
        }
      }
      if (opt->maxrate <= 0 || opt->maxrate > 100000) {
        opt->maxrate = 100000;
        if (opt->log != NULL) {
          HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"* security warning: maximum bandwidth limited to %d to avoid server overload"LF, (int)opt->maxrate);
        }
      }
      if (opt->maxconn <= 0 || opt->maxconn > 5.0) {
        opt->maxconn = 5.0;
        if (opt->log != NULL) {
          HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"* security warning: maximum number of connections per second limited to %f to avoid server overload"LF, (float)opt->maxconn);
        }
      }
    } else {
      if (opt->log != NULL) {
        HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"* security warning: !!! BYPASSING SECURITY LIMITS - MONITOR THIS SESSION WITH EXTREME CARE !!!"LF);
      }
    }

  /* Info for wrappers */
  if ( (opt->debug>0) && (opt->log!=NULL) ) {
    HTS_LOG(opt,LOG_INFO); fprintf(opt->log,"engine: init"LF);
  }

  /* Init external */
  RUN_CALLBACK_NOARG(opt, init);

  // détourner SIGHUP etc.
#if DEBUG_STEPS
  printf("Launching the mirror\n");
#endif

    // Lancement du miroir
    // ------------------------------------------------------------
    opt->state._hts_in_mirror = 1;
    if (httpmirror(url, opt)==0) {
      printf("Error during operation (see log file), site has not been successfully mirrored\n");
    } else {
      if  (opt->shell) {
        HTT_REQUEST_START;
        HT_PRINT("TRANSFER DONE"LF);
        HTT_REQUEST_END
      } else {
        printf("Done.\n");
      }
    }
    opt->state._hts_in_mirror = 0;
    // ------------------------------------------------------------

    //
    // Build top index
    if (opt->dir_topindex) {
      char BIGSTK rpath[1024*2];
      char* a;
      strcpybuff(rpath,StringBuff(opt->path_html));
      if (rpath[0]) {
        if (rpath[strlen(rpath)-1]=='/')
          rpath[strlen(rpath)-1]='\0';
      }
      a=strrchr(rpath,'/');
      if (a) {
        *a='\0';
        hts_buildtopindex(opt,rpath,StringBuff(opt->path_bin));
        if (opt->log) {
          HTS_LOG(opt,LOG_INFO); fprintf(opt->log,"Top index rebuilt (done)"LF);
        }
      }
    }

    if (opt->state.exit_xh ==1) {
      if (opt->log) {
        fprintf(opt->log,"* * MIRROR ABORTED! * *\nThe current temporary cache is required for any update operation and only contains data downloaded during the present aborted session.\nThe former cache might contain more complete information; if you do not want to lose that information, you have to restore it and delete the current cache.\nThis can easily be done here by erasing the hts-cache/new.* files]\n");
      }
    }

    /* Not or cleanly interrupted; erase hts-cache/ref temporary directory */
    if (opt->state.exit_xh == 0) {
      // erase ref files if not interrupted
      DIR *dir;
      struct dirent *entry;
      for(dir = opendir(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log), CACHE_REFNAME))
        ; dir != NULL && ( entry = readdir(dir) ) != NULL 
        ; )
      {
        if (entry->d_name[0] != '\0' && entry->d_name[0] != '.') {
          char *f = OPT_GET_BUFF(opt);
          sprintf(f, "%s/%s", CACHE_REFNAME, entry->d_name);
          (void)unlink(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log), f));
        }
      }
      if (dir != NULL) {
        (void) closedir(dir);
      }
      (void)rmdir(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log), CACHE_REFNAME));
    }

    /* Info for wrappers */
    if ( (opt->debug>0) && (opt->log!=NULL) ) {
      HTS_LOG(opt,LOG_INFO); fprintf(opt->log,"engine: free"LF);
    }

    /* UnInit */
    RUN_CALLBACK_NOARG(opt, uninit);

    if (httrack_logmode!=1) {
      if (opt->errlog == opt->log) opt->errlog=NULL;
      if (opt->log) { fclose(opt->log); opt->log=NULL; }
      if (opt->errlog) { fclose(opt->errlog); opt->errlog=NULL; }
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
  if (x_argv)
    freet(x_argv);
  if (url)
    freet(url);

#ifdef HTS_TRACE_MALLOC
  hts_freeall();
#endif

  printf("Thanks for using HTTrack!\n");
  io_flush;
  htsmain_free();
  return 0;    // OK
}


// main() subroutines

// vérifier chemin path
int check_path(String* s, char* defaultname) {
  int i;
  int return_value=0;

  // Replace name: ~/mywebsites/# -> /home/foo/mywebsites/#
  expand_home(s);
  for(i = 0 ; i < (int) StringLength(*s) ; i++)    // conversion \ -> /
    if (StringSub(*s, i) == '\\')
      StringSubRW(*s, i) = '/';
  
  // remove ending /
  if (StringNotEmpty(*s) && StringRight(*s, 1) == '/')
    StringPopRight(*s);

   // Replace name: /home/foo/mywebsites/# -> /home/foo/mywebsites/wonderfulsite
  if (StringNotEmpty(*s)) {
    if (StringRight(*s, 1) == '#') {
      if (strnotempty((defaultname?defaultname:""))) {
        char* a = strchr(defaultname,'#');      // we never know..
        if (a)
          *a='\0';
        StringPopRight(*s);
        StringCat(*s, defaultname);
      } else {
        StringClear(*s); // Clear path (no name/default url given)
      }
      return_value=1;     // expanded
    }
  }

  // ending /
  if (StringNotEmpty(*s) && StringRight(*s, 1) != '/')    // ajouter slash à la fin
    StringCat(*s, "/");

  return return_value;
}

// détermine si l'argument est une option
int cmdl_opt(char* s) {
  if (s[0]=='-') {  // c'est peut être une option
    if (strchr(s,'.')!=NULL && strchr(s,'%')==NULL)
      return 0;    // sans doute un -www.truc.fr (note: -www n'est pas compris)
    else if (strchr(s,'/')!=NULL)
      return 0;    // idem, -*cgi-bin/
    else if (strchr(s,'*')!=NULL)
      return 0;    // joker, idem
    else
      return 1;
  } else return 0;
}

