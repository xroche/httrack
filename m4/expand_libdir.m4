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
dnl Set hts_multiarch_subdir to the lib/TRIPLET a Debian or Ubuntu host keeps
dnl its libraries under, empty on any other layout. gcc and clang print the
dnl triplet only where that layout is in use, and the compiler is the one that
dnl knows the target, so a cross build gets the triplet it is building for.

AC_DEFUN([HTS_MULTIARCH_SUBDIR], [
AC_REQUIRE([AC_PROG_CC])
AC_CACHE_CHECK([the multiarch library subdirectory], [hts_cv_multiarch_subdir], [
hts_cv_multiarch_subdir=none
hts_multiarch_triplet=`$CC -print-multiarch 2>/dev/null`
# A compiler that does not know the option can answer on stdout, so accept
# the reply only when it spells a single path component.
case $hts_multiarch_triplet in
"") ;;
*[[!-a-zA-Z0-9_.]]*) ;;
*) hts_cv_multiarch_subdir=lib/$hts_multiarch_triplet ;;
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
