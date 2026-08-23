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
/* File: basic FTP protocol manager .h                          */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSFTP_DEFH
#define HTSFTP_DEFH

#include "htsbase.h"
#include "htsbasenet.h"
#include "htsthread.h"

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_lien_back
#define HTS_DEF_FWSTRUCT_lien_back
typedef struct lien_back lien_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif

/* Download structure */
#ifndef HTS_DEF_FWSTRUCT_FTPDownloadStruct
#define HTS_DEF_FWSTRUCT_FTPDownloadStruct
typedef struct FTPDownloadStruct FTPDownloadStruct;
#endif
struct FTPDownloadStruct {
  lien_back *pBack;
  httrackp *pOpt;
  FTPDownloadStruct *pNext; /* live-worker list, owned by htsftp.c */
};

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE
/* Capacity of every FTP control-line buffer; send_line() adds the CRLF. */
#define FTP_LINE_SIZE 1024

#if USE_BEGINTHREAD
void launch_ftp(FTPDownloadStruct * params);
void back_launch_ftp(void *pP);
/* Cancel every live FTP worker and block until each stops touching its backlog
   slot and opt. Call before freeing either. */
void ftp_stop_workers(void);
#else
void launch_ftp(FTPDownloadStruct * params, char *path, char *exec);
int back_launch_ftp(FTPDownloadStruct * params);
#endif

int run_launch_ftp(FTPDownloadStruct * params);
int send_line(T_SOC soc, const char *data);
/* Read one control-channel reply into line[line_size]. Returns 0 on error.
   Optional back ends the wait on a stop, opt clips it to --max-time. */
int get_ftp_line(lien_back *back, T_SOC soc, char *line, size_t line_size,
                 int timeout, const httrackp *opt);
/* Start of the authority in a URL address: the scheme and its slashes skipped.
   Bracket-aware, unlike a hand-rolled split on the first ':'. */
char *ftp_jump_authority(char *url_adr);
/* Split "host[:port]" (host may be a "[IPv6]" literal) into host[host_size] and
   *port. Returns HTS_FALSE with host emptied and the reason in err[err_size]
   when the host does not fit or the port is malformed; an empty ":" keeps the
   caller's default port (#614). */
hts_boolean ftp_split_hostport(const char *adr, char *host, size_t host_size,
                               int *port, char *err, size_t err_size);
/* Split a "user[:pass]@" prefix (end = jump_identification result) into
   NUL-terminated user/pass buffers. Returns HTS_FALSE and empties both when a
   field does not fit, as a clipped one would name another account.
   Both sizes must be nonzero. */
hts_boolean ftp_split_userpass(const char *src, const char *end, char *user,
                               size_t user_size, char *pass, size_t pass_size);
/* ftp_split_userpass() into the caller's fixed buffers; a buffer too wide for
   its "USER <user>" line would be clipped again when the command is built. */
#define ftp_split_userpass_buf(src, end, user, pass)                           \
  (HTS_COMPILE_ASSERT(sizeof(user) + sizeof("USER ") - 1 <= FTP_LINE_SIZE &&   \
                      sizeof(pass) + sizeof("PASS ") - 1 <= FTP_LINE_SIZE),    \
   ftp_split_userpass((src), (end), (user), sizeof(user), (pass),              \
                      sizeof(pass)))
/* Build "<verb> <path>" into line[line_size]. The path is quoted whenever a
   bare one would give the server a second token; it must already have been
   screened for control bytes. Returns HTS_FALSE and empties line when the
   command does not fit, as a clipped one would name a different file. */
hts_boolean ftp_command(char *line, size_t line_size, const char *verb,
                        const char *path);
/* ftp_command() into a control line of the one capacity every FTP buffer has;
   anything narrower fails the build rather than refusing a path that fits. */
#define ftp_command_line(line, verb, path)                                     \
  (HTS_COMPILE_ASSERT(sizeof(line) == FTP_LINE_SIZE),                          \
   ftp_command((line), sizeof(line), (verb), (path)))
T_SOC get_datasocket(char *to_send, size_t to_send_size);
int stop_ftp(lien_back * back);
char *linejmp(char *line);
int check_socket(T_SOC soc);
int check_socket_connect(T_SOC soc);
/* Wait up to timeout seconds for soc to become readable. Optional back ends the
   wait on a stop, opt clips it to --max-time. */
int wait_socket_receive(lien_back *back, T_SOC soc, int timeout,
                        const httrackp *opt);
#endif

#endif
