/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2014 Xavier Roche and other contributors

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
/* File: WinHTTrack subroutines:                                */
/*       Crash reporting                                        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

// Initialize crash reporter.
void CrashReportInit(void);

// Report a crash, with msg as additional message, and file:line information
// A stack trace will be generated from the calling point
void CrashReportReport(const char* msg, const char* file, int line);

// Report a crash, with msg as additional message, and file:line information
// also include an optional trace, or no trace if trace is NULL
void CrashReportReportEx(const char* msg, const char* file, int line, const char *trace);
