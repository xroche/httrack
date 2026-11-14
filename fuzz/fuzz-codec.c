/* ------------------------------------------------------------ */

/* Fuzz the Content-Encoding unpackers the way the receive path drives them: a
   coded body in a file, decoded into another, under the user's -m cap. The
   first byte picks the codec, the cap and whether the
   sniffer's prefix decoder runs too, so one corpus walks every path:
   wrapper detection, the raw-deflate retry, the verbatim copy of a mislabeled
   body and the decompression-bomb limit. br and zstd decode only in a build
   that has them, and otherwise hts_codec_unpack_max() must refuse them. */
#include "fuzz.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "htscodec.h"

/* A coded body past this size says nothing a smaller one cannot. */
#define FUZZ_CODEC_MAXSIZE (256 * 1024)

static char codec_dir[256];
static char codec_in[sizeof(codec_dir) + sizeof("/in.bin")];
static char codec_out[sizeof(codec_dir) + sizeof("/out.bin")];

static void fuzz_codec_cleanup(void) {
  (void) unlink(codec_in);
  (void) unlink(codec_out);
  (void) rmdir(codec_dir);
}

static int fuzz_codec_setup(void) {
  if (codec_in[0] == '\0') {
    const char *const tmp = getenv("TMPDIR");

    snprintf(codec_dir, sizeof(codec_dir), "%s/fuzz-codec-XXXXXX",
             tmp != NULL && *tmp != '\0' ? tmp : "/tmp");
    if (mkdtemp(codec_dir) == NULL) {
      return -1;
    }
    snprintf(codec_in, sizeof(codec_in), "%s/in.bin", codec_dir);
    snprintf(codec_out, sizeof(codec_out), "%s/out.bin", codec_dir);
    atexit(fuzz_codec_cleanup);
  }
  return 0;
}

/* Cap 0 is "no -m limit"; the others sit around the chunk sizes (8 KiB) and
   the built-in ratio floor, so the cap binds on some bodies and not others. */
static const LLint fuzz_codec_caps[8] = {0,    1,    100,   4096,
                                         8192, 8193, 65536, 1 << 20};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static const hts_codec codecs[4] = {HTS_CODEC_DEFLATE, HTS_CODEC_BROTLI,
                                      HTS_CODEC_ZSTD, HTS_CODEC_UNSUPPORTED};
  hts_codec codec;
  LLint cap;
  FILE *fp;
  hts_boolean written;
  unsigned char head[64];
  int ret;

  if (size < 1 || size - 1 > FUZZ_CODEC_MAXSIZE || fuzz_codec_setup() != 0) {
    return 0;
  }
  codec = codecs[data[0] & 3];
  cap = fuzz_codec_caps[(data[0] >> 2) & 7];
  if ((fp = fopen(codec_in, "wb")) == NULL) {
    return 0;
  }
  written =
      fwrite(data + 1, 1, size - 1, fp) == size - 1 ? HTS_TRUE : HTS_FALSE;
  if (fclose(fp) != 0 || !written) {
    return 0;
  }

  /* the MIME sniffer's prefix decoder reads the same body from memory */
  if ((data[0] & 0x20) != 0 && hts_codec_head(codec, data + 1, size - 1, head,
                                              sizeof(head)) > sizeof(head)) {
    abort();
  }
  ret = hts_codec_unpack_max(codec, codec_in, codec_out, cap);
  /* The contract: a size, -1, or the cap verdict, and no verdict without a
     cap or for the codec that never decodes. */
  if (ret < -2 || (ret == HTS_CODEC_TOOBIG && cap == 0) ||
      (codec == HTS_CODEC_UNSUPPORTED && ret != -1)) {
    abort();
  }
  if (ret >= 0) {
    /* the output file holds exactly the bytes reported */
    if ((fp = fopen(codec_out, "rb")) == NULL) {
      abort();
    }
    if (fseek(fp, 0, SEEK_END) != 0 || ftell(fp) != (long) ret) {
      abort();
    }
    fclose(fp);
    if (cap > 0 && ret > cap) {
      abort();
    }
  }
  return 0;
}
