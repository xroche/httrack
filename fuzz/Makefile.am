# libFuzzer harnesses; built only with --enable-fuzzers (requires clang).
if FUZZERS
noinst_PROGRAMS = fuzz-charset fuzz-meta fuzz-idna fuzz-entities \
	fuzz-unescape fuzz-filters fuzz-url fuzz-header fuzz-cachendx
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

# List corpus files explicitly: automake does not expand EXTRA_DIST globs.
EXTRA_DIST = README.md run-fuzzers.sh \
	corpus/charset/utf8.txt corpus/charset/latin1.txt corpus/charset/sjis.txt \
	corpus/meta/meta-charset.html corpus/meta/meta-http-equiv.html \
	corpus/idna/idna.txt corpus/idna/unicode.txt \
	corpus/idna/regress-multilabel-leak.txt \
	corpus/entities/entities.txt \
	corpus/unescape/percent.txt \
	corpus/filters/filter.bin corpus/filters/filter-size.bin \
	corpus/filters/regress-empty-subject-unique.bin \
	corpus/url/http-url.txt corpus/url/relative-path.txt \
	corpus/url/regress-file-empty-path.txt corpus/url/regress-long-path-abort.txt \
	corpus/header/full-response.txt corpus/header/redirect.txt \
	corpus/cachendx/new-format.txt corpus/cachendx/old-format.txt \
	corpus/cachendx/regress-overadvance.bin \
	corpus/cachendx/regress-truncated-entry.bin
