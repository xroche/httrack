# libFuzzer harnesses; built only with --enable-fuzzers (requires clang).
if FUZZERS
noinst_PROGRAMS = fuzz-charset fuzz-codepage fuzz-meta fuzz-idna fuzz-entities \
	fuzz-unescape fuzz-filters fuzz-url fuzz-header fuzz-cachendx \
	fuzz-htsparse fuzz-singlefile fuzz-sitemap fuzz-arc fuzz-cookie \
	fuzz-robots \
	fuzz-accept \
	fuzz-alias \
	fuzz-sniff \
	fuzz-bundle \
	fuzz-savename \
	fuzz-warc \
	fuzz-cache \
	fuzz-codec \
	fuzz-response \
	fuzz-redirect \
	fuzz-socks5 \
	fuzz-ftp
endif

AM_CPPFLAGS = \
	-DHTS_INTERNAL_BUILD \
	@DEFAULT_CFLAGS@ \
	@THREADS_CFLAGS@ \
	@V6_FLAG@ \
	@LFS_FLAG@ \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/coucal

# Static-link libhttrack.la: the internal symbols are hidden in the .so.
# An archive engine (OSS-Fuzz AFL) must follow the objects.
AM_LDFLAGS = @DEFAULT_LDFLAGS@ -static-libtool-libs
LDADD = $(top_builddir)/src/libhttrack.la $(FUZZ_ENGINE) $(THREADS_LIBS)

fuzz_charset_SOURCES = fuzz-charset.c fuzz.h

# A second copy of htscharset.c, forced onto the built-in codepage tables that
# an iconv build never compiles. It defines the same symbols as the library's
# copy, so the linker resolves them here and never pulls that archive member.
fuzz_codepage_SOURCES = fuzz-codepage.c fuzz.h $(top_srcdir)/src/htscharset.c
fuzz_codepage_CPPFLAGS = $(AM_CPPFLAGS) -DDISABLE_ICONV
fuzz_meta_SOURCES = fuzz-meta.c fuzz.h
fuzz_idna_SOURCES = fuzz-idna.c fuzz.h
fuzz_entities_SOURCES = fuzz-entities.c fuzz.h
fuzz_unescape_SOURCES = fuzz-unescape.c fuzz.h
fuzz_filters_SOURCES = fuzz-filters.c fuzz.h
fuzz_url_SOURCES = fuzz-url.c fuzz.h
fuzz_header_SOURCES = fuzz-header.c fuzz.h
# The .ndx reader it drives is proxytrack's, so its store compiles in here too.
fuzz_cachendx_SOURCES = fuzz-cachendx.c fuzz.h $(top_srcdir)/src/proxy/store.c \
	$(top_srcdir)/src/htsshared.c
fuzz_cachendx_CPPFLAGS = $(AM_CPPFLAGS) -DZLIB_CONST
fuzz_htsparse_SOURCES = fuzz-htsparse.c fuzz.h
fuzz_singlefile_SOURCES = fuzz-singlefile.c fuzz.h
fuzz_sitemap_SOURCES = fuzz-sitemap.c fuzz.h

# proxytrack does not link libhttrack, so its store compiles into the harness;
# coucal, minizip and md5 still come from the static libhttrack above.
fuzz_arc_SOURCES = fuzz-arc.c fuzz.h $(top_srcdir)/src/proxy/store.c \
	$(top_srcdir)/src/htsshared.c
fuzz_arc_CPPFLAGS = $(AM_CPPFLAGS) -DZLIB_CONST

# The robots.txt parser and its RFC 9309 wildcard matcher.
fuzz_robots_SOURCES = fuzz-robots.c fuzz.h

# The link-acceptance wizard: hostile links against rules and answers.
fuzz_accept_SOURCES = fuzz-accept.c fuzz.h

# The option alias and command-line splitter, engine side and server side.
fuzz_alias_SOURCES = fuzz-alias.c fuzz.h

# The MIME magic-byte consistency check over a hostile body.
fuzz_sniff_SOURCES = fuzz-sniff.c fuzz.h

# The login-bundle reader: a plain file parser, the library links everything.
fuzz_bundle_SOURCES = fuzz-bundle.c fuzz.h

# The save-name builder: hostile URLs in, on-disk paths out.
fuzz_savename_SOURCES = fuzz-savename.c fuzz.h

fuzz_warc_SOURCES = fuzz-warc.c fuzz.h

# The ZIP cache reader: index walk, header block, record read and repair.
fuzz_cache_SOURCES = fuzz-cache.c fuzz.h

# The Content-Encoding unpackers: gzip, deflate, and br and zstd where built in.
fuzz_codec_SOURCES = fuzz-codec.c fuzz.h

# The HTTP receive path: back_wait() reading a response off a socketpair.
fuzz_response_SOURCES = fuzz-response.c fuzz.h

# Redirect and 412/416 handling: hts_mirror_check_moved() over a fuzzed Location.
fuzz_redirect_SOURCES = fuzz-redirect.c fuzz.h

# The SOCKS5 client handshake against a scripted, hostile proxy reply.
fuzz_socks5_SOURCES = fuzz-socks5.c fuzz.h

# The FTP control channel: get_ftp_line() over a socketpair, then the reply parsers.
fuzz_ftp_SOURCES = fuzz-ftp.c fuzz.h

# Corpus and dictionary lists come from configure: automake does not expand EXTRA_DIST globs.
EXTRA_DIST = README.md run-fuzzers.sh check-corpus-list.sh oss-fuzz-build.sh ubsan-ignorelist.txt \
	generate-extra-seeds.py $(FUZZ_CORPUS_LIST) $(FUZZ_AUX_LIST)

# The cookie jar and basic-auth store: jar files in, credentials scoped out.
fuzz_cookie_SOURCES = fuzz-cookie.c fuzz.h
