#!/bin/bash
#

# ensure the httrack unit tests are available so that ut will not break
# the build in case of network outage

# test url
url=http://ut.httrack.com/enabled

# cache file name
cache=check-network_sh.cache

# cached result ?
if test -f $cache ; then
	if grep -q "ok" $cache ; then
		exit 0
	else
		exit 1
	fi
fi

# fetch single file
if bash crawl-test.sh --errors 0 --files 1 httrack --timeout=3 --max-time=3 "$url" 2>/dev/null >/dev/null ; then
	echo "ok" > $cache
	exit 0
else
	echo "error" > $cache
	exit 1
fi
