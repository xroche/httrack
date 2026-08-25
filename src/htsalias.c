/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998 Xavier Roche and other contributors

SPDX-License-Identifier: GPL-3.0-or-later

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

Ethical use: we kindly ask that you NOT use this software to harvest email
addresses or to collect any other private information about people. Doing so
would dishonor our work and waste the many hours we have spent on it.

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

#include <limits.h>

#define _NOT_NULL(a) ( (a!=NULL) ? (a) : "" )

/*
  Aliases for command-line and config file definitions
  These definitions can be used:
  in command line:
  --sockets=8       --cache=0
  --sockets 8       --cache off
                    --nocache
  -c8               -C0
  --wide-mirror     --tiny-mirror     (--mirror at --wide/--tiny's count)
  in config file:
  sockets=8         cache=0
  set sockets 8     cache off

*/
/* clang-format off: hand-aligned table; clang-format reflows the whole
   initializer (2->4 space) and the class list on any edit. */
/* clang-format off */
/*
  single : no options, and a value (--index=0, --noindex) is refused
  onoff  : optional 0/1 value, whose short form takes a 0 suffix (-I0)
  level  : optional numeric value, which the short form parses (-%v2)
  param  : this option allows a number parameter (1, for example) and can be mixed with other options (R1C1c8)
  param1 : this option must be alone, and needs one distinct parameter (-P <path>)
  param0 : this option must be alone, but the parameter should be put together (+*.gif)
  paramn : glues like param what -N reads glued (1, 1L0, on/off), detaches a %-carrying template, refuses the rest

  A name may appear twice; the FIRST row wins, for the expansion and the help
  text. Later rows exist so a reverse lookup by short option finds a name, and
  that lookup takes the first row too, so -N's class is "structure"'s.
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
  {"httpproxy-ftp", "-%f", "onoff", ""},
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
  {"extended-parsing", "-%P", "onoff", ""},
  {"near", "-n", "single", ""},
  {"delayed-type-check", "-%N", "level", ""},
  {"cached-delayed-type-check", "-%D", "onoff", ""},
  {"delayed-type-check-always", "-%N2", "single", ""},
  {"disable-security-limits", "-%!", "onoff", ""},
  {"test", "-t", "single", ""},
  {"list", "-%L", "param1", ""},
  {"urllist", "-%S", "param1", ""},
  {"language", "-%l", "param1", ""}, {"lang", "-%l", "param1", ""},
  {"accept", "-%a", "param1", ""},
  {"headers", "-%X", "param1", ""},
  {"structure", "-N", "paramn", ""},
  {"user-structure", "-N", "param1", ""},
  {"long-names", "-L", "param", ""},
  {"keep-links", "-K", "param", ""},
  {"mime-html", "-%M", "onoff", ""}, {"mht", "-%M", "onoff", ""},
  {"replace-external", "-x", "single", ""},
  {"disable-passwords", "-%x", "onoff", ""}, {"disable-password", "-%x",
                                               "onoff", ""},
  {"include-query-string", "-%q", "onoff", ""},
  {"strip-query", "-%g", "param1",
   "strip [host/pattern=]key1,key2,... from URLs"},
  {"host-alias", "-%C", "param1",
   "fold other hostnames of one site onto it "
   "([scheme://]alias[,...]=[scheme://]host)"},
  {"cookies-file", "-%K", "param1",
   "load extra cookies from a Netscape cookies.txt"},
  {"changes", "-%d", "onoff",
   "write hts-changes.json: what this crawl changed vs. the previous mirror"},
  {"sitemap", "-%m", "single",
   "seed the crawl from the start host's sitemap (robots.txt, then "
   "/sitemap.xml)"},
  {"sitemap-url", "-%mu", "param1", "seed the crawl from this sitemap URL"},
  {"warc", "-%r", "single", "write an ISO-28500 WARC/1.1 archive of the crawl"},
  {"warc-file", "-%rf", "param1", "write a WARC archive to the given base name"},
  {"warc-max-size", "-%rs", "param1",
   "rotate the WARC archive once a segment passes N bytes (0: single file)"},
  {"warc-cdx", "-%rc", "single",
   "write a sorted CDXJ index next to the WARC archive"},
  {"warc-cdxj", "-%rc", "single", ""},
  {"wacz", "-%rz", "single",
   "package the WARC archive, CDXJ index and pages as a WACZ file"},
  {"single-file", "-%Z", "onoff",
   "after the mirror, inline each page's assets as data: URIs"},
  {"single-file-max-size", "-%Zs", "param1",
   "per-asset cap for --single-file, in bytes (implies it; default 10485760)"},
  {"why", "-%Y", "param1",
   "explain which filter rule accepts or rejects a URL, then exit"},
  {"pause", "-%G", "param1",
   "random pause of MIN[:MAX] seconds between files"},
  {"generate-errors", "-o", "level", ""},
  {"do-not-generate-errors", "-o0", "single", ""},
  {"purge-old", "-X", "onoff", ""},
  {"cookies", "-b", "param", ""},
  {"check-type", "-u", "param", ""},
  {"assume", "-%A", "param1", ""}, {"mimetype", "-%A", "param1", ""},
  {"parse-java", "-j", "param", ""},
  {"protocol", "-@i", "param", ""},
  {"robots", "-s", "param", ""},
  {"http-10", "-%h", "onoff", ""}, {"http-1.0", "-%h", "onoff", ""},
  {"keep-alive", "-%k", "onoff", ""},
  {"build-top-index", "-%i", "onoff", ""},
  {"disable-compression", "-%z", "onoff", ""},
  {"tolerant", "-%B", "onoff", ""},
  {"updatehack", "-%s", "onoff", ""}, {"sizehack", "-%s", "onoff", ""},
  {"urlhack", "-%u", "onoff", ""},
  {"keep-www-prefix", "-%j", "onoff", ""},
  {"keep-double-slashes", "-%o", "onoff", ""},
  {"keep-query-order", "-%y", "onoff", ""},
  {"user-agent", "-F", "param1", "user-agent identity"},
  {"referer", "-%R", "param1", "default referer URL"},
  {"from", "-%E", "param1", "from email address"},
  {"footer", "-%F", "param1", ""},
  {"cache", "-C", "param", "number of retries for non-fatal errors"},
  {"store-all-in-cache", "-k", "single", ""},
  {"do-not-recatch", "-%n", "onoff", ""},
  {"do-not-log", "-Q", "single", ""},
  {"extra-log", "-z", "single", ""},
  {"debug-log", "-Z", "single", ""},
  {"verbose", "-v", "single", ""},
  {"file-log", "-f", "single", ""},
  {"single-log", "-f2", "single", ""},
  {"index", "-I", "onoff", ""},
  {"search-index", "-%I", "level", ""},
  {"priority", "-p", "param", ""},
  {"debug-headers", "-%H", "single", ""},
  {"userdef-cmd", "-V", "param1", ""},
  {"callback", "-%W", "param1", "plug an external callback"}, {"wrapper", "-%W",
                                                               "param1",
                                                               "plug an external callback"},
  {"usercommand", "-V", "param1", "user-defined command"},
  {"display", "-%v", "level",
   "show files transferred and other funny realtime information"},
  {"dos83", "-L0", "single", ""},
  {"iso9660", "-L2", "single", ""},
  {"disable-module", "-%w", "param1", ""},
  {"no-background-on-suspend", "-y0", "single", ""},
  {"background-on-suspend", "-y", "onoff", ""},
  {"utf8-conversion", "-%T", "onoff", ""},
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
  {"advanced-wait", "-#u", "level", ""},
  {"debug-ratestats", "-#Z", "single", ""},
  {"fast-engine", "-#X", "onoff", "Enable fast routines"},
  {"debug-overflows", "-#X0", "single", "Attempt to detect buffer overflows"},
  {"debug-cache", "-#C", "param1", "List files in the cache"},
  {"extract-cache", "-#C", "level", "Extract meta-data"},
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
  {"http10", "-%h", "onoff", ""},
  {"filelist", "-%L", "param1", ""},
  {"filterlist", "-%S", "param1", ""},
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
/* clang-format on */

/* Whether TOKEN is an option name, rather than a value that begins with '-'.
   Only the spellings optalias_check() resolves: a cluster (-c8) is a value. */
static hts_boolean optreal_or_alias(const char *token) {
  char name[64];
  const char *eq;
  size_t len;

  if (optreal_find(token) >= 0)
    return HTS_TRUE;
  if (token[0] != '-' || token[1] != '-')
    return HTS_FALSE;
  eq = strchr(token + 2, '=');
  len = eq != NULL ? (size_t) (eq - (token + 2)) : strlen(token + 2);
  if (len == 0 || len >= sizeof(name))
    return HTS_FALSE;
  memcpy(name, token + 2, len);
  name[len] = '\0';
  if (optalias_find(name) >= 0)
    return HTS_TRUE;
  if (strncmp(name, "no", 2) == 0 && optalias_find(name + 2) >= 0)
    return HTS_TRUE;
  return (strncmp(name, "wide-", 5) == 0 || strncmp(name, "tiny-", 5) == 0) &&
         optalias_find(name + 5) >= 0;
}

/* Whether the real option OPT at ARGV[N_ARG] lacks the separate parameter it
   needs. A strip-query key or a host-alias pattern may begin with '-' (#1179),
   so those two take any following token that is not an option name; write
   --strip-query=-q to hand them one that is. */
static hts_boolean optparam_missing(int argc, const char *const *argv,
                                    int n_arg, const char *opt) {
  /* keep in sync with the strip-query and host-alias cases in htscoremain.c */
  static const char *const rule_opt[] = {"-%g", "-%C", NULL};
  int i;

  if (n_arg + 1 >= argc)
    return HTS_TRUE;
  if (argv[n_arg + 1][0] != '-')
    return HTS_FALSE;
  for (i = 0; rule_opt[i] != NULL; i++) {
    if (strcmp(opt, rule_opt[i]) == 0)
      return optreal_or_alias(argv[n_arg + 1]);
  }
  return HTS_TRUE;
}

/* The short form the --wide-/--tiny- prefix glues onto the alias it prefixes,
   read from the --wide/--tiny row itself so the two cannot drift ("c32"). */
static const char *optalias_prefix_count(const char *name) {
  const int pos = optalias_find(name);

  return pos >= 0 && hts_optalias[pos][1][0] == '-' ? hts_optalias[pos][1] + 1
                                                    : "";
}

/* What a --wide-/--tiny- prefix does where the count cannot be glued on.
   0: apply the alias without the count and warn. 1: refuse the option. */
#define OPTALIAS_PREFIX_STRICT 0

/* Whether the alias expands to a plain cluster a count can be glued onto
   (-w -> -wc32). The rest is refused rather than glued blind: a value sharing
   the word (-N <template>, -c8) or holding it alone (-O <path>), a long form,
   the -% and -# families (-%r plus a c is the -%rc of --warc-cdx), a cluster
   already carrying -c, and -h, which the caller matches as a whole word. */
static hts_boolean optalias_clusters(const char *type, const char *command) {
  size_t i;

  if (strcmp(type, "single") != 0 && strcmp(type, "onoff") != 0 &&
      strcmp(type, "level") != 0)
    return HTS_FALSE;
  if (command[0] != '-' || command[1] == '\0' || strcmp(command, "-h") == 0)
    return HTS_FALSE;
  for (i = 1; command[i] != '\0'; i++) {
    if (command[i] == 'c' || !isalnum((unsigned char) command[i]))
      return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* Suffix the short form takes for a value ("0", "2", or none), or NULL when
   the class refuses it: onoff reads 0/1 only, level reads a number. */
static const char *optalias_suffix(const char *type, const char *value) {
  const hts_boolean level = strcmp(type, "level") == 0 ? HTS_TRUE : HTS_FALSE;

  if (!level && strcmp(type, "onoff") != 0)
    return NULL;
  if (level && isdigit((unsigned char) value[0])) {
    size_t i;

    /* bounded: an overlong digit run is refused, not appended */
    for (i = 0; i < 16 && value[i] != '\0'; i++) {
      if (!isdigit((unsigned char) value[i]))
        return NULL;
    }
    return value[i] == '\0' ? value : NULL;
  }
  if (strcmp(value, "0") == 0 || strcmp(value, "off") == 0)
    return "0";
  if (strcmp(value, "1") == 0 || strcmp(value, "on") == 0)
    return ""; /* enabling is what the bare flag already means */
  return NULL;
}

/* A bare digit run short enough for -N to read it as a preset number. */
static hts_boolean optalias_is_digits(const char *value) {
  size_t i;

  for (i = 0; value[i] != '\0'; i++) {
    if (!isdigit((unsigned char) value[i]))
      return HTS_FALSE;
  }
  return i != 0 && i <= HTS_SAVENAME_PRESET_MAX_DIGITS ? HTS_TRUE : HTS_FALSE;
}

/* Whether a "paramn" value glues onto the short form the way "param" does: a
   preset, on/off, or a preset trailed by more short options (-N1L0). A path
   separator or an extension dot marks a user template instead, which no option
   cluster carries and which only reaches the engine detached (#1380). */
static hts_boolean optalias_paramn_glues(const char *value) {
  size_t i;

  if (strcmp(value, "on") == 0 || strcmp(value, "off") == 0)
    return HTS_TRUE;
  for (i = 0; isdigit((unsigned char) value[i]); i++) {
  }
  if (i == 0 || i > HTS_SAVENAME_PRESET_MAX_DIGITS)
    return HTS_FALSE;
  return strchr(value + i, '/') == NULL && strchr(value + i, '.') == NULL
             ? HTS_TRUE
             : HTS_FALSE;
}

/*
  Check for alias in command-line
  argc,argv     as in main()
  n_arg         argument position
  return_argv   a char[2][] where to put result
  return_error  the syntax error, or a warning the caller shows and carries on

  return value: number of arguments treated (0 if error)
*/
int optalias_check(int argc, const char *const *argv, int n_arg,
                   int *return_argc, char **return_argv,
                   size_t return_argv_size, char *return_error,
                   size_t return_error_size) {
  return_error[0] = '\0';
  *return_argc = 1;
  if (argv[n_arg][0] == '-')
    if (argv[n_arg][1] == '-') {
      /* sized to HTS_CDLMAXSIZE: a long-form option value (--user-agent,
         --headers, ...) is copied into param, and the value is bounded by the
         general per-argument check in htscoremain.c (HTS_CDLMAXSIZE) */
      char command[HTS_CDLMAXSIZE];
      char param[HTS_CDLMAXSIZE];
      char addcommand[256];

      /* */
      char *position;
      const char *addname = NULL;
      int need_param = 1;
      hts_boolean negated = HTS_FALSE;

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
      /* --nocache, unless the whole name is an alias (--no-utf8-conversion) */
      else if (strncmp(argv[n_arg] + 2, "no", 2) == 0 &&
               optalias_find(argv[n_arg] + 2) < 0) {
        strcpybuff(command, argv[n_arg] + 4);
        strcpybuff(param, "0");
        negated = HTS_TRUE;
      }
      /* --sockets 8 */
      else {
        /* --wide-mirror is --mirror carrying --wide's connection count */
        if (strncmp(argv[n_arg] + 2, "wide-", 5) == 0)
          addname = "wide";
        else if (strncmp(argv[n_arg] + 2, "tiny-", 5) == 0)
          addname = "tiny";
        if (addname != NULL) {
          strlcpybuff(addcommand, optalias_prefix_count(addname),
                      sizeof(addcommand));
          strcpybuff(command, argv[n_arg] + 7);
        } else
          strcpybuff(command, argv[n_arg] + 2);
        need_param = 2;
      }

      /* Now solve the alias */
      pos = optalias_find(command);
      if (pos >= 0) {
        /* Copy real name */
        strcpybuff(command, hts_optalias[pos][1]);
        /* Say so where the expansion cannot carry the prefix, rather than
           drop the count in silence */
        if (addcommand[0] != '\0' &&
            !optalias_clusters(hts_optalias[pos][2], command)) {
#if OPTALIAS_PREFIX_STRICT
          slprintfbuff_clip(return_error, return_error_size,
                            "Syntax error:\n\tThe %s- prefix does not apply to "
                            "--%s: write --%s --%s instead\n",
                            addname, hts_optalias[pos][0], addname,
                            hts_optalias[pos][0]);
          return 0;
#else
          slprintfbuff_clip(return_error, return_error_size,
                            "Warning: the %s- prefix cannot add its connection "
                            "count to --%s, so --%s runs without it; write "
                            "--%s --%s instead",
                            addname, hts_optalias[pos][0], hts_optalias[pos][0],
                            addname, hts_optalias[pos][0]);
          addcommand[0] = '\0';
#endif
        }
        /* With parameters? */
        if (strncmp(hts_optalias[pos][2], "param", 5) == 0) {
          /* Copy parameters? */
          if (need_param == 2) {
            if (optparam_missing(argc, argv, n_arg, command)) {
              slprintfbuff_clip(
                  return_error, return_error_size,
                  "Syntax error:\n\tOption %s needs to be followed by a "
                  "parameter: %s <param>\n\t%s\n",
                  command, command, _NOT_NULL(optalias_help(command)));
              return 0;
            }
            strcpybuff(param, argv[n_arg + 1]);
            need_param = 2;
          }
        }
        /* a detached value, but only a word that cannot be a URL of its own */
        else if (need_param == 2 && n_arg + 1 < argc &&
                 optalias_suffix(hts_optalias[pos][2], argv[n_arg + 1]) != NULL)
          strcpybuff(param, argv[n_arg + 1]);
        else
          need_param = 1;

        /* A template with no % maps every URL onto one local name, so
           --structure=flat is a typo rather than a template. --user-structure
           still takes such a value verbatim. */
        if (strcmp(hts_optalias[pos][2], "paramn") == 0 && param[0] != '\0' &&
            !optalias_paramn_glues(param) && strchr(param, '%') == NULL) {
          slprintfbuff_clip(return_error, return_error_size,
                            "Syntax error:\n\tOption --%s does not take the "
                            "value %s\n\t%s\n",
                            hts_optalias[pos][0], param,
                            _NOT_NULL(optalias_help(hts_optalias[pos][0])));
          return 0;
        }

        /* Final result */

        /* Must be alone (-P /tmp), or a paramn value -N cannot take glued */
        if (strcmp(hts_optalias[pos][2], "param1") == 0 ||
            (strcmp(hts_optalias[pos][2], "paramn") == 0 &&
             !optalias_paramn_glues(param))) {
          strlcpybuff(return_argv[0], command, return_argv_size);
          strlcpybuff(return_argv[1], param, return_argv_size);
          *return_argc = 2;     /* 2 parameters returned */
        }
        /* Alone with parameter (+*.gif) */
        else if (strcmp(hts_optalias[pos][2], "param0") == 0) {
          /* Command */
          strlcpybuff(return_argv[0], command, return_argv_size);
          strlcatbuff(return_argv[0], param, return_argv_size);
        }
        /* Together (-c8) */
        else {
          /* Command */
          strlcpybuff(return_argv[0], command, return_argv_size);
          /* Parameters accepted */
          if (strncmp(hts_optalias[pos][2], "param", 5) == 0) {
            /* --cache=off or --index=on */
            if (strcmp(param, "off") == 0)
              strlcatbuff(return_argv[0], "0", return_argv_size);
            else if (strcmp(param, "on") == 0) {
              /* a bare -N reads the next word as a template and would eat the
                 URL, so "on" names the default preset instead (#1434) */
              if (strcmp(hts_optalias[pos][2], "paramn") == 0)
                strlcatbuff(return_argv[0], "0", return_argv_size);
            } else
              strlcatbuff(return_argv[0], param, return_argv_size);
          } else if (param[0] != '\0') {
            const char *const suffix =
                optalias_suffix(hts_optalias[pos][2], param);

            /* refuse rather than drop: a dropped --index=0 reads back as the
               enabling bare --index */
            if (suffix == NULL) {
              if (negated)
                slprintfbuff_clip(return_error, return_error_size,
                                  "Unknown option: %s\n", argv[n_arg] + 2);
              else
                slprintfbuff_clip(
                    return_error, return_error_size,
                    "Syntax error:\n\tOption --%s does not take the value "
                    "%s\n\t%s\n",
                    hts_optalias[pos][0], param,
                    _NOT_NULL(optalias_help(hts_optalias[pos][0])));
              return 0;
            }
            strlcatbuff(return_argv[0], suffix, return_argv_size);
          }
          /* --wide-mirror: -w with the count clustered onto it (-wc32) */
          strlcatbuff(return_argv[0], addcommand, return_argv_size);
          *return_argc = 1;     /* 1 parameter returned */
        }
      } else {
        slprintfbuff_clip(return_error, return_error_size,
                          "Unknown option: %s\n", command);
        return 0;
      }
      return need_param;
    }

  /* Check -O <path> */
  {
    int pos;

    if ((pos = optreal_find(argv[n_arg])) >= 0) {
      /* -N 1 is preset 1; anything else stays detached, since a template may
         legitimately open with a digit (-N 2col/%n.%t) */
      if (strcmp(hts_optalias[pos][2], "paramn") == 0 && n_arg + 1 < argc &&
          optalias_is_digits(argv[n_arg + 1])) {
        strlcpybuff(return_argv[0], argv[n_arg], return_argv_size);
        strlcatbuff(return_argv[0], argv[n_arg + 1], return_argv_size);
        *return_argc = 1;
        return 2;
      }
      if ((strcmp(hts_optalias[pos][2], "param1") == 0)
          || (strcmp(hts_optalias[pos][2], "param0") == 0)) {
        if (optparam_missing(argc, argv, n_arg, argv[n_arg])) {
          slprintfbuff_clip(
              return_error, return_error_size,
              "Syntax error:\n\tOption %s needs to be followed by a "
              "parameter: %s <param>\n\t%s\n",
              argv[n_arg], argv[n_arg], _NOT_NULL(optalias_help(argv[n_arg])));
          return 0;
        }
        /* Copy parameters */
        strlcpybuff(return_argv[0], argv[n_arg], return_argv_size);
        strlcpybuff(return_argv[1], argv[n_arg + 1], return_argv_size);
        /* And return */
        *return_argc = 2;       /* 2 parameters returned */
        return 2;               /* 2 parameters used */
      }
    }
  }

  /* Copy and return other unknown option */
  strlcpybuff(return_argv[0], argv[n_arg], return_argv_size);
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

const char *optreal_value(int p) { return hts_optalias[p][1]; }

const char *optalias_value(int p) { return hts_optalias[p][0]; }

const char *opttype_value(int p) { return hts_optalias[p][2]; }

/* Help for option <token>, empty if not available, or NULL if unknown <token> */
const char *optalias_help(const char *token) {
  int pos = optalias_find(token);

  if (pos >= 0)
    return hts_optalias[pos][3];
  else
    return NULL;
}

/* Largest slot count whose byte size can not wrap. */
#define CMDL_MAX_SLOTS ((int) (INT_MAX / sizeof(char *)))

/* Grow the slot array to hold at least count entries, leaving cmd unchanged
   if it cannot. */
static hts_boolean cmdl_reserve(cmdl_argv *cmd, int count) {
  char **slots;
  hts_boolean *flags;
  int capacity;

  if (count <= cmd->capacity)
    return HTS_TRUE;
  if (count > CMDL_MAX_SLOTS) /* count alone: the product below can not wrap */
    return HTS_FALSE;
  /* double, so rebuilding an n-token command line stays amortized O(n) */
  capacity =
      cmd->capacity <= CMDL_MAX_SLOTS / 2 ? cmd->capacity * 2 : CMDL_MAX_SLOTS;
  if (capacity < count)
    capacity = count;
  slots = (char **) realloct(cmd->argv, sizeof(char *) * (size_t) capacity);
  if (slots == NULL)
    return HTS_FALSE;
  cmd->argv = slots;
  /* hts_boolean is no wider than a pointer, so this cannot wrap either */
  flags = (hts_boolean *) realloct(cmd->unquoted,
                                   sizeof(hts_boolean) * (size_t) capacity);
  if (flags == NULL) /* argv stays grown; capacity does not, so it is retried */
    return HTS_FALSE;
  cmd->unquoted = flags;
  cmd->capacity = capacity;
  return HTS_TRUE;
}

hts_boolean cmdl_init(cmdl_argv *cmd, int slots) {
  memset(cmd, 0, sizeof(*cmd));
  if (!cmdl_reserve(cmd, slots)) {
    cmdl_free(cmd);
    return HTS_FALSE;
  }
  return HTS_TRUE;
}

void cmdl_free(cmdl_argv *cmd) {
  hts_arena_free(&cmd->tokens);
  freet(cmd->argv);
  freet(cmd->unquoted);
  memset(cmd, 0, sizeof(*cmd));
}

hts_boolean cmdl_ins(cmdl_argv *cmd, const char *token, int pos) {
  char *copy;
  int i;

  assertf(pos >= 0 && pos <= cmd->argc);
  /* argc <= capacity <= CMDL_MAX_SLOTS holds here, so argc + 1 can not wrap */
  if (!cmdl_reserve(cmd, cmd->argc + 1))
    return HTS_FALSE;
  copy = hts_arena_strdup(&cmd->tokens, token);
  if (copy == NULL)
    return HTS_FALSE;
  for (i = cmd->argc; i > pos; i--) {
    cmd->argv[i] = cmd->argv[i - 1];
    cmd->unquoted[i] = cmd->unquoted[i - 1];
  }
  cmd->argv[pos] = copy;
  cmd->unquoted[pos] = HTS_FALSE;
  cmd->argc++;
  return HTS_TRUE;
}

hts_boolean cmdl_ins_unquoted(cmdl_argv *cmd, const char *token, int pos) {
  if (!cmdl_ins(cmd, token, pos))
    return HTS_FALSE;
  cmd->unquoted[pos] = HTS_TRUE;
  return HTS_TRUE;
}

hts_boolean cmdl_add(cmdl_argv *cmd, const char *token) {
  return cmdl_ins(cmd, token, cmd->argc);
}

/* Include a file to the current command line */
/* example:
  set sockets 8
  index on
  allow *.gif
  deny ad.*
*/
/* Note: NOT utf-8 */
cmdl_file_result optinclude_file(const char *name, cmdl_argv *cmd) {
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
      /* trim first: a blank line is skipped, not parsed as an option */
      hts_rtrim(line, HTS_REALSPACES);
      if (strnotempty(line)) {
        /* no comment line: # // ; */
        if (strchr("#/;", line[0]) == NULL) {
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

            result = optalias_check(2, (const char *const *) tmp_argv, 0,
                                    &return_argc, (tmp_argv + 2),
                                    sizeof(_tmp_argv[0]), return_error,
                                    sizeof(return_error));
            if (!result) {
              printf("%s\n", return_error);
            } else {
              if (return_error[0] != '\0')
                fprintf(stderr, "* %s\n", return_error);
              /* Insert the option and its parameter after the ones already
                 inserted, so that the file order is preserved */
              if (!cmdl_ins(cmd, tmp_argv[2], insert_after) ||
                  (return_argc > 1 &&
                   !cmdl_ins(cmd, tmp_argv[3], insert_after + 1))) {
                fclose(fp);
                return CMDL_FILE_NOMEM;
              }
              insert_after += return_argc > 1 ? 2 : 1;
            }
          }
        }
      }
    }
    fclose(fp);
    return CMDL_FILE_READ;
  }
  return CMDL_FILE_MISSING;
}

/* Get home directory, '.' if unset or empty */
/* example: /home/smith */
const char *hts_gethome(void) {
  const char *home = getenv("HOME");

  /* An empty $HOME would expand ~/foo into the absolute /foo */
  return strnotempty(home) ? home : ".";
}

/* Convert ~/foo into /home/smith/foo (~user/ left alone: no getpwnam here) */
void expand_home(String * str) {
  if (StringNotEmpty(*str) && StringSub(*str, 0) == '~' &&
      (StringLength(*str) == 1 || StringSub(*str, 1) == '/')) {
    char BIGSTK tempo[HTS_URLMAXSIZE * 2];
    const char *const home = hts_gethome();
    const size_t homelen = strlen(home);
    const size_t taillen = StringLength(*str) - 1;

    /* Leave untouched rather than abort() in strcatbuff on a huge $HOME */
    if (taillen < sizeof(tempo) && homelen < sizeof(tempo) - taillen) {
      strcpybuff(tempo, home);
      strcatbuff(tempo, StringBuff(*str) + 1);
      StringCopy(*str, tempo);
    }
  }
}
