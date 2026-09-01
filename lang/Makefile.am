
langdir = $(datadir)/httrack/lang
# Glob against $(srcdir): a bare "*.txt" is resolved against the build dir and
# stays unexpanded (breaking "make") in an out-of-tree build.
lang_DATA = $(srcdir)/*.txt
langrootdir = $(datadir)/httrack
langroot_DATA = ../lang.def ../lang.indexes

EXTRA_DIST = $(lang_DATA) $(langroot_DATA)

# webhttrack and htsserver reach the served UI through $(langrootdir)/html, so the
# link is relative: an absolute $(htmldir) dangles under DESTDIR or relocation (#885).
# Making $(htmldir) first decides whether a link is wanted. Where it is the link
# path, or under it, a real directory lands there and the branch below leaves it
# alone.
#
# Both paths are folded before they are compared, because a user's "//", "/." or
# trailing "/" survives into the Makefile and the kernel ignores all three. An
# unfolded pair fails the prefix test and leaves the link absolute, which is the
# dangling link #885 fixed. A trailing slash is left for $${data%/} to take, which
# also turns a $(datadir) of "/" into the empty prefix the compare needs.
install-data-hook:
	@fold() { printf '%s\n' "$$1" | \
		sed -e 's|//*|/|g' -e ':a' -e 's|/\./|/|g' -e 'ta' -e 's|/\.$$|/|'; }; \
	html=`fold '$(htmldir)'`; \
	data=`fold '$(datadir)'`; \
	root=$${data%/}; \
	rel="$$html"; \
	case "$$html" in \
		"$$root"/*) rel="../$${html#"$$root"/}" ;; \
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
