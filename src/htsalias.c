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
/* File: htsalias.c subroutines:                                */
/*       alias for command-line options and config files        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htsbase.h"
#include "htsalias.h"
#include "htsglobal.h"
#include "htslib.h"

#define _NOT_NULL(a) ( (a!=NULL) ? (a) : "" )

// COPY OF cmdl_ins in htsmain.c
// Insert a command in the argc/argv
#define cmdl_ins(token,argc,argv,buff,ptr) \
  { \
  int i; \
  for(i=argc;i>0;i--)\
  argv[i]=argv[i-1];\
  } \
  argv[0]=(buff+ptr); \
  strcpybuff(argv[0],token); \
  ptr += (int) (strlen(argv[0])+1); \
  argc++
// END OF COPY OF cmdl_ins in htsmain.c

/*
  Aliases for command-line and config file definitions
  These definitions can be used:
  in command line:
  --sockets=8       --cache=0
  --sockets 8       --cache off
                    --nocache
  -c8               -C0
  in config file:
  sockets=8         cache=0
  set sockets 8     cache off

*/
/*
  single : no options
  param  : this option allows a number parameter (1, for example) and can be mixed with other options (R1C1c8)
  param1 : this option must be alone, and needs one distinct parameter (-P <path>)
  param0 : this option must be alone, but the parameter should be put together (+*.gif)
*/
const char *hts_optalias[][4] = {
  /*   {"","","",""}, */
  {"path", "-O", "param1", "output path"},
  {"mirror", "-w", "single", ""},
  {"mirror-wizard", "-W", "single", ""},
  {"get-files", "-g", "single", ""},
  {"quiet", "-q", "single", ""},
  {"mirrorlinks", "-Y", "single", ""},
  {"proxy", "-P", "param1", "proxy name:port"},
  {"bind", "-%b", "param1", "hostname to bind"},
  {"httpproxy-ftp", "-%f", "param", ""},
  {"depth", "-r", "param", ""}, {"recurse-levels", "-r", "param", ""},
  {"ext-depth", "-%e", "param", ""},
  {"max-files", "-m", "param", ""},
  {"max-size", "-M", "param", ""},
  {"max-time", "-E", "param", ""},
  {"max-rate", "-A", "param", ""},
  {"max-pause", "-G", "param", ""},
  {"sockets", "-c", "param", "number of simultaneous connections allowed"},
    {"socket", "-c", "param", "number of simultaneous connections allowed"},
    {"connection", "-c", "param", "number of simultaneous connections allowed"},
  {"connection-per-second", "-%c", "param",
   "number of connection per second allowed"},
  {"timeout", "-T", "param", ""},
  {"retries", "-R", "param", "number of retries for non-fatal errors"},
  {"min-rate", "-J", "param", ""},
  {"host-control", "-H", "param", ""},
  {"extended-parsing", "-%P", "param", ""},
  {"near", "-n", "single", ""},
  {"delayed-type-check", "-%N", "single", ""},
  {"cached-delayed-type-check", "-%D", "single", ""},
  {"delayed-type-check-always", "-%N2", "single", ""},
  {"disable-security-limits", "-%!", "single", ""},
  {"test", "-t", "single", ""},
  {"list", "-%L", "param1", ""},
  {"urllist", "-%S", "param1", ""},
  {"language", "-%l", "param1", ""}, {"lang", "-%l", "param1", ""},
  {"accept", "-%a", "param1", ""},
  {"headers", "-%X", "param1", ""},
  {"structure", "-N", "param", ""}, {"user-structure", "-N", "param1", ""},
  {"long-names", "-L", "param", ""},
  {"keep-links", "-K", "param", ""},
  {"mime-html", "-%M", "single", ""}, {"mht", "-%M", "single", ""},
  {"replace-external", "-x", "single", ""},
  {"disable-passwords", "-%x", "single", ""}, {"disable-password", "-%x",
                                               "single", ""},
  {"include-query-string", "-%q", "single", ""},
  {"generate-errors", "-o", "single", ""},
  {"do-not-generate-errors", "-o0", "single", ""},
  {"purge-old", "-X", "param", ""},
  {"cookies", "-b", "param", ""},
  {"check-type", "-u", "param", ""},
  {"assume", "-%A", "param1", ""}, {"mimetype", "-%A", "param1", ""},
  {"parse-java", "-j", "param", ""},
  {"protocol", "-@i", "param", ""},
  {"robots", "-s", "param", ""},
  {"http-10", "-%h", "single", ""}, {"http-1.0", "-%h", "single", ""},
  {"keep-alive", "-%k", "single", ""},
  {"build-top-index", "-%i", "single", ""},
  {"disable-compression", "-%z", "single", ""},
  {"tolerant", "-%B", "single", ""},
  {"updatehack", "-%s", "single", ""}, {"sizehack", "-%s", "single", ""},
  {"urlhack", "-%u", "single", ""},
  {"user-agent", "-F", "param1", "user-agent identity"},
  {"referer", "-%R", "param1", "default referer URL"},
  {"from", "-%E", "param1", "from email address"},
  {"footer", "-%F", "param1", ""},
  {"cache", "-C", "param", "number of retries for non-fatal errors"},
  {"store-all-in-cache", "-k", "single", ""},
  {"do-not-recatch", "-%n", "single", ""},
  {"do-not-log", "-Q", "single", ""},
  {"extra-log", "-z", "single", ""},
  {"debug-log", "-Z", "single", ""},
  {"verbose", "-v", "single", ""},
  {"file-log", "-f", "single", ""},
  {"single-log", "-f2", "single", ""},
  {"index", "-I", "single", ""},
  {"search-index", "-%I", "single", ""},
  {"priority", "-p", "param", ""},
  {"debug-headers", "-%H", "single", ""},
  {"userdef-cmd", "-V", "param1", ""},
  {"callback", "-%W", "param1", "plug an external callback"}, {"wrapper", "-%W",
                                                               "param1",
                                                               "plug an external callback"},
  {"structure", "-N", "param1", "user-defined structure"},
  {"usercommand", "-V", "param1", "user-defined command"},
  {"display", "-%v", "single",
   "show files transferred and other funny realtime information"},
  {"dos83", "-L0", "single", ""},
  {"iso9660", "-L2", "single", ""},
  {"disable-module", "-%w", "param1", ""},
  {"no-background-on-suspend", "-y0", "single", ""},
  {"background-on-suspend", "-y", "single", ""},
  {"utf8-conversion", "-%T", "single", ""},
  {"no-utf8-conversion", "-%T0", "single", ""},
  /* */

  /* DEPRECATED */
  {"stay-on-same-dir", "-S", "single",
   "stay on the same directory - DEPRECATED"},
  {"can-go-down", "-D", "single", "can only go down into subdirs - DEPRECATED"},
  {"can-go-up", "-U", "single", "can only go to upper directories- DEPRECATED"},
  {"can-go-up-and-down", "-B", "single",
   "can both go up&down into the directory structure - DEPRECATED"},
  {"stay-on-same-address", "-a", "single",
   "stay on the same address - DEPRECATED"},
  {"stay-on-same-domain", "-d", "single",
   "stay on the same principal domain - DEPRECATED"},
  {"stay-on-same-tld", "-l", "single",
   "stay on the same TLD (eg: .com) - DEPRECATED"},
  {"go-everywhere", "-e", "single", "go everywhere on the web - DEPRECATED"},

  /* Badly documented */
  {"debug-testfilters", "-#0", "param1", "debug: test filters"},
  {"advanced-flushlogs", "-#f", "single", ""},
  {"advanced-maxfilters", "-#F", "param", "maximum number of scan rules"},
  {"version", "-#h", "single", ""},
  {"debug-scanstdin", "-#K", "single", ""},
  {"advanced-maxlinks", "-#L", "param", "maximum number of links (0 to disable limit)"},
  {"advanced-progressinfo", "-#p", "single", "deprecated"},
  {"catch-url", "-#P", "single", "catch complex URL through proxy"},
  /*{"debug-oldftp","-#R","single",""}, */
  {"debug-xfrstats", "-#T", "single", ""},
  {"advanced-wait", "-#u", "single", ""},
  {"debug-ratestats", "-#Z", "single", ""},
  {"fast-engine", "-#X", "single", "Enable fast routines"},
  {"debug-overflows", "-#X0", "single", "Attempt to detect buffer overflows"},
  {"debug-cache", "-#C", "param1", "List files in the cache"},
  {"extract-cache", "-#C", "single", "Extract meta-data"},
  {"debug-parsing", "-#d", "single", "debug: test parser"},
  {"repair-cache", "-#R", "single", "repair the damaged cache ZIP file"},
    {"repair", "-#R", "single", ""},

  /* STANDARD ALIASES */
  {"spider", "-p0C0I0t", "single", ""},
  {"testsite", "-p0C0I0t", "single", ""},
  {"testlinks", "-r1p0C0I0t", "single", ""}, {"test", "-r1p0C0I0t", "single",
                                              ""}, {"bookmark", "-r1p0C0I0t",
                                                    "single", ""},
  {"mirror", "-w", "single", ""},
  {"testscan", "-p0C0I0Q", "single", ""}, {"scan", "-p0C0I0Q", "single", ""},
    {"check", "-p0C0I0Q", "single", ""},
  {"skeleton", "-p1", "single", ""},
  {"preserve", "-%p", "single", ""},
  {"get", "-qg", "single", ""},
  {"update", "-iC2", "single", ""},
  {"continue", "-iC1", "single", ""}, {"restart", "-iC1", "single", ""},
  {"continue", "-i", "single", ""},     /* for help alias */
  {"sucker", "-r999", "single", ""},
  {"help", "-h", "single", ""}, {"documentation", "-h", "single", ""}, {"doc",
                                                                        "-h",
                                                                        "single",
                                                                        ""},
  {"wide", "-c32", "single", ""},
  {"tiny", "-c1", "single", ""},
  {"ultrawide", "-c48", "single", ""},
  {"http10", "-%h", "single", ""},
  {"filelist", "-%L", "single", ""}, {"list", "-%L", "single", ""},
  {"filterlist", "-%S", "single", ""},
  /* END OF ALIASES */

  /* Filters */
  {"allow", "+", "param0", "allow filter"},
  {"deny", "-", "param0", "deny filter"},
  /* */

  /* URLs */
  {"add", "", "param0", "add URLs"},
  /* */

  /* Internal */
  {"catchurl", "--catchurl", "single", "catch complex URL through proxy"},
  {"updatehttrack", "--updatehttrack", "single",
   "update HTTrack Website Copier"},
  {"clean", "--clean", "single", "clean up log files and cache"},
  {"tide", "--clean", "single", "clean up log files and cache"},
  {"autotest", "-#T", "single", ""},
  /* */

  {"", "", "", ""}
};

/* 
  Check for alias in command-line 
  argc,argv     as in main()
  n_arg         argument position
  return_argv   a char[2][] where to put result
  return_error  buffer in case of syntax error

  return value: number of arguments treated (0 if error)
*/
int optalias_check(int argc, const char *const *argv, int n_arg,
                   int *return_argc, char **return_argv, char *return_error) {
  return_error[0] = '\0';
  *return_argc = 1;
  if (argv[n_arg][0] == '-')
    if (argv[n_arg][1] == '-') {
      char command[1000];
      char param[1000];
      char addcommand[256];

      /* */
      char *position;
      int need_param = 1;

      //int return_param=0;
      int pos;

      command[0] = param[0] = addcommand[0] = '\0';

      /* --sockets=8 */
      if ((position = strchr(argv[n_arg], '='))) {
        /* Copy command */
        strncatbuff(command, argv[n_arg] + 2,
                    (int) (position - (argv[n_arg] + 2)));
        /* Copy parameter */
        strcpybuff(param, position + 1);
      }
      /* --nocache */
      else if (strncmp(argv[n_arg] + 2, "no", 2) == 0) {
        strcpybuff(command, argv[n_arg] + 4);
        strcpybuff(param, "0");
      }
      /* --sockets 8 */
      else {
        if (strncmp(argv[n_arg] + 2, "wide-", 5) == 0) {
          strcpybuff(addcommand, "c32");
          strcpybuff(command, strchr(argv[n_arg] + 2, '-') + 1);
        } else if (strncmp(argv[n_arg] + 2, "tiny-", 5) == 0) {
          strcpybuff(addcommand, "c1");
          strcpybuff(command, strchr(argv[n_arg] + 2, '-') + 1);
        } else
          strcpybuff(command, argv[n_arg] + 2);
        need_param = 2;
      }

      /* Now solve the alias */
      pos = optalias_find(command);
      if (pos >= 0) {
        /* Copy real name */
        strcpybuff(command, hts_optalias[pos][1]);
        /* With parameters? */
        if (strncmp(hts_optalias[pos][2], "param", 5) == 0) {
          /* Copy parameters? */
          if (need_param == 2) {
            if ((n_arg + 1 >= argc) || (argv[n_arg + 1][0] == '-')) {   /* no supplemental parameter */
              sprintf(return_error,
                      "Syntax error:\n\tOption %s needs to be followed by a parameter: %s <param>\n\t%s\n",
                      command, command, _NOT_NULL(optalias_help(command)));
              return 0;
            }
            strcpybuff(param, argv[n_arg + 1]);
            need_param = 2;
          }
        } else
          need_param = 1;

        /* Final result */

        /* Must be alone (-P /tmp) */
        if (strcmp(hts_optalias[pos][2], "param1") == 0) {
          strcpybuff(return_argv[0], command);
          strcpybuff(return_argv[1], param);
          *return_argc = 2;     /* 2 parameters returned */
        }
        /* Alone with parameter (+*.gif) */
        else if (strcmp(hts_optalias[pos][2], "param0") == 0) {
          /* Command */
          strcpybuff(return_argv[0], command);
          strcatbuff(return_argv[0], param);
        }
        /* Together (-c8) */
        else {
          /* Command */
          strcpybuff(return_argv[0], command);
          /* Parameters accepted */
          if (strncmp(hts_optalias[pos][2], "param", 5) == 0) {
            /* --cache=off or --index=on */
            if (strcmp(param, "off") == 0)
              strcatbuff(return_argv[0], "0");
            else if (strcmp(param, "on") == 0) {
              // on is the default
              // strcatbuff(return_argv[0],"1");
            } else
              strcatbuff(return_argv[0], param);
          }
          *return_argc = 1;     /* 1 parameter returned */
        }
      } else {
        sprintf(return_error, "Unknown option: %s\n", command);
        return 0;
      }
      return need_param;
    }

  /* Check -O <path> */
  {
    int pos;

    if ((pos = optreal_find(argv[n_arg])) >= 0) {
      if ((strcmp(hts_optalias[pos][2], "param1") == 0)
          || (strcmp(hts_optalias[pos][2], "param0") == 0)) {
        if ((n_arg + 1 >= argc) || (argv[n_arg + 1][0] == '-')) {       /* no supplemental parameter */
          sprintf(return_error,
                  "Syntax error:\n\tOption %s needs to be followed by a parameter: %s <param>\n\t%s\n",
                  argv[n_arg], argv[n_arg],
                  _NOT_NULL(optalias_help(argv[n_arg])));
          return 0;
        }
        /* Copy parameters */
        strcpybuff(return_argv[0], argv[n_arg]);
        strcpybuff(return_argv[1], argv[n_arg + 1]);
        /* And return */
        *return_argc = 2;       /* 2 parameters returned */
        return 2;               /* 2 parameters used */
      }
    }
  }

  /* Copy and return other unknown option */
  strcpybuff(return_argv[0], argv[n_arg]);
  return 1;
}

/* Finds the <token> option alias and returns the index, or -1 if failed */
int optalias_find(const char *token) {
  if (token[0] != '\0') {
    int i = 0;

    while(hts_optalias[i][0][0] != '\0') {
      if (strcmp(token, hts_optalias[i][0]) == 0) {
        return i;
      }
      i++;
    }
  }
  return -1;
}

/* Finds the <token> real option and returns the index, or -1 if failed */
int optreal_find(const char *token) {
  if (token[0] != '\0') {
    int i = 0;

    while(hts_optalias[i][0][0] != '\0') {
      if (strcmp(token, hts_optalias[i][1]) == 0) {
        return i;
      }
      i++;
    }
  }
  return -1;
}

const char *optreal_value(int p) {
  return hts_optalias[p][1];
}
const char *optalias_value(int p) {
  return hts_optalias[p][0];
}
const char *opttype_value(int p) {
  return hts_optalias[p][2];
}
const char *opthelp_value(int p) {
  return hts_optalias[p][3];
}

/* Help for option <token>, empty if not available, or NULL if unknown <token> */
const char *optalias_help(const char *token) {
  int pos = optalias_find(token);

  if (pos >= 0)
    return hts_optalias[pos][3];
  else
    return NULL;
}

/* Include a file to the current command line */
/* example:
  set sockets 8
  index on
  allow *.gif
  deny ad.*
*/
/* Note: NOT utf-8 */
int optinclude_file(const char *name, int *argc, char **argv, char *x_argvblk,
                    int *x_ptr) {
  FILE *fp;

  fp = fopen(name, "rb");
  if (fp) {
    char line[256];
    int insert_after = 1;       /* first, insert after program filename */

    while(!feof(fp)) {
      char *a, *b;
      int result;

      /* read line */
      linput(fp, line, 250);
      hts_lowcase(line);
      if (strnotempty(line)) {
        /* no comment line: # // ; */
        if (strchr("#/;", line[0]) == NULL) {
          /* right trim */
          a = line + strlen(line) - 1;
          while(is_realspace(*a))
            *(a--) = '\0';
          /* jump "set " and spaces */
          a = line;
          while(is_realspace(*a))
            a++;
          if (strncmp(a, "set", 3) == 0) {
            if (is_realspace(*(a + 3))) {
              a += 4;
            }
          }
          while(is_realspace(*a))
            a++;
          /* delete = ("sockets=8") */
          if ((b = strchr(a, '=')))
            *b = ' ';

          /* isolate option and parameter */
          b = a;
          while((!is_realspace(*b)) && (*b))
            b++;
          if (*b) {
            *b = '\0';
            b++;
          }
          /* a is now the option, b the parameter */

          {
            int return_argc;
            char return_error[256];
            char _tmp_argv[4][HTS_CDLMAXSIZE];
            char *tmp_argv[4];

            tmp_argv[0] = _tmp_argv[0];
            tmp_argv[1] = _tmp_argv[1];
            tmp_argv[2] = _tmp_argv[2];
            tmp_argv[3] = _tmp_argv[3];
            strcpybuff(_tmp_argv[0], "--");
            strcatbuff(_tmp_argv[0], a);
            strcpybuff(_tmp_argv[1], b);

            result =
              optalias_check(2, (const char *const *) tmp_argv, 0, &return_argc,
                             (tmp_argv + 2), return_error);
            if (!result) {
              printf("%s\n", return_error);
            } else {
              int insert_after_argc;

              /* Insert parameters BUT so that they can be in the same order */
              /* temporary argc: Number of parameters after minus insert_after_argc */
              insert_after_argc = (*argc) - insert_after;
              cmdl_ins((tmp_argv[2]), insert_after_argc, (argv + insert_after),
                       x_argvblk, (*x_ptr));
              *argc = insert_after_argc + insert_after;
              insert_after++;
              /* Second one */
              if (return_argc > 1) {
                insert_after_argc = (*argc) - insert_after;
                cmdl_ins((tmp_argv[3]), insert_after_argc,
                         (argv + insert_after), x_argvblk, (*x_ptr));
                *argc = insert_after_argc + insert_after;
                insert_after++;
              }
              /* increment to nbr of used parameters */
              /* insert_after+=result; */
            }
          }
        }

      }
    }
    fclose(fp);
    return 1;
  }
  return 0;
}

/* Get home directory, '.' if failed */
/* example: /home/smith */
const char *hts_gethome(void) {
  const char *home = getenv("HOME");

  if (home)
    return home;
  else
    return ".";
}

/* Convert ~/foo into /home/smith/foo */
void expand_home(String * str) {
  if (StringSub(*str, 1) == '~') {
    char BIGSTK tempo[HTS_URLMAXSIZE * 2];

    strcpybuff(tempo, hts_gethome());
    strcatbuff(tempo, StringBuff(*str) + 1);
    StringCopy(*str, tempo);
  }
}
