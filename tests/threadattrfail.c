/* Forces pthread_create() to fail for 113_engine-threadattr-leak.test, and
   tracks which attributes objects were left undestroyed after that failure
   (#772). glibc allocates nothing for a default attr, so the imbalance is
   invisible to a leak checker and has to be counted here. */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The tree builds with -fvisibility=hidden, which would hide the interposer. */
#define SHIM_EXPORT __attribute__((visibility("default")))

#define PENDING_MAX 64

static const void *pending[PENDING_MAX];
static unsigned int pending_n;
static unsigned int sabotaged;

SHIM_EXPORT int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                               void *(*start)(void *), void *arg);
SHIM_EXPORT int pthread_attr_destroy(pthread_attr_t *attr);

SHIM_EXPORT int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                               void *(*start)(void *), void *arg) {
  static int (*real_create)(pthread_t *, const pthread_attr_t *,
                            void *(*) (void *), void *) = NULL;

  if (getenv("THREADATTRFAIL") != NULL) {
    sabotaged++;
    if (attr != NULL && pending_n < PENDING_MAX)
      pending[pending_n++] = attr;
    return EAGAIN;
  }
  if (real_create == NULL) {
    *(void **) &real_create = dlsym(RTLD_NEXT, "pthread_create");
    if (real_create == NULL)
      return ENOSYS;
  }
  return real_create(thread, attr, start, arg);
}

SHIM_EXPORT int pthread_attr_destroy(pthread_attr_t *attr) {
  static int (*real_destroy)(pthread_attr_t *) = NULL;
  unsigned int i;

  /* drop one entry, so an address reused by a later init still counts once */
  for (i = pending_n; i > 0; i--) {
    if (pending[i - 1] == attr) {
      memmove(&pending[i - 1], &pending[i],
              (pending_n - i) * sizeof(pending[0]));
      pending_n--;
      break;
    }
  }
  if (real_destroy == NULL) {
    *(void **) &real_destroy = dlsym(RTLD_NEXT, "pthread_attr_destroy");
    if (real_destroy == NULL)
      return ENOSYS;
  }
  return real_destroy(attr);
}

__attribute__((destructor)) static void threadattrfail_report(void) {
  const char *const path = getenv("THREADATTRFAIL_LOG");
  FILE *fp;

  if (path == NULL)
    return;
  fp = fopen(path, "wb");
  if (fp == NULL)
    return;
  fprintf(fp, "sabotaged %u\nundestroyed %u\n", sabotaged, pending_n);
  fclose(fp);
}
