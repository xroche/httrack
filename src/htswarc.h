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
/* HTTrack WARC/1.1 output writer (ISO 28500). Internal, not installed.
   All WARC record serialization, gzip-member framing, digests, UUID and
   revisit dedup live here; the engine only stashes the request, frees it,
   and calls one entry point per finished transaction. */
/* ------------------------------------------------------------ */

#ifndef HTS_WARC_DEFH
#define HTS_WARC_DEFH

#include "htsopt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* opt->warc_file sentinel: --warc with no argument => auto-name the archive
   under the project's output directory at open time. */
#define WARC_AUTONAME "\001auto"

/* htsblk.warc_truncated / WARC-Truncated reason tokens (ISO 28500 sec 5.13).
   A cap-truncated body is still archived, tagged with why it was cut short. */
#define WARC_TRUNC_NONE 0
#define WARC_TRUNC_LENGTH 1     /* hit a size cap (-M mirror / -m per-file) */
#define WARC_TRUNC_TIME 2       /* hit the mirror time cap (-E/--max-time) */
#define WARC_TRUNC_DISCONNECT 3 /* connection dropped mid-body */

/* WARC-Truncated token for a warc_truncated code, or NULL for none. */
const char *warc_truncated_reason(int code);

typedef struct warc_writer warc_writer;

/* Stash the raw request header block (bstr.buffer) on r for the later WARC
   request record; frees any prior stash. No-op when reqhdr is NULL. */
void warc_stash_request(htsblk *r, const char *reqhdr);

/* Stash the raw response header block: deletehttp frees r.headers when the
   socket closes, before back_finalize, so keep a WARC-owned copy. */
void warc_stash_response(htsblk *r, const char *resphdr);

/* Free both stashed header blocks (idempotent, NULL-safe). */
void warc_free_request(htsblk *r);

/* Emit the request + response (or revisit) records for one finished
   transaction. Lazily opens the writer into opt->state.warc; a no-op (logged
   once) if the archive cannot be created. */
void warc_write_backtransaction(httrackp *opt, lien_back *back);

/* Close and free the writer held in opt->state.warc, if any. */
void warc_close_opt(httrackp *opt);

/* --- Direct writer API (used by the hooks above and the self-test). --- */

/* Create the archive at path (auto-named when path is WARC_AUTONAME), writing
   the warcinfo record. .warc.gz => one gzip member per record; .warc => raw.
   Returns NULL on failure. */
warc_writer *warc_open(httrackp *opt, const char *path);

/* Flush, close and free the writer (NULL-safe). */
void warc_close(warc_writer *w);

/* Write one transaction's request + response (or revisit) records.
   target_uri:  absolute URL fetched.
   ip:          numeric peer IP, or NULL/"" to omit.
   req_hdr:     exact request header block sent, or NULL to skip the request.
   resp_hdr:    raw received response header block (status line + headers).
   body/body_len: decoded in-memory body, or NULL when on disk.
   body_path:   file re-read for the body when body==NULL (may be NULL).
   is_update_unchanged: nonzero for a 304 server-not-modified revisit.
   truncated: a WARC_TRUNC_* reason to tag a cap-truncated body, else 0.
   keep_content_encoding: strategy A: body/body_len are the as-received coded
     bytes, so keep Content-Encoding and set Content-Length to body_len.
   Returns 0 on success, -1 on error. */
int warc_write_transaction(warc_writer *w, const char *target_uri,
                           const char *ip, const char *req_hdr,
                           const char *resp_hdr, const char *body,
                           size_t body_len, const char *body_path,
                           int statuscode, int is_update_unchanged,
                           int truncated, int keep_content_encoding);

/* Write one non-HTTP capture as a single WARC 'resource' record: the block is
   the raw payload (no HTTP envelope), Content-Type is the payload's own MIME.
   Used for ftp:// transfers. truncated is a WARC_TRUNC_* reason or 0.
   Returns 0 on success, -1 on error. */
int warc_write_resource(warc_writer *w, const char *target_uri, const char *ip,
                        const char *content_type, const char *body,
                        size_t body_len, const char *body_path, int truncated);

#ifdef __cplusplus
}
#endif

#endif
