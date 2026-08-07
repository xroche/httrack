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
/* System CSPRNG. Its own file because htsserver links libhttrack but cannot
   see a hidden symbol inside it, and both need this one. */
/* ------------------------------------------------------------ */

#ifndef HTS_RANDOM_DEFH
#define HTS_RANDOM_DEFH

#include "htsglobal.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fill buf with len cryptographically strong random bytes. HTS_FALSE when no
   such source is available: the caller must fail, never fall back to rand(). */
hts_boolean hts_random_bytes(void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif
