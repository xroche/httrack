/*
punycode.c from RFC 3492
http://www.nicemice.net/idn/
Adam M. Costello
http://www.nicemice.net/amc/

This is ANSI C code (C89) implementing Punycode (RFC 3492).

*/

#include "punycode.h"

/******************/
/* Implementation */

#include <string.h>

/*** Bootstring parameters for Punycode ***/

enum { base = 36, tmin = 1, tmax = 26, skew = 38, damp = 700,
  initial_bias = 72, initial_n = 0x80, delimiter = 0x2D
};

/* basic(cp) tests whether cp is a basic code point: */
#define basic(cp) ((punycode_uint)(cp) < 0x80)

/* delim(cp) tests whether cp is a delimiter: */
#define delim(cp) ((cp) == delimiter)

/* decode_digit(cp) returns the numeric value of a basic code */
/* point (for use in representing integers) in the range 0 to */
/* base-1, or base if cp is does not represent a value.       */

static unsigned decode_digit(int cp) {
  return (unsigned) (cp - 48 < 10 ? cp - 22 : cp - 65 < 26 ? cp - 65 : cp - 97 <
    26 ? cp - 97 : base);
}

/* encode_digit(d) returns the lowercase basic code point whose value */
/* (when used for representing integers) is d, in the range 0 to base-1. */

static char encode_digit(punycode_uint d) {
  return d + 22 + 75 * (d < 26);
  /*  0..25 map to ASCII a..z */
  /* 26..35 map to ASCII 0..9 */
}

/*** Platform-specific constants ***/

/* maxint is the maximum value of a punycode_uint variable: */
static const punycode_uint maxint = -1;

/* Because maxint is unsigned, -1 becomes the maximum value. */

/*** Bias adaptation function ***/

static punycode_uint adapt(punycode_uint delta, punycode_uint numpoints,
                           int firsttime) {
  punycode_uint k;

  delta = firsttime ? delta / damp : delta >> 1;
  /* delta >> 1 is a faster way of doing delta / 2 */
  delta += delta / numpoints;

  for(k = 0; delta > ((base - tmin) * tmax) / 2; k += base) {
    delta /= base - tmin;
  }

  return k + (base - tmin + 1) * delta / (delta + skew);
}

/*** Main encode function ***/

enum punycode_status punycode_encode(punycode_uint input_length,
                                     const punycode_uint input[],
                                     punycode_uint *output_length,
                                     char output[]) {
  punycode_uint n, delta, h, b, out, max_out, bias, j, m, q, k, t;

  /* Initialize the state: */

  n = initial_n;
  delta = out = 0;
  max_out = *output_length;
  bias = initial_bias;

  /* Handle the basic code points: */
  for(j = 0; j < input_length; ++j) {
    if (basic(input[j])) {
      if (max_out - out < 2)
        return punycode_big_output;
      output[out++] = (char) input[j];
    }
    /* else if (input[j] < n) return punycode_bad_input; */
    /* (not needed for Punycode with unsigned code points) */
  }

  h = b = out;

  /* h is the number of code points that have been handled, b is the  */
  /* number of basic code points, and out is the number of characters */
  /* that have been output.                                           */

  if (b > 0)
    output[out++] = delimiter;

  /* Main encoding loop: */

  while(h < input_length) {
    /* All non-basic code points < n have been     */
    /* handled already.  Find the next larger one: */

    for(m = maxint, j = 0; j < input_length; ++j) {
      /* if (basic(input[j])) continue; */
      /* (not needed for Punycode) */
      if (input[j] >= n && input[j] < m)
        m = input[j];
    }

    /* Increase delta enough to advance the decoder's    */
    /* <n,i> state to <m,0>, but guard against overflow: */

    if (m - n > (maxint - delta) / (h + 1))
      return punycode_overflow;
    delta += (m - n) * (h + 1);
    n = m;

    for(j = 0; j < input_length; ++j) {
      /* Punycode does not need to check whether input[j] is basic: */
      if (input[j] < n /* || basic(input[j]) */ ) {
        if (++delta == 0)
          return punycode_overflow;
      }

      if (input[j] == n) {
        /* Represent delta as a generalized variable-length integer: */

        for(q = delta, k = base;; k += base) {
          if (out >= max_out)
            return punycode_big_output;
          t = k <= bias /* + tmin */ ? tmin :   /* +tmin not needed */
            k >= bias + tmax ? tmax : k - bias;
          if (q < t)
            break;
          output[out++] = encode_digit(t + (q - t) % (base - t));
          q = (q - t) / (base - t);
        }

        output[out++] = encode_digit(q);
        bias = adapt(delta, h + 1, h == b);
        delta = 0;
        ++h;
      }
    }

    ++delta, ++n;
  }

  *output_length = out;
  return punycode_success;
}

/*** Main decode function ***/

enum punycode_status punycode_decode(punycode_uint input_length,
                                     const char input[],
                                     punycode_uint *output_length,
                                     punycode_uint output[]) {
  punycode_uint n, out, i, max_out, bias, b, j, in, oldi, w, k, digit, t;

  /* Initialize the state: */

  n = initial_n;
  out = i = 0;
  max_out = *output_length;
  bias = initial_bias;

  /* Handle the basic code points:  Let b be the number of input code */
  /* points before the last delimiter, or 0 if there is none, then    */
  /* copy the first b code points to the output.                      */

  for(b = j = 0; j < input_length; ++j)
    if (delim(input[j]))
      b = j;
  if (b > max_out)
    return punycode_big_output;

  for (j = 0; j < b; ++j) {
    if (!basic(input[j]))
      return punycode_bad_input;
    output[out++] = input[j];
  }

  /* Main decoding loop:  Start just after the last delimiter if any  */
  /* basic code points were copied; start at the beginning otherwise. */

  for(in = b > 0 ? b + 1 : 0; in < input_length; ++out) {

    /* in is the index of the next character to be consumed, and */
    /* out is the number of code points in the output array.     */

    /* Decode a generalized variable-length integer into delta,  */
    /* which gets added to i.  The overflow checking is easier   */
    /* if we increase i as we go, then subtract off its starting */
    /* value at the end to obtain delta.                         */

    for(oldi = i, w = 1, k = base;; k += base) {
      if (in >= input_length)
        return punycode_bad_input;
      digit = decode_digit(input[in++]);
      if (digit >= base)
        return punycode_bad_input;
      if (digit > (maxint - i) / w)
        return punycode_overflow;
      i += digit * w;
      t = k <= bias /* + tmin */ ? tmin :       /* +tmin not needed */
        k >= bias + tmax ? tmax : k - bias;
      if (digit < t)
        break;
      if (w > maxint / (base - t))
        return punycode_overflow;
      w *= (base - t);
    }

    bias = adapt(i - oldi, out + 1, oldi == 0);

    /* i was supposed to wrap around from out+1 to 0,   */
    /* incrementing n each time, so we'll fix that now: */

    if (i / (out + 1) > maxint - n)
      return punycode_overflow;
    n += i / (out + 1);
    i %= (out + 1);

    /* Insert n at position i of the output: */

    /* not needed for Punycode: */
    /* if (decode_digit(n) <= base) return punycode_invalid_input; */
    if (out >= max_out)
      return punycode_big_output;

    memmove(output + i + 1, output + i, (out - i) * sizeof *output);
    output[i++] = n;
  }

  *output_length = out;
  return punycode_success;
}
