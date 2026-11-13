/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998 Xavier Roche and other contributors

SPDX-License-Identifier: GPL-3.0-or-later

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

Ethical use: we kindly ask that you NOT use this software to harvest email
addresses or to collect any other private information about people. Doing so
would dishonor our work and waste the many hours we have spent on it.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: -V user command runner                                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htscore.h"

/* after htscore.h, which is what pulls in config.h and so HAVE_PATHS_H */
#ifndef _WIN32
#include <sys/wait.h>
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
/* bionic has no /bin/sh, so take the system's own answer where it gives one. */
#ifndef _PATH_BSHELL
#define _PATH_BSHELL "/bin/sh"
#endif
#endif

/* Shell quoting in force at the point of the template where "$0" appears. */
typedef enum {
  SHELL_CTX_PLAIN,  /* not inside quotes */
  SHELL_CTX_SINGLE, /* inside '...' */
  SHELL_CTX_DOUBLE, /* inside "..." */
  SHELL_CTX_OPAQUE  /* a region whose text the shell reads again, so quotes are
                       ordinary characters in it */
} shell_ctx_t;

/*
 * A command substitution -- $( ) or `` -- starts a fresh shell, so the quoting
 * outside it says nothing about the quoting inside: in
 *   printf "[%s]" "$(printf %s '$0')"
 * the $0 sits inside '...', not the "..." that encloses the substitution. The
 * scanner keeps a stack: opening one saves the quoting it interrupts and
 * restarts at PLAIN, closing it restores what was saved.
 */
typedef struct {
  shell_ctx_t ctx;   /* quoting the region interrupted */
  char closer;       /* ')', ']', '}', or '`' for a backquote */
  unsigned int open; /* openers not yet matched, including this one */
} shell_frame_t;

/* A template nested deeper than this is refused rather than mis-tracked. */
#define SHELL_CTX_MAX_DEPTH 16

/* The opener that matches a region's closer. */
static char usercommand_opener(char closer) {
  switch (closer) {
  case ')':
    return '(';
  case ']':
    return '[';
  case '}':
    return '{';
  }
  return '\0';
}

/* Open a region ending at "closer"; HTS_FALSE when the stack is full. */
static hts_boolean usercommand_push(shell_frame_t *stack, size_t *depth,
                                    shell_ctx_t ctx, char closer,
                                    unsigned int open) {
  if (*depth == SHELL_CTX_MAX_DEPTH)
    return HTS_FALSE;
  stack[*depth].ctx = ctx;
  stack[*depth].closer = closer;
  stack[*depth].open = open;
  (*depth)++;
  return HTS_TRUE;
}

/*
 * Append "len" bytes of "src" at "*pos" in "dest" (capacity "size", kept
 * NUL-terminated). Returns HTS_FALSE and parks *pos at "size" when the result
 * would not fit, so the caller drops the command instead of aborting: both the
 * template and the save name expanded into it can be arbitrarily long.
 */
static hts_boolean usercommand_append(char *dest, size_t size, size_t *pos,
                                      const char *src, size_t len) {
  if (*pos >= size || len >= size - *pos) {
    *pos = size;
    return HTS_FALSE;
  }
  memcpy(dest + *pos, src, len);
  *pos += len;
  dest[*pos] = '\0';
  return HTS_TRUE;
}

static hts_boolean usercommand_append_char(char *dest, size_t size, size_t *pos,
                                           char c) {
  return usercommand_append(dest, size, pos, &c, 1);
}

#ifndef _WIN32
/*
 * The spelling of the shell's first positional parameter that reads as one
 * literal word in "ctx", or NULL where no spelling does. The save name comes
 * from the crawled URL and htsname.c leaves ; ` $ & ' ( ) and spaces in it,
 * so the name is handed to the shell as $1 and never written into the command.
 */
static const char *usercommand_param(shell_ctx_t ctx) {
  switch (ctx) {
  case SHELL_CTX_PLAIN:
    return "\"${1}\"";
  case SHELL_CTX_DOUBLE:
    /* already between quotes; braces so a digit after $0 is not read as $11 */
    return "${1}";
  case SHELL_CTX_SINGLE:
    /* a parameter is literal inside '...', so close the quote and reopen it */
    return "'\"${1}\"'";
  case SHELL_CTX_OPAQUE:
    /* bash evaluates an array subscript in arithmetic, so $1 holding
       x[$(cmd)] runs cmd even though it arrived as a parameter */
    return NULL;
  }
  return NULL;
}
#else
/*
 * Append "value" quoted for the context it lands in, so cmd.exe reads it as
 * one literal word. Windows keeps this route because "cmd /c" has no
 * positional parameters to hand the name to.
 */
static hts_boolean usercommand_append_quoted(char *dest, size_t size,
                                             size_t *pos, const char *value,
                                             shell_ctx_t ctx) {
  size_t i;

  /* cmd.exe has no single-quote syntax and no escape inside a quoted string,
     but & | < > ( ) ^ are inert between double quotes. A " would end that
     protection and cannot be escaped portably, so it is replaced. */
  if (ctx != SHELL_CTX_DOUBLE) {
    if (!usercommand_append_char(dest, size, pos, '\"'))
      return HTS_FALSE;
  }
  for (i = 0; value[i] != '\0'; i++) {
    if (!usercommand_append_char(dest, size, pos,
                                 value[i] == '\"' ? '_' : value[i]))
      return HTS_FALSE;
  }
  if (ctx != SHELL_CTX_DOUBLE) {
    if (!usercommand_append_char(dest, size, pos, '\"'))
      return HTS_FALSE;
  }
  return HTS_TRUE;
}
#endif

/* Name the cause of a refusal, so the user is told what to change. */
static int usercommand_refuse(const char **why, const char *reason) {
  if (why != NULL)
    *why = reason;
  return USERCOMMAND_EXPAND_REFUSED;
}

/*
 * Rewrite the -V template "cmd" into "dest". On POSIX each "$0" becomes a
 * reference to the shell's first positional parameter, and "file" is unused:
 * usercommand_exe() hands the save name to the shell as that parameter, so it
 * never appears in the command text. On Windows "cmd /c" has no positional
 * parameters, so "$0" is still replaced by "file" quoted for cmd.exe.
 * Returns USERCOMMAND_EXPAND_OK, _TOOLONG if the result does not fit, or
 * _REFUSED if "$0" lands where neither holds, with "*why" then naming the
 * cause. Except on OK, "dest" holds a prefix that must not be run.
 */
int usercommand_expand(char *dest, size_t size, const char *cmd,
                       const char *file, const char **why) {
  static const char too_deep[] = "the template nests substitutions deeper than "
                                 "the scanner tracks";
  shell_frame_t stack[SHELL_CTX_MAX_DEPTH];
  size_t depth = 0;
  shell_ctx_t ctx = SHELL_CTX_PLAIN;
  hts_boolean escaped = HTS_FALSE;
  size_t pos = 0;
  size_t i;

#ifndef _WIN32
  (void) file; /* the name travels as a parameter, not in the command text */
#endif
  if (size == 0)
    return USERCOMMAND_EXPAND_TOOLONG;
  dest[0] = '\0';
  for (i = 0; cmd[i] != '\0'; i++) {
    const char c = cmd[i];

    /* Substituted wherever it appears, as it always has been -- except where
       the quoting that makes it safe would not survive. */
    if ((c == '$') && (cmd[i + 1] == '0')) {
      /* The allowlist. A parameter is read as data in a plain word, in "..."
         and in '...', and nowhere else that can be enumerated: bash re-expands
         text after quote removal in ${v:off:len} and in unset, and evaluates
         arithmetic in $(( )), $[ ], (( )) and a [ ] subscript. So every
         region the scanner opens refuses, rather than the scanner deciding
         which of them is dangerous. */
      if (depth != 0)
        return usercommand_refuse(why, "$0 must be a plain word, or inside "
                                       "\"...\" or '...'; it is inside a "
                                       "substitution, an expansion, "
                                       "arithmetic, a subscript or [[ ]], "
                                       "where the shell reads it again");
#ifndef _WIN32
      /* A backslash here is a shell escape, and it would eat the first
         character of the parameter reference. httrack substitutes $0 escaped
         or not, so the backslash was meant for an outer shell that never
         stripped it: drop it. That keeps the documented -V "rm \$0" working
         whether or not a shell got to it first. On Windows a backslash is a
         path separator, and cmd.exe does not treat it as an escape. */
      if (escaped) {
        /* the flag is set only by a backslash this loop just emitted */
        assertf(pos > 0 && dest[pos - 1] == '\\');
        dest[--pos] = '\0';
      }
      {
        const char *const param = usercommand_param(ctx);

        if (param == NULL ||
            !usercommand_append(dest, size, &pos, param, strlen(param)))
          return USERCOMMAND_EXPAND_TOOLONG;
      }
#else
      if (!usercommand_append_quoted(dest, size, &pos, file, ctx))
        return USERCOMMAND_EXPAND_TOOLONG;
#endif
      /* the backslash applied to this $0 and is spent; leaving it set would
         suppress the next character's quote transition, and on the next $0
         would delete the closing quote just emitted */
      escaped = HTS_FALSE;
      i++;
      continue;
    }

    /* A backslash only suppresses the quote transition of the next character;
       it is still copied through verbatim. */
    if (!escaped && ctx != SHELL_CTX_SINGLE && c == '\\') {
      escaped = HTS_TRUE;
    } else {
      if (!escaped && ctx != SHELL_CTX_SINGLE && c == '`') {
        /* A backquote closes the substitution it opened, or opens one. */
        if (depth != 0 && stack[depth - 1].closer == '`' &&
            ctx == SHELL_CTX_PLAIN) {
          ctx = stack[--depth].ctx;
        } else {
          if (!usercommand_push(stack, &depth, ctx, '`', 0))
            return usercommand_refuse(why, too_deep);
          ctx = SHELL_CTX_PLAIN;
        }
      } else if (!escaped && ctx != SHELL_CTX_SINGLE && c == '$' &&
                 (cmd[i + 1] == '(' || cmd[i + 1] == '[' ||
                  cmd[i + 1] == '{')) {
        /* $( ) and `...` start a fresh shell, so quoting is tracked inside
           them; $(( )), $[ ] and ${ } are read again as text by this one. */
        const char opened = cmd[i + 1];

        if (!usercommand_push(stack, &depth, ctx,
                              opened == '(' ? ')' : (opened == '[' ? ']' : '}'),
                              1))
          return usercommand_refuse(why, too_deep);
        ctx = (opened == '{' || (opened == '(' && cmd[i + 2] != '('))
                  ? SHELL_CTX_PLAIN
                  : SHELL_CTX_OPAQUE;
        /* copy the "$", then the bracket, which for arithmetic is the first of
           two and the second is counted below, so it takes both to close */
        if (!usercommand_append_char(dest, size, &pos, c))
          return USERCOMMAND_EXPAND_TOOLONG;
        i++;
        if (!usercommand_append_char(dest, size, &pos, cmd[i]))
          return USERCOMMAND_EXPAND_TOOLONG;
        continue;
      } else if (!escaped && c == '[') {
        /* An array subscript is evaluated as arithmetic, whether it is read
           (${a[$0]}) or written (a[$0]=1, a=([$0]=x), unset a[$0]). It is
           told from the [ test command by being stuck to a word: "[ -f $0 ]"
           starts one, "a[" continues one. "[[" starts no subscript either.
           Spotted inside '...' as well, because the '"${1}"' emitted there
           breaks the quote and the shell re-forms one word across it, so
           unset 'a[$0]' evaluates the name just as unset "a[$0]" does. */
        if (i != 0 && strchr(" \t\n;&|", cmd[i - 1]) == NULL) {
          if (!usercommand_push(stack, &depth, ctx, ']', 1))
            return usercommand_refuse(why, too_deep);
          ctx = SHELL_CTX_OPAQUE;
        } else if (cmd[i + 1] == '[') {
          /* [[ ]] is not a subscript, but bash re-reads its operands: -eq
             evaluates arithmetic and =~ takes a regex. Its own region. */
          if (!usercommand_push(stack, &depth, ctx, ']', 2))
            return usercommand_refuse(why, too_deep);
          if (!usercommand_append_char(dest, size, &pos, c))
            return USERCOMMAND_EXPAND_TOOLONG;
          i++;
          if (!usercommand_append_char(dest, size, &pos, cmd[i]))
            return USERCOMMAND_EXPAND_TOOLONG;
          continue;
        }
      } else if (!escaped && ctx == SHELL_CTX_PLAIN && c == '(' &&
                 cmd[i + 1] == '(') {
        /* A bare (( )) is an arithmetic command, which covers "((n=$0))", the
           arithmetic for loop, and the "$( ((...)) )" that the lookahead
           above reads as an ordinary subshell. */
        if (!usercommand_push(stack, &depth, ctx, ')', 2))
          return usercommand_refuse(why, too_deep);
        ctx = SHELL_CTX_OPAQUE;
        if (!usercommand_append_char(dest, size, &pos, c))
          return USERCOMMAND_EXPAND_TOOLONG;
        i++;
        if (!usercommand_append_char(dest, size, &pos, cmd[i]))
          return USERCOMMAND_EXPAND_TOOLONG;
        continue;
      } else if (!escaped && depth != 0 && stack[depth - 1].closer != '`' &&
                 (ctx == SHELL_CTX_PLAIN || ctx == SHELL_CTX_OPAQUE) &&
                 (c == stack[depth - 1].closer ||
                  c == usercommand_opener(stack[depth - 1].closer))) {
        if (c == stack[depth - 1].closer) {
          if (--stack[depth - 1].open == 0)
            ctx = stack[--depth].ctx;
        } else {
          stack[depth - 1].open++;
        }
      } else if (!escaped) {
        switch (ctx) {
        case SHELL_CTX_PLAIN:
          if (c == '\'')
            ctx = SHELL_CTX_SINGLE;
          else if (c == '\"')
            ctx = SHELL_CTX_DOUBLE;
          break;
        case SHELL_CTX_SINGLE:
          if (c == '\'')
            ctx = SHELL_CTX_PLAIN;
          break;
        case SHELL_CTX_DOUBLE:
          if (c == '\"')
            ctx = SHELL_CTX_PLAIN;
          break;
        case SHELL_CTX_OPAQUE:
          /* a quote here is an ordinary character, so it opens nothing */
          break;
        }
      }
      escaped = HTS_FALSE;
    }

    if (!usercommand_append_char(dest, size, &pos, c))
      return USERCOMMAND_EXPAND_TOOLONG;
  }

  return USERCOMMAND_EXPAND_OK;
}

#ifndef _WIN32
/* execv() does not write through its argv, but its prototype cannot say so. */
static char *usercommand_unconst(const char *s) {
  return (char *) (uintptr_t) s;
}

/*
 * Fill "argv" with the vector usercommand_exe() execs and return how many
 * entries precede its terminating NULL. "file" becomes the shell's $1, which
 * is what usercommand_expand() rewrote each "$0" of the template to.
 */
size_t usercommand_argv(char *argv[USERCOMMAND_ARGV_MAX], const char *command,
                        const char *file) {
  size_t n = 0;

  argv[n++] = usercommand_unconst("sh"); /* the shell's own argv[0] */
  argv[n++] = usercommand_unconst("-c");
  argv[n++] = usercommand_unconst(command);
  argv[n++] = usercommand_unconst("sh"); /* $0 within the command */
  argv[n++] = usercommand_unconst(file); /* $1 */
  argv[n] = NULL;
  return n;
}

/*
 * Run "command" with "sh -c", handing "file" to it as $1. Returns the shell's
 * exit status, or -1 when it could not be run. Not system(): that blocks
 * SIGINT and SIGQUIT in the calling thread and waits on any child, and
 * httrack runs this from its workers.
 */
static int usercommand_spawn(httrackp *opt, const char *command,
                             const char *file) {
  int status = 0;
  const pid_t pid = fork();

  if (pid == -1) {
    hts_log_print(opt, LOG_ERROR | LOG_ERRNO,
                  "User command not run, can not fork: %.64s", command);
    return -1;
  }
  if (pid == 0) {
    char *argv[USERCOMMAND_ARGV_MAX];

    (void) usercommand_argv(argv, command, file);
    execv(_PATH_BSHELL, argv);
    _exit(127);
  }
  while (waitpid(pid, &status, 0) == -1) {
    /* an embedder that set SIGCHLD to SIG_IGN reaped the child itself */
    if (errno != EINTR)
      return -1;
  }
  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  return WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1;
}
#endif

/* Returns the shell's exit status, or -1 when the command was not run. */
int usercommand_exe(httrackp *opt, const char *cmd, const char *file) {
  char BIGSTK temp[8192];
  const char *why = "$0 cannot be quoted safely here";

  /* Truncating a shell command is how a "rm '$0'" becomes a "rm". */
  switch (usercommand_expand(temp, sizeof(temp), cmd, file, &why)) {
  case USERCOMMAND_EXPAND_OK:
    break;
  case USERCOMMAND_EXPAND_TOOLONG:
    /* on the console too: a silently skipped -V looks like a working one */
    printf("User command not run, expansion exceeds %d bytes: %.64s\n",
           (int) sizeof(temp), cmd);
    hts_log_print(opt, LOG_ERROR,
                  "User command not run, expansion exceeds %d bytes: %.64s",
                  (int) sizeof(temp), cmd);
    return -1;
  default:
    printf("User command not run, %s: %.64s\n", why, cmd);
    hts_log_print(opt, LOG_ERROR, "User command not run, %s: %.64s", why, cmd);
    return -1;
  }

#ifdef _WIN32
  return system(temp);
#else
  return usercommand_spawn(opt, temp, file);
#endif
}
