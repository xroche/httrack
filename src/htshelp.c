/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2017 Xavier Roche and other contributors

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
/* File: httrack.c subroutines:                                 */
/*       command-line help system                               */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htshelp.h"

/* specific definitions */
#include "htsbase.h"
#include "htscoremain.h"
#include "htscatchurl.h"
#include "htslib.h"
#include "htsalias.h"
#include "htsmodules.h"
#ifdef _WIN32
#else
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif
/* END specific definitions */

#define waitkey if (more) { char s[4]; printf("\nMORE.. q to quit\n"); linput(stdin,s,4); if (strcmp(s,"q")==0) quit=1; else printf("Page %d\n\n",++m); }
void infomsg(const char *msg) {
  int l = 0;
  int m = 0;
  int more = 0;
  int quit = 0;
  int done = 0;

  //
  if (msg == NULL)
    quit = 0;
  if (msg) {
    if (!quit) {
      if (strlen(msg) == 1) {
        if (msg[0] == '1') {
          more = 1;
          return;
        }
      }

      /* afficher alias? */
      if (((int) strlen(msg)) > 4) {
        if (msg[0] == ' ') {
          if (msg[2] != ' ') {
            if ((msg[3] == ' ') || (msg[4] == ' ')) {
              char cmd[32] = "-";
              int p = 0;

              while(cmd[p] == ' ')
                p++;
              sscanf(msg + p, "%s", cmd + strlen(cmd));
              /* clears cN -> c */
              if ((p = (int) strlen(cmd)) > 2)
                if (cmd[p - 1] == 'N')
                  cmd[p - 1] = '\0';
              /* finds alias (if any) */
              p = optreal_find(cmd);
              if (p >= 0) {
                /* fings type of parameter: number,param,param concatenated,single cmd */
                if (strcmp(opttype_value(p), "param") == 0)
                  printf("%s (--%s[=N])\n", msg, optalias_value(p));
                else if (strcmp(opttype_value(p), "param1") == 0)
                  printf("%s (--%s <param>)\n", msg, optalias_value(p));
                else if (strcmp(opttype_value(p), "param0") == 0)
                  printf("%s (--%s<param>)\n", msg, optalias_value(p));
                else
                  printf("%s (--%s)\n", msg, optalias_value(p));
                done = 1;
              }
            }
          }
        }
      }

      /* sinon */
      if (!done)
        printf("%s\n", msg);
      l++;
      if (l > 20) {
        l = 0;
        waitkey;
      }
    }
  }
}

typedef struct help_wizard_buffers {
  char urls[HTS_URLMAXSIZE * 2];
  char mainpath[256];
  char projname[256];
  char stropt[2048];        // options
  char stropt2[2048];       // options longues
  char strwild[2048];       // wildcards
  char cmd[4096];
  char str[256];
  char *argv[256];
} help_wizard_buffers;

void help_wizard(httrackp * opt) {
  help_wizard_buffers *buffers = malloct(sizeof(help_wizard_buffers));

#undef urls
#undef mainpath
#undef projname
#undef stropt
#undef stropt2
#undef strwild
#undef cmd
#undef str
#undef argv

#define urls (buffers->urls)
#define mainpath (buffers->mainpath)
#define projname (buffers->projname)
#define stropt (buffers->stropt)
#define stropt2 (buffers->stropt2)
#define strwild (buffers->strwild)
#define cmd (buffers->cmd)
#define str (buffers->str)
#define argv (buffers->argv)

  //char *urls = (char *) malloct(HTS_URLMAXSIZE * 2);
  //char *mainpath = (char *) malloct(256);
  //char *projname = (char *) malloct(256);
  //char *stropt = (char *) malloct(2048);        // options
  //char *stropt2 = (char *) malloct(2048);       // options longues
  //char *strwild = (char *) malloct(2048);       // wildcards
  //char *cmd = (char *) malloct(4096);
  //char *str = (char *) malloct(256);
  //char **argv = (char **) malloct(256 * sizeof(char *));

  //
  char *a;

  //
  if (urls == NULL || mainpath == NULL || projname == NULL || stropt == NULL
      || stropt2 == NULL || strwild == NULL || cmd == NULL || str == NULL
      || argv == NULL) {
    fprintf(stderr, "* memory exhausted in %s, line %d\n", __FILE__, __LINE__);
    return;
  }
  urls[0] = mainpath[0] = projname[0] = stropt[0] = stropt2[0] = strwild[0] =
    cmd[0] = str[0] = '\0';
  //
  strcpybuff(stropt, "-");
  mainpath[0] = projname[0] = stropt2[0] = strwild[0] = '\0';
  //

  printf("\n");
  printf("Welcome to HTTrack Website Copier (Offline Browser) " HTTRACK_VERSION
         "%s\n", hts_get_version_info(opt));
  printf("Copyright (C) 1998-2017 Xavier Roche and other contributors\n");
#ifdef _WIN32
  printf("Note: You are running the commandline version,\n");
  printf("run 'WinHTTrack.exe' to get the GUI version.\n");
#endif
#ifdef HTTRACK_AFF_WARNING
  printf("NOTE: " HTTRACK_AFF_WARNING "\n");
#endif
#ifdef HTS_PLATFORM_NAME
#if USE_BEGINTHREAD
  printf("[compiled: " HTS_PLATFORM_NAME " - MT]\n");
#else
  printf("[compiled: " HTS_PLATFORM_NAME "]\n");
#endif
#endif
  printf("To see the option list, enter a blank line or try httrack --help\n");
  //
  // Project name
  while(strnotempty(projname) == 0) {
    printf("\n");
    printf("Enter project name :");
    fflush(stdout);
    linput(stdin, projname, 250);
    if (strnotempty(projname) == 0)
      help("httrack", 1);
  }
  //
  // Path
  if (strnotempty(hts_gethome()))
    printf("\nBase path (return=%s/websites/) :", hts_gethome());
  else
    printf("\nBase path (return=current directory) :");
  linput(stdin, str, 250);
  if (!strnotempty(str)) {
    strcatbuff(str, hts_gethome());
    strcatbuff(str, "/websites/");
  }
  if (strnotempty(str))
    if ((str[strlen(str) - 1] != '/') && (str[strlen(str) - 1] != '\\'))
      strcatbuff(str, "/");
  strcatbuff(stropt2, "-O \"");
  strcatbuff(stropt2, str);
  strcatbuff(stropt2, projname);
  strcatbuff(stropt2, "\" ");
  // Créer si ce n'est fait un index.html 1er niveau
  make_empty_index(str);
  //
  printf("\n");
  printf("Enter URLs (separated by commas or blank spaces) :");
  fflush(stdout);
  linput(stdin, urls, 250);
  if (strnotempty(urls)) {
    while((a = strchr(urls, ',')))
      *a = ' ';
    while((a = strchr(urls, '\t')))
      *a = ' ';

    // Action
    printf("\nAction:\n");
    switch (help_query
            ("Mirror Web Site(s)|Mirror Web Site(s) with Wizard|Just Get Files Indicated|Mirror ALL links in URLs (Multiple Mirror)|Test Links In URLs (Bookmark Test)|Update/Continue a Mirror",
             1)) {
    case 1:
      break;
    case 2:
      strcatbuff(stropt, "W");
      break;
    case 3:
      strcatbuff(stropt2, "--get ");
      break;
    case 4:
      strcatbuff(stropt2, "--mirrorlinks ");
      break;
    case 5:
      strcatbuff(stropt2, "--testlinks ");
      break;
    case 6:
      strcatbuff(stropt2, "--update ");
      break;
    case 0:
      return;
      break;
    }

    // Proxy
    printf("\nProxy (return=none) :");
    linput(stdin, str, 250);
    if (strnotempty(str)) {
      while((a = strchr(str, ' ')))
        *a = ':';               // port
      if (!strchr(jump_identification_const(str), ':')) {
        char str2[256];

        printf("\nProxy port (return=8080) :");
        linput(stdin, str2, 250);
        strcatbuff(str, ":");
        if (strnotempty(str2) == 0)
          strcatbuff(str, "8080");
        else
          strcatbuff(str, str2);
      }
      strcatbuff(stropt2, "-P ");
      strcatbuff(stropt2, str);
      strcatbuff(stropt2, " ");
    }
    // Display
    strcatbuff(stropt2, " -%v ");

    // Wildcards
    printf
      ("\nYou can define wildcards, like: -*.gif +www.*.com/*.zip -*img_*.zip\n");
    printf("Wildcards (return=none) :");
    linput(stdin, strwild, 250);

    // Options
    do {
      printf
        ("\nYou can define additional options, such as recurse level (-r<number>), separated by blank spaces\n");
      printf("To see the option list, type help\n");
      printf("Additional options (return=none) :");
      linput(stdin, str, 250);
      if (strfield2(str, "help")) {
        help("httrack", 2);
      } else if (strnotempty(str)) {
        strcatbuff(stropt2, str);
        strcatbuff(stropt2, " ");
      }
    } while(strfield2(str, "help"));

    {
      int argc = 1;
      int g = 0;
      int i = 0;

      //
      printf("\n");
      if (strlen(stropt) == 1)
        stropt[0] = '\0';       // aucune
      sprintf(cmd, "%s %s %s %s", urls, stropt, stropt2, strwild);
      printf("---> Wizard command line: httrack %s\n\n", cmd);
      printf("Ready to launch the mirror? (Y/n) :");
      fflush(stdout);
      linput(stdin, str, 250);
      if (strnotempty(str)) {
        if (!((str[0] == 'y') || (str[0] == 'Y')))
          return;
      }
      printf("\n");

      // couper en morceaux
      argv[0] = strdup("winhttrack");
      argv[1] = cmd;
      argc++;
      while(cmd[i]) {
        if (cmd[i] == '\"')
          g = !g;
        if (cmd[i] == ' ') {
          if (!g) {
            cmd[i] = '\0';
            argv[argc++] = cmd + i + 1;
          }
        }
        i++;
      }
      hts_main(argc, argv);
    }
    //} else {
    //  help("httrack",1);
  }

  /* Free buffers */
  free(buffers);
#undef urls
#undef mainpath
#undef projname
#undef stropt
#undef stropt2
#undef strwild
#undef cmd
#undef str
#undef argv
}
int help_query(const char *list, int def) {
  char s[256];
  const char *a;
  int opt;
  int n = 1;

  a = list;
  while(strnotempty(a)) {
    const char *b = strchr(a, '|');

    if (b) {
      char str[256];

      str[0] = '\0';
      //
      strncatbuff(str, a, (int) (b - a));
      if (n == def)
        printf("(enter)\t%d\t%s\n", n++, str);
      else
        printf("\t%d\t%s\n", n++, str);
      a = b + 1;
    } else
      a = list + strlen(list);
  }
  printf("\t0\tQuit");
  do {
    printf("\n: ");
    fflush(stdout);
    linput(stdin, s, 250);
  } while((strnotempty(s) != 0) && (sscanf(s, "%d", &opt) != 1));
  if (strnotempty(s))
    return opt;
  else
    return def;
}

// Capture d'URL
void help_catchurl(const char *dest_path) {
  char BIGSTK adr_prox[HTS_URLMAXSIZE * 2];
  int port_prox;
  T_SOC soc = catch_url_init_std(&port_prox, adr_prox);

  if (soc != INVALID_SOCKET) {
    char BIGSTK url[HTS_URLMAXSIZE * 2];
    char method[32];
    char BIGSTK data[32768];

    url[0] = method[0] = data[0] = '\0';
    //
    printf
      ("Okay, temporary proxy installed.\nSet your browser's preferences to:\n\n");
    printf("\tProxy's address: \t%s\n\tProxy's port: \t%d\n", adr_prox,
           port_prox);
    //
    if (catch_url(soc, url, method, data)) {
      char BIGSTK dest[HTS_URLMAXSIZE * 2];
      int i = 0;

      do {
        sprintf(dest, "%s%s%d", dest_path, "hts-post", i);
        i++;
      } while(fexist(dest));
      {
        FILE *fp = fopen(dest, "wb");

        if (fp) {
          fwrite(data, strlen(data), 1, fp);
          fclose(fp);
        }
      }
      // former URL!
      {
        char BIGSTK finalurl[HTS_URLMAXSIZE * 2];

        inplace_escape_check_url(dest, sizeof(dest));
        sprintf(finalurl, "%s" POSTTOK "file:%s", url, dest);
        printf("\nThe URL is: \"%s\"\n", finalurl);
        printf("You can capture it through: httrack \"%s\"\n", finalurl);
      }
    } else
      printf("Unable to analyse the URL\n");
#ifdef _WIN32
    closesocket(soc);
#else
    close(soc);
#endif
  } else
    printf("Unable to create a temporary proxy (no remaining port)\n");
}

// Créer un index.html vide
void make_empty_index(const char *str) {
#if 0
  if (!fexist(fconcat(str, "index.html"))) {
    FILE *fp = fopen(fconcat(str, "index.html"), "wb");

    if (fp) {
      fprintf(fp, "<!-- " HTS_TOPINDEX " -->" CRLF);
      fprintf(fp,
              "<HTML><BODY>Index is empty!<BR>(File used to index all HTTrack projects)</BODY></HTML>"
              CRLF);
      fclose(fp);
    }
  }
#endif
}

// mini-aide  (h: help)
//           y
void help(const char *app, int more) {
  char info[2048];

  infomsg("");
  if (more)
    infomsg("1");
  if (more != 2) {
    sprintf(info,
            "HTTrack version " HTTRACK_VERSION "%s",
            hts_is_available());
    infomsg(info);
#ifdef HTTRACK_AFF_WARNING
    infomsg("NOTE: " HTTRACK_AFF_WARNING);
#endif
    sprintf(info,
            "\tusage: %s <URLs> [-option] [+<URL_FILTER>] [-<URL_FILTER>] [+<mime:MIME_FILTER>] [-<mime:MIME_FILTER>]",
            app);
    infomsg(info);
    infomsg("\twith options listed below: (* is the default value)");
    infomsg("");
  }
  infomsg("General options:");
  infomsg
    ("  O  path for mirror/logfiles+cache (-O path_mirror[,path_cache_and_logfiles])");
  infomsg("");
  infomsg("Action options:");
  infomsg("  w *mirror web sites");
  infomsg("  W  mirror web sites, semi-automatic (asks questions)");
  infomsg("  g  just get files (saved in the current directory)");
  infomsg("  i  continue an interrupted mirror using the cache");
  infomsg
    ("  Y   mirror ALL links located in the first level pages (mirror links)");
  infomsg("");
  infomsg("Proxy options:");
  infomsg("  P  proxy use (-P proxy:port or -P user:pass@proxy:port)");
  infomsg(" %f *use proxy for ftp (f0 don't use)");
  infomsg(" %b  use this local hostname to make/send requests (-%b hostname)");
  infomsg("");
  infomsg("Limits options:");
  infomsg("  rN set the mirror depth to N (* r9999)");
  infomsg(" %eN set the external links depth to N (* %e0)");
  infomsg("  mN maximum file length for a non-html file");
  infomsg("  mN,N2 maximum file length for non html (N) and html (N2)");
  infomsg("  MN maximum overall size that can be uploaded/scanned");
  infomsg("  EN maximum mirror time in seconds (60=1 minute, 3600=1 hour)");
  infomsg("  AN maximum transfer rate in bytes/seconds (1000=1KB/s max)");
  infomsg(" %cN maximum number of connections/seconds (*%c10)");
  infomsg
    ("  GN pause transfer if N bytes reached, and wait until lock file is deleted");
  infomsg("");
  infomsg("Flow control:");
  infomsg("  cN number of multiple connections (*c8)");
  infomsg
    ("  TN timeout, number of seconds after a non-responding link is shutdown");
  infomsg
    ("  RN number of retries, in case of timeout or non-fatal errors (*R1)");
  infomsg
    ("  JN traffic jam control, minimum transfert rate (bytes/seconds) tolerated for a link");
  infomsg
    ("  HN host is abandonned if: 0=never, 1=timeout, 2=slow, 3=timeout or slow");
  infomsg("");
  infomsg("Links options:");
  infomsg
    (" %P *extended parsing, attempt to parse all links, even in unknown tags or Javascript (%P0 don't use)");
  infomsg
    ("  n  get non-html files 'near' an html file (ex: an image located outside)");
  infomsg("  t  test all URLs (even forbidden ones)");
  infomsg
    (" %L <file> add all URL located in this text file (one URL per line)");
  infomsg
    (" %S <file> add all scan rules located in this text file (one scan rule per line)");
  infomsg("");
  infomsg("Build options:");
  infomsg("  NN structure type (0 *original structure, 1+: see below)");
  infomsg("     or user defined structure (-N \"%h%p/%n%q.%t\")");
  infomsg
    (" %N  delayed type check, don't make any link test but wait for files download to start instead (experimental) (%N0 don't use, %N1 use for unknown extensions, * %N2 always use)");
  infomsg
    (" %D  cached delayed type check, don't wait for remote type during updates, to speedup them (%D0 wait, * %D1 don't wait)");
  infomsg(" %M  generate a RFC MIME-encapsulated full-archive (.mht)");
  infomsg
    ("  LN long names (L1 *long names / L0 8-3 conversion / L2 ISO9660 compatible)");
  infomsg
    ("  KN keep original links (e.g. http://www.adr/link) (K0 *relative link, K absolute links, K4 original links, K3 absolute URI links, K5 transparent proxy link)");
  infomsg("  x  replace external html links by error pages");
  infomsg
    (" %x  do not include any password for external password protected websites (%x0 include)");
  infomsg
    (" %q *include query string for local files (useless, for information purpose only) (%q0 don't include)");
  infomsg
    ("  o *generate output html file in case of error (404..) (o0 don't generate)");
  infomsg("  X *purge old files after update (X0 keep delete)");
  infomsg(" %p  preserve html files 'as is' (identical to '-K4 -%F \"\"')");
  infomsg(" %T  links conversion to UTF-8");
  infomsg("");
  infomsg("Spider options:");
  infomsg("  bN accept cookies in cookies.txt (0=do not accept,* 1=accept)");
  infomsg
    ("  u  check document type if unknown (cgi,asp..) (u0 don't check, * u1 check but /, u2 check always)");
  infomsg
    ("  j *parse Java Classes (j0 don't parse, bitmask: |1 parse default, |2 don't parse .class |4 don't parse .js |8 don't be aggressive)");
  infomsg
    ("  sN follow robots.txt and meta robots tags (0=never,1=sometimes,* 2=always, 3=always (even strict rules))");
  infomsg
    (" %h  force HTTP/1.0 requests (reduce update features, only for old servers or proxies)");
  infomsg
    (" %k  use keep-alive if possible, greately reducing latency for small files and test requests (%k0 don't use)");
  infomsg
    (" %B  tolerant requests (accept bogus responses on some servers, but not standard!)");
  infomsg
    (" %s  update hacks: various hacks to limit re-transfers when updating (identical size, bogus response..)");
  infomsg
    (" %u  url hacks: various hacks to limit duplicate URLs (strip //, www.foo.com==foo.com..)");
  infomsg
    (" %A  assume that a type (cgi,asp..) is always linked with a mime type (-%A php3,cgi=text/html;dat,bin=application/x-zip)");
  infomsg("     shortcut: '--assume standard' is equivalent to -%A "
          HTS_ASSUME_STANDARD);
  infomsg
    ("     can also be used to force a specific file type: --assume foo.cgi=text/html");
  infomsg
    (" @iN internet protocol (0=both ipv6+ipv4, 4=ipv4 only, 6=ipv6 only)");
  infomsg
    (" %w  disable a specific external mime module (-%w htsswf -%w htsjava)");
  infomsg("");
  infomsg("Browser ID:");
  infomsg
    ("  F  user-agent field sent in HTTP headers (-F \"user-agent name\")");
  infomsg(" %R  default referer field sent in HTTP headers");
  infomsg(" %E  from email address sent in HTTP headers");
  infomsg
    (" %F  footer string in Html code (-%F \"Mirrored [from host %s [file %s [at %s]]]\"");
  infomsg(" %l  preffered language (-%l \"fr, en, jp, *\"");
  infomsg(" %a  accepted formats (-%a \"text/html,image/png;q=0.9,*/*;q=0.1\"");
  infomsg(" %X  additional HTTP header line (-%X \"X-Magic: 42\"");
  infomsg("");
  infomsg("Log, index, cache");
  infomsg
    ("  C  create/use a cache for updates and retries (C0 no cache,C1 cache is prioritary,* C2 test update before)");
  infomsg("  k  store all files in cache (not useful if files on disk)");
  infomsg(" %n  do not re-download locally erased files");
  infomsg
    (" %v  display on screen filenames downloaded (in realtime) - * %v1 short version - %v2 full animation");
  infomsg("  Q  no log - quiet mode");
  infomsg("  q  no questions - quiet mode");
  infomsg("  z  log - extra infos");
  infomsg("  Z  log - debug");
  infomsg("  v  log on screen");
  infomsg("  f *log in files");
  infomsg("  f2 one single log file");
  infomsg("  I *make an index (I0 don't make)");
  infomsg(" %i  make a top index for a project folder (* %i0 don't make)");
  infomsg(" %I  make an searchable index for this mirror (* %I0 don't make)");
  infomsg("");
  infomsg("Expert options:");
  infomsg("  pN priority mode: (* p3)");
  infomsg("      p0 just scan, don't save anything (for checking links)");
  infomsg("      p1 save only html files");
  infomsg("      p2 save only non html files");
  infomsg("     *p3 save all files");
  infomsg("      p7 get html files before, then treat other files");
  infomsg("  S  stay on the same directory");
  infomsg("  D *can only go down into subdirs");
  infomsg("  U  can only go to upper directories");
  infomsg("  B  can both go up&down into the directory structure");
  infomsg("  a *stay on the same address");
  infomsg("  d  stay on the same principal domain");
  infomsg("  l  stay on the same TLD (eg: .com)");
  infomsg("  e  go everywhere on the web");
  infomsg(" %H  debug HTTP headers in logfile");
  infomsg("");
  infomsg("Guru options: (do NOT use if possible)");
  infomsg(" #X *use optimized engine (limited memory boundary checks)");
  infomsg(" #0  filter test (-#0 '*.gif' 'www.bar.com/foo.gif')");
  infomsg(" #1  simplify test (-#1 ./foo/bar/../foobar)");
  infomsg(" #2  type test (-#2 /foo/bar.php)");
  infomsg(" #C  cache list (-#C '*.com/spider*.gif'");
  infomsg(" #R  cache repair (damaged cache)");
  infomsg(" #d  debug parser");
  infomsg(" #E  extract new.zip cache meta-data in meta.zip");
  infomsg(" #f  always flush log files");
  infomsg(" #FN maximum number of filters");
  infomsg(" #h  version info");
  infomsg(" #K  scan stdin (debug)");
  infomsg(" #L  maximum number of links (-#L1000000)");
  infomsg(" #p  display ugly progress information");
  infomsg(" #P  catch URL");
  infomsg(" #R  old FTP routines (debug)");
  infomsg(" #T  generate transfer ops. log every minutes");
  infomsg(" #u  wait time");
  infomsg(" #Z  generate transfer rate statictics every minutes");
  infomsg("");
  infomsg
    ("Dangerous options: (do NOT use unless you exactly know what you are doing)");
  infomsg
    (" %!  bypass built-in security limits aimed to avoid bandwidth abuses (bandwidth, simultaneous connections)");
  infomsg("     IMPORTANT NOTE: DANGEROUS OPTION, ONLY SUITABLE FOR EXPERTS");
  infomsg("                     USE IT WITH EXTREME CARE");
  infomsg("");
  infomsg("Command-line specific options:");
  infomsg
    ("  V execute system command after each files ($0 is the filename: -V \"rm \\$0\")");
  infomsg
    (" %W use an external library function as a wrapper (-%W myfoo.so[,myparameters])");
  /* infomsg(" %O do a chroot before setuid"); */
  infomsg("");
  infomsg("Details: Option N");
  infomsg("  N0 Site-structure (default)");
  infomsg("  N1 HTML in web/, images/other files in web/images/");
  infomsg("  N2 HTML in web/HTML, images/other in web/images");
  infomsg("  N3 HTML in web/,  images/other in web/");
  infomsg
    ("  N4 HTML in web/, images/other in web/xxx, where xxx is the file extension (all gif will be placed onto web/gif, for example)");
  infomsg("  N5 Images/other in web/xxx and HTML in web/HTML");
  infomsg("  N99 All files in web/, with random names (gadget !)");
  infomsg("  N100 Site-structure, without www.domain.xxx/");
  infomsg
    ("  N101 Identical to N1 exept that \"web\" is replaced by the site's name");
  infomsg
    ("  N102 Identical to N2 exept that \"web\" is replaced by the site's name");
  infomsg
    ("  N103 Identical to N3 exept that \"web\" is replaced by the site's name");
  infomsg
    ("  N104 Identical to N4 exept that \"web\" is replaced by the site's name");
  infomsg
    ("  N105 Identical to N5 exept that \"web\" is replaced by the site's name");
  infomsg
    ("  N199 Identical to N99 exept that \"web\" is replaced by the site's name");
  infomsg("  N1001 Identical to N1 exept that there is no \"web\" directory");
  infomsg("  N1002 Identical to N2 exept that there is no \"web\" directory");
  infomsg
    ("  N1003 Identical to N3 exept that there is no \"web\" directory (option set for g option)");
  infomsg("  N1004 Identical to N4 exept that there is no \"web\" directory");
  infomsg("  N1005 Identical to N5 exept that there is no \"web\" directory");
  infomsg("  N1099 Identical to N99 exept that there is no \"web\" directory");
  infomsg("Details: User-defined option N");
  infomsg("  '%n' Name of file without file type (ex: image)");
  infomsg("  '%N' Name of file, including file type (ex: image.gif)");
  infomsg("  '%t' File type (ex: gif)");
  infomsg("  '%p' Path [without ending /] (ex: /someimages)");
  infomsg("  '%h' Host name (ex: www.someweb.com)");
  infomsg("  '%M' URL MD5 (128 bits, 32 ascii bytes)");
  infomsg("  '%Q' query string MD5 (128 bits, 32 ascii bytes)");
  infomsg("  '%k' full query string");
  infomsg("  '%r' protocol name (ex: http)");
  infomsg("  '%q' small query string MD5 (16 bits, 4 ascii bytes)");
  infomsg("     '%s?' Short name version (ex: %sN)");
  infomsg("  '%[param]' param variable in query string");
  infomsg
    ("  '%[param:before:after:empty:notfound]' advanced variable extraction");
  infomsg("Details: User-defined option N and advanced variable extraction");
  infomsg("   %[param:before:after:empty:notfound]");
  infomsg("   param : parameter name");
  infomsg("   before : string to prepend if the parameter was found");
  infomsg("   after : string to append if the parameter was found");
  infomsg
    ("   notfound : string replacement if the parameter could not be found");
  infomsg("   empty : string replacement if the parameter was empty");
  infomsg
    ("   all fields, except the first one (the parameter name), can be empty");
  infomsg("");
  infomsg("Details: Option K");
  infomsg("  K0  foo.cgi?q=45  ->  foo4B54.html?q=45 (relative URI, default)");
  infomsg
    ("  K                 ->  http://www.foobar.com/folder/foo.cgi?q=45 (absolute URL)");
  infomsg("  K3                ->  /folder/foo.cgi?q=45 (absolute URI)");
  infomsg("  K4                ->  foo.cgi?q=45 (original URL)");
  infomsg
    ("  K5                ->  http://www.foobar.com/folder/foo4B54.html?q=45 (transparent proxy URL)");
  infomsg("");
  infomsg("Shortcuts:");
  infomsg("--mirror      <URLs> *make a mirror of site(s) (default)");
  infomsg
    ("--get         <URLs>  get the files indicated, do not seek other URLs (-qg)");
  infomsg("--list   <text file>  add all URL located in this text file (-%L)");
  infomsg("--mirrorlinks <URLs>  mirror all links in 1st level pages (-Y)");
  infomsg("--testlinks   <URLs>  test links in pages (-r1p0C0I0t)");
  infomsg
    ("--spider      <URLs>  spider site(s), to test links: reports Errors & Warnings (-p0C0I0t)");
  infomsg("--testsite    <URLs>  identical to --spider");
  infomsg
    ("--skeleton    <URLs>  make a mirror, but gets only html files (-p1)");
  infomsg("--update              update a mirror, without confirmation (-iC2)");
  infomsg
    ("--continue            continue a mirror, without confirmation (-iC1)");
  infomsg("");
  infomsg
    ("--catchurl            create a temporary proxy to capture an URL or a form post URL");
  infomsg("--clean               erase cache & log files");
  infomsg("");
  infomsg("--http10              force http/1.0 requests (-%h)");
  infomsg("");
  infomsg("Details: Option %W: External callbacks prototypes");
  infomsg("see htsdefines.h");
  infomsg("");
  infomsg("example: httrack www.someweb.com/bob/");
  infomsg("means:   mirror site www.someweb.com/bob/ and only this site");
  infomsg("");
  infomsg
    ("example: httrack www.someweb.com/bob/ www.anothertest.com/mike/ +*.com/*.jpg -mime:application/*");
  infomsg
    ("means:   mirror the two sites together (with shared links) and accept any .jpg files on .com sites");
  infomsg("");
  infomsg("example: httrack www.someweb.com/bob/bobby.html +* -r6");
  infomsg
    ("means get all files starting from bobby.html, with 6 link-depth, and possibility of going everywhere on the web");
  infomsg("");
  infomsg
    ("example: httrack www.someweb.com/bob/bobby.html --spider -P proxy.myhost.com:8080");
  infomsg("runs the spider on www.someweb.com/bob/bobby.html using a proxy");
  infomsg("");
  infomsg("example: httrack --update");
  infomsg("updates a mirror in the current folder");
  infomsg("");
  infomsg("example: httrack");
  infomsg("will bring you to the interactive mode");
  infomsg("");
  infomsg("example: httrack --continue");
  infomsg("continues a mirror in the current folder");
  infomsg("");
  sprintf(info, "HTTrack version " HTTRACK_VERSION "%s",
          hts_is_available());
  infomsg(info);
  infomsg("Copyright (C) 1998-2017 Xavier Roche and other contributors");
#ifdef HTS_PLATFORM_NAME
  infomsg("[compiled: " HTS_PLATFORM_NAME "]");
#endif
  infomsg(NULL);

//  infomsg("  R  *relative links (e.g ../link)\n");
//  infomsg("  A   absolute links (e.g /www.adr/link)\n");
}
