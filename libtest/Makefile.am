
exemplesdir = $(datadir)/httrack/libtest
exemples_DATA = *.c *.h *.txt
EXTRA_DIST = $(exemples_DATA)

INCLUDES = \
	@DEFAULT_CFLAGS@ \
	@THREADS_CFLAGS@ \
	@V6_FLAG@ \
	@LFS_FLAG@ \
	-DPREFIX=\""$(prefix)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DLIBDIR=\""$(libdir)"\"

AM_CPPFLAGS = -I../src
AM_LDFLAGS = -L../src

# Examples
libbaselinks_la_SOURCES =  callbacks-example-baselinks.c
libbaselinks_la_LIBADD = $(THREADS_LIBS) $(SOCKET_LIBS) -lhttrack
libbaselinks_la_LDFLAGS = -version-info 1:0:0
libchangecontent_la_SOURCES =  callbacks-example-changecontent.c
libchangecontent_la_LIBADD = $(THREADS_LIBS) $(SOCKET_LIBS) -lhttrack
libchangecontent_la_LDFLAGS = -version-info 1:0:0
libcontentfilter_la_SOURCES =  callbacks-example-contentfilter.c
libcontentfilter_la_LIBADD = $(THREADS_LIBS) $(SOCKET_LIBS) -lhttrack
libcontentfilter_la_LDFLAGS = -version-info 1:0:0
libdisplayheader_la_SOURCES =  callbacks-example-displayheader.c
libdisplayheader_la_LIBADD = $(THREADS_LIBS) $(SOCKET_LIBS) -lhttrack
libdisplayheader_la_LDFLAGS = -version-info 1:0:0
libfilename2_la_SOURCES =  callbacks-example-filename2.c
libfilename2_la_LIBADD = $(THREADS_LIBS) $(SOCKET_LIBS) -lhttrack
libfilename2_la_LDFLAGS = -version-info 1:0:0
libfilename_la_SOURCES =  callbacks-example-filename.c
libfilename_la_LIBADD = $(THREADS_LIBS) $(SOCKET_LIBS) -lhttrack
libfilename_la_LDFLAGS = -version-info 1:0:0
libfilenameiisbug_la_SOURCES =  callbacks-example-filenameiisbug.c
libfilenameiisbug_la_LIBADD = $(THREADS_LIBS) $(SOCKET_LIBS) -lhttrack
libfilenameiisbug_la_LDFLAGS = -version-info 1:0:0
liblistlinks_la_SOURCES =  callbacks-example-listlinks.c
liblistlinks_la_LIBADD = $(THREADS_LIBS) $(SOCKET_LIBS) -lhttrack
liblistlinks_la_LDFLAGS = -version-info 1:0:0
liblog_la_SOURCES =  callbacks-example-log.c
liblog_la_LIBADD = $(THREADS_LIBS) $(SOCKET_LIBS) -lhttrack
liblog_la_LDFLAGS = -version-info 1:0:0
libsimple_la_SOURCES =  callbacks-example-simple.c
libsimple_la_LIBADD = $(THREADS_LIBS) $(SOCKET_LIBS) -lhttrack
libsimple_la_LDFLAGS = -version-info 1:0:0
pkglib_LTLIBRARIES = libbaselinks.la libchangecontent.la libcontentfilter.la libdisplayheader.la libfilename2.la libfilename.la libfilenameiisbug.la liblistlinks.la liblog.la libsimple.la
