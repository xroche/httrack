AC_PREREQ([2.71])

AC_INIT([httrack], [3.50.1], [roche+packaging@httrack.com], [httrack], [https://www.httrack.com/])
AC_COPYRIGHT([
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2015 Xavier Roche and other contributors

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
])

AC_CONFIG_SRCDIR(src/httrack.c)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS(config.h)
# The public half of config.h: the switches the installed headers read, in a
# header we can install without handing a consumer our PACKAGE_* and HAVE_*.
AC_CONFIG_HEADERS([htsfeatures.h:src/htsfeatures.h.in])
AM_INIT_AUTOMAKE([subdir-objects])
# 3:20:0: revision-only bump. #1495 added an export (SOCaddr_inetntoa_port_) beside
# SOCaddr_inetntoa_, the same shape as 3:11:0; no installed struct moved and nothing
# went away.
# 3:19:0: revision-only bump for the 3.50.0 release. config.h left the installed
# header set for htsfeatures.h, which carries the same macro values. No installed
# struct changed layout and no export came or went.
# 3:18:0: revision-only bump. hts_print_num moved to htsbacktrace.c, which is not
# in the library and whose header is not installed; no export came or went.
# 3:17:0: revision-only bump. httrackp gained a tail field (links_unqueued) and
# BackStatusCode two enumerators at the end (STATUSCODE_IO_FATAL, STATUSCODE_IO_ERROR);
# no layout moved, no export came or went. This is the release that ships 3:16:0's
# t_cookie break, which 3.49.23 was tagged before.
# 3:16:0: revision-only DESPITE an ABI break. hts_addurl() deep-copies the array it
# is handed; #1365 then widened t_cookie.max_len (installed htsbauth.h) to size_t,
# moving data[] from 4 to 8 on LP64 (auth and sizeof unchanged, the old hole
# absorbed it). The libtool rules want current++ plus a libhttrack4 rename;
# declined, see 3:7:0.
# 3:15:0: revision-only bump. httrackp gained a tail field (wizard_filters) and two
# exports arrived (hts_footer_field_ok, hts_wizard_host_scope); HTS_LOCATION_SIZE only
# names lien_back.location_buffer's existing size, it does not change it.
# 3:14:0: revision-only bump. httrackp gained a tail field (host_alias) and #1202
# added an export (hts_host_alias_rule_ok); no layout moved, nothing went away.
# 3:13:0: revision-only bump. htsstrings.h's StringRoomTotal grew saturation in
# its macro body; no installed struct moved and no export came or went.
# 3:12:0: revision-only bump. httrackp gained a tail field (singlefile_state) and
# htsopt.h added volatile qualifiers; no layout moved, nothing went away.
# 3:11:0: revision-only bump. #1001 moved SOCaddr_inetntoa_ out of htsnet.h, adding
# an export where callers had an inline; nothing changed or went away.
# 3:10:0: revision-only bump; only the version macro moved, the engine is untouched.
# 3:9:0: revision-only bump. #991 and #1005 each added an export
# (hts_set_thread_hooks, escape_control_url); nothing changed or went away.
# 3:8:0: revision-only bump, no ABI change.
# 3:7:0: htsblk gained a tail field and lien_back embeds it by value, so
# lien_back.is_update and everything after it shift +8 (httrackp's own tail growth
# moves nothing). Soname stays .so.3: HTTrackQt is the only consumer of the installed
# headers, so a libhttrack4 rename isn't worth it.
# (3:0:0 was the htsblk mime-buffer widening, the ABI break that moved .so.2 -> .so.3.)
VERSION_INFO="3:20:0"
AM_MAINTAINER_MODE
AC_USE_SYSTEM_EXTENSIONS

# A real bash, for "make deb" and the test harness. Not searched into BASH: bash presets
# that to its own path, and macOS /bin/sh is a bash, so the macro never searches (#895).
# BASH_SHELL isn't preset the way BASH is, so AC_ARG_VAR needs no guard. Kept ahead of the
# compiler probes so a bad override dies before them.
AC_ARG_VAR([BASH_SHELL], [path to a real (non-POSIX-mode) bash])
# AC_PATH_PROGS drops a relative override and searches instead, which loses the user's intent.
# Nothing quotes $(BASH_SHELL) in the Makefiles, and quoting could not save it anyway: make
# splits on whitespace, expands '$' and treats '#' as a comment before any shell sees it.
case $BASH_SHELL in
*[[[:space:]]]* | *'#'* | *'$'* | *'`'* | *'\'* | *'"'* | *"'"* | *';'* | *'&'* | *'|'* | \
*'<'* | *'>'* | *'('* | *')'* | *'*'* | *'?'* | *'@<:@'* | *'@:>@'* | *'{'* | *'}'*)
  AC_MSG_ERROR([BASH_SHELL must not contain shell or make metacharacters, got: $BASH_SHELL]) ;;
'' | [[\\/]]* | ?:[[\\/]]*) ;;
*) AC_MSG_ERROR([BASH_SHELL must be an absolute path, got: $BASH_SHELL]) ;;
esac
hts_bash_override=$BASH_SHELL
AC_PATH_PROGS([BASH_SHELL], [bash], [/bin/bash])

# An absolute override is taken verbatim, so BASH_SHELL=/bin/sh would put #895 back and only
# surface at "make check" or "make deb" (#908). What we found ourselves is only a warning:
# a box with no bash still builds, it just cannot run those two.
AC_MSG_CHECKING([whether $BASH_SHELL is a bash outside POSIX mode])
hts_bash_why=
hts_bash_env=no
# AS_EXECUTABLE_P, not "test -x": the PATH search above already demands a regular file, and
# bash blocks forever reading a FIFO it failed to exec, so -x alone hangs configure (#922).
if ! AS_EXECUTABLE_P(["$BASH_SHELL"]); then
  hts_bash_why="not an executable regular file"
elif test -z "$("$BASH_SHELL" -c 'echo "${BASH_VERSINFO[[0]]}"' 2>/dev/null)"; then
  # Not BASH_VERSION: that is an ordinary variable, so any shell echoes back a spoofed one.
  hts_bash_why="not a bash: it reports no BASH_VERSINFO"
else
  # sh-mode bash reports a version too, so only SHELLOPTS tells the two apart.
  case $("$BASH_SHELL" -c 'echo ":$SHELLOPTS:"' 2>/dev/null) in
  *:posix:*)
    hts_bash_why="a bash in POSIX sh-mode"
    # POSIXLY_CORRECT and an exported SHELLOPTS do that to every bash on the box, so no path
    # can pass and blaming this one would send the user hunting for another. Reading them
    # here would not do: configure puts its own shell in posix mode, which sets both.
    case $(env -u POSIXLY_CORRECT -u SHELLOPTS "$BASH_SHELL" -c 'echo ":$SHELLOPTS:"' 2>/dev/null) in
    '' | *:posix:*) ;; # no "env -u", or posix whatever the environment: blame the path
    *) hts_bash_env=yes ;;
    esac
    ;;
  esac
fi
if test -z "$hts_bash_why"; then
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
  hts_bash_msg="POSIXLY_CORRECT or SHELLOPTS forces every bash into POSIX sh-mode, $BASH_SHELL included. Clear them for configure and for make, which hands them to make check and make deb: env -u POSIXLY_CORRECT -u SHELLOPTS ..."
  if test "$hts_bash_env" = yes; then
    if test -n "$hts_bash_override"; then
      AC_MSG_ERROR([$hts_bash_msg])
    fi
    AC_MSG_WARN([$hts_bash_msg])
  else
    if test -n "$hts_bash_override"; then
      AC_MSG_ERROR([BASH_SHELL=$BASH_SHELL is $hts_bash_why])
    fi
    AC_MSG_WARN([no usable bash found: $BASH_SHELL is $hts_bash_why. "make check" and "make deb" need one; pass BASH_SHELL=/path/to/bash])
  fi
fi

AC_PROG_CC
AM_PROG_CC_C_O
m4_warn([obsolete],
[The preprocessor macro `STDC_HEADERS' is obsolete.
  Except in unusual embedded environments, you can safely include all
  ISO C90 headers unconditionally.])dnl
# Autoupdate added the next two lines to ensure that your configure
# script's behavior did not change.  They are probably safe to remove.
AC_CHECK_INCLUDES_DEFAULT
AC_PROG_EGREP
# $(SED) substitutes $(datadir) into src/webhttrack
AC_PROG_SED

LT_INIT
AC_PROG_LN_S
LT_INIT

# Export LD_LIBRARY_PATH name or equivalent.
AC_SUBST(SHLIBPATH_VAR,$shlibpath_var)

# Export .libs or equivalent.
AC_SUBST(LT_CV_OBJDIR,$lt_cv_objdir)

# Export version info
AC_SUBST(VERSION_INFO)

### Default CFLAGS
# No -Wdeclaration-after-statement: nothing sets -std=, so this builds as gnu17.
DEFAULT_CFLAGS="-Wall -Wformat -Wformat-security \
-Wmultichar -Wwrite-strings -Wcast-qual -Wcast-align \
-Wstrict-prototypes -Wmissing-prototypes \
-Wmissing-declarations \
-Wpointer-arith -Wsequence-point -Wnested-externs \
-D_REENTRANT"
AC_SUBST(DEFAULT_CFLAGS)
DEFAULT_LDFLAGS=""
AC_SUBST(DEFAULT_LDFLAGS)

### Additional flags (if supported)
# -Werror on probes: exit-status-only checks let clang's warn-on-unknown-flag through.
AX_CHECK_COMPILE_FLAG([-Wparentheses], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wparentheses"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Winit-self], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Winit-self"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wunused-but-set-parameter], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wunused-but-set-parameter"], [], [-Werror])
# Implied by -Wall today; named explicitly so dead code stays a build break.
AX_CHECK_COMPILE_FLAG([-Wunused-function], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wunused-function"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wunused-variable], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wunused-variable"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wunused-but-set-variable], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wunused-but-set-variable"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Waddress], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Waddress"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wuninitialized], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wuninitialized"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wformat=2], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wformat=2"], [], [-Werror])
# -Wformat-nonliteral needs -Wformat in the probe or gcc rejects it as ignored.
AX_CHECK_COMPILE_FLAG([-Wformat-nonliteral], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wformat-nonliteral"], [], [-Werror -Wformat])
AX_CHECK_COMPILE_FLAG([-Wmissing-parameter-type], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wmissing-parameter-type"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wold-style-definition], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wold-style-definition"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wignored-qualifiers], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wignored-qualifiers"], [], [-Werror])
# Make htssafe.h's pointer-dest 'warning' attribute a hard error in our build
# (migration is at zero; a new char* dest is a regression). gcc/clang each take
# only their own spelling; downstream keeps the plain warning, not a build break.
AX_CHECK_COMPILE_FLAG([-Werror=attribute-warning], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Werror=attribute-warning"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Werror=user-defined-warnings], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Werror=user-defined-warnings"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-fstrict-aliasing -Wstrict-aliasing], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -fstrict-aliasing -Wstrict-aliasing"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-fstack-protector-strong], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -fstack-protector-strong"],
	[AX_CHECK_COMPILE_FLAG([-fstack-protector], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -fstack-protector"], [], [-Werror])], [-Werror])
AX_CHECK_COMPILE_FLAG([-fstack-clash-protection], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -fstack-clash-protection"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-fcf-protection], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -fcf-protection"], [], [-Werror])
# backtrace() unwinds through these; armhf, unlike amd64/arm64, defaults them off
# and printed frameless crash reports.
AX_CHECK_COMPILE_FLAG([-fasynchronous-unwind-tables], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -fasynchronous-unwind-tables"], [], [-Werror])
# No --discard-all: it drops the local symbols naming every static function, so
# a trace misattributes them to the nearest surviving global. Costs 0.6% size.
AX_CHECK_LINK_FLAG([-Wl,--no-undefined], [DEFAULT_LDFLAGS="$DEFAULT_LDFLAGS -Wl,--no-undefined"])
AX_CHECK_LINK_FLAG([-Wl,-z,relro,-z,now], [DEFAULT_LDFLAGS="$DEFAULT_LDFLAGS -Wl,-z,relro,-z,now"])
AX_CHECK_LINK_FLAG([-Wl,-z,noexecstack], [DEFAULT_LDFLAGS="$DEFAULT_LDFLAGS -Wl,-z,noexecstack"])

# Fortify libc calls (=3, else =2) unless the toolchain predefines it; skip
# sanitizer builds, whose interceptors want the unfortified calls.
case "$CFLAGS" in
*-fsanitize=*) ;;
*) HTS_ADD_FORTIFY_SOURCE ;;
esac

# Force libc back into DT_NEEDED for libraries that reach it only through
# libhttrack (the libtest callbacks), but only with a GNU-style linker; Apple
# ld rejects these flags and links libSystem unconditionally.
AX_CHECK_LINK_FLAG([-Wl,--push-state,--no-as-needed,-lc,--pop-state],
	[LIBC_FORCE_LINK="-Wl,--push-state,--no-as-needed,-lc,--pop-state"])
AC_SUBST([LIBC_FORCE_LINK])

### PIE
CFLAGS_PIE=""
LDFLAGS_PIE=""
AX_CHECK_COMPILE_FLAG([-fpie], [CFLAGS_PIE="-fpie"], [], [-Werror])
AX_CHECK_LINK_FLAG([-pie], [LDFLAGS_PIE="-pie"])
AC_SUBST([CFLAGS_PIE])
AC_SUBST([LDFLAGS_PIE])

# Ties a crash trace from a stripped build back to its separate debug symbols.
AX_CHECK_LINK_FLAG([-Wl,--build-id], [DEFAULT_LDFLAGS="$DEFAULT_LDFLAGS -Wl,--build-id"])

# tools/macos-app.sh rewrites load commands to @rpath, which needs header room ld64 does
# not leave by default; the probe fails on GNU ld, so this self-gates on Darwin.
AX_CHECK_LINK_FLAG([-Wl,-headerpad_max_install_names],
	[DEFAULT_LDFLAGS="$DEFAULT_LDFLAGS -Wl,-headerpad_max_install_names"])

### Relocatable install (#906, #978)
# libtool records the configured libdir as an absolute rpath, so a copied tree
# cannot find libhttrack. A binary-relative rpath fixes that where the loader
# expands the token, which the BSDs disagree on: probe by running one.
AC_ARG_ENABLE([origin-rpath],
	AS_HELP_STRING([--disable-origin-rpath],
		[link the binaries without a binary-relative rpath]),
	[hts_origin_asked=yes], [enable_origin_rpath=yes; hts_origin_asked=no])

# hts_origin_ref is the raw token the loader expands; hts_origin_ref_make is the
# spelling that survives make's expansion and libtool's eval of the link line.
AS_CASE([$host_os],
	[darwin*], [hts_origin_darwin=yes
		hts_origin_ref='@loader_path'
		hts_origin_ref_make='@loader_path'],
	[hts_origin_darwin=no
		hts_origin_ref='$ORIGIN'
		hts_origin_ref_make='\$$ORIGIN'])

HTS_EXPAND_LIBDIR

# Enumerated, not read from libtool's sys_lib_dlsearch_path_spec: that one is
# only augmented when /etc/ld.so.conf exists, so a sysroot or minimal container
# would let a distribution libdir through. Spelled out rather than globbed as
# /lib*, which would swallow /libfoo and /usr/libexec.
AS_CASE([$hts_libdir],
	[/lib | /lib/* | /lib32 | /lib32/* | /lib64 | /lib64/* | /libx32 | /libx32/*], [hts_origin_sysdir=yes],
	[/usr/lib | /usr/lib/* | /usr/lib32 | /usr/lib32/* | /usr/lib64 | /usr/lib64/* | /usr/libx32 | /usr/libx32/*], [hts_origin_sysdir=yes],
	[hts_origin_sysdir=no])

# dyld follows LC_RPATH only for a load path spelled @rpath/..., so Darwin also
# needs libhttrack's own id. That comes from libtool's archive_cmds template and
# not from LDFLAGS, so rewrite it; an unrecognised template leaves the gate off.
hts_origin_id=no
if test "x$hts_origin_darwin" = xyes; then
	hts_origin_subst='s|-install_name $rpath/|-install_name @rpath/|'
	hts_origin_archive=`printf '%s\n' "$archive_cmds" | $SED "$hts_origin_subst"`
	hts_origin_expsym=`printf '%s\n' "$archive_expsym_cmds" | $SED "$hts_origin_subst"`
	# Both templates or neither: should they ever diverge, the -export-symbols
	# path would keep stamping the absolute id the rpath cannot answer.
	if test "x$hts_origin_archive" != "x$archive_cmds" &&
		test "x$hts_origin_expsym" != "x$archive_expsym_cmds"; then
		hts_origin_id=yes
	fi
fi

ORIGIN_RPATH=no
RPATH_ORIGIN_LDFLAGS=""
if test "x$enable_origin_rpath" = xno; then
	AC_MSG_NOTICE([binary-relative rpath disabled by request])
elif test "x$hts_origin_darwin" = xyes && test "x$hts_origin_id" = xno; then
	AC_MSG_NOTICE([binary-relative rpath not used: unrecognised install_name in libtool archive_cmds])
elif test "x$enable_shared" = xno; then
	AC_MSG_NOTICE([binary-relative rpath not needed: no shared libraries])
elif test "x$hts_origin_sysdir" = xyes; then
	# A distribution libdir is already searched, and an rpath there is a lintian tag.
	AC_MSG_NOTICE([binary-relative rpath not used: $hts_libdir is a system libdir])
elif test "x$cross_compiling" = xyes && test "x$hts_origin_asked" = xno; then
	AC_MSG_NOTICE([binary-relative rpath not probed: cross compiling, pass --enable-origin-rpath to force it])
else
	AC_MSG_CHECKING([whether the loader honours a binary-relative rpath])
	AC_CACHE_VAL([hts_cv_origin_rpath], [
		hts_cv_origin_rpath=no
		# Cross compiling cannot run the probe, so take the stricter spelling first.
		hts_origin_cands="none origin"
		test "x$cross_compiling" = xyes && hts_origin_cands="origin none"
		test "x$hts_origin_darwin" = xyes && hts_origin_cands=loader_path
		rm -rf conftest.origin
		AS_MKDIR_P([conftest.origin/bin])
		AS_MKDIR_P([conftest.origin/lib])
		AS_MKDIR_P([conftest.origin/run])
		cat >conftest.origin/lib.c <<_ACEOF
int hts_origin_probe(void);
int hts_origin_probe(void) { return 42; }
_ACEOF
		cat >conftest.origin/main.c <<_ACEOF
int hts_origin_probe(void);
int main(void) { return hts_origin_probe() == 42 ? 0 : 1; }
_ACEOF
		if test "x$hts_origin_darwin" = xyes; then
			# The same @rpath id, so a dead LC_RPATH cannot pass for a live one.
			hts_origin_mklib="$CC $CFLAGS -dynamiclib -install_name @rpath/libhtsorigin.dylib -o conftest.origin/lib/libhtsorigin.dylib conftest.origin/lib.c"
		else
			hts_origin_mklib="$CC $CFLAGS -fPIC -shared -o conftest.origin/lib/libhtsorigin.so conftest.origin/lib.c"
		fi
		hts_origin_link="-Lconftest.origin/lib -lhtsorigin"
		if $hts_origin_mklib >&AS_MESSAGE_LOG_FD 2>&1; then
			for hts_origin_cand in $hts_origin_cands; do
				# Solaris and the older BSDs expand the token only with -z origin.
				hts_origin_try=
				test "x$hts_origin_cand" = xorigin && hts_origin_try=-Wl,-z,origin
				$CC $CFLAGS $LDFLAGS -o conftest.origin/bin/probe conftest.origin/main.c \
					$hts_origin_link $hts_origin_try "-Wl,-rpath,$hts_origin_ref/../lib" \
					>&AS_MESSAGE_LOG_FD 2>&1 || continue
				# From an unrelated cwd, so a loader taking the token literally fails.
				if test "x$cross_compiling" = xyes ||
					(unset LD_LIBRARY_PATH DYLD_LIBRARY_PATH
					cd conftest.origin/run && ../bin/probe) >&AS_MESSAGE_LOG_FD 2>&1; then
					hts_cv_origin_rpath=$hts_origin_cand
					break
				fi
			done
		fi
		rm -rf conftest.origin
	])
	AS_CASE([$hts_cv_origin_rpath],
		[no], [AC_MSG_RESULT([no])],
		[
			# $(libdir) first, so the token only answers once the configured
			# libdir is gone: $(bindir)/../lib is a different directory under a
			# lib64 or split-bindir layout and must not shadow it.
			RPATH_ORIGIN_LDFLAGS='-Wl,-rpath,$(libdir)'
			test "x$hts_cv_origin_rpath" = xorigin && RPATH_ORIGIN_LDFLAGS="$RPATH_ORIGIN_LDFLAGS -Wl,-z,origin"
			RPATH_ORIGIN_LDFLAGS="$RPATH_ORIGIN_LDFLAGS -Wl,-rpath,$hts_origin_ref_make/../lib"
			if test "x$hts_origin_darwin" = xyes; then
				archive_cmds=$hts_origin_archive
				archive_expsym_cmds=$hts_origin_expsym
			fi
			ORIGIN_RPATH=yes
			AC_MSG_RESULT([yes ($hts_origin_ref/../lib)])
		])
fi
AC_SUBST([ORIGIN_RPATH])
AC_SUBST([RPATH_ORIGIN_LDFLAGS])

# The @rpath id is only safe for a consumer carrying its own LC_RPATH, so
# libhttrack.pc hands one out -- under the gates the executables' own rpath
# respects, so that --disable-origin-rpath and an archive-only build stay
# rpath-free too. ${libdir} is pkg-config's own variable, expanded on query.
PKGCONFIG_RPATH=no
PKGCONFIG_RPATH_LDFLAG=
AS_IF([test "x$enable_origin_rpath" != xno && test "x$enable_shared" != xno &&
	test "x$hts_origin_sysdir" = xno],
	[PKGCONFIG_RPATH=yes
	PKGCONFIG_RPATH_LDFLAG=' -Wl,-rpath,${libdir}'])
AC_SUBST([PKGCONFIG_RPATH])
AC_SUBST([PKGCONFIG_RPATH_LDFLAG])

### Check for -fvisibility=hidden support
gl_VISIBILITY
AM_CFLAGS="$AM_CFLAGS $CFLAG_VISIBILITY"
AC_SUBST([AM_CFLAGS])

### Check for platform
#case $host in
#AIX) AC_DEFINE(HTS_PLATFORM, 1, [Defined to build under AIX]);;
#*-solaris*) AC_DEFINE(HTS_PLATFORM, 2, [Defined to build under solaris]);;
#*-linux-gnu | *-irix6*) AC_DEFINE(HTS_PLATFORM, 3, [Defined to build under Linux]);;
#*) AC_DEFINE(HTS_PLATFORM, 3, [Default value used]);;
#esac

### Probe long long size for 64-bit integer support. SIZEOF_LONG is not probed:
### it varies by architecture and would break Multi-Arch co-installation of
### libhttrack-dev (Debian #1133728). md5.h uses <stdint.h> instead.
AC_CHECK_SIZEOF(long long)

### check for in_addr_t
AC_CHECK_TYPE(in_addr_t,
AC_DEFINE(HTS_DO_NOT_REDEFINE_in_addr_t,,[Check for in_addr_t]),
AC_MSG_WARN([*** in_addr_t not found]),
[#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>])

# check for in_port_
AC_CHECK_TYPE(in_port_t, [], [AC_DEFINE([in_port_t], [uint16_t], [in_port_t])], [
#include <sys/types.h>
#include <netinet/in.h>
])

# check for sa_family_t
AC_CHECK_TYPE(sa_family_t, [], [AC_DEFINE([sa_family_t], [uint16_t], [sa_family_t])], [
#include <sys/types.h>
#include <netinet/in.h>
])

# check wether misaligned access is possible or not
AX_CHECK_ALIGNED_ACCESS_REQUIRED

# check for various headers
AC_CHECK_HEADERS([execinfo.h spawn.h sys/ioctl.h sys/random.h sys/sysctl.h])

### Where an optional feature's default comes from (meson's auto_features)
AC_MSG_CHECKING([whether to probe for optional features nobody asked for])
AC_ARG_ENABLE([auto-features],
	[AS_HELP_STRING([--disable-auto-features],
		[Default every optional feature to no rather than probing for it; an explicit --with-FEATURE or --enable-FEATURE still wins @<:@default=yes@:>@])],
	[
	case "${enableval}" in
	no|yes)
		auto_features=$enableval
		AC_MSG_RESULT($enableval)
		;;
	*)
		AC_MSG_ERROR([bad value for auto-features, expected yes/no])
		;;
	esac
	],
	[
	auto_features=yes
	AC_MSG_RESULT(yes)
	])
# What an optional feature's own flag falls back to when it is not passed.
AS_IF([test x"$auto_features" = x"no"],
	[hts_feature_default=no], [hts_feature_default=auto])

## The header can exist while backtrace() lives in another library (Termux's
## libandroid-execinfo); only httrack links it, so keep it out of LIBS.
AC_ARG_ENABLE([backtrace],
	[AS_HELP_STRING([--enable-backtrace=@<:@yes/no/auto@:>@],[Print a symbolic stack trace on a crash, on Linux @<:@default=auto@:>@])],
	[
	case "${enableval}" in
	no|yes|auto)
		backtrace_want=$enableval
		backtrace_asked=yes
		;;
	*)
		AC_MSG_ERROR([bad value for backtrace, expected yes/no/auto])
		;;
	esac
	],
	[
	backtrace_want=$hts_feature_default
	backtrace_asked=no
	])
BACKTRACE_LIBS=
backtrace_have=no
AS_CASE([$host_os], [linux*], [backtrace_platform=yes], [backtrace_platform=no])
AS_IF([test x"$backtrace_want" != x"no" && test x"$backtrace_platform" = x"yes"],
	[AS_IF([test "x$ac_cv_header_execinfo_h" = xyes],
		[hts_backtrace_save_libs=$LIBS
		AC_SEARCH_LIBS([backtrace], [execinfo android-execinfo],
			[backtrace_have=yes
			AC_DEFINE([HAVE_BACKTRACE], [1],
				[Define to 1 if backtrace() can be linked.])
			AS_IF([test "x$ac_cv_search_backtrace" != "xnone required"],
				[BACKTRACE_LIBS=$ac_cv_search_backtrace])])
		LIBS=$hts_backtrace_save_libs])
	AS_IF([test x"$backtrace_have" = x"no" && test x"$backtrace_want" = x"yes"],
		[AC_MSG_ERROR([backtrace requested but no execinfo.h or no linkable backtrace()])])])
AC_MSG_CHECKING([whether to enable symbolic crash traces])
dnl Name who decided: "auto" is the build environment's answer, not the packager's.
AS_IF([test x"$backtrace_platform" = x"no"],
	[AC_MSG_RESULT([no (not implemented on $host_os)])],
	[test x"$backtrace_want" = x"auto"], [AC_MSG_RESULT([$backtrace_have (auto)])],
	[test x"$backtrace_asked" = x"no"], [AC_MSG_RESULT([$backtrace_have (auto-features off)])],
	[AC_MSG_RESULT([$backtrace_have (requested)])])
AC_SUBST([BACKTRACE_LIBS])

## CSPRNG for the --single-file mark; /dev/urandom is the fallback
AC_CHECK_FUNCS([getrandom])

### zlib (mandatory)
CHECK_ZLIB()

### brotli and zstd content codings (optional)
CHECK_BROTLI()
CHECK_ZSTD()

### OpenSSL is explicitly enabled/disabled ?
AC_MSG_CHECKING(whether to enable https support)
AC_ARG_ENABLE([https],
	[AS_HELP_STRING([--enable-https=@<:@yes/no/auto@:>@],[Enable https support @<:@default=yes@:>@])],
	[
	case "${enableval}" in
	no|yes|auto)
		https_support=$enableval
		AC_MSG_RESULT($https_support)
		;;
	*)
		AC_MSG_ERROR([bad value for https, expected yes/no/auto])
		;;
	esac	
	],
	[
	https_support=yes
	AC_MSG_RESULT($https_support)
	])
AC_SUBST(HTTPS_SUPPORT,$https_support)

# Probe OpenSSL ?
if test x"$https_support" = x"no"; then
AC_MSG_NOTICE([disabling https support])
AC_DEFINE(HTS_USEOPENSSL, 0)
else
SAVE_LIBS=$LIBS
AC_CHECK_LIB(crypto, EVP_get_digestbyname,
	[
	OPENSSL_LIBS="-lcrypto"
	]
	)
LIBS="$LIBS $OPENSSL_LIBS"
AC_CHECK_LIB(ssl, SSL_CTX_new,
	[
	# -lssl first: an archive link needs it ahead of the -lcrypto it uses.
	OPENSSL_LIBS="-lssl $OPENSSL_LIBS"
	AC_DEFINE(HTS_USEOPENSSL, 1, [Check for OpenSSL])
	],
	[
	if test x"$https_support" = x"yes"; then
	AC_MSG_ERROR([not available])
	else
	AC_MSG_RESULT([not available])
	fi
	]
	)
LIBS=$SAVE_LIBS
AC_SUBST(OPENSSL_LIBS)
fi

### Support IPv6
V6_SUPPORT=no
AC_CHECK_LIB(c, getaddrinfo, [V6_FLAG="-DINET6"
V6_SUPPORT=yes
AC_DEFINE(HTS_INET6, 1, [Check for IPv6])], AC_MSG_WARN([*** IPv6 not found IPv6 compatibility disabled]))
AC_SUBST(V6_FLAG)
AC_SUBST(V6_SUPPORT)

### Check for LFS
AC_CHECK_LIB(c, fopen64, [LFS_FLAG="-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE"
AC_DEFINE(HTS_LFS, 1,[Check for large files support])], AC_MSG_RESULT([not found]))
AC_SUBST(LFS_FLAG)

### Check for pthread
AC_CHECK_LIB(pthread, pthread_create, [THREADS_LIBS=-lpthread
AC_DEFINE(THREADS, 1,[Check for pthread])], AC_MSG_RESULT([not found]))
AC_SUBST(THREADS_LIBS)
if test x$THREADS_LIBS = x
then
AC_CHECK_LIB(c, pthread_create, [THREADS_LIBS=
AC_DEFINE(THREADS, 1,[Check for pthread in c])], AC_MSG_RESULT([not found]))
AC_SUBST(THREADS_LIBS)
if test x$THREADS_LIBS = x
then
AC_CHECK_LIB(pthreads, pthread_create, [THREADS_LIBS=
AC_DEFINE(THREADS, 1,[Check for pthread in pthreads])], AC_MSG_WARN([*** not found]))
AC_SUBST(THREADS_LIBS)
fi
fi
THREADS_CFLAGS=
AC_SUBST(THREADS_CFLAGS)

### Check for socket/nsl (solaris)
AC_CHECK_LIB(socket, socket, [SOCKET_LIBS="-lnsl -lsocket"
AC_DEFINE(LIBSOCKET, 1,[Check for libsocket])], AC_MSG_RESULT([not necessary]))
AC_SUBST(SOCKET_LIBS)

### Charset conversion: iconv(), or the built-in codepage tables
## HTS_USEICONV 0 is a configuration htscharset.c already supports, so "off" is
## the htscodepages.h fallback rather than a missing library. GNU libiconv's
## header renames iconv_open unless LIBICONV_PLUG is defined, so a bare symbol
## probe can say yes where the compiler would not; link what the header gives us.
AC_ARG_WITH([iconv],
	[AS_HELP_STRING([--with-iconv@<:@=DIR@:>@],[Convert charsets with iconv() rather than the built-in tables @<:@default=auto@:>@])],
	[iconv_want=$withval
	iconv_asked=yes], [iconv_want=$hts_feature_default
	iconv_asked=no])
ICONV_LIBS=""
iconv_have=no
if test "$iconv_want" != "no"; then
	iconv_old_cppflags="$CPPFLAGS"
	iconv_old_ldflags="$LDFLAGS"
	iconv_old_libs="$LIBS"
	iconv_libdir=
	if test "$iconv_want" != "yes" -a "$iconv_want" != "auto"; then
		# An explicit prefix is authoritative: if the header is not under
		# it, error rather than silently pick a system copy.
		if test ! -f "$iconv_want/include/iconv.h"; then
			AC_MSG_ERROR([iconv requested at $iconv_want but $iconv_want/include/iconv.h is missing])
		fi
		HTS_FIND_LIBDIR([$iconv_want], [libiconv.*])
		if test -z "$hts_found_libdir"; then
			AC_MSG_ERROR([iconv requested at $iconv_want but no libiconv found in any of: $hts_libdir_subdirs])
		fi
		iconv_libdir=$hts_found_libdir
		CPPFLAGS="$CPPFLAGS -I$iconv_want/include"
		LDFLAGS="$LDFLAGS -L$iconv_libdir"
	fi
	m4_define([HTS_ICONV_PROG],
		[AC_LANG_PROGRAM([[#include <iconv.h>]],
			[[iconv_t cd = iconv_open("UTF-8", "ISO-8859-1");
			return cd == (iconv_t) -1;]])])
	AC_MSG_CHECKING([for a linkable iconv_open])
	AC_LINK_IFELSE([HTS_ICONV_PROG],
		[iconv_have=yes
		AC_MSG_RESULT([yes])],
		[LIBS="$LIBS -liconv"
		AC_LINK_IFELSE([HTS_ICONV_PROG],
			[iconv_have=yes
			ICONV_LIBS="-liconv"
			AC_DEFINE(LIBICONV, 1,[Check for libiconv])
			AC_MSG_RESULT([yes, in -liconv])],
			[AC_MSG_RESULT([no])])])
	CPPFLAGS="$iconv_old_cppflags"
	LDFLAGS="$iconv_old_ldflags"
	LIBS="$iconv_old_libs"
	if test "$iconv_have" = "yes"; then
		if test -n "$iconv_libdir"; then
			ICONV_LIBS="-L$iconv_libdir $ICONV_LIBS"
			# The rest of configure still needs the header path.
			CPPFLAGS="$CPPFLAGS -I$iconv_want/include"
		fi
	elif test "$iconv_want" != "auto"; then
		AC_MSG_ERROR([iconv requested but no iconv.h with a linkable iconv_open])
	fi
fi
AC_MSG_CHECKING([whether to convert charsets with iconv])
dnl Name who decided: "auto" is the build environment's answer, not the packager's.
if test "$iconv_want" = "auto"; then
	AC_MSG_RESULT([$iconv_have (auto)])
elif test "$iconv_asked" = "no"; then
	AC_MSG_RESULT([$iconv_have (auto-features off)])
else
	AC_MSG_RESULT([$iconv_have (requested)])
fi
if test "$iconv_have" = "yes"; then
	AC_DEFINE([HTS_USEICONV], [1], [Define to 0 to convert charsets with the built-in codepage tables instead of iconv()])
else
	AC_DEFINE([HTS_USEICONV], [0], [Define to 0 to convert charsets with the built-in codepage tables instead of iconv()])
fi
AC_SUBST(ICONV_LIBS)

### Check for dlsym
AC_CHECK_LIB(dl, dlopen, [DL_LIBS=-ldl
AC_DEFINE(DLLIB, 1,[Check for dl])], AC_MSG_RESULT([not found]))
AC_SUBST(DL_LIBS)
if test x$DL_LIBS = x
then
AC_CHECK_LIB(c, dlopen, [DL_LIBS=
AC_DEFINE(DLLIB, 1,[Check for dlopen in c])], AC_MSG_WARN([*** not found * this is bad]))
AC_SUBST(DL_LIBS)
fi

### What libhttrack.pc tells a consumer about its private dependencies
# A static link needs each library ahead of the ones it uses, which only a
# dependency's own .pc gets right (and which alone carries the -L an
# out-of-the-way prefix needs), so name the module wherever one is installed.
# Dependents first: pkg-config emits Requires.private after Libs.private.
# Absent the tool every dependency falls back to Libs.private, so the builder
# decides the installed .pc; a recipe wanting a stable one build-depends on it.
AC_ARG_VAR([PKG_CONFIG], [path to the pkg-config utility])
AC_PATH_TOOL([PKG_CONFIG], [pkg-config], [false])
PKGCONFIG_REQUIRES_PRIVATE=
PKGCONFIG_LIBS_PRIVATE=
hts_pc_dep() { # hts_pc_dep <module or empty> <link line>
	test -n "$2" || return 0
	if test -n "$1" && $PKG_CONFIG --exists "$1" 2>/dev/null; then
		PKGCONFIG_REQUIRES_PRIVATE="${PKGCONFIG_REQUIRES_PRIVATE:+$PKGCONFIG_REQUIRES_PRIVATE }$1"
	else
		PKGCONFIG_LIBS_PRIVATE="${PKGCONFIG_LIBS_PRIVATE:+$PKGCONFIG_LIBS_PRIVATE }$2"
	fi
}
# Whichever half of OpenSSL was actually found; https=auto can settle for libcrypto.
AS_CASE([" $OPENSSL_LIBS "],
	[*" -lssl "*], [hts_pc_openssl=openssl],
	[*" -lcrypto "*], [hts_pc_openssl=libcrypto],
	[hts_pc_openssl=])
hts_pc_dep "$hts_pc_openssl" "$OPENSSL_LIBS"
hts_pc_dep libbrotlidec "$BROTLI_LIBS"
hts_pc_dep libzstd "$ZSTD_LIBS"

# An installed header includes <openssl/ssl.h>, so a consumer needs that include
# path, and outside Requires.private nothing else in the .pc carries it.
PKGCONFIG_SSL_CFLAGS=
case " $PKGCONFIG_REQUIRES_PRIVATE " in
*" openssl "*) ;;
*)
	case " $OPENSSL_LIBS " in
	*" -lssl "*)
		AC_MSG_CHECKING([whether libhttrack.pc must name the OpenSSL include path])
		hts_pc_ssl_save_cppflags=$CPPFLAGS
		CPPFLAGS=
		AC_COMPILE_IFELSE([AC_LANG_SOURCE([[#include <openssl/ssl.h>]])],
			[hts_pc_ssl_default=yes], [hts_pc_ssl_default=no])
		CPPFLAGS=$hts_pc_ssl_save_cppflags
		if test x"$hts_pc_ssl_default" = x"yes"; then
			AC_MSG_RESULT([no, the compiler finds it unaided])
		else
			for hts_pc_ssl_flag in $CPPFLAGS; do
				case $hts_pc_ssl_flag in
				-I*)
					test -r "${hts_pc_ssl_flag#-I}/openssl/ssl.h" || continue
					PKGCONFIG_SSL_CFLAGS=$hts_pc_ssl_flag
					break
					;;
				esac
			done
			if test -n "$PKGCONFIG_SSL_CFLAGS"; then
				AC_MSG_RESULT([$PKGCONFIG_SSL_CFLAGS])
			else
				AC_MSG_RESULT([yes, but no -I in CPPFLAGS holds it])
			fi
		fi
		;;
	esac
	;;
esac
AC_SUBST([PKGCONFIG_SSL_CFLAGS])
AC_SUBST([PKGCONFIG_REQUIRES_PRIVATE])
AC_SUBST([PKGCONFIG_LIBS_PRIVATE])

### Check for setuid
AC_CHECK_LIB(c, setuid, [
AC_DEFINE(SETUID, 1,[Check for setuid])], AC_MSG_RESULT([not found]))

### Check for snprintf
AC_FUNC_SNPRINTF()

### Check for strnlen
AC_CHECK_LIB(c, strnlen, [
AC_DEFINE(HAVE_STRNLEN, 1,[Check for strnlen])], AC_MSG_RESULT([not found]))

## Online unit tests
AC_MSG_CHECKING(whether to enable online unit tests)
AC_ARG_ENABLE([online-unit-tests],
	[AS_HELP_STRING([--enable-online-unit-tests=@<:@yes/no/auto@:>@],[Enable online-unit-tests @<:@default=no@:>@])],
	[
	case "${enableval}" in
	no|yes|auto)
		online_unit_tests=$enableval
		AC_MSG_RESULT($enableval)
		;;
	*)
		AC_MSG_ERROR([bad value for online-unit-tests, expected yes/no/auto])
		;;
	esac
	],
	[
	online_unit_tests=no
	AC_MSG_RESULT(no)
	])
AC_SUBST(ONLINE_UNIT_TESTS,$online_unit_tests)

## Example callback plugins (libtest/): documentation samples every distribution
## has to un-install by hand.
AC_MSG_CHECKING(whether to build the example callback plugins)
AC_ARG_ENABLE([example-libs],
	[AS_HELP_STRING([--disable-example-libs],[Do not build or install the libtest/ example callback plugins; their sources install either way @<:@default=yes@:>@])],
	[
	case "${enableval}" in
	no|yes)
		example_libs=$enableval
		AC_MSG_RESULT($enableval)
		;;
	*)
		AC_MSG_ERROR([bad value for example-libs, expected yes/no])
		;;
	esac
	],
	[
	example_libs=yes
	AC_MSG_RESULT(yes)
	])
AM_CONDITIONAL([EXAMPLE_LIBS], [test x"$example_libs" = x"yes"])
AC_SUBST(EXAMPLE_LIBS,$example_libs)

## libFuzzer harnesses (fuzz/); requires clang
AC_MSG_CHECKING(whether to build fuzzers)
AC_ARG_ENABLE([fuzzers],
	[AS_HELP_STRING([--enable-fuzzers],[Build libFuzzer harnesses in fuzz/ (requires clang) @<:@default=no@:>@])],
	[
	case "${enableval}" in
	no|yes)
		fuzzers=$enableval
		AC_MSG_RESULT($enableval)
		;;
	*)
		AC_MSG_ERROR([bad value for fuzzers, expected yes/no])
		;;
	esac
	],
	[
	fuzzers=no
	AC_MSG_RESULT(no)
	])
if test x"$fuzzers" = x"yes"; then
	# Instrument the whole build for coverage; harnesses link -fsanitize=fuzzer.
	AX_CHECK_COMPILE_FLAG([-fsanitize=fuzzer-no-link],
		[DEFAULT_CFLAGS="$DEFAULT_CFLAGS -fsanitize=fuzzer-no-link"],
		[AC_MSG_ERROR([--enable-fuzzers requires libFuzzer support (clang)])])
	# Ordinary programs are instrumented too, so they need that runtime at link.
	AX_CHECK_LINK_FLAG([-fsanitize=fuzzer-no-link],
		[DEFAULT_LDFLAGS="$DEFAULT_LDFLAGS -fsanitize=fuzzer-no-link"],
		[AC_MSG_ERROR([--enable-fuzzers requires libFuzzer support (clang)])])
	# clang's static sanitizer runtimes clash with -Wl,--no-undefined on the .so.
	if test x"$enable_shared" != x"no"; then
		AC_MSG_ERROR([--enable-fuzzers requires --disable-shared])
	fi
fi
AM_CONDITIONAL([FUZZERS], [test x"$fuzzers" = x"yes"])

# tests/Makefile.am takes its TESTS from here instead of a checked-in list,
# which made every pair of test-adding PRs conflict on the forge (#844).
AC_MSG_CHECKING([for test scripts])
TESTS_LIST=
hts_ntests=0
for hts_t in "$srcdir"/tests/[[0-9]]*_*.test; do
	test -f "$hts_t" || continue
	TESTS_LIST="$TESTS_LIST `echo "$hts_t" | sed 's|.*/||'`"
	hts_ntests=`expr $hts_ntests + 1`
done
AC_SUBST([TESTS_LIST])
AC_MSG_RESULT([$hts_ntests])
test "$hts_ntests" -gt 0 || AC_MSG_ERROR([no test script matched tests/[[0-9]]*_*.test])

# Same for the fuzz corpus, for the same reason: automake expands no EXTRA_DIST glob.
AC_MSG_CHECKING([for fuzz corpus files])
FUZZ_CORPUS_LIST=
hts_ncorpus=0
for hts_c in "$srcdir"/fuzz/corpus/*/*; do
	test -f "$hts_c" || continue
	FUZZ_CORPUS_LIST="$FUZZ_CORPUS_LIST `echo "$hts_c" | sed 's|.*/corpus/|corpus/|'`"
	hts_ncorpus=`expr $hts_ncorpus + 1`
done
AC_SUBST([FUZZ_CORPUS_LIST])
AC_MSG_RESULT([$hts_ncorpus])
test "$hts_ncorpus" -gt 0 || AC_MSG_ERROR([no fuzz corpus file matched fuzz/corpus/*/*])

# The doc pages reach license.txt and its neighbours with "../", so htmldir defaults
# one level below $(docdir), where those install. A --htmldir naming a path wins.
AS_IF([test "x$htmldir" = 'x${docdir}'], [htmldir='${docdir}/html'])

# Final output
AC_CONFIG_FILES([
Makefile
src/Makefile
src/libhttrack.pc
man/Makefile
m4/Makefile
templates/Makefile
lang/Makefile
html/Makefile
libtest/Makefile
tests/Makefile
fuzz/Makefile
tools/Info.plist
])
AC_OUTPUT
