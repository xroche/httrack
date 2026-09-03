dnl @synopsis HTS_EXPAND_LIBDIR()
dnl
dnl Expand $exec_prefix and $libdir into hts_exec_prefix and hts_libdir. Until
dnl config.status runs, $prefix is NONE and $libdir still holds an unexpanded
dnl ${exec_prefix} reference, so neither can be read as a path.

AC_DEFUN([HTS_EXPAND_LIBDIR], [
hts_save_prefix=$prefix
hts_save_exec_prefix=$exec_prefix
test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix=$prefix
eval hts_exec_prefix=\"$exec_prefix\"
eval hts_libdir=\"$libdir\"
eval hts_libdir=\"$hts_libdir\"
prefix=$hts_save_prefix
exec_prefix=$hts_save_exec_prefix
])

dnl @synopsis HTS_MULTIARCH_SUBDIR()
dnl
dnl Set hts_multiarch_subdir to the lib/TRIPLET Debian and Ubuntu keep their
dnl libraries under, empty on any other layout. The compiler is asked rather
dnl than $host, because only the compiler knows what a cross or -m32 build
dnl targets, and $host names the machine configure runs on.

AC_DEFUN([HTS_MULTIARCH_SUBDIR], [
AC_REQUIRE([AC_PROG_CC])
AC_CACHE_CHECK([the multiarch library subdirectory], [hts_cv_multiarch_subdir], [
hts_cv_multiarch_subdir=none
hts_multiarch_name=`$CC -print-multiarch 2>/dev/null`
if test -z "$hts_multiarch_name"; then
	# clang rejects -print-multiarch, but resolves libc in that same directory.
	hts_multiarch_libc=`$CC -print-file-name=libc.so 2>/dev/null`
	case $hts_multiarch_libc in
	*/*)
		hts_multiarch_name=${hts_multiarch_libc%/*}
		hts_multiarch_name=${hts_multiarch_name##*/}
		;;
	esac
fi
# A triplet always spells an ABI, which is what tells it from a plain lib dir.
case $hts_multiarch_name in
*[[!-a-zA-Z0-9_.]]*) ;;
*-*) hts_cv_multiarch_subdir=lib/$hts_multiarch_name ;;
esac
])
if test "x$hts_cv_multiarch_subdir" = xnone; then
	hts_multiarch_subdir=
else
	hts_multiarch_subdir=$hts_cv_multiarch_subdir
fi
])

dnl @synopsis HTS_FIND_LIBDIR(PREFIX, LIB-GLOB)
dnl
dnl Set hts_found_libdir to the subdirectory of PREFIX holding a file matching
dnl LIB-GLOB, empty when none does, and hts_libdir_subdirs to the candidates
dnl tried. The library is not always under lib: a multilib host (Fedora,
dnl openSUSE) keeps the 64-bit copy in lib64, Debian under a multiarch triplet.
dnl The configured libdir is the host's own answer, so ask it before guessing.
dnl The triplet is guessed last, because a reorder would silently swap which
dnl library a prefix that already resolves gets linked against.

AC_DEFUN([HTS_FIND_LIBDIR], [
AC_REQUIRE([HTS_EXPAND_LIBDIR])
AC_REQUIRE([HTS_MULTIARCH_SUBDIR])
case $hts_libdir in
"$hts_exec_prefix"/*) hts_hostdir=${hts_libdir#"$hts_exec_prefix"/} ;;
*/*) hts_hostdir=${hts_libdir##*/} ;;
*) hts_hostdir=lib ;;
esac
hts_libdir_subdirs=$hts_hostdir
for hts_sub in lib lib64 $hts_multiarch_subdir; do
	test "$hts_sub" = "$hts_hostdir" || hts_libdir_subdirs="$hts_libdir_subdirs $hts_sub"
done
hts_found_libdir=
for hts_sub in $hts_libdir_subdirs; do
	for hts_file in "$1/$hts_sub"/$2; do
		test -f "$hts_file" && hts_found_libdir=$1/$hts_sub && break
	done
	test -n "$hts_found_libdir" && break
done
])
