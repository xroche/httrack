EXTRA_DIST = $(TESTS) crawl-test.sh run-all-tests.sh check-network.sh

TESTS_ENVIRONMENT =
TESTS_ENVIRONMENT += PATH=$(top_builddir)/src$(PATH_SEPARATOR)$$PATH
# note: libtool should handle that
### TESTS_ENVIRONMENT += $(SHLIBPATH_VAR)="$(top_builddir)/src/$(LT_CV_OBJDIR)$${$(SHLIBPATH_VAR):+$(PATH_SEPARATOR)}$$$(SHLIBPATH_VAR)"
TESTS_ENVIRONMENT += ONLINE_UNIT_TESTS=$(ONLINE_UNIT_TESTS)
TESTS_ENVIRONMENT += HTTPS_SUPPORT=$(HTTPS_SUPPORT)

TEST_EXTENSIONS = .test
TESTS = 00_runnable.test 01_engine-charset.test 01_engine-entities.test 01_engine-hashtable.test 01_engine-idna.test 01_engine-simplify.test 10_crawl-simple.test 11_crawl-cookies.test 11_crawl-idna.test 11_crawl-international.test 11_crawl-longurl.test 11_crawl-parsing.test 12_crawl_https.test 

CLEANFILES = check-network_sh.cache
