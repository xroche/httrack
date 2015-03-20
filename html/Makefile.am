
HelpHtmlrootdir = $(docdir)
HelpHtmldir = $(htmldir)
HelpHtmlimgdir = $(HelpHtmldir)/img
HelpHtmldivdir = $(HelpHtmldir)/div
HelpHtmlimagesdir = $(HelpHtmldir)/images
HelpHtmlTxtdir = $(HelpHtmldir)
WebHtmldir = $(HelpHtmldir)/server
WebHtmlimagesdir = $(HelpHtmldir)/server/images
WebPixmapdir = $(datadir)/pixmaps
WebIcon16x16dir = $(datadir)/icons/hicolor/16x16/apps
WebIcon32x32dir = $(datadir)/icons/hicolor/32x32/apps
WebIcon48x48dir = $(datadir)/icons/hicolor/48x48/apps
VFolderEntrydir = $(prefix)/share/applications

HelpHtmlroot_DATA = ../httrack-doc.html ../history.txt
HelpHtml_DATA = *.html
HelpHtmldiv_DATA = div/search.sh
HelpHtmlimg_DATA  = img/*
HelpHtmlimages_DATA = images/*
HelpHtmlTxt_DATA = ../greetings.txt ../history.txt ../license.txt
WebHtml_DATA = server/*.html server/*.js server/*.css
WebHtmlimages_DATA = server/images/*
# note: converted & normalized by
# ico2xpm favicon.ico -o httrack.xpm
# mogrify -format xpm -map /usr/share/doc/menu/examples/cmap.xpm httrack.xpm
WebPixmap_DATA = server/div/*.xpm
WebIcon16x16_DATA = server/div/16x16/*.png
WebIcon32x32_DATA = server/div/32x32/*.png
WebIcon48x48_DATA = server/div/48x48/*.png
VFolderEntry_DATA = server/div/*.desktop

EXTRA_DIST = $(HelpHtml_DATA) $(HelpHtmlimg_DATA) $(HelpHtmlimages_DATA) \
	$(HelpHtmldiv_DATA) $(WebHtml_DATA) $(WebHtmlimages_DATA) \
	$(WebPixmap_DATA) $(WebIcon16x16_DATA) $(WebIcon32x32_DATA) $(WebIcon48x48_DATA) \
	$(VFolderEntry_DATA) \
	httrack.css

install-data-hook:
	if test ! -L $(DESTDIR)$(prefix)/share/httrack/html ; then \
		( cd $(DESTDIR)$(prefix)/share/httrack \
			&& $(LN_S) $(htmldir) html \
		) \
	fi
