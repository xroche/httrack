/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2015 Xavier Roche and other contributors

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
/* File: httrack.c subroutines:                                 */
/*       wrapper system (for shell                              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htswrap.h"
#include "htshash.h"
#include "coucal.h"
#include "htslib.h"

HTSEXT_API int htswrap_init(void) {     // LEGACY
  return 1;
}

HTSEXT_API int htswrap_free(void) {     // LEGACY
  return 1;
}

HTSEXT_API int htswrap_add(httrackp * opt, const char *name, void *fct) {
  return hts_set_callback((t_hts_htmlcheck_callbacks *) opt->callbacks_fun,
                          name, fct);
}

HTSEXT_API uintptr_t htswrap_read(httrackp * opt, const char *name) {
  return (uintptr_t) hts_get_callback((t_hts_htmlcheck_callbacks *) opt->
                                      callbacks_fun, name);
}
