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
/* File: Global engine definition file                          */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */
/** @file htsconfig.h
    Fixed compile-time tuning constants of the crawler engine. Configure probes
    none of them, so a consumer reads the values the library was built with. */

#ifndef HTTRACK_GLOBAL_ENGINE_DEFH
#define HTTRACK_GLOBAL_ENGINE_DEFH

/** Should a mirror be readable by other users? 1 makes the HTS_ACCESS_* modes
    in htsglobal.h grant group and other access. */
#define HTS_ACCESS 1

/** Seconds the engine waits for socket activity in one poll. */
#define HTS_SOCK_SEC 0
/** Microseconds of that same wait, despite the name. The pair makes 1/10s. */
#define HTS_SOCK_MS 100000

/** Name given to a URL that ends with a slash. */
#define DEFAULT_HTML "index.html"

/** Name given to an FTP URL that ends with a slash. */
#define DEFAULT_FTP "index.txt"

/** Extension the -N name template gives a URL that carries none. */
#define DEFAULT_EXT ".html"
/** Its 8.3 form, used when the mirror saves short names. */
#define DEFAULT_EXT_SHORT ".htm"

/** Should DOS device names (nul, con, ...) and a trailing dot or space be
    escaped out of save paths? */
#define HTS_OVERRIDE_DOS_FOLDERS 1

/** Should the keyword indexer be built in? */
#define HTS_MAKE_KEYWORD_INDEX 1

/** Should the engine read stdin, so pressing ENTER toggles the progress
    display? */
#define HTS_POLL 1

/** A URL ending with a slash is HTML (example/ is always HTML). Nothing in the
    tree reads this. */
#define HTS_SLASH_ISHTML 1

/** Should a plain file be renamed to "<name>.txt" when the mirror needs a
    directory of the same name? */
#define HTS_REMOVE_ANNOYING_INDEX 1

/** Should non-HTML data go straight to disk instead of being held in memory? */
#define HTS_DIRECTDISK 1

/** Always write straight to disk. Nothing in the tree reads this. */
#define HTS_DIRECTDISK_ALWAYS 1

/** Should a bare ">" end an HTML comment when no "-->" follows anywhere, the
    way browsers accept <!-- foo >? */
#define GT_ENDS_COMMENT 1

/** Should a path holding a "~" gain a trailing slash (/~smith -> /~smith/)? */
#define HTS_TILDE_SLASH 0

/** Should "//" inside a link collapse to "/" when --urlhack is on? */
#define HTS_STRIP_DOUBLE_SLASH 0

/** Should a failed download delete the partial file? Off, because a retry would
    then have nothing left to resume. */
#define HTS_REMOVE_BAD_FILES 0

/** Filter slots the engine adds each time the filter array fills up. */
#define HTS_FILTERSINC 1000

/** Should connect() be non-blocking? */
#define HTS_XCONN 1

/** Should a host name lookup run in its own thread, so it does not block the
    crawl? */
#define HTS_XGETHOST 1

/** Seconds a transfer must run before the engine compares its rate against the
    minimum the user asked for. */
#define HTS_WATCHRATE 15

#endif
