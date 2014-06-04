#!/bin/bash
#

# ensure the httrack unit tests are available so that ut will not break
# the build in case of network outage

# do not enable online tests (./configure --disable-online-unit-tests)
if test "$ONLINE_UNIT_TESTS" == "no"; then
echo "online tests are disabled" >&2
exit 1

# enable online tests (--enable-online-unit-tests)
elif test "$ONLINE_UNIT_TESTS" == "yes"; then
exit 0

# check if online tests are reachable
else

# test url
url=http://ut.httrack.com/enabled

# cache file name
cache=check-network_sh.cache

# cached result ?
if test -f $cache ; then
	if grep -q "ok" $cache ; then
		exit 0
	else
		echo "online tests are disabled (cached)" >&2
		exit 1
	fi

# fetch single file
elif bash crawl-test.sh --errors 0 --files 1 httrack --timeout=3 --max-time=3 "$url" 2>/dev/null >/dev/null ; then
	echo "ok" > $cache
	exit 0
else
	echo "error" > $cache
	echo "online tests are disabled (auto)" >&2
	exit 1
fi

fi
