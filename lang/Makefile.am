
langdir = $(datadir)/httrack/lang
# Glob against $(srcdir): a bare "*.txt" is resolved against the build dir and
# stays unexpanded (breaking "make") in an out-of-tree build.
lang_DATA = $(srcdir)/*.txt
langrootdir = $(datadir)/httrack
langroot_DATA = ../lang.def ../lang.indexes
# htsserver's document root, where html/ installs the served UI.
langhtmldir = $(langrootdir)/html

EXTRA_DIST = $(lang_DATA) $(langroot_DATA)

# The link is relative, because an absolute $(docdir) dangles under DESTDIR or
# relocation (#885). Stripping the documentation only dangles it, and htsserver
# answers a dangling link with a 404. Making $(docdir) first leaves a real
# directory at the link path wherever the two coincide, and a $(docdir) holding
# the link path is refused outright, or htsserver would serve /doc/doc/doc/...
# Both paths are folded first, because a user's "//", "/." or trailing "/"
# survives into the Makefile and defeats the prefix test.
#
# lang precedes html in SUBDIRS, so the served-root symlink an older install
# left is dropped before html writes through it into $(docdir). Test 385 pins
# that ordering.
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
