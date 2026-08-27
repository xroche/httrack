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

/* Sample HTTRACK_NO_SYMBOLIZE, prepare the symbolizer spawn and load the
   unwinder: none of it is signal-safe. Call once, from the process installing
   the fatal handlers. */
void hts_backtrace_init(void);

/* Ensure the calling thread has an alternate signal stack, so a handler
   registered with SA_ONSTACK still runs when the fault is stack exhaustion. One
   already installed is left alone, whoever owns it. Returns the mapping to hand
   back when the thread ends, NULL if nothing was installed, the handler then
   running on the faulting stack as it used to. */
void *hts_backtrace_altstack(void);

/* Unmap what hts_backtrace_altstack() returned, from that same thread: one
   alternate stack per worker adds up over a crawl. */
void hts_backtrace_altstack_release(void *stack);

/* Write the calling thread's stack to stderr, callable from a fatal signal
   handler: raw frames first, then whatever an external symbolizer can name.
   Stderr and not a parameter: hts_backtrace_init() pre-builds the child's
   redirect onto it. Allocates nothing; prints a one-line notice where the OS
   has no backtrace(). */
void hts_print_backtrace(void);

/* Signal-safe decimal rendering of `num` into `buffer`, NUL-terminated;
   returns the length written, at most 11 characters plus the NUL. */
unsigned int hts_print_num(char *buffer, int num);

#endif
