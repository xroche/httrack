/*
* HTTrack Android JAVA Native Interface Stubs.
*/
#include <string.h>
#include <jni.h>
#include <assert.h>

#include "htsglobal.h"
#include "htsbase.h"
#include "htsopt.h"
#include "httrack-library.h"
#include "htsdefines.h"

/* FIXME -- This is dirty... we are supposed to keep the error message. */
static char *getSafeCopy(const char *message) {
  static char *buffer = NULL;
  if (buffer != NULL) {
    free(buffer);
  }
  buffer = strdup(message);
  return buffer;
}

static void throwException(JNIEnv* env, const char *exception, const char *message) {
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

#if 0
static int htsshow_loop(t_hts_callbackarg * carg, httrackp * opt,
		lien_back * back, int back_max, int back_index, int lien_n,
		int lien_tot, int stat_time, hts_stat_struct * stats) {
}
#endif

static httrackp * global_opt = NULL;

void Java_com_httrack_android_jni_HTTrackLib_stop(JNIEnv* env, jobject object, jboolean force) {
  if (global_opt != NULL) {
    hts_request_stop(global_opt, force);
  }
}

jint Java_com_httrack_android_jni_HTTrackLib_main(JNIEnv* env, jobject object, jobjectArray stringArray) {
  const int argc = object != NULL ? (*env)->GetArrayLength(env, stringArray) : 0;
  char **argv = (char**) malloc( ( argc + 1 ) * sizeof(char*));
  if (argv != NULL) {
    int i;
    httrackp * opt;
    int code;

    /* Create array */
    for (i = 0; i < argc; i++) {
      jstring str = (jstring) (*env)->GetObjectArrayElement(env, stringArray, i);
      const char *const utf_string = (*env)->GetStringUTFChars(env, str, 0);
      argv[i] = strdup(utf_string != NULL ? utf_string : "");
      (*env)->ReleaseStringUTFChars(env, str, utf_string);
    }
    argv[i] = NULL;

    /* Create options */
    opt = hts_create_opt();
    global_opt = opt;  /* FIXME mutex */
    /*CHAIN_FUNCTION(opt, loop, htsshow_loop, NULL);
     */

    /* Rock'in! */
    code = hts_main2(argc, argv, opt);
    global_opt = NULL;  /* FIXME mutex */

    /* Cleanup */
    for (i = 0; i < argc; i++) {
      free(argv[i]);
    }
    free(argv);

    // Raise error if suitable
    if (code == -1) {
      const char *message = hts_errmsg(opt);
      if (message != NULL && *message != '\0') {
        throwIOException(env, message);
      }
    }

    /* Cleanup. */
    hts_free_opt(opt);

    /* Return exit code. */
    return code;
  } else {
    throwRuntimeException(env, "not enough memory");
    return -1;
  }
}
