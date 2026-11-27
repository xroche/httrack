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

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: htsmono.h monotonic clock readings                    */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSMONO_DEFH
#define HTSMONO_DEFH

#include "htsglobal.h"

#include <stdint.h>

/* Milliseconds off a clock NTP cannot step, for an elapsed time or a
   deadline. Its zero is arbitrary, so it measures an interval and never names
   a date; anything stored or sent still needs mtime_local(). */
TStamp mtime_monotonic(void);

/* A mtime_monotonic() reading. The struct makes adding seconds or a wall-clock
   stamp to it a compile error, so units go through the named helpers below. */
typedef struct hts_mono {
  TStamp ms;
} hts_mono;

static HTS_INLINE HTS_UNUSED hts_mono hts_mono_from_ms(TStamp ms) {
  const hts_mono t = {ms};

  return t;
}

static HTS_INLINE HTS_UNUSED hts_mono hts_mono_now(void) {
  return hts_mono_from_ms(mtime_monotonic());
}

/* A deadline no reading reaches. */
static HTS_INLINE HTS_UNUSED hts_mono hts_mono_never(void) {
  return hts_mono_from_ms(INT64_MAX);
}

static HTS_INLINE HTS_UNUSED hts_mono hts_mono_add_ms(hts_mono t, TStamp ms) {
  return hts_mono_from_ms(t.ms + ms);
}

static HTS_INLINE HTS_UNUSED hts_mono hts_mono_add_s(hts_mono t, int s) {
  return hts_mono_add_ms(t, (TStamp) s * 1000);
}

/* Milliseconds from since to now. */
static HTS_INLINE HTS_UNUSED TStamp hts_mono_diff_ms(hts_mono now,
                                                     hts_mono since) {
  return now.ms - since.ms;
}

static HTS_INLINE HTS_UNUSED hts_boolean hts_mono_before(hts_mono a,
                                                         hts_mono b) {
  return a.ms < b.ms;
}

/* Has a timeout of timeout_s seconds run out between since and now? A
   timeout_s <= 0 has always run out. */
static HTS_INLINE HTS_UNUSED hts_boolean hts_timeout_elapsed(hts_mono since,
                                                             hts_mono now,
                                                             int timeout_s) {
  return hts_mono_diff_ms(now, since) >= (TStamp) timeout_s * 1000;
}

#endif
