# HTS_ADD_FORTIFY_SOURCE
#
# Append -D_FORTIFY_SOURCE=3 (else =2) to CPPFLAGS, once the compiler has been
# seen to emit a checked entry point for it. Replaces autoconf-archive's
# AX_ADD_FORTIFY_SOURCE, which got this wrong twice: it only asked whether the
# define links, so an unoptimized build -- where glibc gates the checked builtins
# behind __OPTIMIZE__ and, since 2.41, no longer warns -- reported "yes" and
# fortified nothing; and its body assigns a string literal to a char*, so a
# CFLAGS carrying -Wwrite-strings failed the probe under the -Werror it
# force-added, taking the =2 fallback (same body) down with it.
#
# Sets FORTIFY_SOURCE_LEVEL to the level chosen, empty if none.
AC_DEFUN([HTS_ADD_FORTIFY_SOURCE], [
FORTIFY_SOURCE_LEVEL=
AC_MSG_CHECKING([whether _FORTIFY_SOURCE is already defined])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#ifdef _FORTIFY_SOURCE
#error _FORTIFY_SOURCE already defined
#endif
]])], [hts_fortify_predefined=no], [hts_fortify_predefined=yes])
AC_MSG_RESULT([$hts_fortify_predefined])

if test x"$hts_fortify_predefined" = xno; then
	AC_MSG_CHECKING([for a fortifiable optimization level])
	AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#if !defined(__OPTIMIZE__) || __OPTIMIZE__ <= 0
#error _FORTIFY_SOURCE needs -O
#endif
]])], [hts_fortify_opt=yes], [hts_fortify_opt=no])
	AC_MSG_RESULT([$hts_fortify_opt])
	if test x"$hts_fortify_opt" = xno; then
		AC_MSG_WARN([CFLAGS ($CFLAGS) carries no -O, so _FORTIFY_SOURCE would])
		AC_MSG_WARN([expand to nothing: not adding it. Build with -O2.])
	fi

	for hts_fortify_level in 3 2; do
		test x"$hts_fortify_opt" = xyes || break
		AC_MSG_CHECKING([whether to add -D_FORTIFY_SOURCE=$hts_fortify_level to CPPFLAGS])
		hts_fortify_save_cppflags=$CPPFLAGS
		CPPFLAGS="$CPPFLAGS -D_FORTIFY_SOURCE=$hts_fortify_level"
		# Linked, not just compiled: mingw-w64 needs -lssp for the _chk symbols.
		# @<:@16@:>@ is "[16]"; plain brackets would be eaten by m4.
		AC_LINK_IFELSE([AC_LANG_SOURCE([[
#include <string.h>
char hts_fortify_dst@<:@16@:>@;
void hts_fortify_copy(const char *src) { strcpy(hts_fortify_dst, src); }
int main(void) { hts_fortify_copy("x"); return 0; }
]])], [
			# The destination size is known above, so a working fortification
			# had to lower that strcpy to __strcpy_chk. A level we cannot
			# verify is refused: claiming one we do not have is the failure
			# this macro exists to prevent.
			hts_fortify_chk=unknown
			if test -n "$NM" && $NM conftest$ac_exeext >conftest.nm 2>/dev/null &&
				test -s conftest.nm; then
				# __stack_chk_fail is -fstack-protector's, not fortification's.
				if grep '_chk' conftest.nm | grep -v '__stack_chk' >/dev/null 2>&1; then
					hts_fortify_chk=yes
				else
					hts_fortify_chk=no
				fi
			fi
			rm -f conftest.nm
			if test x"$hts_fortify_chk" = xyes; then
				AC_MSG_RESULT([yes])
				FORTIFY_SOURCE_LEVEL=$hts_fortify_level
			elif test x"$hts_fortify_chk" = xno; then
				AC_MSG_RESULT([no (the compiler emitted no checked call)])
				CPPFLAGS=$hts_fortify_save_cppflags
			else
				AC_MSG_RESULT([no ($NM could not read the test binary)])
				CPPFLAGS=$hts_fortify_save_cppflags
			fi
		], [
			AC_MSG_RESULT([no])
			CPPFLAGS=$hts_fortify_save_cppflags
		])
		test -z "$FORTIFY_SOURCE_LEVEL" || break
	done

	if test x"$hts_fortify_opt" = xyes && test -z "$FORTIFY_SOURCE_LEVEL"; then
		AC_MSG_WARN([no usable _FORTIFY_SOURCE level: libc calls stay unchecked.])
	fi
fi
AC_SUBST([FORTIFY_SOURCE_LEVEL])
])
