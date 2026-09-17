EXTRA_DIST = $(TESTS) crawl-test.sh run-all-tests.sh check-network.sh \
	catalog-test.c catalog-test.test metadata-runtime-test.c \
	metadata-runtime-test.test yotsuba-adapter-test.c \
	yotsuba-adapter.test yotsuba-live-smoke.py catalog-cli-fixture.c \
	catalog-cli-test.py catalog-cli.test fixtures/catalog/page1.json \
	fixtures/catalog/page2.json fixtures/catalog/incremental.json \
	fixtures/catalog/rollback.json fixtures/catalog/malformed.json

TESTS_ENVIRONMENT =
TESTS_ENVIRONMENT += PATH=$(top_builddir)/src$(PATH_SEPARATOR)$$PATH
# note: libtool should handle that
### TESTS_ENVIRONMENT += $(SHLIBPATH_VAR)="$(top_builddir)/src/$(LT_CV_OBJDIR)$${$(SHLIBPATH_VAR):+$(PATH_SEPARATOR)}$$$(SHLIBPATH_VAR)"
TESTS_ENVIRONMENT += ONLINE_UNIT_TESTS=$(ONLINE_UNIT_TESTS)
TESTS_ENVIRONMENT += HTTPS_SUPPORT=$(HTTPS_SUPPORT)
TESTS_ENVIRONMENT += CATALOG_CC="$(CC)"
TESTS_ENVIRONMENT += CATALOG_CPPFLAGS="$(CPPFLAGS)"
TESTS_ENVIRONMENT += CATALOG_CFLAGS="$(CFLAGS)"
TESTS_ENVIRONMENT += CATALOG_SQLITE_LIBS="$(SQLITE_LIBS)"
TESTS_ENVIRONMENT += CATALOG_TOP_SRCDIR="$(abs_top_srcdir)"
TESTS_ENVIRONMENT += CATALOG_TOP_BUILDDIR="$(abs_top_builddir)"

TEST_EXTENSIONS = .test
TESTS = 00_runnable.test 01_engine-charset.test 01_engine-cookies.test 01_engine-entities.test 01_engine-hashtable.test 01_engine-idna.test 01_engine-robots.test 01_engine-simplify.test 01_engine-usercommand.test 10_crawl-simple.test 11_crawl-cookies.test 11_crawl-idna.test 11_crawl-international.test 11_crawl-longurl.test 11_crawl-parsing.test 12_crawl_https.test

if BUILD_IMAGEBOARD_CATALOG
TESTS += catalog-test.test metadata-runtime-test.test yotsuba-adapter.test \
	catalog-cli.test
endif

CLEANFILES = check-network_sh.cache yotsuba-adapter-test-bin \
	catalog-cli-fixture-bin

.PHONY: check-yotsuba-live
check-yotsuba-live:
	python3 $(srcdir)/yotsuba-live-smoke.py

EXTRA_DIST += \
	yotsuba-live-smoke.py \
	fixtures/yotsuba/boards.json \
	fixtures/yotsuba/boards-discovery.json \
	fixtures/yotsuba/threads-initial.json \
	fixtures/yotsuba/threads-empty.json \
	fixtures/yotsuba/threads-incremental.json \
	fixtures/yotsuba/threads-steady.json \
	fixtures/yotsuba/threads-404.json \
	fixtures/yotsuba/archive-empty.json \
	fixtures/yotsuba/archive-100.json \
	fixtures/yotsuba/thread-100.json \
	fixtures/yotsuba/thread-200.json \
	fixtures/yotsuba/thread-300.json \
	fixtures/yotsuba/malformed.json
