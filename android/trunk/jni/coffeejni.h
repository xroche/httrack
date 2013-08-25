/* CoffeeCatch, a tiny native signal handler/catcher for JNI code.
 * (especially for Android/Dalvik)
 *
 * Copyright (c) 2013, Xavier Roche (http://www.httrack.com/)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef COFFEECATCH_JNI_H
#define COFFEECATCH_JNI_H

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Setup crash handler to enter in a protected section. If a recognized signal
 * is received in this section, an appropriate native Java Error will be
 * raised.
 *
 * You can not exit the protected section block CODE_TO_BE_EXECUTED, using 
 * statements such as "return", because the cleanup code would not be
 * executed.
 *
 * It is advised to enclose the complete CODE_TO_BE_EXECUTED block in a
 * dedicated function declared extern or __attribute__ ((noinline)).
 *
 * You must build all your libraries with `-funwind-tables', to get proper
 * unwinding information on all binaries. On Android, this can be achieved
 * by using this line in the Android.mk file in each library block:
 *   LOCAL_CFLAGS := -funwind-tables
 *
 * Example:
 *
 * void my_native_function(JNIEnv* env, jobject object, jint *retcode) {
 *   COFFEE_TRY_JNI(env, *retcode = call_dangerous_function(env, object));
 * }
 *
 * In addition, the following restrictions MUST be followed:
 * - the function must be declared extern, or with the special attribute
 *   __attribute__ ((noinline)).
 * - you must not use local variables before the COFFEE_TRY_JNI block,
 *   or define them as "volatile".
 *
COFFEE_TRY_JNI(JNIEnv* env, CODE_TO_BE_EXECUTED)
 */

/** Internal functions & definitions, not to be used directly. **/
extern void coffeecatch_throw_exception(JNIEnv* env);
#define COFFEE_TRY_JNI(ENV, CODE)       \
  do {                                  \
    COFFEE_TRY() {                      \
      CODE;                             \
    } COFFEE_CATCH() {                  \
      coffeecatch_throw_exception(ENV); \
    } COFFEE_END();                     \
  } while(0)
/** End of internal functions & definitions. **/

#ifdef __cplusplus
}
#endif

#endif
