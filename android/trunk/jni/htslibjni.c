/*
 * HTTrack Android JAVA Native Interface Stubs.
 */
#include <string.h>
#include <jni.h>
#include <assert.h>
#include <pthread.h>

#include "htsglobal.h"
#include "htsbase.h"
#include "htsopt.h"
#include "httrack-library.h"
#include "htsdefines.h"
#include "htscore.h"

static httrackp * global_opt = NULL;
static pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

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


/* HTTrackLib class pointer. */
static jclass cls_HTTrackLib = NULL;

/* List of fields. */
#define LIST_OF_FIELDS() \
DECLARE_FIELD(state); \
DECLARE_FIELD(completion); \
DECLARE_FIELD(bytesReceived); \
DECLARE_FIELD(bytesWritten); \
DECLARE_FIELD(startTime); \
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

/* Declare fields ids. */
#define DECLARE_FIELD(NAME) static jfieldID field_ ##NAME = NULL
LIST_OF_FIELDS();
#undef DECLARE_FIELD

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  union {
    void *ptr;
    JNIEnv *env;
  } u;
  if ((*vm)->GetEnv(vm, &u.ptr, JNI_VERSION_1_6) != JNI_OK) {
    return -1;
  }

  /* HTTrackLib class */
  cls_HTTrackLib = (*u.env)->FindClass(u.env, "com/httrack/android/jni/HTTrackLib");
  assert(cls_HTTrackLib != NULL);

  /* Load fields ids. */
#define DECLARE_FIELD(NAME) do {                                 \
  field_ ##NAME = (*u.env)->GetFieldID(u.env, cls_HTTrackLib,    \
                                       #NAME, "J");              \
  assert(field_ ##NAME != NULL);                                 \
} while(0)
  LIST_OF_FIELDS();
#undef DECLARE_FIELD

  /* Java VM 1.6 */
  return JNI_VERSION_1_6;
}

/* FIXME -- This is dirty... we are supposed to keep the error message. */
static char *getSafeCopy(const char *message) {
  static char *buffer = NULL;
  if (buffer != NULL) {
    free(buffer);
  }
  buffer = strdup(message);
  return buffer;
}

static void throwException(JNIEnv* env, const char *exception,
    const char *message) {
  static char *buffer = NULL;
  jclass cls = (*env)->FindClass(env, exception);
  assert(cls != NULL);
  (*env)->ThrowNew(env, cls, getSafeCopy(message));
}

static void throwRuntimeException(JNIEnv* env, const char *message) {
  throwException(env, "java/lang/RuntimeException", message);
}

static void throwIOException(JNIEnv* env, const char *message) {
  throwException(env, "java/io/IOException", message);
}

void Java_com_httrack_android_jni_HTTrackLib_init(JNIEnv* env) {
  hts_init();
}

jstring Java_com_httrack_android_jni_HTTrackLib_getVersion(JNIEnv* env) {
  const char *version = hts_version();
  assert(version != NULL);
  return (*env)->NewStringUTF(env, version);
}

typedef struct jni_context_t {
  JNIEnv *env;
  jobject thiz;
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

static int htsshow_loop(t_hts_callbackarg * carg, httrackp * opt,
    lien_back * back, int back_max, int back_index, int lien_n,
    int lien_tot, int stat_time, hts_stat_struct * stats) {
  void *const arg = (void *) CALLBACKARG_USERDEF(carg);
  jni_context_t *const t = (jni_context_t*) arg;
#define STATE_MAX 256
  hts_state_t state[STATE_MAX];

#define COPY_(VALUE, FIELD) do { \
  (*t->env)->SetLongField(t->env, t->thiz, field_ ##FIELD, \
      (long) VALUE); \
} while(0)
#define COPY(MEMBER, FIELD) COPY_(stats->MEMBER, FIELD)

  /* Copy stats */
  COPY(HTS_TOTAL_RECV, bytesReceived);
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
    const long rate = (long) (stats->HTS_TOTAL_RECV / stat_time);
    COPY_(rate, totalTransferRate);
  }

  COPY_(hts_is_testing(opt), state);
  COPY_(hts_is_parsing(opt, -1), completion);

  /* Collect individual stats */
  if (back_index >= 0) {
    const size_t index_max = STATE_MAX;
    size_t k, index;
    /* current links first */
    for(k = 0, index = 0 ; k < 2 ; k++) {
      /* priority (receive first, then requests, then ready slots) */
      size_t j;
      for(j = 0; j < 3 && index < index_max ; j++) {
        size_t _i;
        /* when k=0, just take first item (possibly being parsed) */
        for(_i = k; _i < ( k == 0 ? 1 : back_max ) && index < index_max; _i++) {        // no lien
          const size_t i = (back_index + _i) % back_max;
          /* active link */
          if (back[i].status >= 0) {
            /* Next one */
            state[index].index = i;
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
              index++;
            }
          }
        }
      }
    }
  }


}

void Java_com_httrack_android_jni_HTTrackLib_stop(JNIEnv* env, jobject object,
    jboolean force) {
  MUTEX_LOCK(global_lock);
  if (global_opt != NULL) {
    hts_request_stop(global_opt, force);
  }
  MUTEX_UNLOCK(global_lock);
}

jint Java_com_httrack_android_jni_HTTrackLib_main(JNIEnv* env, jobject object,
    jobjectArray stringArray) {
  const int argc =
      object != NULL ? (*env)->GetArrayLength(env, stringArray) : 0;
  char **argv = (char**) malloc((argc + 1) * sizeof(char*));
  if (argv != NULL) {
    int i;
    httrackp * opt = NULL;
    int code;
    struct jni_context_t t;
    t.env = env;
    t.thiz = object;

    /* Create options and reference it */
    MUTEX_LOCK(global_lock);
    if (global_opt == NULL) {
      /* Create array */
      for (i = 0; i < argc; i++) {
        jstring str = (jstring)(*env)->GetObjectArrayElement(env, stringArray,
            i);
        const char * const utf_string = (*env)->GetStringUTFChars(env, str, 0);
        argv[i] = strdup(utf_string != NULL ? utf_string : "");
        (*env)->ReleaseStringUTFChars(env, str, utf_string);
      }
      argv[i] = NULL;

      /* Create opt tab */
      opt = hts_create_opt();
      global_opt = opt;
      CHAIN_FUNCTION(opt, loop, htsshow_loop, &t);
    }
    MUTEX_UNLOCK(global_lock);

    if (opt != NULL) {
      /* Rock'in! */
      code = hts_main2(argc, argv, opt);

      /* Raise error if suitable */
      if (code == -1) {
        const char *message = hts_errmsg(opt);
        if (message != NULL && *message != '\0') {
          throwIOException(env, message);
        }
      }

      /* Unreference global option tab */
      MUTEX_LOCK(global_lock);
      hts_free_opt(opt);
      global_opt = NULL;
      MUTEX_UNLOCK(global_lock);

      /* Cleanup */
      for (i = 0; i < argc; i++) {
        free(argv[i]);
      }
      free(argv);
    } else {
      throwRuntimeException(env, "not enough memory");
    }

    /* Return exit code. */
    return code;
  } else {
    throwRuntimeException(env, "not enough memory");
    return -1;
  }
}
