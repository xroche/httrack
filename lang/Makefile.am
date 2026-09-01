
langdir = $(datadir)/httrack/lang
# Glob against $(srcdir): a bare "*.txt" is resolved against the build dir and
# stays unexpanded (breaking "make") in an out-of-tree build.
lang_DATA = $(srcdir)/*.txt
langrootdir = $(datadir)/httrack
langroot_DATA = ../lang.def ../lang.indexes

EXTRA_DIST = $(lang_DATA) $(langroot_DATA)

# webhttrack and htsserver reach the served UI through $(langrootdir)/html, so the
# link is relative: an absolute $(htmldir) dangles under DESTDIR or relocation (#885).
# Making $(htmldir) first is what decides whether a link is wanted at all. Where it
# is the link path, or under it, a real directory lands there and the link would
# point at itself. The filesystem answers that, where comparing the two spellings
# does not: configure keeps a "//" or a "/./" the kernel folds away.
install-data-hook:
	@rel='$(htmldir)'; \
	case '$(htmldir)' in \
		'$(datadir)'/*) rel="../$${rel#$(datadir)/}" ;; \
	esac; \
	link='$(DESTDIR)$(langrootdir)/html'; \
	if test -L "$$link"; then rm -f "$$link" || exit 1; fi; \
	$(MKDIR_P) '$(DESTDIR)$(htmldir)' || exit 1; \
	if test -e "$$link"; then \
		echo "$$link exists and is not a symlink, leaving it alone" >&2; \
	else \
		echo " $(LN_S) $$rel $$link"; \
		$(LN_S) "$$rel" "$$link"; \
	fi

uninstall-hook:
	@link='$(DESTDIR)$(langrootdir)/html'; \
	if test -L "$$link"; then rm -f "$$link"; fi

#dist-hook:
