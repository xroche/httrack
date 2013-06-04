EXTRA_DIST = $(TESTS) crawl-test.sh run-all-tests.sh

TESTS_ENVIRONMENT =
TESTS_ENVIRONMENT += $(SHLIBPATH_VAR)="$(top_builddir)/src/$(LT_CV_OBJDIR)$${$(SHLIBPATH_VAR):+:}$$$(SHLIBPATH_VAR)"
TESTS_ENVIRONMENT += PATH=$(top_builddir)/src/$(LT_CV_OBJDIR):$$PATH
TESTS_ENVIRONMENT += ONLINE_UNIT_TESTS=$(ONLINE_UNIT_TESTS)

TEST_EXTENSIONS = .test
TESTS = 00_runnable.test 01_engine-charset.test 01_engine-entities.test 01_engine-idna.test 01_engine-simplify.test 10_crawl-simple.test 11_crawl-idna.test 11_crawl-international.test 11_crawl-parsing.test 
