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
/* File: htsmirror.h mirror loop                                */
/*       redirects, user interaction, queue wait                */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSMIRROR_DEFH
#define HTSMIRROR_DEFH

#include "htsglobal.h"
#include "htsparse.h"

/*
  Check for 301,302.. errors ("moved") and handle them; re-isuue requests, make
  rediretc file, handle filters considerations..
  Parameters: The public module structure, and the private module variables
  Returns 0 upon success
*/
int hts_mirror_check_moved(htsmoduleStruct *str, htsmoduleStructExtended *stre);

/*
  Take the pending request NAME (HTS_ABORT_LOCKNAME or HTS_PAUSE_LOCKNAME) in
  the output directory, deleting the file, and answer whether the mirror must
  act on it. True only for a request newer than this run's own
  hts-in_progress.lock and deletable, so neither one inherited from an earlier
  run nor one the engine cannot remove ever reaches the mirror.
*/
hts_boolean hts_take_lock_request(httrackp *opt, const char *name);

/*
  Process user intercations: pause, add link, delete link..
*/
void hts_mirror_process_user_interaction(htsmoduleStruct *str,
                                         htsmoduleStructExtended *stre);

/*
  Get the next file on the queue, waiting for it, handling other files in
  background.. Parameters: The public module structure, and the private module
  variables Returns 0 upon success
*/
int hts_mirror_wait_for_next_file(htsmoduleStruct *str,
                                  htsmoduleStructExtended *stre);

/*
  Wait for (adr, fil, save) to be started, that is,
  to be ready for naming, having its header MIME type
  If the final URL is to be forbidden, sets 'forbidden_url' to the corresponding
  value
*/
/* When fetch is not NULL, each redirect hop followed is merged into it. */
int hts_wait_delayed(htsmoduleStruct *str, lien_adrfilsave *afs,
                     char *parent_adr, char *parent_fil, lien_adrfil *former,
                     int *forbidden_url, hts_fetch_meta *fetch);

#endif
