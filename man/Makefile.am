
man_MANS = httrack.1 webhttrack.1 htsserver.1 proxytrack.1

MAN_TMPLS = footer.tmpl httrack.tmpl webhttrack.tmpl htsserver.tmpl \
	proxytrack.tmpl

EXTRA_DIST = $(man_MANS) makeman.sh $(MAN_TMPLS)

# Regenerate every page from its program's --help and its template (httrack.1
# also reads the top-level README). Run by hand after changing options or help
# text:
#   make -C man regen-man
# The generated pages are committed; this target only refreshes them. Honors
# SOURCE_DATE_EPOCH for a reproducible date.
regen-man: makeman.sh $(MAN_TMPLS) $(top_builddir)/src/httrack$(EXEEXT) \
		$(top_builddir)/src/htsserver$(EXEEXT) \
		$(top_builddir)/src/proxytrack$(EXEEXT) $(top_builddir)/src/webhttrack
	for p in httrack htsserver proxytrack; do \
		README='$(top_srcdir)/README' $(SHELL) $(srcdir)/makeman.sh -p $$p \
			"$(top_builddir)/src/$$p$(EXEEXT)" > $(srcdir)/$$p.1 || exit 1; \
	done
	$(SHELL) $(srcdir)/makeman.sh -p webhttrack \
		'$(top_builddir)/src/webhttrack' > $(srcdir)/webhttrack.1
.PHONY: regen-man

# Render html/httrack.man.html from httrack.1. Needs the groff html device
# (Debian: full "groff" package, not "groff-base"). Run by hand: make -C man regen-man-html
# Strip groff's version-stamp and creation-date comments so the committed file
# doesn't churn across groff versions or rebuilds; the ci man-page-sync guard
# only requires this file to change whenever httrack.1 does.
regen-man-html: httrack.1
	groff -t -man -Thtml $(srcdir)/httrack.1 \
		| sed -e 's/groff version [0-9][0-9.]*/groff/' -e '/^<!-- CreationDate:/d' \
		> $(top_srcdir)/html/httrack.man.html
.PHONY: regen-man-html
