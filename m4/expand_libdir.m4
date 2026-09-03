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

dnl @synopsis HTS_FIND_LIBDIR(PREFIX, LIB-GLOB)
dnl
dnl Set hts_found_libdir to the subdirectory of PREFIX holding a file matching
dnl LIB-GLOB, empty when none does, and hts_libdir_subdirs to the candidates
dnl tried. The library is not always under lib: a multilib host (Fedora,
dnl openSUSE) keeps the 64-bit copy in lib64, Debian under a multiarch triplet.
dnl The configured libdir is the host's own answer, so ask it before guessing.

AC_DEFUN([HTS_FIND_LIBDIR], [
AC_REQUIRE([HTS_EXPAND_LIBDIR])
case $hts_libdir in
"$hts_exec_prefix"/*) hts_hostdir=${hts_libdir#"$hts_exec_prefix"/} ;;
*/*) hts_hostdir=${hts_libdir##*/} ;;
*) hts_hostdir=lib ;;
esac
hts_libdir_subdirs=$hts_hostdir
for hts_sub in lib lib64; do
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
