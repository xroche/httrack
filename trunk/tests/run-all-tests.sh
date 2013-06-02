#!/bin/bash
#

error=0
for i in *.test ; do
	if bash $i ; then
		echo "$i: passed" >&2
	else
		echo "$i: ERROR" >&2
		error=1
	fi
done

if test "$error" -eq 0; then
	echo "all tests passed" >&2
else
	echo "one or more tests failed" >&2
fi

exit $error
