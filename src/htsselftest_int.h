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
/* File: htsselftest_int.h                                      */
/*       what every engine self-test file needs                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* The self-tests are split by the engine module they exercise, one
   hts<module>_selftest.c each. This is their common prelude: the engine headers
   they all reach for, and the table each of them publishes. */

#ifndef HTSSELFTEST_INT_DEFH
#define HTSSELFTEST_INT_DEFH

#define HTS_INTERNAL_BYTECODE

#include "htsselftest.h"

#include "htsglobal.h"
#include "htscore.h"
#include "htsmodules.h"
#include "htsback.h"
#include "htsdefines.h"
#include "htslib.h"
#include "htsio.h"
#include "htsalias.h"
#include "htsarrays.h"
#include "htsparse.h"
#include "htscache.h"
#include "htscache_selftest.h"
#include "htsdns_selftest.h"
#include "htsparse_selftest.h"
#include "htscatchurl.h"
#include "htscharset.h"
#include "htscmdline.h"
#include "htscoremain.h"
#include "htsencoding.h"
#include "htsescape.h"
#include "htstools.h"
#include "htsftp.h"
#include "htsmd5.h"
#include "htssniff.h"
#include "htscodec.h"
#include "htsproxy.h"
#include "htsrandom.h"
#include "htssitemap.h"
#include "htswarc.h"
#include "htschanges.h"
#include "htscrashtest.h"
#include "htssinglefile.h"
#include "htszlib.h"
#if HTS_USEZSTD
#include <zstd.h>
#endif
#if HTS_USEOPENSSL
#include <openssl/evp.h>
#endif
#include "coucal/coucal.h"

#ifdef _WIN32
#include <sys/utime.h>
#else
#include <pwd.h> /* getpwnam(): an unprivileged euid for the read-only case */
#include <utime.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#ifndef _WIN32
#include <sys/resource.h> /* RLIMIT_AS, to starve one allocation */
#include <sys/socket.h>
#include <unistd.h>
#else
#include <io.h>       /* _get_osfhandle, for the sparse-file hint */
#include <winioctl.h> /* FSCTL_SET_SPARSE */
#endif

#include "htsselftest_util.h"

/* One per hts<module>_selftest.c. A new module is added here and to
   selftest_tables[] in htsselftest.c. */
extern const struct selftest_entry selftests_back[];
extern const struct selftest_entry selftests_lib[];
extern const struct selftest_entry selftests_cache[];
extern const struct selftest_entry selftests_charset[];
extern const struct selftest_entry selftests_cookie[];
extern const struct selftest_entry selftests_dns[];
extern const struct selftest_entry selftests_filters[];
extern const struct selftest_entry selftests_header[];
extern const struct selftest_entry selftests_io[];
extern const struct selftest_entry selftests_mime[];
extern const struct selftest_entry selftests_name[];
extern const struct selftest_entry selftests_net[];
extern const struct selftest_entry selftests_opt[];
extern const struct selftest_entry selftests_parse[];
extern const struct selftest_entry selftests_warc[];
extern const struct selftest_entry selftests_wizard[];

#endif
