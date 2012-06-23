#SUBDIRS = swf

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
	htsbauth.h \
	htsdefines.h

INCLUDES = \
	@DEFAULT_CFLAGS@ \
	@THREADS_CFLAGS@ \
	@V6_FLAG@ \
	@LFS_FLAG@ \
	-DPREFIX=\""$(prefix)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DLIBDIR=\""$(libdir)"\"

bin_PROGRAMS = proxytrack httrack htsserver

httrack_LDADD = $(THREADS_LIBS) libhttrack.la
htsserver_LDADD = $(THREADS_LIBS) $(SOCKET_LIBS) libhttrack.la
proxytrack_LDADD = $(THREADS_LIBS) $(SOCKET_LIBS)
proxytrack_CFLAGS = $(AM_CFLAGS) -DNO_MALLOCT

lib_LTLIBRARIES = libhttrack.la libhtsjava.la

htsserver_SOURCES = htsserver.c htsserver.h htsweb.c htsweb.h
proxytrack_SOURCES = proxy/main.c \
	proxy/proxytrack.c proxy/store.c \
	htsinthash.c htsmd5.c md5.c \
	minizip/ioapi.c minizip/mztools.c minizip/unzip.c minizip/zip.c

whttrackrundir = $(bindir)
whttrackrun_SCRIPTS = webhttrack

libhttrack_la_SOURCES =  htscore.c htsparse.c htsback.c htscache.c \
	htscatchurl.c htsfilters.c htsftp.c htshash.c htsinthash.c \
	htshelp.c htslib.c htscoremain.c \
	htsname.c htsrobots.c htstools.c htswizard.c \
	htsalias.c htsthread.c htsindex.c htsbauth.c \
	htsmd5.c htszlib.c htswrap.c \
	htsmodules.c htscharset.c \
	md5.c \
	htsmms.c \
	minizip/ioapi.c minizip/mztools.c minizip/unzip.c minizip/zip.c \
	mmsrip/error.c mmsrip/mms.c \
	hts-indextmpl.h htsalias.h htsback.h htsbase.h \
	htsbasenet.h htsbauth.h htscache.h htscatchurl.h  \
	htsconfig.h htscore.h htsparse.h htscoremain.h htsdefines.h  \
	htsfilters.h htsftp.h htsglobal.h htshash.h htsinthash.h \
	htshelp.h htsindex.h htslib.h htsmd5.h \
	htsmodules.h htsname.h htsnet.h \
	htsopt.h htsrobots.h htsthread.h  \
	htstools.h htswizard.h htswrap.h htszlib.h  \
	htsstrings.h httrack-library.h htscharset.h \
	md5.h \
	htsmms.h \
	minizip/crypt.h minizip/ioapi.h minizip/mztools.h minizip/unzip.h minizip/zip.h \
	mmsrip/error.h mmsrip/mms.h
libhttrack_la_LIBADD = $(THREADS_LIBS) $(ZLIB_LIBS) $(DL_LIBS) $(SOCKET_LIBS)
libhttrack_la_LDFLAGS = -version-info $(VERSION_INFO)

libhtsjava_la_SOURCES = htsjava.c htsjava.h
libhtsjava_la_LIBADD = $(THREADS_LIBS) $(DL_LIBS) libhttrack.la
libhtsjava_la_LDFLAGS = -version-info $(VERSION_INFO)

EXTRA_DIST = httrack.h webhttrack \
		minizip/ChangeLogUnzip \
		minizip/iowin32.c \
		minizip/iowin32.h \
		mmsrip/common.h \
		mmsrip/main.c \
		mmsrip/COPYING \
		mmsrip/AUTHORS \
		mmsrip/NEWS \
		mmsrip/README \
		mmsrip/ChangeLog \
		proxy/AUTHORS \
		proxy/COPYING \
		proxy/changelog.txt \
		proxy/proxystrings.h \
		proxy/proxytrack.h \
		proxy/store.h \
		*.dsw *.dsp *.vcproj
