AC_PREREQ(2.59)

AC_INIT([httrack], [3.48.21], [roche+packaging@httrack.com], [httrack], [http://www.httrack.com/])
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

Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.

Please visit our Website: http://www.httrack.com
])

AC_CONFIG_SRCDIR(src/httrack.c)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS(config.h)
AM_INIT_AUTOMAKE([subdir-objects])
VERSION_INFO="2:48:0"
AM_MAINTAINER_MODE
AC_USE_SYSTEM_EXTENSIONS

AC_PROG_CC
AM_PROG_CC_C_O
AC_STDC_HEADERS
LT_INIT
AC_PROG_LN_S
AM_PROG_LIBTOOL

# Export LD_LIBRARY_PATH name or equivalent.
AC_SUBST(SHLIBPATH_VAR,$shlibpath_var)

# Export .libs or equivalent.
AC_SUBST(LT_CV_OBJDIR,$lt_cv_objdir)

# Export version info
AC_SUBST(VERSION_INFO)

### Default CFLAGS
DEFAULT_CFLAGS="-Wall -Wformat -Wformat-security \
-Wmultichar -Wwrite-strings -Wcast-qual -Wcast-align \
-Wstrict-prototypes -Wmissing-prototypes \
-Wmissing-declarations -Wdeclaration-after-statement \
-Wpointer-arith -Wsequence-point -Wnested-externs \
-D_REENTRANT"
AC_SUBST(DEFAULT_CFLAGS)
DEFAULT_LDFLAGS=""
AC_SUBST(DEFAULT_LDFLAGS)

### Additional flags (if supported)
AX_CHECK_COMPILE_FLAG([-Wparentheses], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wparentheses"])
AX_CHECK_COMPILE_FLAG([-Winit-self], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Winit-self"])
AX_CHECK_COMPILE_FLAG([-Wunused-but-set-parameter], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wunused-but-set-parameter"])
AX_CHECK_COMPILE_FLAG([-Waddress], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Waddress"])
AX_CHECK_COMPILE_FLAG([-Wuninitialized], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wuninitialized"])
AX_CHECK_COMPILE_FLAG([-Wformat=2], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wformat=2"])
AX_CHECK_COMPILE_FLAG([-Wformat-nonliteral], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wformat-nonliteral"])
AX_CHECK_COMPILE_FLAG([-Wmissing-parameter-type], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wmissing-parameter-type"])
AX_CHECK_COMPILE_FLAG([-Wold-style-definition], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wold-style-definition"])
AX_CHECK_COMPILE_FLAG([-Wignored-qualifiers], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -Wignored-qualifiers"])
AX_CHECK_COMPILE_FLAG([-fstrict-aliasing -Wstrict-aliasing], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -fstrict-aliasing -Wstrict-aliasing"])
AX_CHECK_LINK_FLAG([-Wl,--discard-all], [DEFAULT_LDLAGS="$DEFAULT_LDLAGS -Wl,--discard-all"])
AX_CHECK_LINK_FLAG([-Wl,--no-undefined], [DEFAULT_LDLAGS="$DEFAULT_LDLAGS -Wl,--no-undefined"])

## Export all symbols for backtraces
AX_CHECK_COMPILE_FLAG([-rdynamic], [DEFAULT_CFLAGS="$DEFAULT_CFLAGS -rdynamic"])

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

### Check size of long and long long.
AC_CHECK_SIZEOF(long)
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
AC_CHECK_HEADERS([execinfo.h])

### zlib
CHECK_ZLIB()

### OpenSSL is explicitly enabled/disabled ?
AC_MSG_CHECKING(whether to enable https support)
AC_ARG_ENABLE([https],
	[AC_HELP_STRING([--enable-https=@<:@yes/no/auto@:>@],
		[Enable https support @<:@default=yes@:>@])],
	[
	case "${enableval}" in
	no|yes|auto)
		https_support=$enableval
		AC_MSG_RESULT($https_support)
		;;
	*)
		AC_MSG_ERROR(bad value for https, expected yes/no/auto)
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
AC_CHECK_LIB(ssl, SSL_library_init,
	[
	OPENSSL_LIBS="$OPENSSL_LIBS -lssl"
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
AC_CHECK_LIB(c, getaddrinfo, [V6_FLAG="-DINET6"
AC_DEFINE(HTS_INET6, 1, [Check for IPv6])], AC_MSG_WARN([*** IPv6 not found IPv6 compatibility disabled]))
AC_SUBST(V6_FLAG)

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

### Check for iconv
AC_CHECK_LIB(iconv, iconv, [ICONV_LIBS="-liconv"
AC_DEFINE(LIBICONV, 1,[Check for libiconv])], AC_MSG_RESULT([not necessary]))
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

### Check for ftime
AC_CHECK_LIB(c, ftime, [
AC_DEFINE(FTIME, 1,[Check for ftime])], AC_MSG_RESULT([not found]))

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
	[AC_HELP_STRING([--enable-online-unit-tests=@<:@yes/no/auto@:>@],
		[Enable online-unit-tests @<:@default=yes@:>@])],
	[
	case "${enableval}" in
	no|yes|auto)
		online_unit_tests=$enableval
		AC_MSG_RESULT($enableval)
		;;
	*)
		AC_MSG_ERROR(bad value for online-unit-tests, expected yes/no/auto)
		;;
	esac
	],
	[
	online_unit_tests=no
	AC_MSG_RESULT(no)
	])
AC_SUBST(ONLINE_UNIT_TESTS,$online_unit_tests)

# Final output
AC_OUTPUT([
Makefile
src/Makefile
man/Makefile
m4/Makefile
templates/Makefile
lang/Makefile
html/Makefile
libtest/Makefile
tests/Makefile
])
