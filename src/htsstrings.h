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
/* File: Strings                                                */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Safer Strings ; standalone .h library */

#ifndef HTS_STRINGS_DEFSTATIC
#define HTS_STRINGS_DEFSTATIC

/* System definitions. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "htssafe.h"

/* GCC extension */
#ifndef HTS_UNUSED
#ifdef __GNUC__
#define HTS_UNUSED __attribute__((unused))

#define HTS_STATIC static __attribute__((unused))

#define HTS_PRINTF_FUN(fmt, arg) __attribute__((format(printf, fmt, arg)))
#else
#define HTS_UNUSED
#define HTS_STATIC static
#define HTS_PRINTF_FUN(fmt, arg)
#endif
#endif

/** Forward definitions **/
#ifndef HTS_DEF_FWSTRUCT_String
#define HTS_DEF_FWSTRUCT_String
typedef struct String String;
#endif
#ifndef HTS_DEF_STRUCT_String
#define HTS_DEF_STRUCT_String

/**
 * Growable owned string.
 *
 * Ownership/lifetime: the String owns buffer_ and frees it (StringFree).
 * buffer_ is allocated lazily, so a freshly STRING_EMPTY/StringInit'd String,
 * or one just StringFree'd/StringAcquire'd, has buffer_ == NULL and
 * length_ == capacity_ == 0. Any growing operation may realloc, so a pointer
 * obtained from StringBuff/StringBuffRW is invalidated by the next append,
 * copy, or room request; do not cache it across such calls.
 *
 * Invariants when buffer_ != NULL: length_ < capacity_, and buffer_[length_]
 * is a NUL (the content is always NUL-terminated). length_ excludes that NUL;
 * capacity_ counts it. The empty state (buffer_ == NULL) has no readable NUL,
 * so callers must not treat StringBuff() of an untouched String as "".
 *
 * Direct field access is internal (trailing underscore); use the macros below.
 */
struct String {
  char *buffer_;
  size_t length_;
  size_t capacity_;
};
#endif

/** Allocator **/
#ifndef STRING_REALLOC
#define STRING_REALLOC(BUFF, SIZE) ((char *) realloct(BUFF, SIZE))

#define STRING_FREE(BUFF) freet(BUFF)
#endif

/** Initializer for an empty String (NULL buffer). Use to declare or reset. **/
#define STRING_EMPTY {(char *) NULL, 0, 0}

/** Read-only buffer pointer. NULL until the String has been written to.
    Invalidated by any subsequent growing operation. **/
#define StringBuff(BLK) ((const char *) ((BLK).buffer_))

/** Read/write buffer pointer. Same NULL/invalidation rules as StringBuff. **/
#define StringBuffRW(BLK) ((BLK).buffer_)

/** Current length in bytes, excluding the terminating NUL. **/
#define StringLength(BLK) ((BLK).length_)

/** Non-zero if the String holds at least one byte. **/
#define StringNotEmpty(BLK) (StringLength(BLK) > 0)

/** Allocated capacity in bytes, including room for the terminating NUL. **/
#define StringCapacity(BLK) ((BLK).capacity_)

/** Byte at POS (read). No bounds check; POS must be < StringLength. **/
#define StringSub(BLK, POS) (StringBuff(BLK)[POS])

/** Byte at POS (read/write). No bounds check; POS must be < StringLength. **/
#define StringSubRW(BLK, POS) (StringBuffRW(BLK)[POS])

/** Byte POS positions from the end (read). POS==1 is the last byte. **/
#define StringRight(BLK, POS) (StringBuff(BLK)[StringLength(BLK) - POS])

/** Byte POS positions from the end (read/write). POS==1 is the last byte. **/
#define StringRightRW(BLK, POS) (StringBuffRW(BLK)[StringLength(BLK) - POS])

/** Drop the last byte and re-terminate. No-op on an empty String: the length
    is unsigned, so an unguarded pop would wrap it and write off the end. **/
#define StringPopRight(BLK)                                                    \
  do {                                                                         \
    if (StringLength(BLK) > 0) {                                               \
      StringBuffRW(BLK)[--StringLength(BLK)] = '\0';                           \
    }                                                                          \
  } while (0)

/** Terminate on allocation failure. The String API returns void throughout, so
    there is no channel to report it on, and continuing would hand the caller a
    NULL buffer to write into. **/
HTS_STATIC void StringOom_(size_t size) {
  fprintf(stderr, "String: out of memory allocating %lu bytes\n",
          (unsigned long) size);
  fflush(stderr); /* abort() flushes nothing; Windows buffers a piped stderr */
  abort();
}

/** What to do when an allocation of SIZE bytes fails; overridable. **/
#ifndef STRING_OOM
#define STRING_OOM(SIZE) StringOom_(SIZE)
#endif

/** Grow so capacity_ >= CAPACITY (total bytes, including the NUL). May realloc
    (invalidating prior buffer pointers); aborts on OOM. Never shrinks. **/
#define StringRoomTotal(BLK, CAPACITY)                                         \
  do {                                                                         \
    const size_t capacity_ = (size_t) (CAPACITY);                              \
    while ((BLK).capacity_ < capacity_) {                                      \
      const size_t maxcap_ = (size_t) -1;                                      \
      const size_t mincap_ = 16;                                               \
      const size_t cur_ = (BLK).capacity_;                                     \
      /* Saturating: a plain cur_*2 wraps below cur_, which no further         \
         doubling climbs back out of. */                                       \
      const size_t newcap_ = cur_ < mincap_       ? mincap_                    \
                             : cur_ > maxcap_ / 2 ? maxcap_                    \
                                                  : cur_ * 2;                  \
      char *const buff_ = STRING_REALLOC((BLK).buffer_, newcap_);              \
                                                                               \
      if (buff_ == NULL) {                                                     \
        STRING_OOM(newcap_);                                                   \
      }                                                                        \
      (BLK).buffer_ = buff_;                                                   \
      (BLK).capacity_ = newcap_;                                               \
    }                                                                          \
  } while (0)

/** Reserve room for SIZE more bytes beyond the current length (plus the NUL).
    May realloc, invalidating prior buffer pointers. **/
#define StringRoom(BLK, SIZE)                                                  \
  StringRoomTotal(BLK, StringLength(BLK) + (SIZE) + 1)

/** Reserve room for SIZE more bytes and return the (post-realloc) RW buffer,
    for appending in place. Does not update length_; the caller must. **/
#define StringBuffN(BLK, SIZE) StringBuffN_(&(BLK), SIZE)

HTS_STATIC char *StringBuffN_(String *blk, int size) {
  StringRoom(*blk, size);
  return StringBuffRW(*blk);
}

/** Ceiling on the pre-C99 doubling search below; BLK is emptied past it. **/
#define STRING_SPRINTF_MAX ((size_t) 16 * 1024 * 1024)

/** Replace BLK's contents with the formatted output, growing to fit, so no
    fixed reserve has to bound an argument carrying remote input. An argument
    may not point into BLK's own buffer, which is reallocated here.
    Leaves BLK empty if the output cannot be produced (a conversion error, or
    past STRING_SPRINTF_MAX): callers must not assume a non-empty result. **/
#define StringSprintf(BLK, ...) StringSprintf_(&(BLK), __VA_ARGS__)

HTS_STATIC HTS_PRINTF_FUN(2, 3) void StringSprintf_(String *blk,
                                                    const char *fmt, ...) {
  size_t capacity = StringCapacity(*blk) > 256 ? StringCapacity(*blk) : 256;

  for (;;) {
    va_list args;
    int ret;

    StringRoomTotal(*blk, capacity);
    va_start(args, fmt);
    ret = vsnprintf(StringBuffRW(*blk), capacity, fmt, args);
    va_end(args);
    if (ret >= 0 && (size_t) ret < capacity) {
      StringBuffRW(*blk)[ret] = '\0';
      StringLength(*blk) = (size_t) ret;
      return;
    }
    if (ret >= 0) {
      capacity = (size_t) ret + 1; /* C99 said what it needs */
    } else if (capacity < STRING_SPRINTF_MAX) {
      capacity *= 2; /* pre-C99 msvcrt only says "too small" */
    } else {
      /* a conversion error returns -1 too, and no capacity ever fixes that */
      StringBuffRW(*blk)[0] = '\0';
      StringLength(*blk) = 0;
      return;
    }
  }
}

/** Zero the fields (NULL buffer, no allocation). Use on an uninitialized
    String only; does NOT free an existing buffer (use StringFree to reset
    an owned one), so calling it on a live String leaks. **/
#define StringInit(BLK)                                                        \
  do {                                                                         \
    (BLK).buffer_ = NULL;                                                      \
    (BLK).capacity_ = 0;                                                       \
    (BLK).length_ = 0;                                                         \
  } while (0)

/** Truncate to length 0, keeping the allocation. Forces a non-NULL buffer
    (allocates if empty) and writes the leading NUL, so StringBuff is "". **/
#define StringClear(BLK)                                                       \
  do {                                                                         \
    (BLK).length_ = 0;                                                         \
    StringRoom(BLK, 0);                                                        \
    (BLK).buffer_[0] = '\0';                                                   \
  } while (0)

/** Set length_ to SIZE, or to strlen(buffer_) if SIZE is negative. Caller
    asserts SIZE fits the existing content; does not (re)allocate. **/
#define StringSetLength(BLK, SIZE)                                             \
  do {                                                                         \
    const int len__ = (SIZE); /* signed: negative means strlen(buffer_) */     \
    if (len__ >= 0) {                                                          \
      (BLK).length_ = len__;                                                   \
    } else {                                                                   \
      (BLK).length_ = strlen((BLK).buffer_);                                   \
    }                                                                          \
  } while (0)

/** Release the owned buffer and reset to the empty state (NULL buffer).
    Idempotent; safe on an already-empty String. **/
#define StringFree(BLK)                                                        \
  do {                                                                         \
    if ((BLK).buffer_ != NULL) {                                               \
      STRING_FREE((BLK).buffer_);                                              \
      (BLK).buffer_ = NULL;                                                    \
    }                                                                          \
    (BLK).capacity_ = 0;                                                       \
    (BLK).length_ = 0;                                                         \
  } while (0)

/** Take ownership of a NUL-terminated heap string STR (the String will free
    it). Frees any current buffer first. STR MUST have been allocated by an
    allocator compatible with STRING_REALLOC()/STRING_FREE(), and must not be
    freed or used by the caller afterwards. length_/capacity_ are set to
    strlen(STR) (capacity_ here excludes the NUL, so the next append reallocs).
   **/
#define StringSetBuffer(BLK, STR)                                              \
  do {                                                                         \
    size_t len__ = strlen(STR);                                                \
    StringFree(BLK);                                                           \
    (BLK).buffer_ = (STR);                                                     \
    (BLK).capacity_ = len__;                                                   \
    (BLK).length_ = len__;                                                     \
  } while (0)

/** Append SIZE raw bytes from STR (NULs allowed as data). Grows as needed and
    re-terminates with a NUL after the appended bytes. STR must not alias
    BLK's buffer (a realloc would invalidate it). **/
#define StringMemcat(BLK, STR, SIZE)                                           \
  do {                                                                         \
    const char *str_mc_ = (STR);                                               \
    const size_t size_mc_ = (size_t) (SIZE);                                   \
    StringRoom(BLK, size_mc_);                                                 \
    if (size_mc_ > 0) {                                                        \
      memcpy((BLK).buffer_ + (BLK).length_, str_mc_, size_mc_);                \
      (BLK).length_ += size_mc_;                                               \
    }                                                                          \
    *((BLK).buffer_ + (BLK).length_) = '\0';                                   \
  } while (0)

/** Replace content with SIZE raw bytes from STR (NULs allowed as data).
    Same non-aliasing requirement as StringMemcat. **/
#define StringMemcpy(BLK, STR, SIZE)                                           \
  do {                                                                         \
    (BLK).length_ = 0;                                                         \
    StringMemcat(BLK, STR, SIZE);                                              \
  } while (0)

/** Append one byte and re-terminate. Grows as needed. **/
#define StringAddchar(BLK, c)                                                  \
  do {                                                                         \
    String *const s__ = &(BLK);                                                \
    char c__ = (c);                                                            \
    StringRoom(*s__, 1);                                                       \
    StringBuffRW(*s__)[StringLength(*s__)++] = c__;                            \
    StringBuffRW(*s__)[StringLength(*s__)] = 0;                                \
  } while (0)

/** Hand the buffer to the caller and reset the String to empty (NULL buffer).
    The returned pointer is now owned by the caller, who must STRING_FREE() it.
    Returns NULL if the String was empty. **/
HTS_STATIC char *StringAcquire(String *blk) {
  char *buff = StringBuffRW(*blk);

  StringBuffRW(*blk) = NULL;
  StringCapacity(*blk) = 0;
  StringLength(*blk) = 0;
  return buff;
}

/** Return an independent deep copy of *src (its own allocation). The caller
    owns the result and must StringFree it. **/
HTS_STATIC String StringDup(const String *src) {
  String s = STRING_EMPTY;

  StringMemcat(s, StringBuff(*src), StringLength(*src));
  return s;
}

/** Take ownership of *str (a NUL-terminated heap string) and NULL it out, so
    ownership transfers and the caller keeps no dangling alias. Frees any
    current buffer first. *str MUST be allocator-compatible (see
    StringSetBuffer). No-op if str or *str is NULL. **/
HTS_STATIC void StringAttach(String *blk, char **str) {
  StringFree(*blk);
  if (str != NULL && *str != NULL) {
    StringBuffRW(*blk) = *str;
    StringCapacity(*blk) = StringLength(*blk) = strlen(StringBuff(*blk));
    *str = NULL;
  }
}

/** Append the C string STR (up to its NUL). No-op if STR is NULL. STR must not
    alias BLK's buffer. **/
#define StringCat(BLK, STR)                                                    \
  do {                                                                         \
    const char *const str__ = (STR);                                           \
    if (str__ != NULL) {                                                       \
      const size_t size__ = strlen(str__);                                     \
      StringMemcat(BLK, str__, size__);                                        \
    }                                                                          \
  } while (0)

/** Append at most SIZE leading bytes of the C string STR. No-op if STR is
    NULL. STR must not alias BLK's buffer. **/
#define StringCatN(BLK, STR, SIZE)                                             \
  do {                                                                         \
    const char *str__ = (STR);                                                 \
    const size_t usize__ = (SIZE);                                             \
    if (str__ != NULL) {                                                       \
      size_t size__ = strlen(str__);                                           \
      if (size__ > usize__) {                                                  \
        size__ = usize__;                                                      \
      }                                                                        \
      StringMemcat(BLK, str__, size__);                                        \
    }                                                                          \
  } while (0)

/** Replace content with at most SIZE leading bytes of the C string STR.
    If STR is NULL, clears to "". STR must not alias BLK's buffer. **/
#define StringCopyN(BLK, STR, SIZE)                                            \
  do {                                                                         \
    const char *str__ = (STR);                                                 \
    const size_t usize__ = (SIZE);                                             \
    (BLK).length_ = 0;                                                         \
    if (str__ != NULL) {                                                       \
      size_t size__ = strlen(str__);                                           \
      if (size__ > usize__) {                                                  \
        size__ = usize__;                                                      \
      }                                                                        \
      StringMemcat(BLK, str__, size__);                                        \
    } else {                                                                   \
      StringClear(BLK);                                                        \
    }                                                                          \
  } while (0)

/** Replace blk's content with a copy of String blk2. blk and blk2 must be
    distinct Strings (use StringCopyOverlapped if they may be the same). **/
#define StringCopyS(blk, blk2) StringCopyN(blk, (blk2).buffer_, (blk2).length_)

/** Replace content with a copy of the C string STR. If STR is NULL, clears to
    "". STR must not alias BLK's buffer (use StringCopyOverlapped if it might).
   **/
#define StringCopy(BLK, STR)                                                   \
  do {                                                                         \
    const char *str__ = (STR);                                                 \
    if (str__ != NULL) {                                                       \
      size_t size__ = strlen(str__);                                           \
      StringMemcpy(BLK, str__, size__);                                        \
    } else {                                                                   \
      StringClear(BLK);                                                        \
    }                                                                          \
  } while (0)

/** Like StringCopy but safe when STR aliases BLK's own buffer: copies via a
    temporary, so a self-copy or overlap is well-defined. **/
#define StringCopyOverlapped(BLK, STR)                                         \
  do {                                                                         \
    String s__ = STRING_EMPTY;                                                 \
    StringCopy(s__, STR);                                                      \
    StringCopyS(BLK, s__);                                                     \
    StringFree(s__);                                                           \
  } while (0)

#endif
