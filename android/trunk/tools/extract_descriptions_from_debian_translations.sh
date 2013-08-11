#!/bin/bash
#

# Extract descriptions from the Debian Description Translation Project

project=httrack

for i in Translation-*; do
	loc=$(grep -n "^Package: ${project}\$" $i | cut -f1 -d':')
	if test -n "$loc"; then
	tail -n +${loc} "$i" | head -32 | (
title=
eof=
IFS= read -r l
while IFS= read -r l; do
	if echo "$l" | grep -qE "^Description-..:"; then
		iso="$(echo "$l" | sed -e "s/^Description-\(..\):.*/\1/")"
		title="$(echo "$l" | cut -f2- -d':' | sed -e 's/^[[:space:]]*//g')"
		echo "$iso: $title"
		echo " ."
	elif echo "$l" | grep -qE '^[[:space:]]'; then
		if ! test -n "$eof"; then
			echo "$l"
		fi
	elif echo "$l" | grep -qE "^Package:"; then
		eof=1
	fi
done
) \
	| sed -e 's/^[[:space:]]*//' \
	| sed -e 's/^\.$/@@NL@@/' \
	| tr '\n' ' ' \
	| sed -e 's/@@NL@@/\n\n/g' \
	| sed -e 's/^[[:space:]]*//' \
	| sed -e 's/[[:space:]]*$//'

echo -e "(description provided by the Debian Description Translation Project)"
echo

fi

done
