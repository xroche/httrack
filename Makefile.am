
SUBDIRS = src man m4 libtest templates lang html tests fuzz

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = INSTALL.Linux \
		gpl-fr.txt license.txt greetings.txt history.txt \
		httrack-doc.html lang.def README.md tools/mkdeb.sh \
		tools/macos-app.sh tools/Info.plist.in \
		bootstrap build.sh

# Build the signed Debian packages from a clean source export. Pass the signing
# key and other options through DEB_FLAGS, e.g.:
#   make deb DEB_FLAGS="--key BB71C7E6CB1AD8FAF53FE42A60C3AA7180598EFB"
# See tools/mkdeb.sh --help for all options. Not $(SHELL): that is /bin/sh, which
# on the Debian boxes you build packages on is dash, and mkdeb.sh is bash (#891).
DEB_FLAGS =
deb:
	$(BASH_SHELL) $(top_srcdir)/tools/mkdeb.sh $(DEB_FLAGS)
.PHONY: deb

# Assemble the macOS application bundle from an installed prefix, e.g.
#   make macos-app APP_FLAGS="--prefix /tmp/inst --out /tmp"
APP_FLAGS =
macos-app:
	$(SHELL) $(top_srcdir)/tools/macos-app.sh --plist tools/Info.plist $(APP_FLAGS)
.PHONY: macos-app
