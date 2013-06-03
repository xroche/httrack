EXTRA_DIST = $(TESTS) crawl-test.sh run-all-tests.sh

TESTS_ENVIRONMENT =
TESTS_ENVIRONMENT += $(SHLIBPATH_VAR)="$(top_builddir)/src/$(LT_CV_OBJDIR)$${$(SHLIBPATH_VAR):+:}$$$(SHLIBPATH_VAR)"
TESTS_ENVIRONMENT += PATH=$(top_builddir)/src/$(LT_CV_OBJDIR):$$PATH

TESTS = *.test
