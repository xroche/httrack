
langdir = $(datadir)/httrack/lang
# Glob against $(srcdir): a bare "*.txt" is resolved against the build dir and
# stays unexpanded (breaking "make") in an out-of-tree build.
lang_DATA = $(srcdir)/*.txt
langrootdir = $(datadir)/httrack
langroot_DATA = ../lang.def ../lang.indexes

EXTRA_DIST = $(lang_DATA) $(langroot_DATA)

# webhttrack and htsserver reach the served UI through $(langrootdir)/html, so the
# link is relative: an absolute $(htmldir) dangles under DESTDIR or relocation (#885).
# An htmldir at or under that path would make the link contain itself, an ELOOP the
# next install rule dies on, so it is skipped there.
install-data-hook:
	@rel='$(htmldir)'; \
	case '$(htmldir)' in \
		'$(datadir)'/*) rel="../$${rel#$(datadir)/}" ;; \
	esac; \
	link='$(DESTDIR)$(langrootdir)/html'; \
	case '$(htmldir)/' in \
		'$(langrootdir)/html/'*) \
			echo "$(htmldir) installs at or under $(langrootdir)/html, so no link" >&2; \
			exit 0 ;; \
	esac; \
	if test -L "$$link"; then rm -f "$$link" || exit 1; fi; \
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
