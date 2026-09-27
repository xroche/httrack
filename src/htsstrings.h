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

/**
 * @file htsstrings.h
 * Growable string (String) that owns its buffer, so a caller never has to size
 * a destination in advance. Every operation here is a macro or a static
 * function, so there is nothing to link against. Release a String with
 * StringFree(), or hand its buffer over with StringAcquire().
 *
 * A macro names the String itself, not a pointer to it, and may evaluate that
 * argument several times. So pass a plain variable, never an expression with
 * side effects. Every length and capacity counts bytes, not characters.
 */

#ifndef HTS_STRINGS_DEFSTATIC
#define HTS_STRINGS_DEFSTATIC

/* System definitions. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Take the attribute helpers from their single definition: a partial copy here
   would win their #ifndef guard and starve later headers of the rest. */
#include "htsglobal.h"

/** Forward definitions **/
#ifndef HTS_DEF_FWSTRUCT_String
#define HTS_DEF_FWSTRUCT_String
typedef struct String String;
#endif
#ifndef HTS_DEF_STRUCT_String
#define HTS_DEF_STRUCT_String

/**
 * Growable string. The String owns its buffer and frees it in StringFree().
 *
 * The buffer is allocated on the first write. A String initialized with
 * STRING_EMPTY or StringInit(), and one just freed or acquired, has no buffer
 * at all, so StringBuff() returns NULL rather than "". An operation that writes
 * content leaves it NUL-terminated at buffer_[length_], but StringRoomTotal()
 * and StringSetLength() write no terminator, so neither leaves a readable C
 * string behind.
 *
 * Any growing operation may move the buffer, so a pointer read through
 * StringBuff() or StringBuffRW() stops being valid at the next append, copy or
 * room request.
 *
 * Reach the fields through the macros below, never directly.
 */
struct String {
  char *buffer_;    /**< owned content, NULL until the first write */
  size_t length_;   /**< bytes before the terminating NUL */
  size_t capacity_; /**< allocated size in bytes */
};
#endif

/** Allocator hooks. Define BOTH before including this header to replace them,
    because defining STRING_REALLOC alone also drops the default STRING_FREE.
    STRING_REALLOC returns NULL when it cannot allocate SIZE bytes. **/
#ifndef STRING_REALLOC
#define STRING_REALLOC(BUFF, SIZE) ((char *) realloc(BUFF, SIZE))

#define STRING_FREE(BUFF) free(BUFF)
#endif

/** Initializer for a String with no buffer. It frees nothing, so a String that
    already owns a buffer needs StringFree() first. **/
#define STRING_EMPTY {(char *) NULL, 0, 0}

/** Read-only content pointer, NULL until the String has been written to. The
    next growing operation invalidates it. **/
#define StringBuff(BLK) ((const char *) ((BLK).buffer_))

/** Read/write content pointer, with the same NULL and invalidation rules as
    StringBuff. **/
#define StringBuffRW(BLK) ((BLK).buffer_)

/** Content length in bytes, not counting the terminating NUL. **/
#define StringLength(BLK) ((BLK).length_)

/** Does the String hold at least one byte? **/
#define StringNotEmpty(BLK) (StringLength(BLK) > 0)

/** Allocated size in bytes. It covers the terminating NUL, except after
    StringSetBuffer or StringAttach, which do not count it. **/
#define StringCapacity(BLK) ((BLK).capacity_)

/** Byte at POS (read). No bounds check, so POS must be below StringLength. **/
#define StringSub(BLK, POS) (StringBuff(BLK)[POS])

/** Byte at POS (read/write). No bounds check, so POS must be below
    StringLength. **/
#define StringSubRW(BLK, POS) (StringBuffRW(BLK)[POS])

/** Byte POS positions from the end (read). POS==1 is the last byte, and POS
    must not exceed StringLength. **/
#define StringRight(BLK, POS) (StringBuff(BLK)[StringLength(BLK) - POS])

/** Byte POS positions from the end (read/write). POS==1 is the last byte, and
    POS must not exceed StringLength. **/
#define StringRightRW(BLK, POS) (StringBuffRW(BLK)[StringLength(BLK) - POS])

/** Drop the last byte and re-terminate. It does nothing to a String that holds
    no byte. **/
#define StringPopRight(BLK)                                                    \
  do {                                                                         \
    if (StringLength(BLK) > 0) {                                               \
      StringBuffRW(BLK)[--StringLength(BLK)] = '\0';                           \
    }                                                                          \
  } while (0)

/** Report a failed allocation of @p size bytes and kill the process. The String
    operations return no status, so a caller cannot be told about the
    failure. **/
HTS_STATIC void StringOom_(size_t size) {
  fprintf(stderr, "String: out of memory allocating %lu bytes\n",
          (unsigned long) size);
  fflush(stderr); /* abort() flushes nothing; Windows buffers a piped stderr */
  abort();
}

/** Handle a failed allocation of SIZE bytes. Define it before including this
    header to override it. It must not return, because the caller would then
    write into a NULL buffer. **/
#ifndef STRING_OOM
#define STRING_OOM(SIZE) StringOom_(SIZE)
#endif

/** Grow the allocation so it holds at least CAPACITY bytes, the terminating NUL
    included. It never shrinks, it may move the buffer, it writes no terminator,
    and it aborts when the allocation fails. **/
#define StringRoomTotal(BLK, CAPACITY)                                         \
  do {                                                                         \
    const size_t capacity_ = (size_t) (CAPACITY);                              \
    while ((BLK).capacity_ < capacity_) {                                      \
      const size_t maxcap_ = (size_t) -1;                                      \
      const size_t mincap_ = 16;                                               \
      const size_t cur_ = (BLK).capacity_;                                     \
      /* Past the halfway point cur_*2 wraps below cur_, which no further      \
         doubling climbs back out of; ask for the requested total instead. */  \
      const size_t newcap_ = cur_ < mincap_       ? mincap_                    \
                             : cur_ > maxcap_ / 2 ? capacity_                  \
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

/** Reserve room for SIZE more content bytes after the current length, plus the
    terminating NUL. It may move the buffer. **/
#define StringRoom(BLK, SIZE)                                                  \
  StringRoomTotal(BLK, StringLength(BLK) + (SIZE) + 1)

/** Reserve room for SIZE more bytes and return the buffer start, so write at
    offset StringLength and then set the new length with StringSetLength. **/
#define StringBuffN(BLK, SIZE) StringBuffN_(&(BLK), SIZE)

HTS_STATIC char *StringBuffN_(String *blk, int size) {
  StringRoom(*blk, size);
  return StringBuffRW(*blk);
}

/** Largest buffer StringSprintf tries on a libc that does not report the length
    it needs. Once it reaches that size it gives up and empties the String. **/
#define STRING_SPRINTF_MAX ((size_t) 16 * 1024 * 1024)

/** Replace BLK's contents with the formatted output, growing to fit, so no
    fixed reserve has to bound an argument carrying remote input. No argument
    may point into BLK's own buffer, which this reallocates. The result is
    NUL-terminated, and is empty when the output cannot be produced (a
    conversion error, or a length past STRING_SPRINTF_MAX). **/
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

/** Set BLK to the empty state, with no buffer and no allocation. It frees
    nothing, so calling it on a String that owns a buffer leaks that buffer. **/
#define StringInit(BLK)                                                        \
  do {                                                                         \
    (BLK).buffer_ = NULL;                                                      \
    (BLK).capacity_ = 0;                                                       \
    (BLK).length_ = 0;                                                         \
  } while (0)

/** Truncate the content to nothing, keeping the allocation. It allocates a
    buffer if the String has none, so StringBuff then returns "" and not
    NULL. **/
#define StringClear(BLK)                                                       \
  do {                                                                         \
    (BLK).length_ = 0;                                                         \
    StringRoom(BLK, 0);                                                        \
    (BLK).buffer_[0] = '\0';                                                   \
  } while (0)

/** Set the content length to SIZE bytes, or to the strlen of the buffer when
    SIZE is negative, which needs a buffer to exist. It neither reallocates nor
    writes a terminator, so SIZE must fit the bytes already there. **/
#define StringSetLength(BLK, SIZE)                                             \
  do {                                                                         \
    const int len__ = (SIZE); /* signed: negative means strlen(buffer_) */     \
    if (len__ >= 0) {                                                          \
      (BLK).length_ = len__;                                                   \
    } else {                                                                   \
      (BLK).length_ = strlen((BLK).buffer_);                                   \
    }                                                                          \
  } while (0)

/** Free the owned buffer and reset BLK to the empty state. Safe to call on a
    String that owns nothing, and safe to call twice. **/
#define StringFree(BLK)                                                        \
  do {                                                                         \
    if ((BLK).buffer_ != NULL) {                                               \
      STRING_FREE((BLK).buffer_);                                              \
      (BLK).buffer_ = NULL;                                                    \
    }                                                                          \
    (BLK).capacity_ = 0;                                                       \
    (BLK).length_ = 0;                                                         \
  } while (0)

/** Take ownership of the NUL-terminated heap string STR, freeing BLK's current
    buffer first. STR must not be NULL, must come from the STRING_REALLOC
    allocator, and must not be used by the caller afterwards. STR is evaluated
    twice. The recorded capacity leaves out the NUL, so the next append
    reallocates. **/
#define StringSetBuffer(BLK, STR)                                              \
  do {                                                                         \
    size_t len__ = strlen(STR);                                                \
    StringFree(BLK);                                                           \
    (BLK).buffer_ = (STR);                                                     \
    (BLK).capacity_ = len__;                                                   \
    (BLK).length_ = len__;                                                     \
  } while (0)

/** Append SIZE bytes from STR, NUL bytes included as data, and re-terminate.
    STR must not point into BLK's own buffer, which may move. **/
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

/** Replace the content with SIZE bytes from STR, NUL bytes included as data.
    STR must not point into BLK's own buffer. **/
#define StringMemcpy(BLK, STR, SIZE)                                           \
  do {                                                                         \
    (BLK).length_ = 0;                                                         \
    StringMemcat(BLK, STR, SIZE);                                              \
  } while (0)

/** Append the byte c and re-terminate. **/
#define StringAddchar(BLK, c)                                                  \
  do {                                                                         \
    String *const s__ = &(BLK);                                                \
    char c__ = (c);                                                            \
    StringRoom(*s__, 1);                                                       \
    StringBuffRW(*s__)[StringLength(*s__)++] = c__;                            \
    StringBuffRW(*s__)[StringLength(*s__)] = 0;                                \
  } while (0)

/** Hand the buffer over to the caller and reset *blk to the empty state. The
    caller owns the returned pointer and must release it with STRING_FREE().
    @return NULL when the String holds no buffer. **/
HTS_STATIC char *StringAcquire(String *blk) {
  char *buff = StringBuffRW(*blk);

  StringBuffRW(*blk) = NULL;
  StringCapacity(*blk) = 0;
  StringLength(*blk) = 0;
  return buff;
}

/** Return a copy of *src with its own allocation, which the caller releases
    with StringFree. The copy always owns a buffer, even when @p src owns
    none. **/
HTS_STATIC String StringDup(const String *src) {
  String s = STRING_EMPTY;

  StringMemcat(s, StringBuff(*src), StringLength(*src));
  return s;
}

/** Take ownership of *str, a NUL-terminated heap string, and set *str to NULL
    so the caller keeps no alias to it. It frees blk's current buffer first, so
    a NULL @p str or *str leaves @p blk empty. *str must come from the
    STRING_REALLOC allocator. **/
HTS_STATIC void StringAttach(String *blk, char **str) {
  StringFree(*blk);
  if (str != NULL && *str != NULL) {
    StringBuffRW(*blk) = *str;
    StringCapacity(*blk) = StringLength(*blk) = strlen(StringBuff(*blk));
    *str = NULL;
  }
}

/** Append the C string STR, up to its NUL. It does nothing when STR is NULL.
    STR must not point into BLK's own buffer. **/
#define StringCat(BLK, STR)                                                    \
  do {                                                                         \
    const char *const str__ = (STR);                                           \
    if (str__ != NULL) {                                                       \
      const size_t size__ = strlen(str__);                                     \
      StringMemcat(BLK, str__, size__);                                        \
    }                                                                          \
  } while (0)

/** Append at most SIZE leading bytes of the C string STR. It does nothing when
    STR is NULL. STR must not point into BLK's own buffer. **/
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

/** Replace the content with at most SIZE leading bytes of the C string STR, or
    with "" when STR is NULL. STR must not point into BLK's own buffer. **/
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

/** Replace blk's content with a copy of the String blk2, which must be a
    different String from blk. The copy stops at the first NUL in blk2, so
    content holding NUL bytes needs StringMemcpy. blk2 is evaluated twice. **/
#define StringCopyS(blk, blk2) StringCopyN(blk, (blk2).buffer_, (blk2).length_)

/** Replace the content with a copy of the C string STR, or with "" when STR is
    NULL. STR must not point into BLK's own buffer (use StringCopyOverlapped
    when it might). **/
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

/** Like StringCopy, but STR may point into BLK's own buffer. **/
#define StringCopyOverlapped(BLK, STR)                                         \
  do {                                                                         \
    String s__ = STRING_EMPTY;                                                 \
    StringCopy(s__, STR);                                                      \
    StringCopyS(BLK, s__);                                                     \
    StringFree(s__);                                                           \
  } while (0)

#endif
