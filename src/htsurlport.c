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
/* File: TCP port parser, shared by the engine, htsserver and    */
/*       proxytrack                                              */
/* Author: Xavier Roche                                          */
/* ------------------------------------------------------------ */

#include "htsurlport.h"

#include <ctype.h>
#include <stdlib.h>

hts_boolean hts_parse_url_port(const char *a, int *port) {
  char *end;
  long p;

  if (!isdigit((unsigned char) *a)) // strtol would eat a sign or leading space
    return HTS_FALSE;
  p = strtol(a, &end, 10);
  if (*end != '\0' || p < 1 || p > 65535) // ERANGE lands out of range too
    return HTS_FALSE;
  *port = (int) p;
  return HTS_TRUE;
}
