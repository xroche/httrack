
SUBDIRS = src man m4 libtest templates lang html tests fuzz

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = INSTALL.Linux \
		gpl-fr.txt license.txt greetings.txt history.txt \
		httrack-doc.html lang.def README.md PACKAGING.md tools/mkdeb.sh \
		tools/macos-app.sh tools/macos-bundle.sh tools/macos-release.sh \
		tools/httrack-launcher.c \
		tools/Info.plist.in tools/HTTrack.icns \
		bootstrap build.sh doc/winprofile-ini.md \
		winprofile-keys.tsv winprofile-escapes.tsv \
		tools/gen-winprofile-keys.py \
		tools/doc-chrome.py \
		tools/check-includes.py \
		tools/ci-sanitizer-report.sh \
		tools/downstream-drift.sh

# The suite's staged installs serialise on this; a killed holder leaves it behind.
clean-local:
	-rm -f $(builddir)/.install-lock

# Build the signed Debian packages from a clean source export. Pass the signing
# key and other options through DEB_FLAGS, e.g.:
#   make deb DEB_FLAGS="--key BB71C7E6CB1AD8FAF53FE42A60C3AA7180598EFB"
# See tools/mkdeb.sh --help for all options. Not $(SHELL): mkdeb.sh is bash, and dash chokes (#891).
DEB_FLAGS =
deb:
	$(BASH_SHELL) $(top_srcdir)/tools/mkdeb.sh $(DEB_FLAGS)
.PHONY: deb

# Assemble the macOS application bundle from an installed prefix, e.g.
#   make macos-app APP_FLAGS="--prefix /tmp/inst --out /tmp"
APP_FLAGS =
macos-app:
	$(SHELL) $(top_srcdir)/tools/macos-app.sh --plist tools/Info.plist \
		--icon $(top_srcdir)/tools/HTTrack.icns $(APP_FLAGS)
.PHONY: macos-app

# Sign, notarize and pack that bundle into the DMG we publish, e.g.
#   make macos-release RELEASE_FLAGS="--app /tmp/HTTrack.app --identity ABCD1234 \
#       --notary-key AuthKey.p8 --notary-key-id KEYID --notary-issuer ISSUERID"
# See tools/macos-release.sh for the rest; .github/workflows/macos-release.yml runs it.
RELEASE_FLAGS =
macos-release:
	$(SHELL) $(top_srcdir)/tools/macos-release.sh $(RELEASE_FLAGS)
.PHONY: macos-release
