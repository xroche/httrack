#SUBDIRS = swf

DevIncludesdir = $(includedir)/httrack
DevIncludes_DATA = httrack-library.h \
	htsglobal.h \
	htsopt.h \
	htswrap.h \
	htssystem.h \
	htsconfig.h \
	../config.h \
	htsmodules.h \
	htsbasenet.h \
	htsbauth.h

INCLUDES = \
	@DEFAULT_CFLAGS@ \
	@THREADS_CFLAGS@ \
	@V6_FLAG@ \
	@LFS_FLAG@ \
	-DPREFIX=\""$(prefix)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DLIBDIR=\""$(libdir)"\"

bin_PROGRAMS = httrack htsserver

httrack_LDADD = $(THREADS_LIBS) -lhttrack
htsserver_LDADD = $(THREADS_LIBS) $(SOCKET_LIBS) -lhttrack

lib_LTLIBRARIES = libhttrack.la

htsserver_SOURCES = htsserver.c htsserver.h htsweb.c htsweb.h

whttrackrundir = $(bindir)
whttrackrun_SCRIPTS = webhttrack

libhttrack_la_SOURCES =  htscore.c htsparse.c htsback.c htscache.c \
	htscatchurl.c htsfilters.c htsftp.c htshash.c htsinthash.c \
	htshelp.c htsjava.c htslib.c htscoremain.c \
	htsname.c htsrobots.c htstools.c htswizard.c \
	htsalias.c htsthread.c htsindex.c htsbauth.c \
	htsmd5.c htszlib.c htsnostatic.c htswrap.c \
	htsmodules.c \
	md5.c \
	hts-indextmpl.h htsalias.h htsback.h htsbase.h \
	htsbasenet.h htsbauth.h htscache.h htscatchurl.h  \
	htsconfig.h htscore.h htsparse.h htscoremain.h htsdefines.h  \
	htsfilters.h htsftp.h htsglobal.h htshash.h htsinthash.h \
	htshelp.h htsindex.h htsjava.h htslib.h htsmd5.h \
	htsmodules.h htsname.h htsnet.h htsnostatic.h  \
	htsopt.h htsrobots.h htssystem.h htsthread.h  \
	htstools.h htswizard.h htswrap.h htszlib.h  \
	httrack-library.h md5.h
	
libhttrack_la_LIBADD = $(THREADS_LIBS) $(DL_LIBS) $(SOCKET_LIBS)
libhttrack_la_LDFLAGS = -version-info $(VERSION_INFO)

EXTRA_DIST = httrack.h webhttrack \
		httrack.dsp httrack.dsw \
		webhttrack.dsp webhttrack.dsw
