
SUBDIRS = src man m4 libtest templates lang html tests

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = INSTALL.Linux \
		gpl-fr.txt license.txt greetings.txt history.txt \
		httrack-doc.html lang.def README.md tools/mkdeb.sh

# Build the signed Debian packages from a clean source export. Pass the signing
# key and other options through DEB_FLAGS, e.g.:
#   make deb DEB_FLAGS="--key BB71C7E6CB1AD8FAF53FE42A60C3AA7180598EFB"
# See tools/mkdeb.sh --help for all options.
DEB_FLAGS =
deb:
	$(SHELL) $(top_srcdir)/tools/mkdeb.sh $(DEB_FLAGS)
.PHONY: deb
