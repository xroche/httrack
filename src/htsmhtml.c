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
/* File: MIME-HTML (index.mht) writer                           */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htscore.h"
#include "htsbase.h"
#include "htsio.h"

/* MIME/MHTML writer states, held in opt->state.mimehtml_created. */
#define MIMEHTML_NONE 0    /* no part written yet */
#define MIMEHTML_WRITING 1 /* writing index.mht */
#define MIMEHTML_HELD 2    /* holding parts in ~index.mht.part, no root yet */
#define MIMEHTML_FAILED (-1)

/* Without start=, RFC 2387 defaults the root to the first part, whichever file
   the mirror happened to finish first. `startcid` is NULL when the archive
   holds no page. */
static void mimehtml_write_header(httrackp *opt, const char *startcid) {
  char BIGSTK currtime[256];

  time_gmt_rfc822(currtime);
  fprintf(opt->state.mimefp,
          "From: HTTrack Website Copier <nobody@localhost>\r\n"
          "Subject: Local mirror\r\n"
          "Date: %s\r\n"
          "Message-ID: <httrack_%d_%d@localhost>\r\n"
          "Content-Type: multipart/related;\r\n"
          "\tboundary=\"%s\";\r\n"
          "\ttype=\"text/html\"",
          currtime, (int) time(NULL), (int) rand(),
          StringBuff(opt->state.mimemid));
  if (startcid != NULL) {
    fprintf(opt->state.mimefp, ";\r\n\tstart=\"<%s>\"", startcid);
  }
  fprintf(opt->state.mimefp,
          "\r\nMIME-Version: 1.0\r\n"
          "\r\nThis message is a RFC MIME-compliant multipart message.\r\n"
          "\r\n");
}

/* Create index.mht with `startcid` as its root, and move in the parts held in
   opt->state.mimefp. */
static void mimehtml_create(httrackp *opt, const char *startcid) {
  FILE *const held = opt->state.mimefp;
  char BIGSTK heldname[HTS_URLMAXSIZE * 2];

  strcpybuff(heldname, fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                               StringBuff(opt->path_html), "~index.mht.part"));
  opt->state.mimefp = FOPEN(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                    StringBuff(opt->path_html), "index.mht"),
                            "wb");
  if (opt->state.mimefp == NULL) {
    if (held != NULL) {
      fclose(held);
      (void) UNLINK(heldname);
    }
    opt->state.mimehtml_created = MIMEHTML_FAILED;
    hts_log_print(opt, LOG_ERROR, "unable to create index.mht");
    return;
  }
  (void) UNLINK(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                        StringBuff(opt->path_html), "index.eml"));
#ifndef _WIN32
  if (symlink("index.mht", fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                   StringBuff(opt->path_html), "index.eml")) !=
      0) {
    if (errno != EPERM) {
      hts_log_print(
          opt, LOG_WARNING | LOG_ERRNO,
          "could not create symbolic link from index.mht to index.eml");
    }
  }
#endif
  mimehtml_write_header(opt, startcid);
  if (held != NULL) {
    char BIGSTK buff[32768];
    size_t len;

    rewind(held);
    while ((len = fread(buff, 1, sizeof(buff), held)) > 0) {
      if (fwrite(buff, 1, len, opt->state.mimefp) != len) {
        hts_log_print(opt, LOG_ERROR | LOG_ERRNO,
                      "index.mht is truncated, could not move in %s", heldname);
        break;
      }
    }
    fclose(held);
    (void) UNLINK(heldname);
  }
  opt->state.mimehtml_created = MIMEHTML_WRITING;
}

void mimehtml_add_file(httrackp *opt, const char *save, const char *adr,
                       const char *fil) {
  /* MIME-html archive to build */
  if (opt != NULL && opt->mimehtml) {
    if (adr != NULL && strcmp(adr, "primary") == 0) {
      adr = NULL;
    }
    if (save != NULL && opt != NULL && adr != NULL && adr[0] &&
        strnotempty(save) && fexist_utf8(save)) {
      const char *rsc_save = save;
      const char *rsc_fil = strrchr(fil, '/');
      /* Read the part before naming it as start=, since a part that fails to
         open must never become the root. */
      FILE *const fp = FOPEN(save, "rb");
      int n;

      if (rsc_fil == NULL)
        rsc_fil = fil;
      if (strncmp(fslash(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), save),
                  fslash(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                         StringBuff(opt->path_html_utf8)),
                  (n = (int) strlen(StringBuff(opt->path_html_utf8)))) == 0) {
        rsc_save += n;
      }

      if (fp != NULL) {
        const int isHtml = (ishtml(opt, save) == 1);
        char BIGSTK cid[HTS_URLMAXSIZE * 3];

        make_content_id(adr, fil, cid, sizeof(cid));
        if (opt->state.mimehtml_created == MIMEHTML_NONE) {
          srand((unsigned int) time(NULL));
          StringRoom(opt->state.mimemid, 256);
          snprintf(StringBuffRW(opt->state.mimemid), 256,
                   "----=_MIMEPart_%d_%d_=----", (int) time(NULL),
                   (int) rand());
          StringSetLength(opt->state.mimemid, -1);
          if (isHtml) {
            mimehtml_create(opt, cid);
          } else {
            /* Hold it aside until a page turns up to be the root. A saved
               name never carries a '~', so no mirrored file lands here. */
            opt->state.mimefp =
                FOPEN(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                              StringBuff(opt->path_html), "~index.mht.part"),
                      "w+b");
            if (opt->state.mimefp == NULL) {
              opt->state.mimehtml_created = MIMEHTML_FAILED;
              hts_log_print(opt, LOG_ERROR, "unable to create index.mht");
            } else {
              opt->state.mimehtml_created = MIMEHTML_HELD;
            }
          }
        } else if (opt->state.mimehtml_created == MIMEHTML_HELD && isHtml) {
          mimehtml_create(opt, cid);
        }
        if (opt->state.mimefp != NULL) {
          char buff[60 * 100];
          char mimebuff[256];
          size_t len;

          mimebuff[0] = '\0';
          guess_httptype_sized(opt, mimebuff, sizeof(mimebuff), save);
          fprintf(opt->state.mimefp, "--%s\r\n",
                  StringBuff(opt->state.mimemid));
          fprintf(opt->state.mimefp,
                  "Content-disposition: attachment; filename=\"%s\"\r\n",
                  rsc_save);
          fprintf(opt->state.mimefp,
                  "Content-Type: %s\r\n"
                  "Content-Transfer-Encoding: %s\r\n"
                  "Content-ID: <%s>\r\n"
                  "\r\n",
                  mimebuff, isHtml ? "8bit" : "base64", cid);
          while ((len = fread(buff, 1, sizeof(buff), fp)) > 0) {
            if (!isHtml) {
              char base64buff[60 * 100 * 2];

              code64((unsigned char *) buff, (int) len,
                     (unsigned char *) base64buff, 1);
              fprintf(opt->state.mimefp, "%s", base64buff);
            } else {
              fwrite(buff, 1, len, opt->state.mimefp);
            }
          }
          fprintf(opt->state.mimefp, "\r\n\r\n");
        }
        fclose(fp);
      }
    } else if (save == NULL) {
      /* The mirror saved no page, so the archive gets no root. */
      if (opt->state.mimehtml_created == MIMEHTML_HELD) {
        mimehtml_create(opt, NULL);
      }
      if (opt->state.mimehtml_created == MIMEHTML_WRITING &&
          opt->state.mimefp != NULL) {
        fprintf(opt->state.mimefp, "--%s--\r\n",
                StringBuff(opt->state.mimemid));
        fclose(opt->state.mimefp);
        opt->state.mimefp = NULL;
      }
    }
  }
}
