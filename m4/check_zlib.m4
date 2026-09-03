dnl @synopsis CHECK_ZLIB()
dnl
dnl Look for zlib. It is a hard requirement, not an option: the cache and the
dnl WARC output are zip/gzip containers, and the bundled minizip calls zlib
dnl directly. --with-zlib=DIR points at a non-standard prefix.
dnl
dnl Adds -lz to LIBS and defines HAVE_LIBZ.

AC_DEFUN([CHECK_ZLIB], [
AC_REQUIRE([HTS_EXPAND_LIBDIR])
AC_ARG_WITH([zlib],
	[AS_HELP_STRING([--with-zlib=DIR],[root directory of the zlib installation])],
	[zlib_want=$withval], [zlib_want=yes])
if test "$zlib_want" = "no"; then
	AC_MSG_ERROR([zlib cannot be disabled: the cache and the WARC output are zip/gzip containers, and the bundled minizip calls zlib directly])
fi
if test "$zlib_want" != "yes"; then
	# An explicit prefix is authoritative: if the header is not under it,
	# error rather than silently pick a system copy.
	if test ! -f "$zlib_want/include/zlib.h"; then
		AC_MSG_ERROR([zlib requested at $zlib_want but $zlib_want/include/zlib.h is missing])
	fi
	HTS_FIND_LIBDIR([$zlib_want], [libz.*])
	if test -z "$hts_found_libdir"; then
		AC_MSG_ERROR([zlib requested at $zlib_want but no libz found in any of: $hts_libdir_subdirs])
	fi
	CPPFLAGS="$CPPFLAGS -I$zlib_want/include"
	LDFLAGS="$LDFLAGS -L$hts_found_libdir"
elif test -f /usr/local/include/zlib.h; then
	# Where the BSD ports tree lands zlib, and not always searched by default.
	CPPFLAGS="$CPPFLAGS -I/usr/local/include"
	LDFLAGS="$LDFLAGS -L/usr/local/lib"
fi
AC_CHECK_HEADER([zlib.h], [],
	[AC_MSG_ERROR([zlib.h not found; install the zlib development files or pass --with-zlib=DIR])])
AC_CHECK_LIB([z], [inflateEnd], [],
	[AC_MSG_ERROR([libz not found; install the zlib development files or pass --with-zlib=DIR])])
])
