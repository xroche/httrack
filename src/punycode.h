/*
punycode.c from RFC 3492
http://www.nicemice.net/idn/
Adam M. Costello
http://www.nicemice.net/amc/

This is ANSI C code (C89) implementing Punycode (RFC 3492).

*/

#ifndef PUNYCODE_COSTELLO_RFC3492_H
#define PUNYCODE_COSTELLO_RFC3492_H

/********************/
/* Public interface */

#include <limits.h>

typedef enum punycode_status {
  punycode_success,
  punycode_bad_input,           /* Input is invalid.                       */
  punycode_big_output,          /* Output would exceed the space provided. */
  punycode_overflow             /* Input needs wider integers to process.  */
} punycode_status;

#if UINT_MAX >= (1 << 26) - 1
typedef unsigned int punycode_uint;
#else
typedef unsigned long punycode_uint;
#endif

/* punycode_encode() converts Unicode to Punycode.  The input     */
/* is represented as an array of Unicode code points (not code    */
/* units; surrogate pairs are not allowed), and the output        */
/* will be represented as an array of ASCII code points.  The     */
/* output string is *not* null-terminated; it will contain        */
/* zeros if and only if the input contains zeros.  (Of course     */
/* the caller can leave room for a terminator and add one if      */
/* needed.)  The input_length is the number of code points in     */
/* the input.  The output_length is an in/out argument: the       */
/* caller passes in the maximum number of code points that it     */
/* can receive, and on successful return it will contain the      */
/* number of code points actually output.  ASCII code points are  */
/* encoded literally.  The return value can be any of the        */
/* punycode_status values defined above except punycode_bad_input;*/
/* if not punycode_success, then output_size and output might     */
/* contain garbage.                                               */
enum punycode_status punycode_encode(punycode_uint input_length,
                                     const punycode_uint input[],
                                     punycode_uint *output_length,
                                     char output[]);

/* punycode_decode() converts Punycode to Unicode.  The input is  */
/* represented as an array of ASCII code points, and the output   */
/* will be represented as an array of Unicode code points.  The   */
/* input_length is the number of code points in the input.  The   */
/* output_length is an in/out argument: the caller passes in      */
/* the maximum number of code points that it can receive, and     */
/* on successful return it will contain the actual number of      */
/* code points output.  The return value can be any of the        */
/* punycode_status values defined above; if not punycode_success, */
/* then output_length and output might contain garbage.  On       */
/* success, the decoder will never need to write an output_length */
/* greater than input_length, because of how the encoding is      */
/* defined.                                                       */
enum punycode_status punycode_decode(punycode_uint input_length,
                                     const char input[],
                                     punycode_uint *output_length,
                                     punycode_uint output[]);

#endif
