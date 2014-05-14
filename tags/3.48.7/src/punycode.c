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

static punycode_uint decode_digit(punycode_uint cp) {
  return cp - 48 < 10 ? cp - 22 : cp - 65 < 26 ? cp - 65 : cp - 97 <
    26 ? cp - 97 : base;
}

/* encode_digit(d,flag) returns the basic code point whose value      */
/* (when used for representing integers) is d, which needs to be in   */
/* the range 0 to base-1.  The lowercase form is used unless flag is  */
/* nonzero, in which case the uppercase form is used.  The behavior   */
/* is undefined if flag is nonzero and digit d has no uppercase form. */

static char encode_digit(punycode_uint d, int flag) {
  return d + 22 + 75 * (d < 26) - ((flag != 0) << 5);
  /*  0..25 map to ASCII a..z or A..Z */
  /* 26..35 map to ASCII 0..9         */
}

/* flagged(bcp) tests whether a basic code point is flagged */
/* (uppercase).  The behavior is undefined if bcp is not a  */
/* basic code point.                                        */

#define flagged(bcp) ((punycode_uint)(bcp) - 65 < 26)

/* encode_basic(bcp,flag) forces a basic code point to lowercase */
/* if flag is zero, uppercase if flag is nonzero, and returns    */
/* the resulting code point.  The code point is unchanged if it  */
/* is caseless.  The behavior is undefined if bcp is not a basic */
/* code point.                                                   */

static char encode_basic(punycode_uint bcp, int flag) {
  bcp -= (bcp - 97 < 26) << 5;
  return bcp + ((!flag && (bcp - 65 < 26)) << 5);
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
                                     const unsigned char case_flags[],
                                     punycode_uint * output_length,
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
      output[out++] =
        case_flags ? encode_basic(input[j], case_flags[j]) : input[j];
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
          output[out++] = encode_digit(t + (q - t) % (base - t), 0);
          q = (q - t) / (base - t);
        }

        output[out++] = encode_digit(q, case_flags && case_flags[j]);
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
                                     punycode_uint * output_length,
                                     punycode_uint output[],
                                     unsigned char case_flags[]) {
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

  for(j = 0; j < b; ++j) {
    if (case_flags)
      case_flags[out] = flagged(input[j]);
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

    if (case_flags) {
      memmove(case_flags + i + 1, case_flags + i, out - i);
      /* Case of last character determines uppercase flag: */
      case_flags[i] = flagged(input[in - 1]);
    }

    memmove(output + i + 1, output + i, (out - i) * sizeof *output);
    output[i++] = n;
  }

  *output_length = out;
  return punycode_success;
}

#ifdef PUNYCODE_COSTELLO_RFC3492_INCLUDE_TEST
#define PUNYCODE_COSTELLO_RFC3492_INCLUDE_TEST

/******************************************************************/
/* Wrapper for testing (would normally go in a separate .c file): */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* For testing, we'll just set some compile-time limits rather than */
/* use malloc(), and set a compile-time option rather than using a  */
/* command-line option.                                             */

enum {
  unicode_max_length = 256,
  ace_max_length = 256
};

static void usage(char **argv) {
  fprintf(stderr,
          "\n" "%s -e reads code points and writes a Punycode string.\n"
          "%s -d reads a Punycode string and writes code points.\n" "\n"
          "Input and output are plain text in the native character set.\n"
          "Code points are in the form u+hex separated by whitespace.\n"
          "Although the specification allows Punycode strings to contain\n"
          "any characters from the ASCII repertoire, this test code\n"
          "supports only the printable characters, and needs the Punycode\n"
          "string to be followed by a newline.\n"
          "The case of the u in u+hex is the force-to-uppercase flag.\n",
          argv[0], argv[0]);
  exit(EXIT_FAILURE);
}

static void fail(const char *msg) {
  fputs(msg, stderr);
  exit(EXIT_FAILURE);
}

static const char too_big[] =
  "input or output is too large, recompile with larger limits\n";
static const char invalid_input[] = "invalid input\n";
static const char overflow[] = "arithmetic overflow\n";
static const char io_error[] = "I/O error\n";

#endif

/* The following string is used to convert printable */
/* characters between ASCII and the native charset:  */

#ifdef PUNYCODE_COSTELLO_RFC3492_INCLUDE_MAIN

static const char print_ascii[] =
  "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n" "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
  " !\"#$%&'()*+,-./" "0123456789:;<=>?" "@ABCDEFGHIJKLMNO" "PQRSTUVWXYZ[\\]^_"
  "`abcdefghijklmno" "pqrstuvwxyz{|}~\n";

int main(int argc, char **argv) {
  enum punycode_status status;
  int r;
  unsigned int input_length, output_length, j;
  unsigned char case_flags[unicode_max_length];

  if (argc != 2)
    usage(argv);
  if (argv[1][0] != '-')
    usage(argv);
  if (argv[1][2] != 0)
    usage(argv);

  if (argv[1][1] == 'e') {
    punycode_uint input[unicode_max_length];
    unsigned long codept;
    char output[ace_max_length + 1], uplus[3];
    int c;

    /* Read the input code points: */

    input_length = 0;

    for(;;) {
      r = scanf("%2s%lx", uplus, &codept);
      if (ferror(stdin))
        fail(io_error);

      if (r == EOF || r == 0)
        break;

      if (r != 2 || uplus[1] != '+' || codept > (punycode_uint) - 1) {
        fail(invalid_input);
      }

      if (input_length == unicode_max_length)
        fail(too_big);

      if (uplus[0] == 'u')
        case_flags[input_length] = 0;
      else if (uplus[0] == 'U')
        case_flags[input_length] = 1;
      else
        fail(invalid_input);

      input[input_length++] = codept;
    }

    /* Encode: */

    output_length = ace_max_length;
    status =
      punycode_encode(input_length, input, case_flags, &output_length, output);
    if (status == punycode_bad_input)
      fail(invalid_input);
    if (status == punycode_big_output)
      fail(too_big);
    if (status == punycode_overflow)
      fail(overflow);
    assert(status == punycode_success);

    /* Convert to native charset and output: */

    for(j = 0; j < output_length; ++j) {
      c = output[j];
      assert(c >= 0 && c <= 127);
      if (print_ascii[c] == 0)
        fail(invalid_input);
      output[j] = print_ascii[c];
    }

    output[j] = 0;
    r = puts(output);
    if (r == EOF)
      fail(io_error);
    return EXIT_SUCCESS;
  }

  if (argv[1][1] == 'd') {
    char input[ace_max_length + 2], *p, *pp;
    punycode_uint output[unicode_max_length];

    /* Read the Punycode input string and convert to ASCII: */

    fgets(input, ace_max_length + 2, stdin);
    if (ferror(stdin))
      fail(io_error);
    if (feof(stdin))
      fail(invalid_input);
    input_length = strlen(input) - 1;
    if (input[input_length] != '\n')
      fail(too_big);
    input[input_length] = 0;

    for(p = input; *p != 0; ++p) {
      pp = strchr(print_ascii, *p);
      if (pp == 0)
        fail(invalid_input);
      *p = pp - print_ascii;
    }

    /* Decode: */

    output_length = unicode_max_length;
    status =
      punycode_decode(input_length, input, &output_length, output, case_flags);
    if (status == punycode_bad_input)
      fail(invalid_input);
    if (status == punycode_big_output)
      fail(too_big);
    if (status == punycode_overflow)
      fail(overflow);
    assert(status == punycode_success);

    /* Output the result: */

    for(j = 0; j < output_length; ++j) {
      r =
        printf("%s+%04lX\n", case_flags[j] ? "U" : "u",
               (unsigned long) output[j]);
      if (r < 0)
        fail(io_error);
    }

    return EXIT_SUCCESS;
  }

  usage(argv);
  return EXIT_SUCCESS;          /* not reached, but quiets compiler warning */
}

#endif
