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
/* File: htssafe.h safe strings operations, and asserts         */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSSAFE_DEFH
#define HTSSAFE_DEFH

#include "htsglobal.h"

/**
 * Optional user-defined callback upon fatal error.
 */
typedef void (*htsErrorCallback) (const char *msg, const char *file, int line);

/**
 * Emergency logging.
 */
#ifndef HTSSAFE_ABORT_FUNCTION
HTSEXT_API htsErrorCallback htsCallbackErr;
#define HTSSAFE_ABORT_FUNCTION(A,B,C) do { if (htsCallbackErr != NULL) { htsCallbackErr(A,B,C); } } while(0)
#endif

/**
 * Log an abort condition, and calls abort().
 */
#define abortLog(a) abortf_(a, __FILE__, __LINE__)

/**
 * Fatal assertion check.
 */
#define assertf__(exp, sexp, file, line) (void) ( (exp) || (abortf_(sexp, file, line), 0) )

/**
 * Fatal assertion check.
 */
#define assertf_(exp, file, line) assertf__(exp, #exp, __FILE__, __LINE__)

/**
 * Fatal assertion check.
 */
#define assertf(exp) assertf_(exp, __FILE__, __LINE__)

static void log_abort_(const char *msg, const char *file, int line) {
  fprintf(stderr, "%s failed at %s:%d\n", msg, file, line);
  fflush(stderr);
}

static void abortf_(const char *exp, const char *file, int line) {
  HTSSAFE_ABORT_FUNCTION(exp, file, line);
  log_abort_(exp, file, line);
  abort();
}

/**
 * Append at most N characters from "B" to "A".
 * If "A" is a char[] variable whose size is not sizeof(char*), then the size 
 * is assumed to be the capacity of this array.
 */
#define strncatbuff(A, B, N) \
  ( sizeof(A) == sizeof(char*) \
  ? strncat(A, B, N) \
  : strncat_safe_(A, sizeof(A), B, \
  sizeof(B) == sizeof(char*) ? (size_t) -1 : sizeof(B), N, \
  "overflow while copying '" #B "' to '"#A"'", __FILE__, __LINE__) )

/* note: "size_t is an unsigned integral type" */

/**
 * Append characters of "B" to "A".
 * If "A" is a char[] variable whose size is not sizeof(char*), then the size 
 * is assumed to be the capacity of this array.
 */
#define strcatbuff(A, B) strncatbuff(A, B, (size_t) -1)

/**
 * Copy characters of "B" to "A".
 * If "A" is a char[] variable whose size is not sizeof(char*), then the size 
 * is assumed to be the capacity of this array.
 */
#define strcpybuff(A, B) strcatbuff(clear_buffer_(A), B)

static HTS_INLINE size_t strlen_safe_(const char *source, const size_t sizeof_source, 
                                      const char *file, int line) {
  size_t size;
  assertf_( source != NULL, file, line );
  size = strnlen(source, sizeof_source);
  assertf_( size < sizeof_source, file, line );
  return size;
}

static HTS_INLINE char* clear_buffer_(char *buffer) {
  buffer[0] = '\0';
  return buffer;
}

static HTS_INLINE char* strncat_safe_(char *const dest, const size_t sizeof_dest,
                                      const char *const source, const size_t sizeof_source, 
                                      const size_t n,
                                      const char *exp, const char *file, int line) {
  const size_t source_len = strlen_safe_(source, sizeof_source, file, line);
  const size_t dest_len = strlen_safe_(dest, sizeof_dest, file, line);
  const size_t source_copy = source_len <= n ? source_len : n;
  const size_t dest_final_len = dest_len + source_copy;
  assertf__(dest_final_len < sizeof_dest, exp, file, line);
  memcpy(dest + dest_len, source, source_copy);
  dest[dest_final_len] = '\0';
  return dest;
}

#define malloct(A)          malloc(A)
#define calloct(A,B)        calloc((A), (B))
#define freet(A)            do { if ((A) != NULL) { free(A); (A) = NULL; } } while(0)
#define strdupt(A)          strdup(A)
#define realloct(A,B)       realloc(A, B)
#define memcpybuff(A, B, N) memcpy((A), (B), (N))

#endif
