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
	# The library is not always under lib: a multilib host (Fedora, openSUSE)
	# keeps the 64-bit copy in lib64, Debian under a multiarch triplet. The
	# configured libdir is the host's own answer, so ask it before guessing.
	case $hts_libdir in
	"$hts_exec_prefix"/*) zlib_hostdir=${hts_libdir#"$hts_exec_prefix"/} ;;
	*/*) zlib_hostdir=${hts_libdir##*/} ;;
	*) zlib_hostdir=lib ;;
	esac
	zlib_subdirs=$zlib_hostdir
	for zlib_sub in lib lib64; do
		test "$zlib_sub" = "$zlib_hostdir" || zlib_subdirs="$zlib_subdirs $zlib_sub"
	done
	zlib_libdir=
	for zlib_sub in $zlib_subdirs; do
		for zlib_file in "$zlib_want/$zlib_sub"/libz.*; do
			test -f "$zlib_file" && zlib_libdir=$zlib_want/$zlib_sub && break
		done
		test -n "$zlib_libdir" && break
	done
	if test -z "$zlib_libdir"; then
		AC_MSG_ERROR([zlib requested at $zlib_want but no libz found in any of: $zlib_subdirs])
	fi
	CPPFLAGS="$CPPFLAGS -I$zlib_want/include"
	LDFLAGS="$LDFLAGS -L$zlib_libdir"
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
