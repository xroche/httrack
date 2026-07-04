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
/* File: MIME magic-byte consistency checks                     */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htssniff.h"

#include <string.h>

#include "htslib.h"

/* One magic rule: `len` bytes at `off` confirm `mime`. */
typedef struct sniff_magic {
  const char *mime;
  unsigned short off;
  unsigned char len;
  const char *bytes;
} sniff_magic;

/* Direction is mime -> magic (verify a claim, never classify); types with
   no reliable magic (plain text, css, js..) are deliberately absent. Patterns
   follow the WHATWG MIME Sniffing Standard tables where it defines them
   (https://mimesniff.spec.whatwg.org/); the rest covers httrack's wider MIME
   set. Spec-only types absent from our MIME tables (EOT, font/collection)
   are omitted as unreachable. */
static const sniff_magic sniff_table[] = {
    /* images */
    {"image/jpeg", 0, 3, "\xff\xd8\xff"},
    {"image/pipeg", 0, 3, "\xff\xd8\xff"},
    {"image/pjpeg", 0, 3, "\xff\xd8\xff"},
    {"image/png", 0, 8, "\x89PNG\r\n\x1a\n"},
    {"image/gif", 0, 6, "GIF87a"},
    {"image/gif", 0, 6, "GIF89a"},
    {"image/bmp", 0, 2, "BM"},
    {"image/tiff", 0, 4, "II*\0"},
    {"image/tiff", 0, 4, "MM\0*"},
    {"image/x-icon", 0, 4, "\0\0\1\0"},
    {"image/x-icon", 0, 4, "\0\0\2\0"}, /* Windows cursor, per the spec */
    {"image/x-portable-bitmap", 0, 2, "P1"},
    {"image/x-portable-bitmap", 0, 2, "P4"},
    {"image/x-portable-pixmap", 0, 2, "P3"},
    {"image/x-portable-pixmap", 0, 2, "P6"},
    {"image/x-xpixmap", 0, 9, "/* XPM */"},
    {"image/x-xbitmap", 0, 7, "#define"},
    {"image/x-rgb", 0, 2, "\x01\xda"},
    {"image/x-cmu-raster", 0, 4, "\xf1\x00\x40\xbb"},
    /* audio */
    {"audio/mpeg", 0, 3, "ID3"},
    {"audio/basic", 0, 4, ".snd"},
    {"audio/mid", 0, 8, "MThd\0\0\0\6"},
    {"audio/midi", 0, 8, "MThd\0\0\0\6"},
    {"audio/x-pn-realaudio", 0, 4, ".ra\xfd"},
    {"audio/x-pn-realaudio", 0, 4, ".RMF"},
    {"audio/x-pn-realaudio-plugin", 0, 4, ".ra\xfd"},
    {"audio/x-pn-realaudio-plugin", 0, 4, ".RMF"},
    {"audio/flac", 0, 4, "fLaC"},
    {"audio/aac", 0, 4, "ADIF"},
    /* video */
    {"video/mpeg", 0, 4, "\x00\x00\x01\xba"},
    {"video/mpeg", 0, 4, "\x00\x00\x01\xb3"},
    {"video/x-sgi-movie", 0, 4, "MOVI"},
    /* archives / compression */
    {"application/x-gzip", 0, 3, "\x1f\x8b\x08"},
    {"multipart/x-gzip", 0, 3, "\x1f\x8b\x08"},
    {"application/x-compressed", 0, 3, "\x1f\x8b\x08"},
    {"application/x-compress", 0, 2, "\x1f\x9d"},
    {"application/x-bzip2", 0, 3, "BZh"},
    {"application/x-7z-compressed", 0, 6, "7z\xbc\xaf\x27\x1c"},
    /* 6-byte prefix common to RAR4 (spec) and RAR5 */
    {"application/x-rar-compressed", 0, 6, "Rar!\x1a\x07"},
    {"application/zstd", 0, 4, "\x28\xb5\x2f\xfd"},
    {"application/arj", 0, 2, "\x60\xea"},
    {"application/x-cpio", 0, 6, "070701"},
    {"application/x-cpio", 0, 6, "070707"},
    {"application/x-cpio", 0, 2, "\xc7\x71"},
    {"application/x-sv4cpio", 0, 6, "070701"},
    {"application/x-sv4crc", 0, 6, "070702"},
    {"application/x-stuffit", 0, 8, "StuffIt "},
    {"application/x-stuffit", 0, 4, "SIT!"},
    {"application/mac-binhex40", 0, 10, "(This file"},
    /* documents */
    {"application/pdf", 0, 5, "%PDF-"},
    {"application/postscript", 0, 2, "%!"},
    {"application/rtf", 0, 5, "{\\rtf"},
    {"application/x-dvi", 0, 2, "\xf7\x02"},
    {"application/x-hdf", 0, 4, "\x0e\x03\x13\x01"},
    {"application/x-hdf", 0, 8, "\x89HDF\r\n\x1a\n"},
    {"application/x-netcdf", 0, 4, "CDF\x01"},
    {"application/x-netcdf", 0, 4, "CDF\x02"},
    {"application/x-msaccess", 0, 19, "\0\1\0\0Standard Jet DB"},
    /* fonts */
    {"font/woff", 0, 4, "wOFF"},
    {"font/woff2", 0, 4, "wOF2"},
    {"font/ttf", 0, 4, "\0\1\0\0"},
    {"font/ttf", 0, 4, "true"},
    {"font/otf", 0, 4, "OTTO"},
    /* misc */
    {"application/x-shockwave-flash", 0, 3, "FWS"},
    {"application/x-shockwave-flash", 0, 3, "CWS"},
    {"application/x-shockwave-flash", 0, 3, "ZWS"},
    {"application/futuresplash", 0, 3, "FWS"},
    {"application/x-director", 0, 4, "RIFX"},
    {"application/x-director", 0, 4, "XFIR"},
    {"application/x-java-vm", 0, 4, "\xca\xfe\xba\xbe"},
    {"application/wasm", 0, 4, "\0asm"},
    {"application/x-msmetafile", 0, 4, "\xd7\xcd\xc6\x9a"},
    {"application/x-msmetafile", 0, 4, "\x01\x00\x09\x00"},
    {"application/x-x509-ca-cert", 0, 2, "\x30\x82"},
    {"application/x-pkcs12", 0, 2, "\x30\x82"},
    {"application/x-pkcs7-mime", 0, 2, "\x30\x82"},
    {"application/x-pkcs7-signature", 0, 2, "\x30\x82"},
    {"application/x-pkcs7-certificates", 0, 2, "\x30\x82"},
    {"x-world/x-vrml", 0, 5, "#VRML"},
    {"application/x-bittorrent", 0, 11, "d8:announce"},
    {"drawing/x-dwf", 0, 4, "(DWF"},
    {"application/acad", 0, 4, "AC10"},
    {NULL, 0, 0, NULL}};

/* MIME families sharing a container magic */
static const char *const zip_mimes[] = {
    "application/zip", "application/x-zip-compressed", "multipart/x-zip", NULL};
static const char *const zip_mime_prefixes[] = {
    "application/vnd.openxmlformats-officedocument.",
    "application/vnd.oasis.opendocument.", NULL};
static const char *const ole_mimes[] = {"application/msword",
                                        "application/excel",
                                        "application/vnd.ms-excel",
                                        "application/powerpoint",
                                        "application/vnd.ms-powerpoint",
                                        "application/vnd.ms-project",
                                        "application/vnd.ms-works",
                                        "application/x-msmoney",
                                        "application/x-mspublisher",
                                        NULL};
static const char *const tar_mimes[] = {
    "application/x-tar", "application/x-ustar", "application/x-gtar", NULL};
static const char *const ogg_mimes[] = {"application/ogg", "audio/ogg",
                                        "video/ogg", "audio/opus", NULL};
static const char *const ebml_mimes[] = {"video/webm", "audio/webm", NULL};
/* ISO-BMFF, any 'ftyp' brand: containers overlap too much to split */
static const char *const bmff_mimes[] = {"video/mp4", "audio/mp4",
                                         "video/quicktime", NULL};
static const char *const avif_mimes[] = {"image/avif", NULL};
static const char *const heic_mimes[] = {"image/heic", NULL};
static const char *const asf_mimes[] = {"video/x-ms-asf", "video/x-ms-wmv",
                                        "video/x-la-asf", NULL};
static const char *const xml_mimes[] = {"application/xml", "text/xml",
                                        "image/svg+xml", "image/svg-xml", NULL};
static const char *const svg_mimes[] = {"image/svg+xml", "image/svg-xml", NULL};
static const char *const html_mimes[] = {"text/html", NULL};
static const char *const pem_mimes[] = {
    "application/x-x509-ca-cert", "application/x-pkcs7-certificates",
    "application/x-pkcs7-mime", "application/x-pkcs7-signature", NULL};

static hts_boolean mime_in(const char *const *list, const char *mime) {
  size_t i;

  for (i = 0; list[i] != NULL; i++)
    if (strfield2(list[i], mime))
      return HTS_TRUE;
  return HTS_FALSE;
}

static hts_boolean mime_in_prefix(const char *const *list, const char *mime) {
  size_t i;

  for (i = 0; list[i] != NULL; i++)
    if (strfield(mime, list[i]))
      return HTS_TRUE;
  return HTS_FALSE;
}

static hts_boolean has_bytes(const unsigned char *d, size_t n, size_t off,
                             const char *bytes, size_t len) {
  /* overflow-safe: untrusted n alone on one side */
  return n >= off && len <= n - off && memcmp(d + off, bytes, len) == 0
             ? HTS_TRUE
             : HTS_FALSE;
}

static unsigned char ascii_lower(unsigned char c) {
  return c >= 'A' && c <= 'Z' ? (unsigned char) (c + 32) : c;
}

/* Case-insensitive text prefix after an optional UTF-8 BOM and whitespace. */
static hts_boolean has_text_prefix(const unsigned char *d, size_t n,
                                   const char *prefix) {
  const size_t len = strlen(prefix);
  size_t i, k;

  i = n >= 3 && memcmp(d, "\xef\xbb\xbf", 3) == 0 ? 3 : 0;
  while (i < n && (d[i] == ' ' || d[i] == '\t' || d[i] == '\r' || d[i] == '\n'))
    i++;
  if (len > n - i) /* i <= n from the loop above */
    return HTS_FALSE;
  for (k = 0; k < len; k++)
    if (ascii_lower(d[i + k]) != ascii_lower((unsigned char) prefix[k]))
      return HTS_FALSE;
  return HTS_TRUE;
}

typedef enum sniff_op {
  SNIFF_QUERY_KNOWN, /* is any rule defined for this MIME? */
  SNIFF_QUERY_MATCH  /* do the bytes confirm this MIME? */
} sniff_op;

/* Single walk for both queries so the rule set can't drift apart. */
static hts_boolean sniff_eval(sniff_op op, const unsigned char *d, size_t n,
                              const char *mime) {
  size_t i;

/* KNOWN short-circuits; MATCH tests the magic */
#define SNIFF_RULE(cond)                                                       \
  do {                                                                         \
    if (op == SNIFF_QUERY_KNOWN)                                               \
      return HTS_TRUE;                                                         \
    if (cond)                                                                  \
      return HTS_TRUE;                                                         \
  } while (0)

  for (i = 0; sniff_table[i].mime != NULL; i++) {
    if (strfield2(sniff_table[i].mime, mime)) {
      SNIFF_RULE(has_bytes(d, n, sniff_table[i].off, sniff_table[i].bytes,
                           sniff_table[i].len));
    }
  }
  if (mime_in(zip_mimes, mime) || mime_in_prefix(zip_mime_prefixes, mime)) {
    SNIFF_RULE(has_bytes(d, n, 0, "PK\3\4", 4) ||
               has_bytes(d, n, 0, "PK\5\6", 4));
  }
  if (mime_in(ole_mimes, mime)) {
    SNIFF_RULE(has_bytes(d, n, 0, "\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1", 8));
  }
  if (mime_in(tar_mimes, mime)) {
    SNIFF_RULE(has_bytes(d, n, 257, "ustar", 5));
  }
  if (mime_in(ogg_mimes, mime)) {
    SNIFF_RULE(has_bytes(d, n, 0, "OggS\0", 5));
  }
  if (mime_in(ebml_mimes, mime)) {
    SNIFF_RULE(has_bytes(d, n, 0, "\x1a\x45\xdf\xa3", 4));
  }
  if (mime_in(bmff_mimes, mime)) {
    SNIFF_RULE(has_bytes(d, n, 4, "ftyp", 4));
  }
  if (mime_in(avif_mimes, mime)) {
    SNIFF_RULE(has_bytes(d, n, 4, "ftypavif", 8) ||
               has_bytes(d, n, 4, "ftypavis", 8));
  }
  if (mime_in(heic_mimes, mime)) {
    SNIFF_RULE(
        has_bytes(d, n, 4, "ftyphei", 7) || has_bytes(d, n, 4, "ftyphev", 7) ||
        has_bytes(d, n, 4, "ftypmif1", 8) || has_bytes(d, n, 4, "ftypmsf1", 8));
  }
  if (mime_in(asf_mimes, mime)) {
    SNIFF_RULE(has_bytes(d, n, 0, "\x30\x26\xb2\x75\x8e\x66\xcf\x11", 8));
  }
  if (strfield2("audio/x-wav", mime)) {
    SNIFF_RULE(has_bytes(d, n, 0, "RIFF", 4) && has_bytes(d, n, 8, "WAVE", 4));
  }
  if (strfield2("video/x-msvideo", mime)) {
    SNIFF_RULE(has_bytes(d, n, 0, "RIFF", 4) && has_bytes(d, n, 8, "AVI ", 4));
  }
  if (strfield2("image/webp", mime)) {
    SNIFF_RULE(has_bytes(d, n, 0, "RIFF", 4) &&
               has_bytes(d, n, 8, "WEBPVP", 6));
  }
  if (strfield2("image/x-portable-anymap", mime)) {
    SNIFF_RULE(n >= 2 && d[0] == 'P' && d[1] >= '1' && d[1] <= '6');
  }
  if (strfield2("audio/x-aiff", mime)) {
    SNIFF_RULE(
        has_bytes(d, n, 0, "FORM", 4) &&
        (has_bytes(d, n, 8, "AIFF", 4) || has_bytes(d, n, 8, "AIFC", 4)));
  }
  if (strfield2("audio/mpeg", mime)) {
    /* MPEG audio frame sync (11 bits), valid layer and bitrate fields */
    SNIFF_RULE(n >= 2 && d[0] == 0xff && (d[1] & 0xe0) == 0xe0 &&
               (d[1] & 0x06) != 0);
  }
  if (strfield2("audio/aac", mime)) {
    /* ADTS sync */
    SNIFF_RULE(n >= 2 && d[0] == 0xff && (d[1] & 0xf6) == 0xf0);
  }
  if (strfield2("video/mp2t", mime)) {
    SNIFF_RULE(n >= 1 && d[0] == 0x47 && (n <= 188 || d[188] == 0x47));
  }
  if (mime_in(xml_mimes, mime)) {
    SNIFF_RULE(has_text_prefix(d, n, "<?xml"));
  }
  if (mime_in(svg_mimes, mime)) {
    SNIFF_RULE(has_text_prefix(d, n, "<svg") ||
               has_text_prefix(d, n, "<!DOCTYPE svg"));
  }
  if (mime_in(html_mimes, mime)) {
    SNIFF_RULE(has_text_prefix(d, n, "<!DOCTYPE") ||
               has_text_prefix(d, n, "<html") ||
               has_text_prefix(d, n, "<head"));
  }
  if (mime_in(pem_mimes, mime)) {
    SNIFF_RULE(has_text_prefix(d, n, "-----BEGIN"));
  }
  if (strfield2("audio/x-mpegurl", mime)) {
    SNIFF_RULE(has_text_prefix(d, n, "#EXTM3U"));
  }
  if (strfield2("text/x-vcard", mime)) {
    SNIFF_RULE(has_text_prefix(d, n, "BEGIN:VCARD"));
  }
#undef SNIFF_RULE
  return HTS_FALSE;
}

hts_boolean hts_sniff_mime_known(const char *mime) {
  if (mime == NULL || *mime == '\0')
    return HTS_FALSE;
  return sniff_eval(SNIFF_QUERY_KNOWN, NULL, 0, mime);
}

hts_boolean hts_sniff_mime_consistent(const void *data, size_t size,
                                      const char *mime) {
  if (data == NULL || size == 0 || mime == NULL || *mime == '\0')
    return HTS_FALSE;
  return sniff_eval(SNIFF_QUERY_MATCH, (const unsigned char *) data, size,
                    mime);
}
