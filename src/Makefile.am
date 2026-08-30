DevIncludesdir = $(includedir)/httrack
DevIncludes_DATA = \
	httrack-library.h \
	htsglobal.h \
	htsopt.h \
	htswrap.h \
	htsconfig.h \
	../config.h \
	htsmodules.h \
	htsbasenet.h \
	htsnet.h \
	htsbauth.h \
	htsdefines.h \
	htsstrings.h \
	htsarrays.h \
	htssafe.h

# What hands a consumer the include path and the rpath the @rpath id needs (#978).
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libhttrack.pc

AM_CPPFLAGS = \
	@DEFAULT_CFLAGS@ \
	@THREADS_CFLAGS@ \
	@V6_FLAG@ \
	@LFS_FLAG@ \
	-DPREFIX=\""$(prefix)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DBINDIR=\""$(bindir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-I"$(abs_srcdir)" \
	-I"$(abs_srcdir)/coucal"

AM_LDFLAGS = \
	@DEFAULT_LDFLAGS@

bin_PROGRAMS = proxytrack httrack htsserver

httrack_SOURCES = httrack.c htsbacktrace.c htsbacktrace.h
# $(DL_LIBS): dladdr() in the crash handler, still in libdl on pre-2.34 glibc.
httrack_LDADD = $(THREADS_LIBS) $(DL_LIBS) libhttrack.la
htsserver_LDADD = $(THREADS_LIBS) $(SOCKET_LIBS) libhttrack.la
proxytrack_LDADD = $(THREADS_LIBS) $(SOCKET_LIBS)

httrack_CFLAGS = $(AM_CFLAGS) $(CFLAGS_PIE)
# -DHTS_NO_LIBHTTRACK: see htsnet.h's HTSNET_API guard.
proxytrack_CFLAGS = $(AM_CFLAGS) $(CFLAGS_PIE) -DZLIB_CONST -DHTS_INTHASH_USES_MD5 -DHTS_NO_LIBHTTRACK
htsserver_CFLAGS = $(AM_CFLAGS) $(CFLAGS_PIE) -DZLIB_CONST -DHTS_INTHASH_USES_MD5

# @RPATH_ORIGIN_LDFLAGS@ makes a copied tree find libhttrack next to it (#906),
# pairing on Darwin with the dylib's @rpath id (#978); proxytrack needs none of it.
httrack_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_PIE) @RPATH_ORIGIN_LDFLAGS@
proxytrack_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_PIE)
htsserver_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_PIE) @RPATH_ORIGIN_LDFLAGS@

lib_LTLIBRARIES = libhttrack.la

htsserver_SOURCES = htsserver.c htsserver.h htsweb.c htsweb.h htsstats.h \
	htsrandom.c htsrandom.h \
	htscmdline.c htscmdline.h \
	htsurlport.c htsurlport.h htsescape.c htsescape.h
proxytrack_SOURCES = proxy/main.c \
	proxy/proxytrack.c proxy/store.c \
	htsurlport.c htsurlport.h htsnet.c \
	coucal/coucal.c htsmd5.c md5.c \
	minizip/ioapi.c minizip/mztools.c minizip/unzip.c minizip/zip.c

whttrackrundir = $(bindir)
whttrackrun_SCRIPTS = webhttrack

# $(datadir) only expands at make time, so substitute here rather than through
# AC_CONFIG_FILES (#887).
webhttrack: webhttrack.in Makefile
	$(AM_V_GEN)$(SED) -e 's|@datadir[@]|$(datadir)|g' $(srcdir)/webhttrack.in >$@.tmp \
		&& chmod +x $@.tmp && mv -f $@.tmp $@
CLEANFILES = webhttrack

libhttrack_la_SOURCES =  htscore.c htsparse.c htsback.c htscache.c \
	htscache_selftest.c htsdns_selftest.c htsselftest.c \
	htscatchurl.c htsfilters.c htsftp.c htshash.c coucal/coucal.c \
	htscmdline.c htshelp.c htslib.c htsurlport.c htscoremain.c \
	htsname.c htsrobots.c htstools.c htsrandom.c htswizard.c \
	htsalias.c htsthread.c htsindex.c htsbauth.c \
	htscrashtest.c \
	htsmd5.c htsnet.c htscodec.c htswarc.c htschanges.c htssinglefile.c htssitemap.c htsproxy.c htszlib.c htswrap.c htsconcat.c \
	htsmodules.c htscharset.c punycode.c htsencoding.c htsescape.c htssniff.c \
	md5.c \
	minizip/ioapi.c minizip/mztools.c minizip/unzip.c minizip/zip.c \
	hts-indextmpl.h htsalias.h htsback.h htsbase.h htssafe.h \
	htsbasenet.h htsbauth.h htscache.h htscache_selftest.h htsdns_selftest.h htsselftest.h htscatchurl.h  \
	htscmdline.h htsconfig.h htscore.h htsparse.h htscoremain.h htsdefines.h  \
	htsfilters.h htsftp.h htsglobal.h htshash.h coucal/coucal.h \
	htshelp.h htsindex.h htsio.h htslib.h htsurlport.h htsmd5.h \
	htsmodules.h htsname.h htsnet.h htssniff.h \
	htsopt.h htsrobots.h htsthread.h  \
	htscrashtest.h htstools.h htsrandom.h htswizard.h htswrap.h htscodec.h htswarc.h htschanges.h htssinglefile.h htssitemap.h htsproxy.h htszlib.h  \
	htsstrings.h htsarrays.h htsarena.h httrack-library.h \
	htscharset.h punycode.h htsencoding.h htsescape.h \
	htsentities.h htsentities.sh htsbasiccharsets.sh htscodepages.h \
	winprofile-keys.h \
	md5.h coucal/murmurhash3.h \
	minizip/crypt.h minizip/ioapi.h minizip/mztools.h minizip/unzip.h minizip/zip.h
libhttrack_la_LIBADD = $(THREADS_LIBS) $(ZLIB_LIBS) $(BROTLI_LIBS) $(ZSTD_LIBS) $(OPENSSL_LIBS) $(DL_LIBS) $(SOCKET_LIBS) $(ICONV_LIBS)
libhttrack_la_CFLAGS = $(AM_CFLAGS) -DLIBHTTRACK_EXPORTS -DZLIB_CONST
libhttrack_la_LDFLAGS = $(AM_LDFLAGS) -version-info $(VERSION_INFO)

EXTRA_DIST = httrack.h htsstats.h webhttrack.in \
		version.rc \
		libhttrack.rc \
		httrack.rc \
		coucal/murmurhash3.h.diff \
		coucal/murmurhash3.h.orig \
		minizip/iowin32.c \
		minizip/iowin32.h \
		minizip/iowin32.c.diff \
		minizip/iowin32.c.orig \
		minizip/ioapi.c.diff \
		minizip/ioapi.h.diff \
		minizip/zip.c.diff \
		minizip/zip.h.diff \
		minizip/mztools.c.diff \
		minizip/ioapi.c.orig \
		minizip/ioapi.h.orig \
		minizip/mztools.c.orig \
		minizip/zip.c.orig \
		minizip/zip.h.orig \
		minizip/MiniZip64_Changes.txt \
		minizip/MiniZip64_info.txt \
		proxy/AUTHORS \
		proxy/COPYING \
		proxy/changelog.txt \
		proxy/proxystrings.h \
		proxy/proxytrack.h \
		proxy/store.h \
		coucal/LICENSE \
		coucal/Makefile \
		coucal/README.md \
		coucal/sample.c \
		coucal/tests.c \
		vcpkg.json \
		httrack.vcxproj \
		libhttrack.vcxproj \
		proxytrack.vcxproj \
		webhttrack.vcxproj
