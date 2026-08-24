/* PATH shim the sanitizer legs run the engine through: a copy of PROGRAM's
   stderr lands in $HTTRACK_STDERR_CAPTURE_DIR for tools/ci-sanitizer-report.sh,
   where gcc's UBSan writes findings that most tests discard.

   Usage: stderrwrap PROGRAM [ARG...]. Without a capture directory this is a
   bare exec. With one, stderr has to be fanned out, and the caller reads the
   file it redirected stderr into the moment we exit -- so whatever writes that
   file has to be reaped before we are, which no exec'd process can do (#1374).
   The capture path therefore keeps PROGRAM as a child and hands back what a
   parent can: exit status, death by signal, the pid-directed signals it is
   sent, stdin and our process group. A shell cannot be that parent, since
   POSIX leaves SIGINT and SIGQUIT untrappable in a backgrounded shell, and a
   backgrounded shell is how every crawl test runs the engine. */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/prctl.h>
#endif

/* Pid-directed signals the suite sends the engine, plus SIGPIPE, which we take
   for a caller whose stderr went away. SIGKILL cannot be caught at all, which
   is what PR_SET_PDEATHSIG covers where there is one. */
static const int handled[] = {SIGHUP,  SIGINT,  SIGQUIT, SIGABRT, SIGALRM,
                              SIGTERM, SIGUSR1, SIGUSR2, SIGCHLD, SIGPIPE};
#define NHANDLED ((int) (sizeof handled / sizeof handled[0]))

static struct sigaction inherited[NHANDLED];
static volatile sig_atomic_t child = 0;
/* Self-pipe: the loop below has to learn of the child's death at once, an
   unreaped child being a pid the suite still reads as a running server. */
static int wakefd[2] = {-1, -1};

/* Only a kill(2) from elsewhere: what the kernel raises here -- a tty ^C, our
   own child's SIGCHLD -- has reached the child already, or is not for it. That
   is the si_code <= 0 convention, not SI_USER: Darwin leaves si_code 0 for
   kill(2) while defining SI_USER as 0x10001, so naming the constant forwarded
   nothing there and the suite hung on the first pid-directed signal. */
static void forward(int sig, siginfo_t *info, void *ctx) {
  const int saved = errno;

  (void) ctx;
  if (child > 0 && info != NULL && sig != SIGCHLD && info->si_code <= 0) {
    kill((pid_t) child, sig);
  }
  if (wakefd[1] >= 0) {
    const ssize_t ignored = write(wakefd[1], "!", 1);

    (void) ignored;
  }
  errno = saved;
}

/* Catching SIGINT and SIGQUIT here is the point: unlike a shell, we may take
   them over from the SIG_IGN a backgrounded caller left us. */
static void catch_signals(void) {
  struct sigaction sa;
  int i;

  memset(&sa, 0, sizeof sa);
  sa.sa_sigaction = forward;
  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sigfillset(&sa.sa_mask);
  for (i = 0; i < NHANDLED; i++) {
    if (handled[i] == SIGPIPE) {
      struct sigaction ign;
      memset(&ign, 0, sizeof ign);
      ign.sa_handler = SIG_IGN;
      sigaction(SIGPIPE, &ign, &inherited[i]);
    } else {
      sigaction(handled[i], &sa, &inherited[i]);
    }
  }
}

/* PROGRAM must see the dispositions the caller gave us, not ours. */
static void restore_signals(void) {
  int i;

  for (i = 0; i < NHANDLED; i++) {
    sigaction(handled[i], &inherited[i], NULL);
  }
}

static void block_signals(int how) {
  sigset_t set;
  int i;

  sigemptyset(&set);
  for (i = 0; i < NHANDLED; i++) {
    sigaddset(&set, handled[i]);
  }
  sigprocmask(how, &set, NULL);
}

/* Always _exit: an interposer a test preloaded lands on us too, and 113's
   rewrites its report file from every process whose destructors run. */
static void run_bare(char **argv) {
  execvp(argv[0], argv);
  fprintf(stderr, "stderrwrap: %s: %s\n", argv[0], strerror(errno));
  fflush(stderr);
  _exit(127);
}

/* <dir>/<tag>.<pid>.log: the name ci-sanitizer-report.sh scans, carrying the
   test the report belongs to. Anything but a filename is not a tag. */
static int open_log(const char *dir) {
  const char *tag = getenv("HTTRACK_STDERR_CAPTURE_TAG");
  char path[4096];
  int i;

  if (tag == NULL || tag[0] == '\0') {
    tag = "engine";
  }
  for (i = 0; tag[i] != '\0'; i++) {
    if (!isalnum((unsigned char) tag[i]) && strchr("._-", tag[i]) == NULL) {
      tag = "engine";
      break;
    }
  }
  if (snprintf(path, sizeof path, "%s/%s.%ld.log", dir, tag, (long) getpid()) >=
      (int) sizeof path) {
    return -1;
  }
  return open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);
}

/* False once the descriptor is unusable, so a caller that closed its stderr
   stops the copy without stopping the drain. */
static int write_all(int fd, const char *buf, size_t len) {
  while (len > 0) {
    const ssize_t n = write(fd, buf, len);

    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return 0;
    }
    buf += n;
    len -= (size_t) n;
  }
  return 1;
}

int main(int argc, char **argv) {
  const char *dir;
  char buf[8192];
  int pipefd[2];
  int logfd, status = 0, caller_ok = 1, reaped = 0;
  pid_t pid;
#ifdef __linux__
  const pid_t parent = getpid();
#endif

  if (argc < 2) {
    fprintf(stderr, "usage: %s PROGRAM [ARG...]\n", argv[0]);
    _exit(2);
  }
  dir = getenv("HTTRACK_STDERR_CAPTURE_DIR");
  if (dir == NULL || dir[0] == '\0') {
    run_bare(argv + 1);
  }
  /* Setting the capture up must never cost a test its run. */
  mkdir(dir, 0777);
  logfd = open_log(dir);
  if (logfd < 0) {
    run_bare(argv + 1);
  }
  if (pipe(pipefd) != 0) {
    close(logfd);
    run_bare(argv + 1);
  }
  if (pipe(wakefd) != 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    close(logfd);
    run_bare(argv + 1);
  }
  /* Never block the handler, whatever the loop is doing. */
  fcntl(wakefd[0], F_SETFL, O_NONBLOCK);
  fcntl(wakefd[1], F_SETFL, O_NONBLOCK);

  catch_signals();
  /* Or a signal landing before the pid is known would be lost. */
  block_signals(SIG_BLOCK);
  pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    close(wakefd[0]);
    close(wakefd[1]);
    close(logfd);
    block_signals(SIG_UNBLOCK);
    restore_signals();
    run_bare(argv + 1);
  }
  if (pid == 0) {
    restore_signals();
    block_signals(SIG_UNBLOCK);
    close(pipefd[0]);
    close(logfd);
    close(wakefd[0]);
    close(wakefd[1]);
    if (dup2(pipefd[1], STDERR_FILENO) < 0) {
      _exit(127);
    }
    close(pipefd[1]);
#ifdef __linux__
    /* A kill -9 reaches us and not PROGRAM; the re-check closes the window
       where the parent died before the request was in. BSD has no equivalent,
       so there a killed wrapper orphans PROGRAM; only the two Linux sanitizer
       jobs export the capture variable, so nothing reaches that today. */
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    if (getppid() != parent) {
      _exit(127);
    }
#endif
    run_bare(argv + 1);
  }
  child = (sig_atomic_t) pid;
  block_signals(SIG_UNBLOCK);
  close(pipefd[1]);

  for (;;) {
    struct pollfd pfd[2];
    char drain[64];
    ssize_t n;
    int ready;

    pfd[0].fd = pipefd[0];
    pfd[0].events = POLLIN;
    pfd[0].revents = 0;
    pfd[1].fd = wakefd[0];
    pfd[1].events = POLLIN;
    pfd[1].revents = 0;
    /* Zero once the child is reaped: everything it wrote is here already, and
       only something it left behind still holds the pipe open -- htsserver
       does, and waiting out that EOF would outlive the server 289 is timing. */
    ready = poll(pfd, 2, reaped ? 0 : -1);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if (pfd[0].revents != 0) {
      n = read(pipefd[0], buf, sizeof buf);
      if (n < 0) {
        if (errno == EINTR) {
          continue;
        }
        break;
      }
      if (n == 0) {
        break;
      }
      /* The caller's own copy first: it is the one a test reads back. */
      if (caller_ok) {
        caller_ok = write_all(STDERR_FILENO, buf, (size_t) n);
      }
      write_all(logfd, buf, (size_t) n);
      continue;
    }
    if (pfd[1].revents != 0) {
      while (read(wakefd[0], drain, sizeof drain) > 0) {
        /* a signal arrived; what it means is decided below */
      }
      if (!reaped && waitpid(pid, &status, WNOHANG) == pid) {
        reaped = 1;
      }
      continue;
    }
    if (reaped) {
      break;
    }
  }
  close(pipefd[0]);
  close(wakefd[0]);
  close(wakefd[1]);
  close(logfd);

  while (!reaped) {
    if (waitpid(pid, &status, 0) == pid) {
      break;
    }
    if (errno != EINTR) {
      _exit(127);
    }
  }
  if (WIFSIGNALED(status)) {
    const int sig = WTERMSIG(status);
    struct sigaction dfl;
    sigset_t one;

    /* Death by signal has to stay death by signal for the caller. */
    memset(&dfl, 0, sizeof dfl);
    dfl.sa_handler = SIG_DFL;
    sigaction(sig, &dfl, NULL);
    sigemptyset(&one);
    sigaddset(&one, sig);
    sigprocmask(SIG_UNBLOCK, &one, NULL);
    raise(sig);
    _exit(128 + sig); /* the caller had it ignored: the shell's own encoding */
  }
  _exit(WEXITSTATUS(status));
}
