/* Fires the exported stop a GUI's Stop button pushes, from outside the engine:
   444_local-stop-keeps-resume.test writes STOPRESUME_TRIGGER once a partial
   transfer is on disk, and STOPRESUME_KEEP picks the flag to pass. */

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

static void *stopresume_opt = NULL;

/* Polls for the trigger rather than sleeping a fixed delay: the test decides
   when the transfer is really in flight. Gives up so a missed trigger fails
   the test rather than pinning a core. */
static void *stopresume_thread(void *unused) {
  int (*request_stop)(void *, int) = NULL;
  const char *const trigger = getenv("STOPRESUME_TRIGGER");
  const char *const keep = getenv("STOPRESUME_KEEP");
  int left = 600;

  (void) unused;
  *(void **) &request_stop = dlsym(RTLD_DEFAULT, "hts_request_stop");
  if (trigger == NULL || keep == NULL || request_stop == NULL) {
    return NULL;
  }
  while (left-- > 0) {
    if (access(trigger, R_OK) == 0) {
      request_stop(stopresume_opt, atoi(keep));
      return NULL;
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
  stopresume_opt = real_create();
  if (stopresume_opt != NULL &&
      pthread_create(&thread, NULL, stopresume_thread, NULL) == 0) {
    pthread_detach(thread);
  }
  return stopresume_opt;
}
