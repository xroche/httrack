
HelpHtmlrootdir = $(docdir)
HelpHtmldir = $(htmldir)
HelpHtmlimgdir = $(HelpHtmldir)/img
HelpHtmldivdir = $(HelpHtmldir)/div
HelpHtmlimagesdir = $(HelpHtmldir)/images
HelpHtmlfontsdir = $(HelpHtmldir)/fonts
WebHtmldir = $(HelpHtmldir)/server
WebHtmlimagesdir = $(HelpHtmldir)/server/images
WebPixmapdir = $(datadir)/pixmaps
WebIcon16x16dir = $(datadir)/icons/hicolor/16x16/apps
WebIcon32x32dir = $(datadir)/icons/hicolor/32x32/apps
WebIcon48x48dir = $(datadir)/icons/hicolor/48x48/apps
WebIcon64x64dir = $(datadir)/icons/hicolor/64x64/apps
WebIcon128x128dir = $(datadir)/icons/hicolor/128x128/apps
WebIcon256x256dir = $(datadir)/icons/hicolor/256x256/apps
WebIconScalabledir = $(datadir)/icons/hicolor/scalable/apps
VFolderEntrydir = $(datadir)/applications
MetaInfodir = $(datadir)/metainfo

# Wildcards are globbed against $(srcdir): a bare "*.html" is resolved against
# the build dir and stays unexpanded (breaking "make") in an out-of-tree build.
# Explicit filenames (e.g. ../history.txt, div/search.sh) resolve via VPATH and
# need no prefix.
# In $(docdir), one level above the pages, which is where their "../" points.
HelpHtmlroot_DATA = ../httrack-doc.html ../history.txt ../greetings.txt ../license.txt
HelpHtml_DATA = $(srcdir)/*.html $(srcdir)/*.js $(srcdir)/*.css
HelpHtmldiv_DATA = div/search.sh
HelpHtmlimg_DATA  = $(srcdir)/img/*
HelpHtmlimages_DATA = $(srcdir)/images/*
# doc.css reaches the woff2 by a relative URL, so it installs beside the pages.
HelpHtmlfonts_DATA = $(srcdir)/fonts/*
WebHtml_DATA = $(srcdir)/server/*.html $(srcdir)/server/*.js $(srcdir)/server/*.css
WebHtmlimages_DATA = $(srcdir)/server/images/*
# Generated from the brand master by gen/export.py. Asserted by tests/160.
# httrack.xpm duplicates the 32x32 one under the bare basename the .desktop
# Icon= key falls back to here when no icon theme is installed (#932).
WebPixmap_DATA = $(srcdir)/server/div/*.xpm
WebIcon16x16_DATA = $(srcdir)/server/div/16x16/*.png
WebIcon32x32_DATA = $(srcdir)/server/div/32x32/*.png
WebIcon48x48_DATA = $(srcdir)/server/div/48x48/*.png
WebIcon64x64_DATA = $(srcdir)/server/div/64x64/*.png
WebIcon128x128_DATA = $(srcdir)/server/div/128x128/*.png
WebIcon256x256_DATA = $(srcdir)/server/div/256x256/*.png
WebIconScalable_DATA = $(srcdir)/server/div/scalable/*.svg
VFolderEntry_DATA = $(srcdir)/server/div/*.desktop
MetaInfo_DATA = $(srcdir)/server/div/*.metainfo.xml

EXTRA_DIST = $(HelpHtml_DATA) $(HelpHtmlimg_DATA) $(HelpHtmlimages_DATA) \
	$(HelpHtmlfonts_DATA) \
	$(HelpHtmldiv_DATA) $(WebHtml_DATA) $(WebHtmlimages_DATA) \
	$(WebPixmap_DATA) $(WebIcon16x16_DATA) $(WebIcon32x32_DATA) $(WebIcon48x48_DATA) \
	$(WebIcon64x64_DATA) $(WebIcon128x128_DATA) $(WebIcon256x256_DATA) \
	$(WebIconScalable_DATA) $(VFolderEntry_DATA) $(MetaInfo_DATA)
