
# man_MANS = httrack.1
man_MANS = httrack.1 webhttrack.1 htsserver.1 proxytrack.1

EXTRA_DIST = $(man_MANS) makeman.sh

# Regenerate httrack.1 from the "httrack --help" output and the top-level
# README. Run by hand after changing options or help text:
#   make -C man regen-man
# The generated page is committed; this target only refreshes it. Honors
# SOURCE_DATE_EPOCH for a reproducible date.
regen-man: makeman.sh $(top_builddir)/src/httrack$(EXEEXT)
	README='$(top_srcdir)/README' $(SHELL) $(srcdir)/makeman.sh \
		'$(top_builddir)/src/httrack$(EXEEXT)' > $(srcdir)/httrack.1
.PHONY: regen-man

# Render html/httrack.man.html from httrack.1. Needs the groff html device
# (Debian: full "groff" package, not "groff-base"). Run by hand: make -C man regen-man-html
regen-man-html: httrack.1
	groff -t -man -Thtml $(srcdir)/httrack.1 > $(top_srcdir)/html/httrack.man.html
.PHONY: regen-man-html
