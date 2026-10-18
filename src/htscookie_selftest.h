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
/* File: htscookie_selftest.h                                   */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSCOOKIE_SELFTEST_DEFH
#define HTSCOOKIE_SELFTEST_DEFH

#ifdef HTS_INTERNAL_BYTECODE

/* Each bit makes one step of a cookie save fail in the self-tests. */
#define COOKIE_FAULT_NOLINK 1     /**< back up by copy, as without hard links */
#define COOKIE_FAULT_REPLACE 2    /**< the final move fails */
#define COOKIE_FAULT_COPY_WRITE 4 /**< the copy writes half of a block */
#define COOKIE_FAULT_COPY_READ 8  /**< the copy reports a read error */
#define COOKIE_FAULT_COPY_CLOSE 16 /**< closing the copy fails */
#define COOKIE_FAULT_WRITE 32      /**< the jar write fails after its header */
#define COOKIE_FAULT_CLOSE 64      /**< closing the jar write fails */

/* The self-tests set the faults to inject here. It stays 0 otherwise. */
extern int cookie_save_fault_selftest;

#endif

#endif
