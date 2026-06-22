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
/* File: htsdns_selftest.h                                      */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSDNS_SELFTEST_DEFH
#define HTSDNS_SELFTEST_DEFH

#ifdef HTS_INTERNAL_BYTECODE

#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif

/* Drive the DNS resolver and cache through a scripted (mock) getaddrinfo,
   asserting address family, single-address selection, negative caching, the
   IPv4/IPv6 family filter, and that a cached host is resolved only once.
   Returns the number of failed checks (0 == success). */
int dns_selftests(httrackp *opt);

#endif

#endif
