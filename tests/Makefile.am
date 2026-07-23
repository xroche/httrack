# Committed binary fixture read by 01_zlib-cache-golden.test. List it
# explicitly: automake does not expand wildcards in EXTRA_DIST, so a glob would
# silently drop it from the dist tarball and break "make distcheck".
EXTRA_DIST = $(TESTS) crawl-test.sh run-all-tests.sh check-network.sh \
	proxy-https-server.py socks5-server.py proxy-connect-server.py \
	proxytestlib.py tls-stall-server.py warc-validate.py wacz-validate.py \
	local-crawl.sh local-server.py testlib.sh server.crt server.key \
	server-root/simple/basic.html server-root/simple/link.html \
	server-root/stripquery/index.html server-root/stripquery/a.html \
	server-root/fraglink/index.html server-root/fraglink/target.html \
	fixtures/cache-golden/hts-cache/new.zip

TESTS_ENVIRONMENT =
TESTS_ENVIRONMENT += PATH=$(top_builddir)/src$(PATH_SEPARATOR)$$PATH
# note: libtool should handle that
### TESTS_ENVIRONMENT += $(SHLIBPATH_VAR)="$(top_builddir)/src/$(LT_CV_OBJDIR)$${$(SHLIBPATH_VAR):+$(PATH_SEPARATOR)}$$$(SHLIBPATH_VAR)"
TESTS_ENVIRONMENT += ONLINE_UNIT_TESTS=$(ONLINE_UNIT_TESTS)
TESTS_ENVIRONMENT += HTTPS_SUPPORT=$(HTTPS_SUPPORT)
TESTS_ENVIRONMENT += BROTLI_ENABLED=$(BROTLI_ENABLED)
TESTS_ENVIRONMENT += ZSTD_ENABLED=$(ZSTD_ENABLED)
TESTS_ENVIRONMENT += V6_SUPPORT=$(V6_SUPPORT)
TESTS_ENVIRONMENT += top_srcdir=$(top_srcdir)

TEST_EXTENSIONS = .test
# Run each .test through bash instead of execve()ing it. This lets "make check"
# work when the source tree sits on a noexec filesystem (the driver would
# otherwise fail with "Permission denied"), and removes any reliance on the
# scripts' executable bit. The scripts are #!/bin/bash and use bash features.
TEST_LOG_COMPILER = $(BASH)
TESTS = \
	00_runnable.test \
	01_engine-charset.test \
	01_engine-cmdline.test \
	01_engine-cookies.test \
	01_engine-copyopt.test \
	01_engine-crange.test \
	01_engine-dns.test \
	01_engine-dnstimeout.test \
	01_engine-doitlog.test \
	01_engine-entities.test \
	01_engine-footerfmt.test \
	01_engine-footer-overflow.test \
	01_engine-filelist.test \
	01_engine-filter.test \
	01_engine-filterdual.test \
	01_engine-ftp-line.test \
	01_engine-ftp-userpass.test \
	01_engine-cacheindex.test \
	01_engine-hashtable.test \
	01_engine-header.test \
	01_engine-idna.test \
	01_engine-identurl.test \
	01_engine-proxyurl.test \
	01_engine-socks5.test \
	01_engine-identabs.test \
	01_engine-escape-room.test \
	01_engine-inplace-escape.test \
	01_engine-makeindex.test \
	01_engine-mime.test \
	01_engine-parse.test \
	01_engine-pause.test \
	01_engine-rcfile.test \
	01_engine-reconcile.test \
	01_engine-expandhome.test \
	01_engine-fsize.test \
	01_engine-redirect.test \
	01_engine-longpath-io.test \
	01_engine-mirror-io.test \
	01_engine-direnum.test \
	01_engine-relative.test \
	01_engine-robots.test \
	01_engine-savename.test \
	01_engine-selftest-dispatch.test \
	01_engine-simplify.test \
	01_engine-sniff.test \
	01_engine-status.test \
	01_engine-stripquery.test \
	01_engine-strsafe.test \
	01_engine-topindex.test \
	01_engine-urlhack.test \
	01_engine-unescape-bounds.test \
	01_engine-useragent.test \
	01_engine-version-macros.test \
	01_engine-warc-surt.test \
	01_engine-xfread.test \
	01_zlib-acceptencoding.test \
	01_zlib-warc.test \
	01_zlib-warc-cdx.test \
	01_zlib-warc-wacz.test \
	01_zlib-contentcodings.test \
	01_zlib-cache.test \
	01_zlib-cache-corrupt.test \
	01_zlib-cache-legacy.test \
	01_zlib-cache-golden.test \
	01_zlib-cache-writefail.test \
	01_zlib-repair-shift.test \
	01_zlib-savename-cached.test \
	02_manpage-regen.test \
	02_update-cache.test \
	10_crawl-simple.test \
	11_crawl-cookies.test \
	11_crawl-idna.test \
	11_crawl-international.test \
	11_crawl-longurl.test \
	11_crawl-parsing.test \
	12_crawl_https.test \
	13_crawl_proxy_https.test \
	13_local-cookies.test \
	14_local-https.test \
	15_local-types.test \
	16_local-assume.test \
	17_local-empty-ct.test \
	18_local-update.test \
	19_local-connect-fallback.test \
	20_local-resume-loop.test \
	21_local-intl-update.test \
	22_local-broken-size.test \
	23_local-errpage.test \
	24_local-resume-overlap.test \
	25_local-mime-exclude.test \
	26_local-strip-query.test \
	27_local-cookies-file.test \
	28_local-pause.test \
	29_local-redirect-fragment.test \
	30_local-fragment-link.test \
	32_local-cdispo.test \
	33_local-delayed.test \
	34_local-maxtime.test \
	35_local-maxsize.test \
	36_local-bigcrawl.test \
	37_local-cache-outage.test \
	38_local-update-304.test \
	39_local-delayed-cancel.test \
	40_local-why.test \
	41_local-utf8-link.test \
	42_local-maxsize-slow.test \
	43_local-update-truncate.test \
	44_local-update-errormask.test \
	45_local-maxsize-recv.test \
	46_local-update-304-resume.test \
	47_local-crange-overflow.test \
	48_local-crange-memresume.test \
	49_local-cookiewall.test \
	50_local-contentcodings.test \
	51_local-update-codec.test \
	52_local-socks5.test \
	53_local-proxytrack-cache-corrupt.test \
	54_local-update-truncate-purge.test \
	55_local-chunked.test \
	56_local-proxy-noleak.test \
	57_local-proxy-connect.test \
	58_watchdog.test \
	59_local-tls-stall.test \
	60_crawl-log-salvage.test \
	61_webhttrack-locale.test \
	62_lang-integrity.test \
	63_webhttrack-home.test \
	64_local-intl-outdir.test \
	65_port-siblings.test \
	66_engine-port80-strip.test \
	67_engine-delayed-truncate.test \
	68_webhttrack-outdir-charset.test \
	69_local-intl-logdir.test \
	71_local-crange-repaircache.test \
	72_watchdog-crawl.test \
	73_local-warc.test \
	74_local-warc-wacz.test \
	74_local-warc-verbatim.test \
	75_engine-longpath-posix.test

CLEANFILES = check-network_sh.cache
