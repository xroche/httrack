AC_INIT([httrack], [3.47.99], [roche+packaging@httrack.com], [httrack], [http://www.httrack.com/])
AC_CONFIG_SRCDIR(src/httrack.c)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS(config.h)
AM_INIT_AUTOMAKE
VERSION_INFO="2:47:0"
AM_MAINTAINER_MODE

AC_PREREQ(2.50)

AC_PROG_CC
AC_PROG_CXX
AC_STDC_HEADERS
LT_INIT
AC_PROG_LN_S

# Export LD_LIBRARY_PATH name or equivalent.
AC_SUBST(SHLIBPATH_VAR,$shlibpath_var)

# Export .libs or equivalent.
AC_SUBST(LT_CV_OBJDIR,$lt_cv_objdir)

### Default CFLAGS
DEFAULT_CFLAGS="-Wall -Wcast-align -Wstrict-prototypes \
-Wmissing-prototypes -Wmissing-declarations -Wpointer-arith \
-Wnested-externs -D_REENTRANT"
AC_SUBST(DEFAULT_CFLAGS)
AC_SUBST(VERSION_INFO)

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
AC_CHECK_LIB(ssl, SSL_library_init,
	[
	OPENSSL_LIBS="-lcrypto -lssl"
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
