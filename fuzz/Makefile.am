# libFuzzer harnesses; built only with --enable-fuzzers (requires clang).
if FUZZERS
noinst_PROGRAMS = fuzz-charset fuzz-meta fuzz-idna fuzz-entities \
	fuzz-unescape fuzz-filters fuzz-url fuzz-header fuzz-cachendx \
	fuzz-htsparse fuzz-singlefile fuzz-sitemap fuzz-arc
endif

AM_CPPFLAGS = \
	@DEFAULT_CFLAGS@ \
	@THREADS_CFLAGS@ \
	@V6_FLAG@ \
	@LFS_FLAG@ \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/coucal

# Static-link libhttrack.la: the internal symbols are hidden in the .so.
AM_LDFLAGS = @DEFAULT_LDFLAGS@ -fsanitize=fuzzer -static-libtool-libs
LDADD = $(top_builddir)/src/libhttrack.la $(THREADS_LIBS)

fuzz_charset_SOURCES = fuzz-charset.c fuzz.h
fuzz_meta_SOURCES = fuzz-meta.c fuzz.h
fuzz_idna_SOURCES = fuzz-idna.c fuzz.h
fuzz_entities_SOURCES = fuzz-entities.c fuzz.h
fuzz_unescape_SOURCES = fuzz-unescape.c fuzz.h
fuzz_filters_SOURCES = fuzz-filters.c fuzz.h
fuzz_url_SOURCES = fuzz-url.c fuzz.h
fuzz_header_SOURCES = fuzz-header.c fuzz.h
fuzz_cachendx_SOURCES = fuzz-cachendx.c fuzz.h
fuzz_htsparse_SOURCES = fuzz-htsparse.c fuzz.h
fuzz_singlefile_SOURCES = fuzz-singlefile.c fuzz.h
fuzz_sitemap_SOURCES = fuzz-sitemap.c fuzz.h

# proxytrack does not link libhttrack, so its store compiles into the harness;
# coucal, minizip and md5 still come from the static libhttrack above.
fuzz_arc_SOURCES = fuzz-arc.c fuzz.h $(top_srcdir)/src/proxy/store.c
fuzz_arc_CPPFLAGS = $(AM_CPPFLAGS) -DZLIB_CONST

# Corpus list comes from configure: automake does not expand EXTRA_DIST globs.
EXTRA_DIST = README.md run-fuzzers.sh check-corpus-list.sh $(FUZZ_CORPUS_LIST)
