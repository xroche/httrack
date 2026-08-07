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
/* On-demand crash, to test crash handlers only: httrack's own fatal-signal
   handler, WinHTTrack's, and the Android app's mirror-recovery path. Reached
   through the undocumented -#c option; never called by the engine itself. */
/* ------------------------------------------------------------ */

#ifndef HTSCRASHTEST_DEFH
#define HTSCRASHTEST_DEFH

#include "htsglobal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Crash the process on purpose, after announcing it on stderr so the log tells
   a deliberate crash from a real one. 'kind' selects the fault and defaults to
   "segv" when NULL or empty; hts_crash_test_kinds() lists the accepted names.
   Returns HTS_FALSE, without crashing, only when 'kind' is unknown. */
hts_boolean hts_crash_test(const char *kind);

/* The names hts_crash_test() accepts, comma-separated, for diagnostics. */
const char *hts_crash_test_kinds(void);

#ifdef __cplusplus
}
#endif

#endif
