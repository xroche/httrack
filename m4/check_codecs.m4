dnl @synopsis CHECK_BROTLI()
dnl @synopsis CHECK_ZSTD()
dnl
dnl Look for libbrotlidec and libzstd, the decoders for the "br" and "zstd"
dnl HTTP content codings. Both are optional: a missing library only drops the
dnl coding from Accept-Encoding. --with-brotli=DIR / --with-zstd=DIR point at a
dnl prefix; --without-brotli / --without-zstd disable the coding; the default
dnl is to use the library when it is found, unless --disable-auto-features asked
dnl for every unrequested optional feature to default to no.
dnl
dnl Define HTS_USEBROTLI / HTS_USEZSTD to 1 or 0, and substitute BROTLI_LIBS /
dnl ZSTD_LIBS.

dnl CHECK_CODEC(name, NAME, header, library, symbol, companion-libs)
AC_DEFUN([CHECK_CODEC], [
AC_REQUIRE([HTS_EXPAND_LIBDIR])
AC_ARG_WITH([$1],
	[AS_HELP_STRING([--with-$1@<:@=DIR@:>@],[Enable the $1 content coding @<:@default=auto@:>@])],
	[codec_want=$withval
	codec_asked=yes], [codec_want=$hts_feature_default
	codec_asked=no])
$2_LIBS=""
codec_have=no
if test "$codec_want" != "no"; then
	codec_old_cppflags="$CPPFLAGS"
	codec_old_ldflags="$LDFLAGS"
	codec_libdir=
	if test "$codec_want" != "yes" -a "$codec_want" != "auto"; then
		# An explicit prefix is authoritative: if the header is not under
		# it, error rather than silently pick a system copy.
		if test ! -f "$codec_want/include/$3"; then
			AC_MSG_ERROR([$1 requested at $codec_want but $codec_want/include/$3 is missing])
		fi
		HTS_FIND_LIBDIR([$codec_want], [lib$4.*])
		if test -z "$hts_found_libdir"; then
			AC_MSG_ERROR([$1 requested at $codec_want but no lib$4 found in any of: $hts_libdir_subdirs])
		fi
		codec_libdir=$hts_found_libdir
		CPPFLAGS="$CPPFLAGS -I$codec_want/include"
		LDFLAGS="$LDFLAGS -L$codec_libdir"
	fi
	AC_CHECK_HEADER([$3], [codec_h=yes], [codec_h=no])
	AC_CHECK_LIB([$4], [$5], [codec_lib=yes], [codec_lib=no], [$6])
	CPPFLAGS="$codec_old_cppflags"
	LDFLAGS="$codec_old_ldflags"
	if test "$codec_h" = "yes" -a "$codec_lib" = "yes"; then
		codec_have=yes
		$2_LIBS="-l$4 $6"
		if test -n "$codec_libdir"; then
			$2_LIBS="-L$codec_libdir -l$4 $6"
			# The rest of configure still needs the header path.
			CPPFLAGS="$CPPFLAGS -I$codec_want/include"
		fi
	elif test "$codec_want" != "auto"; then
		AC_MSG_ERROR([$1 requested but lib$4/$3 not found])
	fi
fi
AC_MSG_CHECKING([whether to enable the $1 content coding])
dnl Name who decided: "auto" is the build environment's answer, not the packager's.
if test "$codec_want" = "auto"; then
	AC_MSG_RESULT([$codec_have (auto)])
elif test "$codec_asked" = "no"; then
	AC_MSG_RESULT([$codec_have (auto-features off)])
else
	AC_MSG_RESULT([$codec_have (requested)])
fi
if test "$codec_have" = "yes"; then
	AC_DEFINE([HTS_USE$2], [1], [Define to 1 to decode the $1 content coding])
else
	AC_DEFINE([HTS_USE$2], [0], [Define to 1 to decode the $1 content coding])
fi
AC_SUBST([$2_LIBS])
AC_SUBST([$2_ENABLED], [$codec_have])
])

dnl brotlidec needs brotlicommon (the static dictionary) for a fully-static link.
AC_DEFUN([CHECK_BROTLI],
	[CHECK_CODEC([brotli], [BROTLI], [brotli/decode.h], [brotlidec], [BrotliDecoderDecompressStream], [-lbrotlicommon])])

AC_DEFUN([CHECK_ZSTD],
	[CHECK_CODEC([zstd], [ZSTD], [zstd.h], [zstd], [ZSTD_decompressStream], [])])
