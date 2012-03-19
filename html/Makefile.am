
HelpHtmlrootdir = $(datadir)/doc/httrack
HelpHtmldir = $(datadir)/doc/httrack/html
HelpHtmlimgdir = $(datadir)/doc/httrack/html/img
HelpHtmldivdir = $(datadir)/doc/httrack/html/div
HelpHtmlimagesdir = $(datadir)/doc/httrack/html/images
HelpHtmlTxtdir = $(datadir)/doc/httrack/html
WebHtmldir = $(datadir)/doc/httrack/html/server
WebHtmlimagesdir = $(datadir)/doc/httrack/html/server/images
WebHtmlsfxdir = $(datadir)/doc/httrack/html/server/sfx
WebPixmapdir = $(datadir)/pixmaps
WebGPixmapdir = $(datadir)/httrack/icons
VFolderEntrydir = $(prefix)/share/applications

HelpHtmlroot_DATA = ../httrack-doc.html ../history.txt
HelpHtml_DATA = *.html
HelpHtmldiv_DATA = div/search.sh
HelpHtmlimg_DATA  = img/*
HelpHtmlimages_DATA = images/*
HelpHtmlTxt_DATA = ../greetings.txt ../history.txt ../license.txt
WebHtml_DATA = server/*.html
WebHtmlimages_DATA = server/images/*
WebHtmlsfx_DATA = server/sfx/*
# note: converted & normalized by
# ico2xpm favicon.ico -o httrack.xpm
# mogrify -format xpm -map /usr/share/doc/menu/examples/cmap.xpm httrack.xpm
WebPixmap_DATA = server/div/httrack.xpm
WebGPixmap_DATA = server/div/webhttrack.xpm
VFolderEntry_DATA = server/div/*.desktop

EXTRA_DIST = $(HelpHtml_DATA) $(HelpHtmlimg_DATA) $(HelpHtmlimages_DATA) \
	$(HelpHtmldiv_DATA) $(WebHtml_DATA) $(WebHtmlimages_DATA) $(WebHtmlsfx_DATA) \
	$(WebPixmap_DATA) $(WebGPixmap_DATA) $(VFolderEntry_DATA) \
	httrack.css

install-data-hook:
	if test ! -L $(DESTDIR)$(prefix)/share/httrack/html ; then \
		( cd $(DESTDIR)$(prefix)/share/httrack \
			&& mv -f ../doc/httrack/html html \
			&& cd ../doc/httrack/ \
			&& $(LN_S) ../../httrack/html html \
		) \
	fi
