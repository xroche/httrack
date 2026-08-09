/* Fires the engine's per-link cancel, the one WebHTTrack's stop button pushes,
   from outside the engine: 253_local-ftp-close-once.test writes the save path
   to cancel into FTPCANCEL_TRIGGER once the FTP transfer is in flight. */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* The tree builds with -fvisibility=hidden, which would hide the interposer. */
#define SHIM_EXPORT __attribute__((visibility("default")))

/* The engine's own httrackp, opaque here. */
SHIM_EXPORT void *hts_create_opt(void);

static void *ftpcancel_opt = NULL;

/* Polls for the trigger rather than sleeping a fixed delay: the test decides
   when the transfer is really in flight. Gives up so a missed trigger fails
   the test rather than pinning a core. */
static void *ftpcancel_thread(void *unused) {
  int (*push)(void *, const char *) = NULL;
  const char *const trigger = getenv("FTPCANCEL_TRIGGER");
  int left = 600;

  (void) unused;
  *(void **) &push = dlsym(RTLD_DEFAULT, "hts_cancel_file_push");
  if (trigger == NULL || push == NULL) {
    return NULL;
  }
  while (left-- > 0) {
    FILE *const fp = fopen(trigger, "rb");

    if (fp != NULL) {
      char url[2048];
      const char *const got = fgets(url, (int) sizeof(url), fp);
      char *const eol = got != NULL ? strchr(url, '\n') : NULL;

      fclose(fp);
      if (eol != NULL) {
        *eol = '\0';
        push(ftpcancel_opt, url);
        return NULL;
      }
    }
    usleep(100000);
  }
  return NULL;
}

SHIM_EXPORT void *hts_create_opt(void) {
  static void *(*real_create)(void) = NULL;
  pthread_t thread;

  if (real_create == NULL) {
    *(void **) &real_create = dlsym(RTLD_NEXT, "hts_create_opt");
    if (real_create == NULL) {
      return NULL;
    }
  }
  ftpcancel_opt = real_create();
  if (ftpcancel_opt != NULL &&
      pthread_create(&thread, NULL, ftpcancel_thread, NULL) == 0) {
    pthread_detach(thread);
  }
  return ftpcancel_opt;
}
