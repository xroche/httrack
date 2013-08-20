/* CoffeeCatch, a tiny native signal handler/catcher for JNI code.
 * (especially for Android/Dalvik)
 *
 * Copyright (c) 2013, Xavier Roche (http://www.httrack.com/)
 * All rights reserved.
 * See the "License" section below for the licensing terms.
 *
 * Description:
 *
 * Allows to "gracefully" recover from a signal (segv, sibus...) as if it was
 * a Java exception. It will not gracefully recover from allocator/mutexes
 * corruption etc., however, but at least "most" gentle crashes (null pointer
 * dereferencing, integer division, stack overflow etc.) should be handled
 * without too much troubles.
 *
 * Currently the handler is not really thread-safe ; which means that only one
 * thread can use it at a time (but not necessarily from the same thread)
 *
 * Example:
 *
 * COFFEE_TRY() {
 *   call_some_native_function()
 * } COFFEE_CATCH() {
 *   const char*const message = native_code_crash_handler_get_message();
 *   jclass cls = (*env)->FindClass(env, "java/lang/RuntimeException");
 *   (*env)->ThrowNew(env, cls, strdup(message));
 * } COFFEE_END();
 *
 * Implementation notes:
 *
 * Currently the library is installing both alternate stack and signal
 * handlers for known signals (SIGABRT, SIGILL, SIGTRAP, SIGBUS, SIGFPE,
 * SIGSEGV, SIGSTKFLT), and is using sigsetjmp()/siglongjmp() to return to
 * "userland" (compared to signal handler context). As a security, an alarm
 * is started as soon as a fatal signal is detected (ie. not something the
 * JVM will handle) to kill the process after a grace period. Be sure your
 * program will exit quickly after the error is caught, or call alarm(0)
 * to cancel the pending time-bomb.
 * The signal handlers had to be written with caution, because the virtual
 * machine might be using signals (including SEGV) to handle JIT compiler,
 * and some clever optimizations (such as NullPointerException handling)
 *
 * License:
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

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <setjmp.h>
#define UNUSED __attribute__ ((unused))
#ifdef USE_UNWIND
#include <unwind.h>
#endif

#if 01
#define DEBUG(A) do { A; } while(0)
#define FD_ERRNO 2
static void print(const char *const s) {
  size_t count;
  for(count = 0; s[count] != '\0'; count++) ;
  /* write() is async-signal-safe. */
  (void) write(FD_ERRNO, s, count);
}
#else
#define DEBUG(A)
#endif

/* Alternative stack size. */
#define SIG_STACK_BUFFER_SIZE 32768

#ifdef USE_UNWIND
/* Number of backtraces to get. */
#define BACKTRACE_FRAMES_MAX 32
#endif

/* Signals to be caught. */
#define SIG_CATCH_COUNT 7
static const int native_sig_catch[SIG_CATCH_COUNT + 1]
  = { SIGABRT, SIGILL, SIGTRAP, SIGBUS, SIGFPE, SIGSEGV, SIGSTKFLT, 0 };

/* Maximum value of a caught signal. */
#define SIG_NUMBER_MAX 32

/* Crash handler structure. */
typedef struct native_code_handler_struct {
  /* Restore point. */
  sigjmp_buf env;

  /* Restore point is defined. */
  int env_set;

  /* Backup of sigaction. */
  struct sigaction sa_old[SIG_NUMBER_MAX];
  struct sigaction sa;
  struct sigaction sa_pass;

  /* Alternate stack. */
  stack_t stack;
  stack_t stack_old;
  char *stack_buffer;
  size_t stack_buffer_size;

  /* Signal code and info. */
  int code;
  siginfo_t si;

  /* Uwind context. */
#ifdef USE_UNWIND
  uintptr_t frames[BACKTRACE_FRAMES_MAX];
  size_t frames_size;
  size_t frames_skip;
#endif
} native_code_handler_struct;

/* Global crash handler structure. */
static native_code_handler_struct native_code_s;

#ifdef USE_UNWIND

/* Unwind callback */
static UNUSED _Unwind_Reason_Code unwind(struct _Unwind_Context* context, void* arg) {
  native_code_handler_struct *const s = (native_code_handler_struct*) arg;

  const uintptr_t ip = _Unwind_GetIP(context);
  if (ip != 0x0) {
    if (s->frames_skip == 0) {
      s->frames[s->frames_size++] = ip;
    } else {
      s->frames_skip--;
    }
  }

  if (s->frames_size == BACKTRACE_FRAMES_MAX) {
    return _URC_END_OF_STACK;
  } else {
    return _URC_NO_REASON;
  }
}

#endif

/* Call the old handler. */
static UNUSED void call_old_signal_handler(const int code, siginfo_t *const si,
                                           void * const sc) {
  /* Call the "real" Java handler for JIT and internals. */
  if (code >= 0 && code < SIG_NUMBER_MAX) {
    if (native_code_s.sa_old[code].sa_sigaction != NULL) {
      native_code_s.sa_old[code].sa_sigaction(code, si, sc);
    } else if (native_code_s.sa_old[code].sa_handler != NULL) {
      native_code_s.sa_old[code].sa_handler(code);
    }
  }
}

/* Try to jump to userland. */
static UNUSED void try_jump_to_userland(const int code, siginfo_t *const si,
                                        void * const sc) {
  (void) si;
  (void) sc;

  /* Back to the future. */
  if (native_code_s.env_set) {
    DEBUG(print("calling siglongjmp()\n"));
    native_code_s.env_set = 0;
    siglongjmp(native_code_s.env, code);
  }
}

static UNUSED void start_alarm() {
  /* Ensure we do not deadlock. Default of ALRM is to die.
   * (signal() and alarm() are signal-safe) */
  (void) alarm(30);
}

/* Internal signal pass-through. Allows to peek the "real" crash before
 * calling the Java handler. Remember than Java needs many of the signals
 * (for the JIT, for test-free NullPointerException handling, etc.)
 * We record the siginfo_t context in this function each time it is being
 * called, to be able to know what error caused an issue.
 */
static UNUSED void signal_handler_pass(const int code, siginfo_t *const si,
                                       void *const sc) {
  DEBUG(print("caught signal\n"));

  /* Call the "real" Java handler for JIT and internals. */
  call_old_signal_handler(code, si, sc);

  /* Still here ?
   * FIXME TODO: This is the Dalvik behavior - but is it the SunJVM one ? */

  /* Ensure we do not deadlock. Default of ALRM is to die.
   * (signal() and alarm() are signal-safe) */
  signal(code, SIG_DFL);
  start_alarm();

  /* Take note of the signal. */
  native_code_s.code = code;
  native_code_s.si = *si;

  /* Back to the future. */
  try_jump_to_userland(code, si, sc);

  /* Nope. (abort() is signal-safe) */
  DEBUG(print("calling abort()\n"));
  abort();
}

/* Internal crash handler for abort(). Java calls abort() if its signal handler
 * could not resolve the signal ; thus calling us through this handler. */
static UNUSED void signal_handler(const int code, siginfo_t *const si,
                                  void *const sc) {
  /* Unused */
  (void) sc;

  DEBUG(print("caught abort\n"));

  /* Ensure we do not deadlock. Default of ALRM is to die.
   * (signal() and alarm() are signal-safe) */
  signal(code, SIG_DFL);
  start_alarm();

  /* Take note (real "abort()") */
  native_code_s.code = code;
  native_code_s.si = *si;

#ifdef USE_UNWIND
  /* Frame buffer initial position. */
  native_code_s.frames_size = 0;

  /* Skip us and the caller. */
  native_code_s.frames_skip = 2;

  /* Unwind frames (equivalent to backtrace()) */
  _Unwind_Backtrace(unwind, &native_code_s);
#endif

  /* Back to the future. */
  try_jump_to_userland(code, si, sc);

  /* No such restore point, call old signal handler then. */
  DEBUG(print("calling old signal handler\n"));
  call_old_signal_handler(code, si, sc);

  /* Nope. (abort() is signal-safe) */
  DEBUG(print("calling abort()\n"));
  abort();
}

/**
 * Setup a crash handler for the current thread.
 **/
static UNUSED int native_code_crash_handler_setup() {
  size_t i;

  DEBUG(print("installing handlers\n"));

  /* Cleanup structure */
  memset(&native_code_s, 0, sizeof(native_code_s));
  native_code_s.stack_buffer_size = SIG_STACK_BUFFER_SIZE;
  native_code_s.stack_buffer = malloc(native_code_s.stack_buffer_size);
  if (native_code_s.stack_buffer == NULL) {
    return -1;
  }
  
  /* Setup handler structure. */
  sigemptyset(&native_code_s.sa.sa_mask);
  sigemptyset(&native_code_s.sa_pass.sa_mask);
  native_code_s.sa.sa_sigaction = signal_handler;
  native_code_s.sa_pass.sa_sigaction = signal_handler_pass;
  native_code_s.sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  native_code_s.sa_pass.sa_flags = SA_SIGINFO | SA_ONSTACK;

  /* Setup alternative stack. */
  native_code_s.stack.ss_sp = native_code_s.stack_buffer;
  native_code_s.stack.ss_size = native_code_s.stack_buffer_size;
  native_code_s.stack.ss_flags = 0;

  /* Install alternative stack. This is thread-safe
   * (but for now with a static buffer) */
  if (sigaltstack(&native_code_s.stack, &native_code_s.stack_old) != 0) {
    return -1;
  }
  
  /* Setup signal handlers for SIGABRT (Java calls abort()) and others.
   * This is obviously making this function not thread-safe as handlers are
   * process-wide. */
  for(i = 0; native_sig_catch[i] != 0; i++) {
    const int sig = native_sig_catch[i];
    const struct sigaction *const action =
      sig == SIGABRT ? &native_code_s.sa : &native_code_s.sa_pass;
    assert(sig < SIG_NUMBER_MAX);
    if (sigaction(sig, action, &native_code_s.sa_old[sig]) != 0) {
      return -1;
    }
  }
  
  DEBUG(print("installed handlers\n"));

  /* So far so good */
  return 0;
}

/**
 * Remove the crash handler previously setup by the
 * native_code_crash_handler_setup() function.
 **/
static UNUSED int native_code_crash_handler_cleanup() {
  size_t i;

  DEBUG(print("removing handlers\n"));

  /* Restore signal handler. */
  for(i = 0; native_sig_catch[i] != 0; i++) {
    const int sig = native_sig_catch[i];
    assert(sig < SIG_NUMBER_MAX);
    if (sigaction(sig, &native_code_s.sa_old[sig], NULL) != 0) {
      return -1;
    }
  }

  /* Restore previous alternative stack. */
  if (sigaltstack(&native_code_s.stack_old, NULL) != 0) {
    return -1;
  }

  /* Free alternative stack */
  if (native_code_s.stack_buffer != NULL) {
    free(native_code_s.stack_buffer);
    native_code_s.stack_buffer = NULL;
    native_code_s.stack_buffer_size = 0;
  }
  
  DEBUG(print("removed handlers\n"));

  /* So far so good */
  return 0;
}

/**
 * Get the signal associated with the crash.
 */
static UNUSED int native_code_crash_handler_get_signal() {
  return native_code_s.code;
}

/**
 * Get the native context associated with the crash.
 */
static UNUSED siginfo_t* native_code_crash_handler_get_info() {
  return &native_code_s.si;
}

/* Signal descriptions.
   See <http://pubs.opengroup.org/onlinepubs/009696699/basedefs/signal.h.html>
*/
static UNUSED const char* native_code_crash_handler_desc_sig(int sig,
                                                             int code) {
  switch(sig) {
  case SIGILL:
    switch(code) {
    case ILL_ILLOPC:
      return "Illegal opcode";
    case ILL_ILLOPN:
      return "Illegal operand";
    case ILL_ILLADR:
      return "Illegal addressing mode";
    case ILL_ILLTRP:
      return "Illegal trap";
    case ILL_PRVOPC:
      return "Privileged opcode";
    case ILL_PRVREG:
      return "Privileged register";
    case ILL_COPROC:
      return "Coprocessor error";
    case ILL_BADSTK:
      return "Internal stack error";
    default:
      return "Illegal operation";
    }
    break;
  case SIGFPE:
    switch(code) {
    case FPE_INTDIV:
      return "Integer divide by zero";
    case FPE_INTOVF:
      return "Integer overflow";
    case FPE_FLTDIV:
      return "Floating-point divide by zero";
    case FPE_FLTOVF:
      return "Floating-point overflow";
    case FPE_FLTUND:
      return "Floating-point underflow";
    case FPE_FLTRES:
      return "Floating-point inexact result";
    case FPE_FLTINV:
      return "Invalid floating-point operation";
    case FPE_FLTSUB:
      return "Subscript out of range";
    default:
      return "Floating-point";
    }
    break;
  case SIGSEGV:
    switch(code) {
    case SEGV_MAPERR:
      return "Address not mapped to object";
    case SEGV_ACCERR:
      return "Invalid permissions for mapped object";
    default:
      return "Segmentation violation";
    }
    break;
  case SIGBUS:
    switch(code) {
    case BUS_ADRALN:
      return "Invalid address alignment";
    case BUS_ADRERR:
      return "Nonexistent physical address";
    case BUS_OBJERR:
      return "Object-specific hardware error";
    default:
      return "Bus error";
    }
    break;
  case SIGTRAP:
    switch(code) {
    case TRAP_BRKPT:
      return "Process breakpoint";
    case TRAP_TRACE:
      return "Process trace trap";
    default:
      return "Trap";
    }
    break;
  case SIGCHLD:
    switch(code) {
    case CLD_EXITED:
      return "Child has exited";
    case CLD_KILLED:
      return "Child has terminated abnormally and did not create a core file";
    case CLD_DUMPED:
      return "Child has terminated abnormally and created a core file";
    case CLD_TRAPPED:
      return "Traced child has trapped";
    case CLD_STOPPED:
      return "Child has stopped";
    case CLD_CONTINUED:
      return "Stopped child has continued";
    default:
      return "Child";
    }
    break;
  case SIGPOLL:
    switch(code) {
    case POLL_IN:
      return "Data input available";
    case POLL_OUT:
      return "Output buffers available";
    case POLL_MSG:
      return "Input message available";
    case POLL_ERR:
      return "I/O error";
    case POLL_PRI:
      return "High priority input available";
    case POLL_HUP:
      return "Device disconnected";
    default:
      return "Pool";
    }
    break;
  case SIGABRT:
    return "Process abort signal";
  case SIGALRM:
    return "Alarm clock";
  case SIGCONT:
    return "Continue executing, if stopped";
  case SIGHUP:
    return "Hangup";
  case SIGINT:
    return "Terminal interrupt signal";
  case SIGKILL:
    return "Kill";
  case SIGPIPE:
    return "Write on a pipe with no one to read it";
  case SIGQUIT:
    return "Terminal quit signal";
  case SIGSTOP:
    return "Stop executing";
  case SIGTERM:
    return "Termination signal";
  case SIGTSTP:
    return "Terminal stop signal";
  case SIGTTIN:
    return "Background process attempting read";
  case SIGTTOU:
    return "Background process attempting write";
  case SIGUSR1:
    return "User-defined signal 1";
  case SIGUSR2:
    return "User-defined signal 2";
  case SIGPROF:
    return "Profiling timer expired";
  case SIGSYS:
    return "Bad system call";
  case SIGVTALRM:
    return "Virtual timer expired";
  case SIGURG:
    return "High bandwidth data is available at a socket";
  case SIGXCPU:
    return "CPU time limit exceeded";
  case SIGXFSZ:
    return "File size limit exceeded";
  default:
    switch(code) {
    case SI_USER:
      return "Signal sent by kill()";
    case SI_QUEUE:
      return "Signal sent by the sigqueue()";
    case SI_TIMER:
      return "Signal generated by expiration of a timer set by timer_settime()";
    case SI_ASYNCIO:
      return "Signal generated by completion of an asynchronous I/O request";
    case SI_MESGQ:
      return
        "Signal generated by arrival of a message on an empty message queue";
    default:
      return "Unknown signal";
    }
    break;
  }
}

/**
 * Get the full error message associated with the crash.
 */
static UNUSED const char* native_code_crash_handler_get_message() {
  char *const buffer = native_code_s.stack_buffer;
  const size_t buffer_len = native_code_s.stack_buffer_size;
  size_t buffer_offs = 0;

  const char*const posix_desc =
    native_code_crash_handler_desc_sig(native_code_s.si.si_signo,
                                       native_code_s.si.si_code);

  /* Signal */
  snprintf(&buffer[buffer_offs], buffer_len - buffer_offs, "signal %d",
           native_code_s.si.si_signo);
  buffer_offs += strlen(&buffer[buffer_offs]);

  /* Description */
  snprintf(&buffer[buffer_offs], buffer_len - buffer_offs, " (%s)", posix_desc);
  buffer_offs += strlen(&buffer[buffer_offs]);

  /* Address of faulting instruction */
  if (native_code_s.si.si_signo == SIGILL
      || native_code_s.si.si_signo == SIGSEGV) {
    snprintf(&buffer[buffer_offs], buffer_len - buffer_offs,
             " at address %p", native_code_s.si.si_addr);
    buffer_offs += strlen(&buffer[buffer_offs]);
  }

  /* [POSIX] If non-zero, an errno value associated with  this signal,
     as defined in <errno.h>. */
  if (native_code_s.si.si_errno != 0) {
    snprintf(&buffer[buffer_offs], buffer_len - buffer_offs, ": ");
    buffer_offs += strlen(&buffer[buffer_offs]);
    if (strerror_r(native_code_s.si.si_errno, &buffer[buffer_offs],
                   buffer_len - buffer_offs) == 0) {
      snprintf(&buffer[buffer_offs], buffer_len - buffer_offs, "unknown error");
      buffer_offs += strlen(&buffer[buffer_offs]);
    }
  }

  /* Sending process ID. */
  if (native_code_s.si.si_pid != 0) {
    snprintf(&buffer[buffer_offs], buffer_len - buffer_offs,
             " (sent by pid %d)", (int) native_code_s.si.si_pid);
    buffer_offs += strlen(&buffer[buffer_offs]);
  }

  /* Return string. */
  buffer[buffer_offs] = '\0';
  return native_code_s.stack_buffer;
}

/* Pseudo-TRY/CATCH definitions. */

#define COFFEE_TRY() do {                     \
  native_code_crash_handler_setup();          \
  native_code_s.env_set = 1;                  \
  if (sigsetjmp(native_code_s.env, 0) == 0)
#define COFFEE_CATCH() else
#define COFFEE_END()                          \
  native_code_crash_handler_cleanup();        \
} while(0)

#undef UNUSED
