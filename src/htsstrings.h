/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.


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
#include <string.h>

/* GCC extension */
#ifndef HTS_UNUSED
#ifdef __GNUC__
#define HTS_UNUSED __attribute__ ((unused))
#define HTS_STATIC static __attribute__ ((unused))
#else
#define HTS_UNUSED
#define HTS_STATIC static
#endif
#endif

/** Forward definitions **/
#ifndef HTS_DEF_FWSTRUCT_String
#define HTS_DEF_FWSTRUCT_String
typedef struct String String;
#endif
#ifndef HTS_DEF_STRUCT_String
#define HTS_DEF_STRUCT_String
struct String {
  char* buffer_;
  size_t length_;
  size_t capacity_;
};
#endif

/** Allocator **/
#ifndef STRING_REALLOC
#define STRING_REALLOC(BUFF, SIZE) ( (char*) realloc(BUFF, SIZE) )
#define STRING_FREE(BUFF) free(BUFF)
#endif
#ifndef STRING_ASSERT
#include <assert.h>
#define STRING_ASSERT(EXP) assert(EXP)
#endif

/** An empty string **/
#define STRING_EMPTY { (char*) NULL, 0, 0 }

/** String buffer **/
#define StringBuff(BLK) ( (const char*) ((BLK).buffer_) )

/** String buffer (read/write) **/
#define StringBuffRW(BLK) ((BLK).buffer_)

/** String length **/
#define StringLength(BLK) ((BLK).length_)

/** String not empty ? **/
#define StringNotEmpty(BLK) ( StringLength(BLK) > 0 )

/** String capacity **/
#define StringCapacity(BLK) ((BLK).capacity_)

/** Subcharacter **/
#define StringSub(BLK, POS) ( StringBuff(BLK)[POS] )

/** Subcharacter (read/write) **/
#define StringSubRW(BLK, POS) ( StringBuffRW(BLK)[POS] )

/** Subcharacter (read/write) **/
#define StringSubRW(BLK, POS) ( StringBuffRW(BLK)[POS] )

/** Right subcharacter **/
#define StringRight(BLK, POS) ( StringBuff(BLK)[StringLength(BLK) - POS] )

/** Right subcharacter (read/write) **/
#define StringRightRW(BLK, POS) ( StringBuffRW(BLK)[StringLength(BLK) - POS] )

/** Remove the utter right character from the string. **/
#define StringPopRight(BLK) do { \
  StringBuffRW(BLK)[--StringLength(BLK)] = '\0'; \
} while(0)

/** Ensure the string is large enough for exactly CAPACITY bytes overall (including \0). **/
#define StringRoomTotal(BLK, CAPACITY) do { \
  const size_t capacity_ = (size_t) (CAPACITY); \
  while ((BLK).capacity_ < capacity_) { \
    if ((BLK).capacity_ < 16) { \
      (BLK).capacity_ = 16; \
    } else { \
      (BLK).capacity_ *= 2; \
    } \
    (BLK).buffer_ = STRING_REALLOC((BLK).buffer_, (BLK).capacity_); \
    STRING_ASSERT((BLK).buffer_ != NULL); \
  } \
} while(0)

/** Ensure the string is large enough for exactly SIZE more characters (not including \0). **/
#define StringRoom(BLK, SIZE) StringRoomTotal(BLK, StringLength(BLK) + (SIZE) + 1)

/** Return the RW buffer for a strcat() operation of at most SIZE characters. **/
#define StringBuffN(BLK, SIZE) StringBuffN_(&(BLK), SIZE) 
HTS_STATIC char* StringBuffN_(String* blk, int size) {
  StringRoom(*blk, size);
  return StringBuffRW(*blk);
}

/** Initialize a string. **/
#define StringInit(BLK) do { \
  (BLK).buffer_ = NULL; \
  (BLK).capacity_ = 0; \
  (BLK).length_ = 0; \
} while(0)

/** Clear a string (set its length to 0) **/
#define StringClear(BLK) do { \
  (BLK).length_ = 0; \
  StringRoom(BLK, 0); \
  (BLK).buffer_[0] = '\0'; \
} while(0)

/** Set the length of a string to 'SIZE'. If SIZE is negative, check the size using strlen(). **/
#define StringSetLength(BLK, SIZE) do { \
  if (SIZE >= 0) { \
    (BLK).length_ = SIZE; \
  } else { \
    (BLK).length_ = strlen((BLK).buffer_); \
  } \
} while(0)

/** Free a string (release memory) **/
#define StringFree(BLK) do { \
  if ((BLK).buffer_ != NULL) { \
    STRING_FREE((BLK).buffer_); \
    (BLK).buffer_ = NULL; \
  } \
  (BLK).capacity_ = 0; \
  (BLK).length_ = 0; \
} while(0)

/** Assign an allocated pointer to a a string.
The pointer _MUST_ be compatible with STRING_REALLOC() and STRING_FREE() **/
#define StringSetBuffer(BLK, STR) do { \
  size_t len__ = strlen( STR ); \
  StringFree(BLK); \
  (BLK).buffer_ = ( STR ); \
  (BLK).capacity_ = len__; \
  (BLK).length_ = len__; \
} while(0)

/** Append a memory block to a string **/
#define StringMemcat(BLK, STR, SIZE) do { \
  const char* str_mc_ = (STR); \
  const size_t size_mc_ = (size_t) (SIZE); \
  StringRoom(BLK, size_mc_); \
  if (size_mc_ > 0) { \
    memcpy((BLK).buffer_ + (BLK).length_, str_mc_, size_mc_); \
    (BLK).length_ += size_mc_; \
  } \
  *((BLK).buffer_ + (BLK).length_) = '\0'; \
} while(0)

/** Copy a memory block to a string **/
#define StringMemcpy(BLK, STR, SIZE) do { \
  (BLK).length_ = 0; \
  StringMemcat(BLK, STR, SIZE); \
} while(0)

/** Add a character **/
#define StringAddchar(BLK, c) do { \
  String * const s__ = &(BLK); \
  char c__ = (c); \
  StringRoom(*s__, 1); \
  StringBuffRW(*s__)[StringLength(*s__)++] = c__; \
  StringBuffRW(*s__)[StringLength(*s__)  ] = 0; \
} while(0)

/** Acquire a string ; it's the client's responsability to free() it **/
HTS_STATIC char* StringAcquire(String* blk) {
  char* buff = StringBuffRW(*blk);
  StringBuffRW(*blk) = NULL;
  StringCapacity(*blk) = 0;
  StringLength(*blk) = 0;
  return buff;
}

/** Clone a string. **/
HTS_STATIC String StringDup(const String* src) {
  String s = STRING_EMPTY;
  StringMemcat(s, StringBuff(*src), StringLength(*src));
  return s;
}

/** Attach a string using a pointer. **/
HTS_STATIC void StringAttach(String* blk, char** str) {
  StringFree(*blk);
  if (str != NULL && *str != NULL) {
    StringBuffRW(*blk) = *str;
    StringCapacity(*blk) = StringLength(*blk) = strlen(StringBuff(*blk));
    *str = NULL;
  }
}

/** Append a string to another one. **/
#define StringCat(BLK, STR) do { \
  const char *const str__ = ( STR ); \
  if (str__ != NULL) { \
    const size_t size__ = strlen(str__); \
    StringMemcat(BLK, str__, size__); \
  } \
} while(0)

#define StringCatN(BLK, STR, SIZE) do { \
  const char *str__ = ( STR ); \
  if (str__ != NULL) { \
    size_t size__ = strlen(str__); \
    if (size__ > (SIZE)) { \
      size__ = (SIZE); \
    } \
    StringMemcat(BLK, str__, size__); \
  } \
} while(0)

#define StringCopyN(BLK, STR, SIZE) do { \
  const char *str__ = ( STR ); \
  const size_t usize__ = (SIZE); \
  (BLK).length_ = 0; \
  if (str__ != NULL) { \
    size_t size__ = strlen(str__); \
    if (size__ > usize__ ) { \
      size__ = usize__; \
    } \
    StringMemcat(BLK, str__, size__); \
  } else { \
    StringClear(BLK); \
  } \
} while(0)

#define StringCopyS(blk, blk2) StringCopyN(blk, (blk2).buffer_, (blk2).length_)

/** Copy a string to another one. **/
#define StringCopy(BLK, STR) do { \
  const char *str__ = ( STR ); \
  if (str__ != NULL) { \
    size_t size__ = strlen(str__); \
    StringMemcpy(BLK, str__, size__); \
  } else { \
    StringClear(BLK); \
  } \
} while(0)

/** Copy a (potentially overlapping) string to another one. **/
#define StringCopyOverlapped(BLK, STR) do { \
  String s__ = STRING_EMPTY; \
  StringCopy(s__, STR); \
  StringCopyS(BLK, s__); \
  StringFree(s__); \
} while(0)

#endif
