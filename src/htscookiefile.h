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

#ifndef HTSCOOKIEFILE_DEFH
#define HTSCOOKIEFILE_DEFH

#include <stdio.h>

#include "htsbauth.h"

/* A cookies.txt line marks HttpOnly by this prefix. */
#define COOKIE_HTTPONLY_PREFIX "#HttpOnly_"
/* Internal: the cookie jar file writer, used by htsbauth.c. */

/* Is the record at rec host-only? Defined in htsbauth.c. */
hts_boolean cookie_is_host_only(const char *rec);

/* Flush fp to the disk and close it. Did every step succeed? */
hts_boolean cookie_close_synced(FILE *fp);

/* Write the jar to name, or only a header if it is empty. Return 0 on
   success. */
int cookie_write(t_cookie *cookie, const char *name);

/* Name in dst a temporary beside path that no other run or thread writes. */
hts_boolean cookie_tmpname(char *dst, size_t size, const char *path);

/* Make bak hold what file holds, leaving file in place. */
hts_boolean cookie_backup(const char *file, const char *bak);

/* Move tmp over file in one step. */
hts_boolean cookie_replace(const char *tmp, const char *file);

#endif
