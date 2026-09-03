
langdir = $(datadir)/httrack/lang
# Glob against $(srcdir): a bare "*.txt" is resolved against the build dir and
# stays unexpanded (breaking "make") in an out-of-tree build.
lang_DATA = $(srcdir)/*.txt
langrootdir = $(datadir)/httrack
langroot_DATA = ../lang.def ../lang.indexes
# htsserver's document root. html/ installs the served UI below it as real files,
# and the panes reach the documentation through the "doc" link made here.
langhtmldir = $(langrootdir)/html

EXTRA_DIST = $(lang_DATA) $(langroot_DATA)

# The link is relative: an absolute $(docdir) dangles under DESTDIR or relocation
# (#885). A build that strips the documentation only dangles it, which htsserver
# answers with a 404. Making $(docdir) first decides whether a link is wanted:
# where it is the link path, or under it, a real directory lands there and the
# branch below leaves it alone. A $(docdir) holding the link path is refused
# instead, because a link into its own ancestor would serve /doc/doc/doc/...
#
# Both paths are folded before they are compared, because a user's "//", "/." or
# trailing "/" survives into the Makefile and the kernel ignores all three. An
# unfolded pair fails the prefix test and leaves the link absolute, which is the
# dangling link #885 fixed. A trailing slash is left for $${data%/} to take, which
# also turns a $(datadir) of "/" into the empty prefix the compare needs.
#
# An install over a tree older than this one finds the whole served root a
# symlink into $(docdir), which is dropped so html/ writes real files. lang comes
# before html in SUBDIRS, so that happens first.
install-data-hook:
	@fold() { printf '%s\n' "$$1" | \
		sed -e 's|//*|/|g' -e ':a' -e 's|/\./|/|g' -e 'ta' -e 's|/\.$$|/|'; }; \
	doc=`fold '$(docdir)'`; \
	data=`fold '$(datadir)'`; \
	root=$${data%/}; \
	rel="$$doc"; \
	case "$$doc" in \
		"$$root"/*) rel="../../$${doc#"$$root"/}" ;; \
	esac; \
	served='$(DESTDIR)$(langhtmldir)'; \
	if test -L "$$served"; then rm -f "$$served" || exit 1; fi; \
	$(MKDIR_P) "$$served" || exit 1; \
	link="$$served/doc"; \
	if test -L "$$link"; then rm -f "$$link" || exit 1; fi; \
	$(MKDIR_P) '$(DESTDIR)$(docdir)' || exit 1; \
	case `fold '$(langhtmldir)/doc'` in \
		"$${doc%/}" | "$${doc%/}"/*) \
			echo "$$link sits inside $(docdir), so no link is made" >&2 ;; \
		*) \
			if test -e "$$link"; then \
				echo "$$link exists and is not a symlink, leaving it alone" >&2; \
			else \
				echo " $(LN_S) $$rel $$link"; \
				$(LN_S) "$$rel" "$$link"; \
			fi ;; \
	esac

uninstall-hook:
	@link='$(DESTDIR)$(langhtmldir)/doc'; \
	if test -L "$$link"; then rm -f "$$link"; fi

#dist-hook:
