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
/* File: Out-of-line htsnet.h helpers, kept here so the installed */
/*       header needs nothing beyond strict ISO C                 */
/* Author: Xavier Roche                                           */
/* ------------------------------------------------------------ */

#include "htsnet.h"

HTSNET_API void SOCaddr_inetntoa_(char *namebuf, size_t namebuflen,
                                  SOCaddr *const ss, const char *file,
                                  const int line) {
  assertf_(namebuf != NULL, file, line);
  assertf_(ss != NULL, file, line);

  if (getnameinfo(&ss->m_addr.sa, sizeof(ss->m_addr), namebuf, namebuflen, NULL,
                  0, NI_NUMERICHOST) == 0) {
    /* remove scope id(s) */
    char *const pos = strchr(namebuf, '%');
    if (pos != NULL) {
      *pos = '\0';
    }
  } else {
    namebuf[0] = '\0';
  }
}

HTSNET_API hts_boolean SOCaddr_inetntoa_port_(char *namebuf, size_t namebuflen,
                                              SOCaddr *const ss,
                                              const char *file,
                                              const int line) {
  const size_t reserved = sizeof(":65535") - 1;
  size_t used;

  assertf_(namebuf != NULL && namebuflen != 0, file, line);
  namebuf[0] = '\0';
  if (namebuflen <= reserved)
    return HTS_FALSE;
  SOCaddr_inetntoa_(namebuf, namebuflen - reserved, ss, file, line);
  used = strlen(namebuf);
  return slcatprintfbuff(
      namebuf, namebuflen, &used, ":%u",
      (unsigned int) ntohs(*SOCaddr_sinport_(ss, file, line)));
}
