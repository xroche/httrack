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

#ifndef HTSSNIFF_DEFH
#define HTSSNIFF_DEFH

#include <stddef.h>
#include "htsglobal.h"

/* Leading-body window read to arbitrate a wire/extension MIME conflict. */
#define HTS_SNIFF_LEN 512

/* Can a magic rule ever confirm this MIME? (whether sniffing is worth it) */
hts_boolean hts_sniff_mime_known(const char *mime);

/* TRUE when the leading body bytes are consistent with the claimed MIME;
   FALSE on unknown MIME, unknown magic, or too-short data (fail-safe). */
hts_boolean hts_sniff_mime_consistent(const void *data, size_t size,
                                      const char *mime);

#endif
