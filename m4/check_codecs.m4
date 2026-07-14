dnl @synopsis CHECK_BROTLI()
dnl @synopsis CHECK_ZSTD()
dnl
dnl Look for libbrotlidec and libzstd, the decoders for the "br" and "zstd"
dnl HTTP content codings. Both are optional: a missing library only drops the
dnl coding from Accept-Encoding. --with-brotli=DIR / --with-zstd=DIR point at a
dnl prefix; --without-brotli / --without-zstd disable the coding; the default
dnl is to use the library when it is found.
dnl
dnl Define HTS_USEBROTLI / HTS_USEZSTD to 1 or 0, and substitute BROTLI_LIBS /
dnl ZSTD_LIBS.

dnl CHECK_CODEC(name, NAME, header, library, symbol, companion-libs)
AC_DEFUN([CHECK_CODEC], [
AC_ARG_WITH([$1],
	[AS_HELP_STRING([--with-$1@<:@=DIR@:>@],[Enable the $1 content coding @<:@default=auto@:>@])],
	[codec_want=$withval], [codec_want=auto])
$2_LIBS=""
codec_have=no
if test "$codec_want" != "no"; then
	codec_old_cppflags="$CPPFLAGS"
	codec_old_ldflags="$LDFLAGS"
	codec_dir=no
	if test "$codec_want" != "yes" -a "$codec_want" != "auto"; then
		codec_dir=yes
		# An explicit prefix is authoritative: if the header is not under
		# it, error rather than silently pick a system copy.
		if test ! -f "$codec_want/include/$3"; then
			AC_MSG_ERROR([$1 requested at $codec_want but $codec_want/include/$3 is missing])
		fi
		CPPFLAGS="$CPPFLAGS -I$codec_want/include"
		LDFLAGS="$LDFLAGS -L$codec_want/lib"
	fi
	AC_CHECK_HEADER([$3], [codec_h=yes], [codec_h=no])
	AC_CHECK_LIB([$4], [$5], [codec_lib=yes], [codec_lib=no], [$6])
	CPPFLAGS="$codec_old_cppflags"
	LDFLAGS="$codec_old_ldflags"
	if test "$codec_h" = "yes" -a "$codec_lib" = "yes"; then
		codec_have=yes
		$2_LIBS="-l$4 $6"
		if test "$codec_dir" = "yes"; then
			$2_LIBS="-L$codec_want/lib -l$4 $6"
			# The rest of configure still needs the header path.
			CPPFLAGS="$CPPFLAGS -I$codec_want/include"
		fi
	elif test "$codec_want" != "auto"; then
		AC_MSG_ERROR([$1 requested but lib$4/$3 not found])
	fi
fi
AC_MSG_CHECKING([whether to enable the $1 content coding])
AC_MSG_RESULT([$codec_have])
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
