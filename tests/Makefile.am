# configure globs tests/[0-9]*_*.test into this, so adding a test edits no shared
# file. Not a make $(wildcard): automake word-splits TESTS at automake time and
# emits a broken per-word rule for each.
TESTS = @TESTS_LIST@

# Committed binary fixture read by 01_zlib-cache-golden.test. List it
# explicitly: automake does not expand wildcards in EXTRA_DIST, so a glob would
# silently drop it from the dist tarball and break "make distcheck".
EXTRA_DIST = $(TESTS) renamefail.c unlinkfail.c threadattrfail.c nobacktrace.c altstackprobe.c nopieprobe.c closetrack.c ftpcancel.c crawl-test.sh run-all-tests.sh check-network.sh check-test-names.sh \
	proxy-https-server.py socks5-server.py proxy-connect-server.py \
	proxytestlib.py tls-stall-server.py warc-validate.py wacz-validate.py \
	header-injection-server.py header-injection-check.py \
	pty-resize.py ttyrun.py test-timeout.sh png-colors.py \
	sanitizer-stderr-wrap.sh \
	local-crawl.sh local-server.py ftp-server.py testlib.sh proclib.sh \
	webhttracklib.sh httpclient.py webtestlib.py crawllib.sh arclib.sh \
	guide-image-check.py \
	ci-windows-suite.sh ci-windows-watchdog.ps1 install-headers-sweep.sh \
	server.crt server.key \
	server-root/simple/basic.html server-root/simple/link.html \
	server-root/stripquery/index.html server-root/stripquery/a.html \
	server-root/hostalias/index.html server-root/hostalias/a.html \
	server-root/hostalias/b.html server-root/hostalias/c.html \
	server-root/hostalias/e.html \
	server-root/fraglink/index.html server-root/fraglink/target.html \
	fixtures/cache-golden/hts-cache/new.zip \
	install-manifest.txt \
	62_lang-untranslated.counts \
	62_lang-linebreaks.counts

TESTS_ENVIRONMENT =
TESTS_ENVIRONMENT += PATH=$(top_builddir)/src$(PATH_SEPARATOR)$$PATH
# note: libtool should handle that
### TESTS_ENVIRONMENT += $(SHLIBPATH_VAR)="$(top_builddir)/src/$(LT_CV_OBJDIR)$${$(SHLIBPATH_VAR):+$(PATH_SEPARATOR)}$$$(SHLIBPATH_VAR)"
TESTS_ENVIRONMENT += ONLINE_UNIT_TESTS=$(ONLINE_UNIT_TESTS)
TESTS_ENVIRONMENT += HTTPS_SUPPORT=$(HTTPS_SUPPORT)
TESTS_ENVIRONMENT += BROTLI_ENABLED=$(BROTLI_ENABLED)
TESTS_ENVIRONMENT += ZSTD_ENABLED=$(ZSTD_ENABLED)
TESTS_ENVIRONMENT += V6_SUPPORT=$(V6_SUPPORT)
# Asserted by 146_bash-shell.test.
TESTS_ENVIRONMENT += BASH_SHELL=$(BASH_SHELL)
TESTS_ENVIRONMENT += top_srcdir=$(top_srcdir)
TESTS_ENVIRONMENT += abs_top_srcdir=$(abs_top_srcdir)
TESTS_ENVIRONMENT += abs_top_builddir=$(abs_top_builddir)
TESTS_ENVIRONMENT += CONFIGURED_DATADIR=$(datadir)
TESTS_ENVIRONMENT += CONFIGURED_LIBDIR=$(libdir)
TESTS_ENVIRONMENT += CONFIGURED_BINDIR=$(bindir)
# 225_install-manifest.test folds these back out of the staged paths.
TESTS_ENVIRONMENT += CONFIGURED_MANDIR=$(mandir)
TESTS_ENVIRONMENT += CONFIGURED_DOCDIR=$(docdir)
TESTS_ENVIRONMENT += CONFIGURED_HTMLDIR=$(htmldir)
TESTS_ENVIRONMENT += ORIGIN_RPATH=$(ORIGIN_RPATH)
# 222_pkgconfig-consumer.test asserts the staged .pc against these, and both
# sides of the gate deciding whether it hands a consumer an rpath.
TESTS_ENVIRONMENT += CONFIGURED_PREFIX=$(prefix)
TESTS_ENVIRONMENT += CONFIGURED_INCLUDEDIR=$(includedir)
TESTS_ENVIRONMENT += PKGCONFIG_RPATH=$(PKGCONFIG_RPATH)
TESTS_ENVIRONMENT += MAKE="$(MAKE)"
# 205_install-headers.test compiles the installed headers with the build's compiler.
# CPPFLAGS carries the dependency include paths a consumer needs too: brew's
# brew's openssl is keg-only, so htsbasenet.h finds <openssl/ssl.h> only through it.
TESTS_ENVIRONMENT += CC="$(CC)"
TESTS_ENVIRONMENT += TEST_CPPFLAGS="$(CPPFLAGS)"
# 331_fortify-source.test rebuilds a fortifiable probe with the build's own
# flags, and skips when configure found no usable level.
TESTS_ENVIRONMENT += TEST_CFLAGS="$(CFLAGS)"
TESTS_ENVIRONMENT += FORTIFY_SOURCE_LEVEL=$(FORTIFY_SOURCE_LEVEL)
# 218_crash-nopie-frames.test links the crash printer, which calls dladdr().
TESTS_ENVIRONMENT += DL_LIBS="$(DL_LIBS)"
# 207_install-headers-symbols.test links a consumer probe against it; absent on a
# static-only build and named .dylib on macOS, where that ELF-only test skips.
TESTS_ENVIRONMENT += HTTRACK_SHLIB=$(abs_top_builddir)/src/$(LT_CV_OBJDIR)/libhttrack.so
TESTS_ENVIRONMENT += RENAMEFAIL_LA=$(abs_builddir)/librenamefail.la
TESTS_ENVIRONMENT += RENAMEFAIL_LIB=$(abs_builddir)/$(LT_CV_OBJDIR)/librenamefail.so
TESTS_ENVIRONMENT += UNLINKFAIL_LA=$(abs_builddir)/libunlinkfail.la
TESTS_ENVIRONMENT += UNLINKFAIL_LIB=$(abs_builddir)/$(LT_CV_OBJDIR)/libunlinkfail.so
TESTS_ENVIRONMENT += THREADATTRFAIL_LA=$(abs_builddir)/libthreadattrfail.la
TESTS_ENVIRONMENT += THREADATTRFAIL_LIB=$(abs_builddir)/$(LT_CV_OBJDIR)/libthreadattrfail.so
TESTS_ENVIRONMENT += NOBACKTRACE_LA=$(abs_builddir)/libnobacktrace.la
TESTS_ENVIRONMENT += NOBACKTRACE_LIB=$(abs_builddir)/$(LT_CV_OBJDIR)/libnobacktrace.so
TESTS_ENVIRONMENT += ALTSTACKPROBE_LA=$(abs_builddir)/libaltstackprobe.la
TESTS_ENVIRONMENT += ALTSTACKPROBE_LIB=$(abs_builddir)/$(LT_CV_OBJDIR)/libaltstackprobe.so
TESTS_ENVIRONMENT += CLOSETRACK_LA=$(abs_builddir)/libclosetrack.la
TESTS_ENVIRONMENT += CLOSETRACK_LIB=$(abs_builddir)/$(LT_CV_OBJDIR)/libclosetrack.so
TESTS_ENVIRONMENT += FTPCANCEL_LIB=$(abs_builddir)/$(LT_CV_OBJDIR)/libftpcancel.so
TESTS_ENVIRONMENT += STRINGOOM_BIN=$(abs_builddir)/stringoom$(EXEEXT)
# 215_engine-datadir-ospath.test runs the engine from a copy, so it needs the
# real binary rather than the libtool wrapper script, plus the loader variable
# libtool picked for this platform.
TESTS_ENVIRONMENT += HTTRACK_BIN=$(abs_top_builddir)/src/$(LT_CV_OBJDIR)/httrack$(EXEEXT)
TESTS_ENVIRONMENT += HTTRACK_LIBDIR=$(abs_top_builddir)/src/$(LT_CV_OBJDIR)
TESTS_ENVIRONMENT += SHLIBPATH_VAR=$(SHLIBPATH_VAR)
# The shim directory test-timeout.sh puts on PATH under a sanitizer.
TESTS_ENVIRONMENT += HTTRACK_STDERR_WRAP_DIR=$(abs_builddir)/$(STDERR_WRAP_DIR)

# rename() interposer for 01_engine-renameover.test; -rpath is what makes
# libtool build a shared module rather than a static-only convenience library.
check_LTLIBRARIES = librenamefail.la libunlinkfail.la libthreadattrfail.la \
	libnobacktrace.la libaltstackprobe.la libclosetrack.la libftpcancel.la
librenamefail_la_SOURCES = renamefail.c
librenamefail_la_LDFLAGS = -module -avoid-version -rpath $(abs_builddir)
librenamefail_la_LIBADD = $(DL_LIBS)

# unlink() interposer for 235_local-resume-recovery.test
libunlinkfail_la_SOURCES = unlinkfail.c
libunlinkfail_la_LDFLAGS = -module -avoid-version -rpath $(abs_builddir)
libunlinkfail_la_LIBADD = $(DL_LIBS)

# pthread_create() interposer for 113_engine-threadattr-leak.test
libthreadattrfail_la_SOURCES = threadattrfail.c
libthreadattrfail_la_LDFLAGS = -module -avoid-version -rpath $(abs_builddir)
libthreadattrfail_la_LIBADD = $(DL_LIBS)

# backtrace() interposer for 143_engine-backtrace-empty.test
libnobacktrace_la_SOURCES = nobacktrace.c
libnobacktrace_la_LDFLAGS = -module -avoid-version -rpath $(abs_builddir)

# alt-stack observer for 181_altstack-honoured.test and 183_altstack-worker.test
libaltstackprobe_la_SOURCES = altstackprobe.c
libaltstackprobe_la_LDFLAGS = -module -avoid-version -rpath $(abs_builddir)
libaltstackprobe_la_LIBADD = $(DL_LIBS)

# close() observer for 253_local-ftp-close-once.test
libclosetrack_la_SOURCES = closetrack.c
libclosetrack_la_LDFLAGS = -module -avoid-version -rpath $(abs_builddir)
libclosetrack_la_LIBADD = $(DL_LIBS)

# per-link cancel driver for 253_local-ftp-close-once.test
libftpcancel_la_SOURCES = ftpcancel.c
libftpcancel_la_LDFLAGS = -module -avoid-version -rpath $(abs_builddir)
libftpcancel_la_LIBADD = $(DL_LIBS) -lpthread

# realloc stub for 152_engine-string-oom.test; links nothing, so it stays a
# plain binary rather than a libtool wrapper script.
check_PROGRAMS = stringoom
stringoom_SOURCES = stringoom.c
stringoom_CPPFLAGS = -I$(top_srcdir)/src

# PATH shims the sanitizer legs run the engine through, so a UBSan report only
# its stderr carries survives a test that redirects it. Built always, prepended
# only when HTTRACK_STDERR_CAPTURE_DIR names a directory (test-timeout.sh).
STDERR_WRAP_DIR = stderr-wrap
check_SCRIPTS = $(STDERR_WRAP_DIR)/httrack $(STDERR_WRAP_DIR)/htsserver \
	$(STDERR_WRAP_DIR)/proxytrack

$(check_SCRIPTS): Makefile $(srcdir)/sanitizer-stderr-wrap.sh
	$(AM_V_GEN)$(MKDIR_P) $(STDERR_WRAP_DIR) && \
	  prog=`echo $@ | sed 's|.*/||'` && \
	  { echo '#!/bin/sh'; \
	    echo "exec $(BASH_SHELL) $(abs_srcdir)/sanitizer-stderr-wrap.sh $(abs_top_builddir)/src/$$prog$(EXEEXT) \"\$$@\""; \
	  } >$@ && chmod +x $@

clean-local:
	-rm -rf $(STDERR_WRAP_DIR)

TEST_EXTENSIONS = .test
# Run each .test through bash instead of execve()ing it. This lets "make check"
# work when the source tree sits on a noexec filesystem (the driver would
# otherwise fail with "Permission denied"), and removes any reliance on the
# scripts' executable bit. The scripts are #!/bin/bash and use bash features.
#
# test-timeout.sh wraps that in a wall-clock budget: a wedged test must fail
# named, with diagnostics, rather than hang the job until CI cancels it -- a
# cancelled step keeps neither its log nor its artifacts, so today a hang tells
# us nothing at all. HTTRACK_TEST_TIMEOUT overrides the budget; 0 disables it.
TEST_LOG_COMPILER = $(BASH_SHELL) $(srcdir)/test-timeout.sh
CLEANFILES = check-network_sh.cache 62_lang-untranslated.counts.new
