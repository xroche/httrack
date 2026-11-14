/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 2026 Xavier Roche and other contributors

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
/* File: htsmime.c subroutines:                                 */
/*       MIME type tables and extension lookups                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htscore.h"
#include "htsbase.h"

/* ne pas renommer les types renvoyés (souvent types inconnus) */
static const char *hts_mime_keep[] = {"application/octet-stream", "text/plain",
                                      "application/xml", "text/xml", ""};

/* bogus servers returns these mime types when the extension is seen within the
 * filename */
static const char *hts_mime_bogus_multiple[] = {
    "application/x-wais-source", /* src (src.rpm) */
    ""};

/* pas de type mime connu, mais extension connue */
static const char *hts_ext_dynamic[] = {"php3", "php", "php4", "php2", "cgi",
                                        "asp", "jsp", "pl",
                                        /*"exe", */
                                        "cfm", "nsf", /* lotus */
                                        ""};

/* types MIME
   note: application/octet-stream should not be used here
*/
static const char *hts_mime[][2] = {
    {"application/acad", "dwg"},
    {"application/arj", "arj"},
    {"application/clariscad", "ccad"},
    {"application/drafting", "drw"},
    {"application/dxf", "dxf"},
    {"application/excel", "xls"},
    {"application/i-deas", "unv"},
    {"application/iges", "isg"},
    {"application/iges", "iges"},
    {"application/mac-binhex40", "hqx"},
    {"application/mac-compactpro", "cpt"},
    {"application/msword", "doc"},
    {"application/msword", "w6w"},
    {"application/msword", "word"},
    {"application/mswrite", "wri"},
    /*{"application/octet-stream","dms"}, */
    /*{"application/octet-stream","lzh"}, */
    /*{"application/octet-stream","lha"}, */
    /*{"application/octet-stream","bin"}, */
    {"application/oda", "oda"},
    {"application/pdf", "pdf"},
    {"application/postscript", "ps"},
    {"application/postscript", "ai"},
    {"application/postscript", "eps"},
    {"application/powerpoint", "ppt"},
    {"application/pro_eng", "prt"},
    {"application/pro_eng", "part"},
    {"application/rtf", "rtf"},
    {"application/set", "set"},
    {"application/sla", "stl"},
    {"application/smil", "smi"},
    {"application/smil", "smil"},
    {"application/smil", "sml"},
    {"application/solids", "sol"},
    {"application/STEP", "stp"},
    {"application/STEP", "step"},
    {"application/vda", "vda"},
    {"application/x-authorware-map", "aam"},
    {"application/x-authorware-seg", "aas"},
    {"application/x-authorware-bin", "aab"},
    {"application/x-bzip2", "bz2"},
    {"application/x-cocoa", "cco"},
    {"application/x-csh", "csh"},
    {"application/x-director", "dir"},
    {"application/x-director", "dcr"},
    {"application/x-director", "dxr"},
    {"application/x-mif", "mif"},
    {"application/x-dvi", "dvi"},
    {"application/x-gzip", "gz"},
    {"application/x-gzip", "gzip"},
    {"application/x-hdf", "hdf"},
    {"application/x-javascript", "js"},
    {"application/x-koan", "skp"},
    {"application/x-koan", "skd"},
    {"application/x-koan", "skt"},
    {"application/x-koan", "skm"},
    {"application/x-latex", "latex"},
    {"application/x-netcdf", "nc"},
    {"application/x-netcdf", "cdf"},
    /* {"application/x-sh","sh"}, */
    /* {"application/x-csh","csh"}, */
    /* {"application/x-ksh","ksh"}, */
    {"application/x-shar", "shar"},
    {"application/x-stuffit", "sit"},
    {"application/x-tcl", "tcl"},
    {"application/x-tex", "tex"},
    {"application/x-texinfo", "texinfo"},
    {"application/x-texinfo", "texi"},
    {"application/x-troff", "t"},
    {"application/x-troff", "tr"},
    {"application/x-troff", "roff"},
    {"application/x-troff-man", "man"},
    {"application/x-troff-me", "ms"},
    {"application/x-wais-source", "src"},
    {"application/zip", "zip"},
    {"application/x-zip-compressed", "zip"},
    {"application/x-bcpio", "bcpio"},
    {"application/x-cdlink", "vcd"},
    {"application/x-cpio", "cpio"},
    {"application/x-gtar", "tgz"},
    {"application/x-gtar", "gtar"},
    {"application/x-shar", "shar"},
    {"application/x-shockwave-flash", "swf"},
    {"application/x-sv4cpio", "sv4cpio"},
    {"application/x-sv4crc", "sv4crc"},
    {"application/x-tar", "tar"},
    {"application/x-ustar", "ustar"},
    {"application/x-winhelp", "hlp"},
    {"application/xml", "xml"},
    {"audio/midi", "mid"},
    {"audio/midi", "midi"},
    {"audio/midi", "kar"},
    {"audio/mpeg", "mp3"},
    {"audio/mpeg", "mpga"},
    {"audio/mpeg", "mp2"},
    {"audio/basic", "au"},
    {"audio/basic", "snd"},
    {"audio/x-aiff", "aif"},
    {"audio/x-aiff", "aiff"},
    {"audio/x-aiff", "aifc"},
    {"audio/x-pn-realaudio", "rm"},
    {"audio/x-pn-realaudio", "ram"},
    {"audio/x-pn-realaudio", "ra"},
    {"audio/x-pn-realaudio-plugin", "rpm"},
    {"audio/x-wav", "wav"},
    {"chemical/x-pdb", "pdb"},
    {"chemical/x-pdb", "xyz"},
    {"drawing/x-dwf", "dwf"},
    {"image/gif", "gif"},
    {"image/ief", "ief"},
    {"image/jpeg", "jpg"},
    {"image/jpeg", "jpe"},
    {"image/jpeg", "jpeg"},
    {"image/pict", "pict"},
    {"image/png", "png"},
    {"image/tiff", "tiff"},
    {"image/tiff", "tif"},
    {"image/svg+xml", "svg"},
    {"image/svg-xml", "svg"},
    {"image/x-cmu-raster", "ras"},
    {"image/x-freehand", "fh4"},
    {"image/x-freehand", "fh7"},
    {"image/x-freehand", "fh5"},
    {"image/x-freehand", "fhc"},
    {"image/x-freehand", "fh"},
    {"image/x-portable-anymap", "pnm"},
    {"image/x-portable-bitmap", "pgm"},
    {"image/x-portable-pixmap", "ppm"},
    {"image/x-rgb", "rgb"},
    {"image/x-xbitmap", "xbm"},
    {"image/x-xpixmap", "xpm"},
    {"image/x-xwindowdump", "xwd"},
    {"model/mesh", "msh"},
    {"model/mesh", "mesh"},
    {"model/mesh", "silo"},
    {"multipart/x-zip", "zip"},
    {"multipart/x-gzip", "gzip"},
    {"text/css", "css"},
    {"text/html", "html"},
    {"text/html", "htm"},
    {"text/plain", "txt"},
    {"text/plain", "g"},
    {"text/plain", "h"},
    {"text/plain", "c"},
    {"text/plain", "cc"},
    {"text/plain", "hh"},
    {"text/plain", "m"},
    {"text/plain", "f90"},
    {"text/richtext", "rtx"},
    {"text/tab-separated-values", "tsv"},
    {"text/x-setext", "etx"},
    {"text/x-sgml", "sgml"},
    {"text/x-sgml", "sgm"},
    {"text/xml", "xml"},
    {"text/xml", "dtd"},
    {"video/mpeg", "mpeg"},
    {"video/mpeg", "mpg"},
    {"video/mpeg", "mpe"},
    {"video/quicktime", "qt"},
    {"video/quicktime", "mov"},
    {"video/x-msvideo", "avi"},
    {"video/x-sgi-movie", "movie"},
    {"x-conference/x-cooltalk", "ice"},
    /*{"application/x-httpd-cgi","cgi"}, */
    {"x-world/x-vrml", "wrl"},

    /* More from w3schools.com */
    {"application/envoy", "evy"},
    {"application/fractals", "fif"},
    {"application/futuresplash", "spl"},
    {"application/hta", "hta"},
    {"application/internet-property-stream", "acx"},
    {"application/msword", "dot"},
    {"application/olescript", "axs"},
    {"application/pics-rules", "prf"},
    {"application/pkcs10", "p10"},
    {"application/pkix-crl", "crl"},
    {"application/set-payment-initiation", "setpay"},
    {"application/set-registration-initiation", "setreg"},
    {"application/vnd.ms-excel", "xls"},
    {"application/vnd.ms-excel", "xla"},
    {"application/vnd.ms-excel", "xlc"},
    {"application/vnd.ms-excel", "xlm"},
    {"application/vnd.ms-excel", "xlt"},
    {"application/vnd.ms-excel", "xlw"},
    {"application/vnd.ms-pkicertstore", "sst"},
    {"application/vnd.ms-pkiseccat", "cat"},
    {"application/vnd.ms-powerpoint", "ppt"},
    {"application/vnd.ms-powerpoint", "pot"},
    {"application/vnd.ms-powerpoint", "pps"},
    {"application/vnd.ms-project", "mpp"},
    {"application/vnd.ms-works", "wcm"},
    {"application/vnd.ms-works", "wdb"},
    {"application/vnd.ms-works", "wks"},
    {"application/vnd.ms-works", "wps"},
    {"application/vnd.oasis.opendocument.chart", "odc"},
    {"application/vnd.oasis.opendocument.database", "odb"},
    {"application/vnd.oasis.opendocument.formula", "odf"},
    {"application/vnd.oasis.opendocument.graphics", "odg"},
    {"application/vnd.oasis.opendocument.graphics-template", "otg"},
    {"application/vnd.oasis.opendocument.image", "odi"},
    {"application/vnd.oasis.opendocument.presentation", "odp"},
    {"application/vnd.oasis.opendocument.presentation-template", "otp"},
    {"application/vnd.oasis.opendocument.spreadsheet", "ods"},
    {"application/vnd.oasis.opendocument.spreadsheet-template", "ots"},
    {"application/vnd.oasis.opendocument.text", "odt"},
    {"application/vnd.oasis.opendocument.text-master", "odm"},
    {"application/vnd.oasis.opendocument.text-template", "ott"},
    {"application/vnd.oasis.opendocument.text-web", "oth"},
    {"application/"
     "vnd.openxmlformats-officedocument.presentationml.presentation",
     "pptx"},
    {"application/vnd.openxmlformats-officedocument.presentationml.slide",
     "sldx"},
    {"application/vnd.openxmlformats-officedocument.presentationml.slideshow",
     "ppsx"},
    {"application/vnd.openxmlformats-officedocument.presentationml.template",
     "potx"},
    {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
     "xlsx"},
    {"application/vnd.openxmlformats-officedocument.spreadsheetml.template",
     "xltx"},
    {"application/vnd.openxmlformats-officedocument.wordprocessingml.document",
     "docx"},
    {"application/vnd.openxmlformats-officedocument.wordprocessingml.template",
     "dotx"},
    {"application/x-compress", "z"},
    {"application/x-compressed", "tgz"},
    {"application/x-internet-signup", "ins"},
    {"application/x-internet-signup", "isp"},
    {"application/x-iphone", "iii"},
    {"application/x-javascript", "js"},
    {"application/x-msaccess", "mdb"},
    {"application/x-mscardfile", "crd"},
    {"application/x-msclip", "clp"},
    {"application/x-msmediaview", "m13"},
    {"application/x-msmediaview", "m14"},
    {"application/x-msmediaview", "mvb"},
    {"application/x-msmetafile", "wmf"},
    {"application/x-msmoney", "mny"},
    {"application/x-mspublisher", "pub"},
    {"application/x-msschedule", "scd"},
    {"application/x-msterminal", "trm"},
    {"application/x-perfmon", "pma"},
    {"application/x-perfmon", "pmc"},
    {"application/x-perfmon", "pml"},
    {"application/x-perfmon", "pmr"},
    {"application/x-perfmon", "pmw"},
    {"application/x-pkcs12", "p12"},
    {"application/x-pkcs12", "pfx"},
    {"application/x-pkcs7-certificates", "p7b"},
    {"application/x-pkcs7-certificates", "spc"},
    {"application/x-pkcs7-certreqresp", "p7r"},
    {"application/x-pkcs7-mime", "p7c"},
    {"application/x-pkcs7-mime", "p7m"},
    {"application/x-pkcs7-signature", "p7s"},
    {"application/x-troff-me", "me"},
    {"application/x-x509-ca-cert", "cer"},
    {"application/x-x509-ca-cert", "crt"},
    {"application/x-x509-ca-cert", "der"},
    {"application/ynd.ms-pkipko", "pko"},
    {"audio/mid", "mid"},
    {"audio/mid", "rmi"},
    {"audio/mpeg", "mp3"},
    {"audio/x-mpegurl", "m3u"},
    {"image/bmp", "bmp"},
    {"image/cis-cod", "cod"},
    {"image/pipeg", "jfif"},
    {"image/x-cmx", "cmx"},
    {"image/x-icon", "ico"},
    {"image/x-portable-bitmap", "pbm"},
    {"message/rfc822", "mht"},
    {"message/rfc822", "mhtml"},
    {"message/rfc822", "nws"},
    {"text/css", "css"},
    {"text/h323", "323"},
    {"text/html", "stm"},
    {"text/iuls", "uls"},
    {"text/plain", "bas"},
    {"text/scriptlet", "sct"},
    {"text/webviewhtml", "htt"},
    {"text/x-component", "htc"},
    {"text/x-vcard", "vcf"},
    {"video/mpeg", "mp2"},
    {"video/mpeg", "mpa"},
    {"video/mpeg", "mpv2"},
    {"video/x-la-asf", "lsf"},
    {"video/x-la-asf", "lsx"},
    {"video/x-ms-asf", "asf"},
    {"video/x-ms-asf", "asr"},
    {"video/x-ms-asf", "asx"},
    {"video/x-ms-wmv", "wmv"},
    {"x-world/x-vrml", "flr"},
    {"x-world/x-vrml", "vrml"},
    {"x-world/x-vrml", "wrz"},
    {"x-world/x-vrml", "xaf"},
    {"x-world/x-vrml", "xof"},

    /* Various */
    {"application/ogg", "ogg"},

    {"application/x-java-vm", "class"},
    {"application/x-bittorrent", "torrent"},

    {"", ""}};

/* Post-2010 formats. Scanned after hts_mime[], so a legacy mapping wins. */
static const char *hts_mime_modern[][2] = {
    {"image/webp", "webp"},
    {"image/avif", "avif"},
    {"image/heic", "heic"},
    {"font/woff", "woff"},
    {"font/woff2", "woff2"},
    {"font/ttf", "ttf"},
    {"font/otf", "otf"},
    {"application/json", "json"},
    {"application/ld+json", "jsonld"},
    {"application/manifest+json", "webmanifest"},
    {"application/wasm", "wasm"},
    {"text/javascript", "js"},
    {"text/javascript", "mjs"},
    {"text/markdown", "md"},
    {"video/mp4", "mp4"},
    {"video/webm", "webm"},
    {"video/ogg", "ogv"},
    {"video/mp2t", "ts"},
    {"application/vnd.apple.mpegurl", "m3u8"},
    {"audio/mp4", "m4a"},
    {"audio/aac", "aac"},
    {"audio/ogg", "oga"},
    {"audio/opus", "opus"},
    {"audio/flac", "flac"},
    {"audio/webm", "weba"},
    {"application/x-7z-compressed", "7z"},
    {"application/x-rar-compressed", "rar"},
    {"application/zstd", "zst"},
    {"", ""}};

// guess a local file's mime type (e.g. fil="toto.gif" -> s="image/gif")
// returns 1 if a type was written to s, 0 otherwise
hts_boolean guess_httptype_sized(httrackp *opt, char *s, size_t ssize,
                                 const char *fil) {
  return get_httptype_sized(opt, s, ssize, fil, 1);
}

// deprecated variant; kept for ABI compatibility. Bounds to the implicit
// contract the old callers relied on (a contenttype-sized buffer).
void guess_httptype(httrackp *opt, char *s, const char *fil) {
  (void) get_httptype_sized(opt, s, HTS_MIMETYPE_SIZE, fil, 1);
}

// first match in a NUL-terminated {mime,ext} table. key selects the lookup
// column (0=mime, 1=ext); returns the other column, or NULL if no row matches
// (a "*" partner means the row carries no value).
static const char *hts_mime_lookup(const char *(*table)[2], int key,
                                   const char *needle) {
  int j;

  for (j = 0; strnotempty(table[j][1]); j++) {
    if (strfield2(table[j][key], needle) && table[j][!key][0] != '*')
      return table[j][!key];
  }
  return NULL;
}

// hts_mime_lookup() over the legacy table, then the modern one
static const char *hts_mime_find(int key, const char *needle) {
  const char *const found = hts_mime_lookup(hts_mime, key, needle);

  return found != NULL ? found : hts_mime_lookup(hts_mime_modern, key, needle);
}

// write the mime type for fil into s (capacity ssize)
// flag: 1 to always return a type (the "application/..." / octet-stream
// fallback) returns 1 if a type was written to s, 0 otherwise
HTSEXT_API hts_boolean get_httptype_sized(httrackp *opt, char *s, size_t ssize,
                                          const char *fil, hts_boolean flag) {
  // userdef overrides get_httptype (a rule with an empty value, e.g. "--assume
  // cgi=", matches but writes nothing: report it as "no type" like the old
  // code, whose callers tested strnotempty(s))
  if (get_userhttptype(opt, s, ssize, fil)) {
    return s[0] != '\0';
  }
  // regular tests
  if (ishtml(opt, fil) == 1) {
    strlcpybuff(s, "text/html", ssize);
    return 1;
  } else {
    /* Check html -> text/html */
    const char *a = hts_lastcharptr(fil);

    while ((a > fil) && (*a != '.') && (*a != '/'))
      a--;
    if (a >= fil && *a == '.' && strlen(a) < 32) {
      const char *mime;

      a++;
      mime = hts_mime_find(1, a);
      if (mime != NULL) {
        strlcpybuff(s, mime, ssize);
        return 1;
      }

      if (flag) {
        snprintf(s, ssize, "application/%s", a);
        return 1;
      }
    } else {
      if (flag) {
        strlcpybuff(s, "application/octet-stream", ssize);
        return 1;
      }
    }
  }
  return 0;
}

// deprecated variant; kept for ABI compatibility. Bounds to the implicit
// contract the old callers relied on (a contenttype-sized buffer).
HTSEXT_API void get_httptype(httrackp *opt, char *s, const char *fil,
                             int flag) {
  (void) get_httptype_sized(opt, s, HTS_MIMETYPE_SIZE, fil, flag);
}

// get type of fil (php)
// s: destination buffer, or NULL
hts_boolean get_userhttptype(httrackp *opt, char *s, size_t ssize,
                             const char *fil) {
  if (s != NULL) {
    assertf(ssize != 0);
    if (s)
      s[0] = '\0';
    if (fil == NULL || *fil == '\0')
      return HTS_FALSE;
#if 1
    if (StringLength(opt->mimedefs) > 0) {

      /* Check --assume foooo/foo/bar.cgi=text/html, then foo/bar.cgi=text/html,
       * then bar.cgi=text/html */
      /* also: --assume baz,bar,foooo/foo/bar.cgi=text/html */
      /* start from path beginning */
      do {
        const char *next;
        const char *mimedefs =
            StringBuff(opt->mimedefs); /* loop through mime definitions :
                                          \nfoo=bar\nzoo=baz\n.. */

        while (*mimedefs != '\0') {
          const char *segment = fil + 1;

          if (*mimedefs == '\n') {
            mimedefs++;
          }
          /* compare current segment with user's definition */
          do {
            int i;

            /* check current item */
            for (i = 0; mimedefs[i] != '\0'    /* end of all defs */
                        && mimedefs[i] != ' '  /* next item in left list */
                        && mimedefs[i] != '='  /* end of left list */
                        && mimedefs[i] != '\n' /* end of this def (?) */
                        && mimedefs[i] == segment[i] /* same item */
                 ;
                 i++)
              ;
            /* success */
            if ((mimedefs[i] == '=' || mimedefs[i] == ' ') &&
                segment[i] == '\0') {
              while (mimedefs[i] != 0 && mimedefs[i] != '\n' &&
                     mimedefs[i] != '=')
                i++;
              if (mimedefs[i] == '=') {
                size_t len;

                i++;
                len = strcspn(&mimedefs[i], "\n");
                /* clip, don't abort: the consumers match whole strings far
                   shorter than any destination, so a value long enough to be
                   clipped already matched nothing (#1276) */
                if (len >= ssize) {
                  hts_log_print(opt, LOG_WARNING,
                                "--assume value for '%s' is %d bytes, clipped "
                                "to %d",
                                fil, (int) len, (int) (ssize - 1));
                  len = ssize - 1;
                }
                memcpy(s, &mimedefs[i], len);
                s[len] = '\0';
                return HTS_TRUE; /* SUCCESS! */
              }
            }
            /* next item in list */
            for (mimedefs += i; *mimedefs != '\0' && *mimedefs != '\n' &&
                                *mimedefs != '=' && *mimedefs != ' ';
                 mimedefs++)
              ;
            if (*mimedefs == ' ') {
              mimedefs++;
            }
          } while (*mimedefs != '\0' && *mimedefs != '\n' && *mimedefs != '=');
          /* next user-def */
          for (; *mimedefs != '\0' && *mimedefs != '\n'; mimedefs++)
            ;
        }
        /* shorten segment */
        next = strchr(fil + 1, '/');
        if (next == NULL) {
          /* ext tests */
          next = strchr(fil + 1, '.');
        }
        fil = next;
      } while (fil != NULL);
    }
#else
    if (*buffer) {
      char BIGSTK search[1024];
      char *detect;

      sprintf(search, "\n%s=", ext); // php=text/html
      detect = strstr(*buffer, search);
      if (!detect) {
        sprintf(search, "\n%s\n", ext); // php\ncgi=text/html
        detect = strstr(*buffer, search);
      }
      if (detect) {
        detect = strchr(detect, '=');
        if (detect) {
          detect++;
          if (s) {
            char *a;

            a = strchr(detect, '\n');
            if (a) {
              strncatbuff(s, detect, (int) (a - detect));
            }
          }
          return 1;
        }
      }
    }
#endif
  }
  return HTS_FALSE;
}

// give the file extension for a mime type (e.g. "image/gif" -> "gif")
// returns 1 if an extension was found (and written to s), 0 otherwise
int give_mimext(char *s, size_t ssize, const char *st) {
  int ok = 0;
  const char *ext;

  st = hts_effective_mime(st); /* no declared type: derive an html ext */
  s[0] = '\0';
  ext = hts_mime_find(0, st);
  if (ext != NULL) {
    strlcpybuff(s, ext, ssize);
    ok = 1;
  }
  // wrap "x" mimetypes, such as:
  // application/x-mp3
  // or
  // application/mp3
  if (!ok) {
    int p;
    const char *a = NULL;

    if ((p = strfield(st, "application/x-")))
      a = st + p;
    else if ((p = strfield(st, "application/")))
      a = st + p;
    if (a) {
      if ((int) strlen(a) >= 1) {
        if ((int) strlen(a) <= 4) {
          strlcpybuff(s, a, ssize);
          ok = 1;
        }
      }
    }
  }
  return ok;
}

// known extension? 0: no, 1: yes, 2: html
HTSEXT_API int is_knowntype(httrackp *opt, const char *fil) {
  char catbuff[CATBUFF_SIZE];
  const char *mime;

  if (!fil)
    return 0;
  mime = hts_mime_find(1, get_ext(catbuff, sizeof(catbuff), fil));
  if (mime != NULL)
    return is_html_mime_type(mime) ? 2 : 1;

  // Known by user?
  return (is_userknowntype(opt, fil));
}

// known type?..
//  0 : no
//  1 : yes
//  2 : html
// setdefs : set mime buffer:
//   file=(char*) "asp=text/html\nphp=text/html\n"
HTSEXT_API int is_userknowntype(httrackp *opt, const char *fil) {
  char BIGSTK mime[1024];

  if (!fil)
    return 0;
  if (!strnotempty(fil))
    return 0;
  mime[0] = '\0';
  get_userhttptype(opt, mime, sizeof(mime), fil);
  if (!strnotempty(mime))
    return 0;
  else if (is_html_mime_type(mime))
    return 2;
  else
    return 1;
}

// page dynamique?
// is_dyntype(get_ext("foo.asp"))
HTSEXT_API hts_boolean is_dyntype(const char *fil) {
  int j = 0;

  if (!fil)
    return 0;
  if (!strnotempty(fil))
    return 0;
  while (strnotempty(hts_ext_dynamic[j])) {
    if (strfield2(hts_ext_dynamic[j], fil)) {
      return 1;
    }
    j++;
  }
  return 0;
}

// types critiques qui ne doivent pas être changés car renvoyés par des serveurs
// qui ne connaissent pas le type
hts_boolean may_unknown(httrackp *opt, const char *st) {
  int j = 0;

  // types média
  if (may_be_hypertext_mime(opt, st, "")) {
    return 1;
  }
  while (strnotempty(hts_mime_keep[j])) {
    if (strfield2(hts_mime_keep[j], st)) { // trouvé
      return 1;
    }
    j++;
  }
  return 0;
}

/* returns 1 if the mime/filename seems to be bogus because of badly recognized
  multiple extension ; such as "application/x-wais-source" for
  "httrack-3.42-1.el5.src.rpm" reported by Hippy Dave 08/2008 (3.43) */
int may_bogus_multiple(httrackp *opt, const char *mime, const char *filename) {
  int j;

  for (j = 0; strnotempty(hts_mime_bogus_multiple[j]); j++) {
    if (strfield2(hts_mime_bogus_multiple[j],
                  mime)) { /* found mime type in suspicious list */
      char ext[64];

      if (give_mimext(ext, sizeof(ext),
                      mime)) { /* we have an extension for that */
        const size_t ext_size = strlen(ext);
        const char *file = strrchr(filename, '/'); /* fetch terminal filename */

        if (file != NULL) {
          int i;

          for (i = 0; file[i] != 0; i++) {
            if (i > 0 && file[i - 1] == '.' &&
                strncasecmp(&file[i], ext, ext_size) == 0 &&
                (file[i + ext_size] == 0 || file[i + ext_size] == '.' ||
                 file[i + ext_size] == '?')) {
              return 1; /* is ambiguous */
            }
          }
        }
      }
      return 0;
    }
  }
  return 0;
}

/* filename extension should not be changed because potentially bogus ; replaces
 * may_unknown() (3.43) */
int may_unknown2(httrackp *opt, const char *mime, const char *filename) {
  int ret = may_unknown(opt, mime);

  if (ret == 0) {
    ret = may_bogus_multiple(opt, mime, filename);
  }
  return ret;
}
