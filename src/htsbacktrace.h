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
/* File: crash backtrace printer                                 */
/* Author: Xavier Roche                                          */
/* ------------------------------------------------------------ */

#ifndef HTSBACKTRACE_DEFH
#define HTSBACKTRACE_DEFH

#include "htsglobal.h"

/* Sample HTTRACK_NO_SYMBOLIZE and load the unwinder before any crash: neither
   is signal-safe. Call once, from the process installing the fatal handlers. */
void hts_backtrace_init(void);

/* Ensure the calling thread has an alternate signal stack, so a handler
   registered with SA_ONSTACK still runs when the fault is stack exhaustion. One
   already installed is left alone, whoever owns it. Per-thread and kept until
   the thread dies; HTS_FALSE if none could be provided, the handler then
   running on the faulting stack as it used to. */
hts_boolean hts_backtrace_altstack(void);

/* Write the calling thread's stack to fd, callable from a fatal signal handler:
   raw frames first, then whatever an external symbolizer can name. Allocates
   nothing; prints a one-line notice where the OS has no backtrace(). */
void hts_print_backtrace(int fd);

#endif
