# Committed binary fixture read by 01_engine-cache-golden.test. List it
# explicitly: automake does not expand wildcards in EXTRA_DIST, so a glob would
# silently drop it from the dist tarball and break "make distcheck".
EXTRA_DIST = $(TESTS) crawl-test.sh run-all-tests.sh check-network.sh \
	fixtures/cache-golden/hts-cache/new.zip

TESTS_ENVIRONMENT =
TESTS_ENVIRONMENT += PATH=$(top_builddir)/src$(PATH_SEPARATOR)$$PATH
# note: libtool should handle that
### TESTS_ENVIRONMENT += $(SHLIBPATH_VAR)="$(top_builddir)/src/$(LT_CV_OBJDIR)$${$(SHLIBPATH_VAR):+$(PATH_SEPARATOR)}$$$(SHLIBPATH_VAR)"
TESTS_ENVIRONMENT += ONLINE_UNIT_TESTS=$(ONLINE_UNIT_TESTS)
TESTS_ENVIRONMENT += HTTPS_SUPPORT=$(HTTPS_SUPPORT)
TESTS_ENVIRONMENT += top_srcdir=$(top_srcdir)

TEST_EXTENSIONS = .test
# Run each .test through bash instead of execve()ing it. This lets "make check"
# work when the source tree sits on a noexec filesystem (the driver would
# otherwise fail with "Permission denied"), and removes any reliance on the
# scripts' executable bit. The scripts are #!/bin/bash and use bash features.
TEST_LOG_COMPILER = $(BASH)
TESTS = \
	00_runnable.test \
	01_engine-cache.test \
	01_engine-cache-golden.test \
	01_engine-charset.test \
	01_engine-cmdline.test \
	01_engine-doitlog.test \
	01_engine-entities.test \
	01_engine-filter.test \
	01_engine-hashtable.test \
	01_engine-idna.test \
	01_engine-mime.test \
	01_engine-parse.test \
	01_engine-rcfile.test \
	01_engine-simplify.test \
	01_engine-strsafe.test \
	02_manpage-regen.test \
	02_update-cache.test \
	10_crawl-simple.test \
	11_crawl-cookies.test \
	11_crawl-idna.test \
	11_crawl-international.test \
	11_crawl-longurl.test \
	11_crawl-parsing.test \
	12_crawl_https.test

CLEANFILES = check-network_sh.cache
