/*
HTTrack Android JAVA Native Interface Stubs.

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
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <jni.h>
#include <assert.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <android/log.h>

#include "htsglobal.h"
#include "htsbase.h"
#include "htsopt.h"
#include "httrack-library.h"
#include "htsdefines.h"
#include "htscore.h"

#define USE_COFFEECATCH

#ifdef USE_COFFEECATCH
#include "coffeecatch.h"
#include "coffeejni.h"
#endif

/* fine-grained stats */
#define GENERATE_FINE_STATS 1

/* redirect stdio on a log file ? */
#define REDIRECT_STDIO_LOG_FILE

/* root path */
static char *emergencyLog = NULL;
static char *rootPath = NULL;

/* log assertion failure. */
static void log_assert_failure(const char* exp, const char* file, int line) {
  __android_log_print(ANDROID_LOG_VERBOSE, "httrack", "assertion '%s' failed at %s:%d", exp, file, line);
#define MKDIR_MODE (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)
  /* FIXME TODO: pass the getExternalStorageDirectory() in init. */
  FILE *dumpFile;
  const char *const filename = emergencyLog != NULL ? emergencyLog
    : "/mnt/sdcard/Download/HTTrack/error.txt";
  dumpFile = fopen(filename, "wb");
  if (dumpFile != NULL) {
    fprintf(dumpFile, "assertion '%s' failed at %s:%d\n", exp, file, line);
    fclose(dumpFile);
  }
}

/* our own assert version. */
static void assert_failure(const char* exp, const char* file, int line) {
  log_assert_failure(exp, file, line);
#ifdef USE_COFFEECATCH
  coffeecatch_abort(exp, file, line);
#else
  abort();
#endif
}
#undef assert
#define assert(EXP) (void)( (EXP) || (assert_failure(#EXP, __FILE__, __LINE__), 0) )

/* httrack-to-android error level dispatch */
static int get_android_prio(const int type) {
  switch(type & 0xff) {
  case LOG_PANIC:
    return ANDROID_LOG_ERROR;
  case LOG_ERROR:
    return ANDROID_LOG_INFO;
  case LOG_WARNING:
  case LOG_NOTICE:
  case LOG_INFO:
  case LOG_DEBUG:
    return ANDROID_LOG_DEBUG;
  case LOG_TRACE:
  default:
    return ANDROID_LOG_VERBOSE;
  }
}

/* our own debugging log callabck */
static void httrackLogCallback(httrackp *opt, int type, 
                               const char *format, va_list args) {
  __android_log_vprint(get_android_prio(type), "httrack", format, args);
}

static void debug(const char *format, ...)
  __attribute__ ((format (printf, 1, 2)));
static void debug(const char *format, ...) {
  va_list args;
  va_start(args, format);
  __android_log_vprint(ANDROID_LOG_DEBUG, "httrack", format, args);
  va_end(args);
}

static void error(const char *format, ...)
  __attribute__ ((format (printf, 1, 2)));
static void error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  __android_log_vprint(ANDROID_LOG_ERROR, "httrack", format, args);
  va_end(args);
}

/** Context for HTTrackLib. **/
typedef struct HTTrackLib_context {
  pthread_mutex_t lock;
  httrackp * opt;
  int stop;
} HTTrackLib_context;

#define MUTEX_LOCK(MUTEX) do {                  \
    if (pthread_mutex_lock(&MUTEX) != 0) {      \
      assert(! "pthread_mutex_lock failed");    \
    }                                           \
  } while(0)

#define MUTEX_UNLOCK(MUTEX) do {                \
    if (pthread_mutex_unlock(&MUTEX) != 0) {    \
      assert(! "pthread_mutex_unlock failed");  \
    }                                           \
  } while(0)

#define UNUSED(VAR) (void) VAR

/**
 * Thread-specific context.
 */
typedef struct thread_context_t {
  char *buffer;
  size_t capa;
} thread_context_t;

/* Thread variable holding context. */
static pthread_key_t thread_variables;

static void thread_variables_dtor(void *arg) {
  thread_context_t *const context = (thread_context_t*) arg;
  if (context != NULL) {
    if (context->buffer != NULL) {
      free(context->buffer);
      context->buffer = NULL;
    }
    free(context);
  }
}

static thread_context_t* thread_get_variables(void) {
  void *arg = pthread_getspecific(thread_variables);
  if (arg == NULL) {
    arg = calloc(sizeof(thread_context_t), 1);
    if (pthread_setspecific(thread_variables, arg) != 0) {
      assert(! "pthread_setspecific() failed");
    }
  }
  return (thread_context_t*) arg;
}

/* HTTrackLib global class pointer. */
static jclass cls_HTTrackLib = NULL;
static jclass cls_HTTrackCallbacks = NULL;
static jclass cls_HTTrackStats = NULL;
static jclass cls_HTTrackStats_Element = NULL;

/* HTTrackStats default constructor. */
static jmethodID cons_HTTrackStats = NULL;
static jmethodID cons_HTTrackStats_Element = NULL;

/* HTTrackStats methods */
static jmethodID meth_HTTrackCallbacks_onRefresh = NULL;

/* The stats field */
static jfieldID field_callbacks = NULL;
static jfieldID field_nativeObject = NULL;

/* The elements field */
static jfieldID field_elements = NULL;

/* List of fields (main stats). */
#define LIST_OF_FIELDS() \
DECLARE_FIELD(state); \
DECLARE_FIELD(completion); \
DECLARE_FIELD(linksScanned); \
DECLARE_FIELD(linksTotal); \
DECLARE_FIELD(linksBackground); \
DECLARE_FIELD(bytesReceived); \
DECLARE_FIELD(bytesWritten); \
DECLARE_FIELD(startTime); \
DECLARE_FIELD(elapsedTime); \
DECLARE_FIELD(bytesReceivedCompressed); \
DECLARE_FIELD(bytesReceivedUncompressed); \
DECLARE_FIELD(filesReceivedCompressed); \
DECLARE_FIELD(filesWritten); \
DECLARE_FIELD(filesUpdated); \
DECLARE_FIELD(filesWrittenBackground); \
DECLARE_FIELD(requestsCount); \
DECLARE_FIELD(socketsAllocated); \
DECLARE_FIELD(socketsCount); \
DECLARE_FIELD(errorsCount); \
DECLARE_FIELD(warningsCount); \
DECLARE_FIELD(infosCount); \
DECLARE_FIELD(totalTransferRate); \
DECLARE_FIELD(transferRate)

#define STRING "Ljava/lang/String;"

/* List of fields (Element). **/
#define LIST_OF_FIELDS_ELT() \
DECLARE_FIELD(STRING, address); \
DECLARE_FIELD(STRING, path); \
DECLARE_FIELD(STRING, filename); \
DECLARE_FIELD("Z", isUpdate); \
DECLARE_FIELD("I", state); \
DECLARE_FIELD("I", code); \
DECLARE_FIELD(STRING, message); \
DECLARE_FIELD("Z", isNotModified); \
DECLARE_FIELD("Z", isCompressed); \
DECLARE_FIELD("J", size); \
DECLARE_FIELD("J", totalSize); \
DECLARE_FIELD(STRING, mime); \
DECLARE_FIELD(STRING, charset)

/* Declare fields ids. */
#define DECLARE_FIELD(NAME) static jfieldID field_ ##NAME = NULL
LIST_OF_FIELDS();
#undef DECLARE_FIELD

/* Declare fields ids (stats). */
#define DECLARE_FIELD(TYPE, NAME) static jfieldID field_elt_ ##NAME = NULL
LIST_OF_FIELDS_ELT();
#undef DECLARE_FIELD

static jclass findClass(JNIEnv *env, const char *name) {
  jclass localClass = (*env)->FindClass(env, name);
  /* "Note however that the jclass is a class reference and must be protected
   * with a call to NewGlobalRef " -- DARN! */
  if (localClass != NULL) {
    jclass globalClass = (*env)->NewGlobalRef(env, localClass);
    (*env)->DeleteLocalRef(env, localClass);
    return globalClass;
  }
  return NULL;
}

static void releaseClass(JNIEnv *env, jclass *cls) {
  if (cls != NULL) {
    (*env)->DeleteGlobalRef(env, *cls);
    *cls = NULL;
  }
}

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  union {
    void *ptr;
    JNIEnv *env;
  } u;
  if ((*vm)->GetEnv(vm, &u.ptr, JNI_VERSION_1_6) != JNI_OK) {
    return -1;
  }
  UNUSED(reserved);

  /**
   * Note: we're not doing much here, because we do not have any clear
   * guarantee over what is safe inside the JNI_OnLoad() handler (throwing
   * exceptions ?). The static initializer will do the real job later.
   */

  /* Java VM 1.6 */
  return JNI_VERSION_1_6;
}

/* We are supposed to keep the error message string when throwing an
 * exception, because Java does not copy it and just keeps the UTF-8 buffer
 * reference. We do it thread-safely at least, but every new exception is
 * crushing the buffer. */
static char *getSafeCopy(const char *message) {
  const size_t size = strlen(message) + 1;
  thread_context_t*const context = thread_get_variables();

  if (context->capa < size) {
    for(context->capa = 16 ; context->capa < size ; context->capa <<= 1) ;
    context->buffer = realloc(context->buffer, context->capa);
    assert(context->buffer != NULL);
  }
  strcpy(context->buffer, message);
  return context->buffer;
}

static void throwException(JNIEnv* env, const char *exception,
    const char *message) {
  jclass cls = (*env)->FindClass(env, exception);
  assert(cls != NULL);
  (*env)->ThrowNew(env, cls, getSafeCopy(message));
  (*env)->DeleteLocalRef(env, cls);
}

static void throwRuntimeException(JNIEnv* env, const char *message) {
  throwException(env, "java/lang/RuntimeException", message);
}

static void throwIOException(JNIEnv* env, const char *message) {
  throwException(env, "java/io/IOException", message);
}

static void throwNPException(JNIEnv* env, const char *message) {
  throwException(env, "java/lang/NullPointerException", message);
}

static void httrackAssertFailure(const char* exp, const char* file, int line) {
  assert_failure(exp, file, line);
}

/* Static initialization. */
void Java_com_httrack_android_jni_HTTrackLib_initStatic(JNIEnv* env, jclass clazz) {
#define L_(X) #X
#define L(X) L_(X)
#define ASSERT_THROWS(EXP)                                          \
  do {                                                              \
    if (!(EXP)) {                                                   \
      throwRuntimeException(env,                                    \
          "assertion '" #EXP "' failed at " __FILE__ L(__LINE__));  \
      return ;                                                      \
    }                                                               \
  } while(0)

  debug("calling Java_com_httrack_android_jni_HTTrackLib_initStatic");

  /* UNUSED */
  (void) clazz;

  /* Initialize thread variables. */
  if (pthread_key_create(&thread_variables, thread_variables_dtor) != 0) {
    assert(! "pthread_key_create() failed");
  }

  /* HTTrackLib class */
  cls_HTTrackLib = findClass(env, "com/httrack/android/jni/HTTrackLib");
  ASSERT_THROWS(cls_HTTrackLib != NULL);
  cls_HTTrackCallbacks = findClass(env, "com/httrack/android/jni/HTTrackCallbacks");
  ASSERT_THROWS(cls_HTTrackCallbacks != NULL);
  cls_HTTrackStats = findClass(env, "com/httrack/android/jni/HTTrackStats");
  ASSERT_THROWS(cls_HTTrackStats != NULL);
  cls_HTTrackStats_Element = findClass(env, "com/httrack/android/jni/HTTrackStats$Element");
  ASSERT_THROWS(cls_HTTrackStats_Element != NULL);

  /* "Note that jfieldIDs and jmethodIDs are opaque types, not object
   * references, and should not be passed to NewGlobalRef" */

  /* Constructors */
  cons_HTTrackStats = (*env)->GetMethodID(env, cls_HTTrackStats, "<init>", "()V");
  ASSERT_THROWS(cons_HTTrackStats != NULL);
  cons_HTTrackStats_Element =
    (*env)->GetMethodID(env, cls_HTTrackStats_Element, "<init>", "()V");
  ASSERT_THROWS(cons_HTTrackStats_Element != NULL);

  /* Methods */
  meth_HTTrackCallbacks_onRefresh =
    (*env)->GetMethodID(env, cls_HTTrackCallbacks, "onRefresh",
                        "(Lcom/httrack/android/jni/HTTrackStats;)V");
  ASSERT_THROWS(meth_HTTrackCallbacks_onRefresh != NULL);

  /* The "callbacks" field */
  field_callbacks = (*env)->GetFieldID(env, cls_HTTrackLib, "callbacks",
                                       "Lcom/httrack/android/jni/HTTrackCallbacks;");
  ASSERT_THROWS(field_callbacks != NULL);

  /* The "nativeObject" opaque object. */
  field_nativeObject = (*env)->GetFieldID(env, cls_HTTrackLib, "nativeObject", "J");
  ASSERT_THROWS(field_nativeObject != NULL);

  /* Load HTTrackStats fields ids. */
#define DECLARE_FIELD(NAME) do {                                 \
  field_ ##NAME = (*env)->GetFieldID(env, cls_HTTrackStats,      \
                                     #NAME, "J");                \
  ASSERT_THROWS(field_ ##NAME != NULL);                          \
} while(0)
  LIST_OF_FIELDS();
#undef DECLARE_FIELD

  /* The elements array */
  field_elements = (*env)->GetFieldID(env, cls_HTTrackStats, "elements",
                                      "[Lcom/httrack/android/jni/HTTrackStats$Element;");
  ASSERT_THROWS(field_elements != NULL);

  /* Load HTTrackStats fields (element) ids. */
#define DECLARE_FIELD(TYPE, NAME) do {                               \
  field_elt_ ##NAME = (*env)->GetFieldID(env,                        \
                                         cls_HTTrackStats_Element,   \
                                         #NAME, TYPE);               \
  ASSERT_THROWS(field_elt_ ##NAME != NULL);                          \
} while(0)
  LIST_OF_FIELDS_ELT();
#undef DECLARE_FIELD

  /* Initialize engine. */
  if (hts_init() != 1) {
    ASSERT_THROWS(! "hts_init() failed");
  }

  /* Register assert callback */
  hts_set_error_callback(httrackAssertFailure);

  /* Register log callback */
  hts_set_log_vprint_callback(httrackLogCallback);
}

void Java_com_httrack_android_jni_HTTrackLib_initRootPath(JNIEnv* env, 
                                                          jclass clazz, 
                                                          jstring opath) {
  if (opath != NULL) {
    const char* path = (*env)->GetStringUTFChars(env, opath, NULL);

    debug("calling Java_com_httrack_android_jni_HTTrackLib_initRootPath(%s)",
          path);

    /* Root path. */
    if (rootPath != NULL) {
      free(rootPath);
      rootPath = NULL;
    }
    rootPath = strdup(path);

    /* Emergency log. */
    {
      const size_t buffer_size = strlen(path) + 256;
      char *const buffer = malloc(buffer_size);
      if (buffer != NULL) {
        snprintf(buffer, buffer_size, "%s/error.txt", path);
        if (emergencyLog != NULL) {
          free(emergencyLog);
          emergencyLog = NULL;
        }
        emergencyLog = buffer;
      }
    }

    /* Redirect stdio. */
#ifdef REDIRECT_STDIO_LOG_FILE
    {
      const size_t buffer_size = strlen(path) + 256;
      char *const buffer = malloc(buffer_size);
      snprintf(buffer, buffer_size, "%s/log.txt", path);
      FILE * const log = fopen(buffer, "wb");
      if (log != NULL) {
        const int fd = dup(fileno(log));
        if (dup2(fd, 1) == -1 || dup2(fd, 2) == -1) {
          ASSERT_THROWS(!"could not redirect stdin/stdout");
        }
        fclose(log);
        fprintf(stderr, "started stdio logging in file\n");
      }
      free(buffer);
    }
#endif

    (*env)->ReleaseStringUTFChars(env, opath, path);
  }
#undef ASSERT_THROWS
#undef L
#undef L_
}

/* note: never called on Android */
void JNI_OnUnload(JavaVM *vm, void *reserved) {
  union {
    void *ptr;
    JNIEnv *env;
  } u;
  UNUSED(reserved);

  debug("calling JNI_OnUnload");

  if ((*vm)->GetEnv(vm, &u.ptr, JNI_VERSION_1_6) != JNI_OK) {
    return ;
  }
  releaseClass(u.env, &cls_HTTrackLib);
  releaseClass(u.env, &cls_HTTrackCallbacks);
  releaseClass(u.env, &cls_HTTrackStats);
  releaseClass(u.env, &cls_HTTrackStats_Element);

  pthread_key_delete(thread_variables);
}

static void setNativeOpt(JNIEnv* env, jobject object, HTTrackLib_context *opt) {
  (*env)->SetLongField(env, object, field_nativeObject,
      (jlong) (uintptr_t) (void*) opt);
}

static HTTrackLib_context* getNativeOpt(JNIEnv* env, jobject object) {
  return (HTTrackLib_context*) (void*) (uintptr_t)
      (*env)->GetLongField(env, object, field_nativeObject);
}

jstring Java_com_httrack_android_jni_HTTrackLib_getVersion(JNIEnv* env, jclass clazz) {
  const char *version = hts_version();
  assert(version != NULL);
  UNUSED(clazz);
  return (*env)->NewStringUTF(env, version);
}

jstring Java_com_httrack_android_jni_HTTrackLib_getFeatures(JNIEnv* env, jclass clazz) {
  const char *features = hts_is_available();
  assert(features != NULL);
  UNUSED(clazz);
  return (*env)->NewStringUTF(env, features);
}

typedef struct jni_context_t {
  JNIEnv *env;
  /* HTTrackCallbacks object */
  jobject callbacks; 
  /* Context */
  HTTrackLib_context *context;
} jni_context_t;

typedef enum hts_state_id_t {
  STATE_NONE = 0,
  STATE_RECEIVE,
  STATE_CONNECTING,
  STATE_DNS,
  STATE_FTP,
  STATE_READY,
  STATE_MAX
} hts_state_id_t;

typedef struct hts_state_t {
  size_t index;
  hts_state_id_t state;
  int code;
  const char *message;
} hts_state_t;

/* NewStringUTF, but ignore invalid UTF-8 or NULL input. */
static jobject newStringSafe(JNIEnv *env, const char *s) {
  if (s != NULL) {
    const int ne = ! (*env)->ExceptionOccurred(env);
    jobject str = (*env)->NewStringUTF(env, s);
    /* Silently ignore UTF-8 exception. */
    if (str == NULL && (*env)->ExceptionOccurred(env) && ne) {
      (*env)->ExceptionClear(env);
    }
    return str;
  }
  return NULL;
}

void Java_com_httrack_android_jni_HTTrackLib_init(JNIEnv* env, jobject object) {
  HTTrackLib_context *const context = (HTTrackLib_context*)
      calloc(sizeof(HTTrackLib_context), 1);

  debug("calling Java_com_httrack_android_jni_HTTrackLib_init");

  if (context == NULL) {
    throwRuntimeException(env, "memory exhausted");
    return;
  }

  pthread_mutex_init(&context->lock, NULL);
  context->opt = NULL;
  context->stop = 0;
  setNativeOpt(env, object, context);
}

void Java_com_httrack_android_jni_HTTrackLib_free(JNIEnv* env, jobject object) {
  HTTrackLib_context *const context = getNativeOpt(env, object);

  debug("calling Java_com_httrack_android_jni_HTTrackLib_free");

  if (context != NULL) {
    setNativeOpt(env, object, NULL);
    pthread_mutex_destroy(&context->lock);
    if (context->opt != NULL) {
      /* This should never hapend */
      if (!hts_has_stopped(context->opt)) {
        const int timeout = 50;  /* Wait at most 5 seconds */
        int i;
        error("HTTrack thread is still running but the object is finalized, stopping running thread!");
        for(i = 0 ; i < timeout && !hts_has_stopped(context->opt) ; i++) {
          hts_request_stop(context->opt, 1);
          usleep(/* us ; 1/10s */100000);
        }
        assert(hts_has_stopped(context->opt));
        debug("HTTrack thread has now ended")
      } else {
        debug("HTTrack thread has ended");
      }
      debug("freeing 'opt' structure");
      hts_free_opt(context->opt);
      context->opt = NULL;
    }
    free(context);
  }
}

static jobject build_stats(jni_context_t *const t, httrackp * opt,
  lien_back * back, int back_max, int back_index, int lien_n,
  int lien_tot, int stat_time, hts_stat_struct * stats) {
#define STATE_MAX 32
  hts_state_t state[STATE_MAX];
  size_t index = 0;

  /* create stats object */
  jobject ostats = (*t->env)->NewObject(t->env, cls_HTTrackStats, cons_HTTrackStats);
  if (ostats == NULL) {
    return NULL;
  }

  assert(stats != NULL);

#define COPY_(VALUE, FIELD) do { \
  (*t->env)->SetLongField(t->env, ostats, field_ ##FIELD, \
      (jlong) (VALUE)); \
} while(0)
#define COPY(MEMBER, FIELD) COPY_(stats->MEMBER, FIELD)

  /* Copy stats */
  COPY(HTS_TOTAL_RECV, bytesReceived);
  COPY(rate, transferRate);
  COPY(stat_bytes, bytesWritten);
  COPY(stat_timestart, startTime);
  COPY(total_packed, bytesReceivedCompressed);
  COPY(total_unpacked, bytesReceivedUncompressed);
  COPY(total_packedfiles, filesReceivedCompressed);
  COPY(stat_files, filesWritten);
  COPY(stat_updated_files, filesUpdated);
  COPY(stat_background, filesWrittenBackground);
  COPY(stat_nrequests, requestsCount);
  COPY(stat_sockid, socketsAllocated);
  COPY(stat_nsocket, socketsCount);
  COPY(stat_errors, errorsCount);
  COPY(stat_warnings, warningsCount);
  COPY(stat_infos, infosCount);
  if (stat_time > 0 && stats->HTS_TOTAL_RECV > 0) {
    const jlong rate = (jlong) (stats->HTS_TOTAL_RECV / stat_time);
    COPY_(rate, totalTransferRate);
  }
  COPY(nbk, linksBackground);

  COPY_(hts_is_testing(opt), state);
  COPY_(hts_is_parsing(opt, -1), completion);
  COPY_(lien_n, linksScanned);
  COPY_(lien_tot, linksTotal);
  COPY_(stat_time, elapsedTime);

  /* Collect individual stats */
  if (back_index >= 0) {
    const size_t index_max = STATE_MAX;
    size_t k;
    /* current links first */
    for(k = 0, index = 0 ; k < 2 ; k++) {
      /* priority (receive first, then requests, then ready slots) */
      size_t j;
      for(j = 0; j < 3 && index < index_max ; j++) {
        size_t _i;
        /* when k=0, just take first item (possibly being parsed) */
        for(_i = k; _i < ( k == 0 ? 1 : (size_t) back_max ) && index < index_max; _i++) {        // no lien
          const size_t i = (back_index + _i) % back_max;
          /* active link */
          if (back[i].status >= 0) {
            /* Cleanup */
            state[index].state = STATE_NONE;
            state[index].code = 0;
            state[index].message = NULL;

            /* Check state */
            switch (j) {
            case 0:          // prioritaire
              if (back[i].status > 0 && back[i].status < 99) {
                state[index].state = STATE_RECEIVE;
              }
              break;
            case 1:
              if (back[i].status == STATUS_WAIT_HEADERS) {
                state[index].state = STATE_RECEIVE;
              } else if (back[i].status == STATUS_CONNECTING) {
                state[index].state = STATE_CONNECTING;
              } else if (back[i].status == STATUS_WAIT_DNS) {
                state[index].state = STATE_DNS;
              } else if (back[i].status == STATUS_FTP_TRANSFER) {
                state[index].state = STATE_FTP;
              }
              break;
            default:
              if (back[i].status == STATUS_READY) {   // prêt
                state[index].state = STATE_READY;
                state[index].code = back[i].r.statuscode;
                state[index].message = back[i].r.msg;
              }
              break;
            }

            /* Increment */
            if (state[index].state != STATE_NONE) {
              /* Take note of the offset */
              state[index].index = i;

              /* Next one */
              index++;
            }
          }
        }
      }
    }
  }

  if (GENERATE_FINE_STATS && back_index >= 0) {
    size_t i;

    /* Elements */
    jobjectArray elements =
      (*t->env)->NewObjectArray(t->env, index, cls_HTTrackStats_Element, NULL);
    if (elements == NULL) {
      return NULL;
    }

    /* Put elements in elements field of stats */
    (*t->env)->SetObjectField(t->env, ostats, field_elements, elements);

    /* Fill */
    for(i = 0 ; i < index ; i++) {
      /* Index to back[] */
      const int index = state[i].index;

      /* Create item */
      jobject element =
        (*t->env)->NewObject(t->env, cls_HTTrackStats_Element,
                             cons_HTTrackStats_Element);
      if (element == NULL) {
        return NULL;
      }
      (*t->env)->SetObjectArrayElement(t->env, elements, i, element);

      /* Set a string inside "element". */
#define SET_ELEMENT_STRING(FIELD, STRING) do {                 \
    jobject str_ = newStringSafe(t->env, STRING);              \
    if (str_ != NULL) {                                        \
      (*t->env)->SetObjectField(t->env, element, FIELD, str_); \
      (*t->env)->DeleteLocalRef(t->env, str_);                 \
    }                                                          \
} while(0)

      /* Fill item */
      SET_ELEMENT_STRING(field_elt_address, back[index].url_adr);
      SET_ELEMENT_STRING(field_elt_path, back[index].url_fil);
      SET_ELEMENT_STRING(field_elt_filename, back[index].url_sav);
      (*t->env)->SetBooleanField(t->env, element, field_elt_isUpdate,
                                 back[index].is_update != 0);
      (*t->env)->SetIntField(t->env, element, field_elt_state,
                             state[i].state);
      (*t->env)->SetIntField(t->env, element, field_elt_code,
                             back[index].r.statuscode);
      SET_ELEMENT_STRING(field_elt_message, back[index].r.msg);
      (*t->env)->SetBooleanField(t->env, element, field_elt_isNotModified,
                                 back[index].r.notmodified != 0);
      (*t->env)->SetBooleanField(t->env, element, field_elt_isCompressed,
                                 back[index].r.compressed!= 0);
      (*t->env)->SetLongField(t->env, element, field_elt_size,
                                 back[index].r.size);
      (*t->env)->SetLongField(t->env, element, field_elt_totalSize,
                                 back[index].r.totalsize);
      SET_ELEMENT_STRING(field_elt_mime, back[index].r.contenttype);
      SET_ELEMENT_STRING(field_elt_charset, back[index].r.charset);

#undef SET_ELEMENT_STRING

      /* Release local element reference. */
      (*t->env)->DeleteLocalRef(t->env, element);
    }

    /* Release local elements reference. */
    (*t->env)->DeleteLocalRef(t->env, elements);
  }

  return ostats;
}

static int htsshow_loop_internal(jni_context_t *t, httrackp * opt,
  lien_back * back, int back_max, int back_index, int lien_n,
  int lien_tot, int stat_time, hts_stat_struct * stats) {
  int code = 1;

  /* maximum number of objects on local frame */
  const jint capacity = 128;

  /* object */
  jobject ostats;

  /* no stats ? (even loop refresh) */
  if (stats == NULL) {
    return 1;
  }

  /* no callbacks */
  assert(t != NULL);
  if (t->callbacks == NULL) {
    return 1;
  }

  /* create a new local frame for local objects (stats, strings ...) */
  if ((*t->env)->PushLocalFrame(t->env, capacity) == 0) {
    /* build stats object */
    if (stats != NULL) {
      ostats = build_stats(t, opt, back, back_max, back_index, lien_n, lien_tot,
                           stat_time, stats);
    } else {
      ostats = NULL;
    }

    /* Call refresh method */
    if (ostats != NULL || stats == NULL) {
      (*t->env)->CallVoidMethod(t->env, t->callbacks,
                                meth_HTTrackCallbacks_onRefresh, ostats);
      if ((*t->env)->ExceptionOccurred(t->env)) {
        code = 0;
      }
    } else {
      code = 0;
    }

    /* wipe local frame */
    (void) (*t->env)->PopLocalFrame(t->env, NULL);
  } else {
    return 0;  /* error */
  }

  return code;
}

static int htsshow_loop(t_hts_callbackarg * carg, httrackp * opt,
  lien_back * back, int back_max, int back_index, int lien_n,
  int lien_tot, int stat_time, hts_stat_struct * stats) {

  /* get args context */
  void *const arg = (void *) CALLBACKARG_USERDEF(carg);
  jni_context_t *const t = (jni_context_t*) arg;

  /* exit now */
  if (t->context->stop) {
    return 0;
  }

  /* pass to internal version */
  return htsshow_loop_internal(t, opt, back, back_max, back_index,
      lien_n, lien_tot, stat_time, stats);
}

jboolean Java_com_httrack_android_jni_HTTrackLib_stop(JNIEnv* env, jobject object,
    jboolean force) {
  HTTrackLib_context *const context = getNativeOpt(env, object);
  jboolean stopped = JNI_FALSE;

  if (context == NULL) {
    throwRuntimeException(env, "null context");
  }

  MUTEX_LOCK(context->lock);
  if (context->opt != NULL) {
    stopped = JNI_TRUE;
    if (force) {
      context->stop = 1;
    }
    hts_request_stop(context->opt, force);
  }
  MUTEX_UNLOCK(context->lock);

  return stopped;
}

jint Java_com_httrack_android_jni_HTTrackLib_buildTopIndex(JNIEnv* env, jclass clazz,
    jstring opath, jstring otemplates) {
  if (opath != NULL && otemplates != NULL) {
    const char* path = (*env)->GetStringUTFChars(env, opath, NULL);
    const char* templates = (*env)->GetStringUTFChars(env, otemplates, NULL);
    int ret;

    httrackp * const opt = hts_create_opt();
    opt->log = opt->errlog = NULL;
    fprintf(stderr, "building top index to %s using templates %s\n", path, templates);
    ret = hts_buildtopindex(opt, path, templates);
    hts_free_opt(opt);

    (*env)->ReleaseStringUTFChars(env, opath, path);
    (*env)->ReleaseStringUTFChars(env, otemplates, templates);

    return ret;
  } else {
    throwNPException(env, "null argument(s)");
    return -1;
  }
  UNUSED(clazz);
}

jint HTTrackLib_main(JNIEnv* env, jobject object, jobjectArray stringArray) {
  HTTrackLib_context *const context = getNativeOpt(env, object);
  const int argc =
      object != NULL ? (*env)->GetArrayLength(env, stringArray) : 0;
  const size_t argv_size = (argc + 1) * sizeof(char*);
  char **const argv = (char**) malloc(argv_size);

  debug("calling HTTrackLib_main");

  if (argv != NULL) {
    int i;
    int code = -1;
    int already_running = 0;
    struct jni_context_t t;
    t.env = env;
    t.callbacks = (*env)->GetObjectField(env, object, field_callbacks);
    t.context = context;

    /* Create options and reference it */
    MUTEX_LOCK(context->lock);
    if (context->opt == NULL) {
      /* Create array */
      for (i = 0; i < argc; i++) {
        /* Note: a local reference is created here */
        jstring str = (jstring)(*env)->GetObjectArrayElement(env, stringArray,
            i);
        const char * const utf_string = (*env)->GetStringUTFChars(env, str, 0);
        argv[i] = strdup(utf_string != NULL ? utf_string : "");
        (*env)->ReleaseStringUTFChars(env, str, utf_string);
        (*env)->DeleteLocalRef(env, str);
      }
      argv[i] = NULL;

      /* Create opt tab */
      context->opt = hts_create_opt();
      context->stop = 0;
      CHAIN_FUNCTION(context->opt, loop, htsshow_loop, &t);
    } else {
      already_running = 1;
    }
    MUTEX_UNLOCK(context->lock);

    if (context->opt != NULL && !already_running) {
      const hts_stat_struct* stats;

      /* Rock'in! */
      code = hts_main2(argc, argv, context->opt);

      /* Fetch last stats before cleaning up */
      stats = hts_get_stats(context->opt);
      assert(stats != NULL);
      fprintf(stderr, "status code %d, %d errors, %d warnings\n",
          code, stats->stat_errors, stats->stat_warnings);
      (void) htsshow_loop_internal(&t, context->opt, NULL, 0, -1, 0, 0, 0,
          (hts_stat_struct*) stats);

      /* Raise error if suitable */
      if (code == -1) {
        const char *message = hts_errmsg(context->opt);
        if (message != NULL && *message != '\0') {
          throwIOException(env, message);
        }
      }

      /* Unreference global option tab */
      /* Nope - do this at destructor time */
      /*MUTEX_LOCK(context->lock);
      hts_free_opt(context->opt);
      context->opt = NULL;
      context->stop = 0;
      MUTEX_UNLOCK(context->lock);*/

      /* Cleanup */
      for (i = 0; i < argc; i++) {
        free(argv[i]);
      }
      free(argv);
    } else {
      if (already_running) {
        throwRuntimeException(env, "an instance of httrack is already running");
      } else {
        throwRuntimeException(env, "not enough native memory to create an httrack context");
      }
    }

    /* Return exit code. */
    return code;
  } else {
    char format[256];
    snprintf(format, sizeof(format), "not enough native memory (%d bytes)",
             (int) argv_size);
    throwRuntimeException(env, format);
    return -1;
  }
}

jint Java_com_httrack_android_jni_HTTrackLib_main(JNIEnv* env, jobject object,
    jobjectArray stringArray) {
#ifdef USE_COFFEECATCH
  volatile jint code = -1;
  COFFEE_TRY_JNI(env, code = HTTrackLib_main(env, object, stringArray));
  return code;
#else
  return HTTrackLib_main(env, object, stringArray);
#endif
}
