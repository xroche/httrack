#ifndef MD5_H
#define MD5_H

#include <stdint.h>

/* Exact 32-bit type for the MD5 state. uint32_t replaces a SIZEOF_LONG-derived
   type so config.h stays architecture-independent (Debian #1133728). */
typedef uint32_t uint32;

struct MD5Context {
  union {
    unsigned char ui8[64];
    uint32 ui32[16];
  } in;
  uint32 buf[4];
  uint32 bits[2];
  int doByteReverse;
};

void MD5Init(struct MD5Context *context, int brokenEndian);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
               unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);
void MD5Transform(uint32 buf[4], uint32 const in[16]);

int mdfile(char *fn, unsigned char *digest);
int mdbinfile(char *fn, unsigned char *bindigest);

/* These assume a little endian machine and return incorrect results! 
They are here for compatibility with old (broken) versions of RPM */
int mdfileBroken(char *fn, unsigned char *digest);
int mdbinfileBroken(char *fn, unsigned char *bindigest);

/*
* This is needed to make RSAREF happy on some MS-DOS compilers.
*/
typedef struct MD5Context MD5CTX;

#endif /* !MD5_H */
