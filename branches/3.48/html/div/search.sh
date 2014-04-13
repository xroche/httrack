
#!/bin/sh

# Simple indexing test using HTTrack
# A "real" script/program would use advanced search, and 
# use dichotomy to find the word in the index.txt file
# This script is really basic and NOT optimized, and
# should not be used for professional purpose :)

TESTSITE="http://localhost/"

# Create an index if necessary
if ! test -f "index.txt"; then
	echo "Building the index .."
	rm -rf test
	httrack --display "$TESTSITE"  -%I -O test
	mv test/index.txt ./
fi

# Convert crlf to lf
if test "`head index.txt -n 1 | tr '\r' '#' | grep -c '#'`" = "1"; then
	echo "Converting index to Unix LF style (not CR/LF) .."
	mv -f index.txt index.txt.old
	cat index.txt.old|tr -d '\r' > index.txt
fi

keyword=-
while test -n "$keyword"; do
	printf "Enter a keyword: "
	read keyword

	if test -n "$keyword"; then
		FOUNDK="`grep -niE \"^$keyword\" index.txt`"

		if test -n "$FOUNDK"; then	
			if ! test `echo "$FOUNDK"|wc -l` = "1"; then
				# Multiple matches
				printf "Found multiple keywords: "
				echo "$FOUNDK"|cut -f2 -d':'|tr '\n' ' '
				echo ""
				echo "Use keyword$ to find only one"
			else
				# One match
				N=`echo "$FOUNDK"|cut -f1 -d':'`
				PM=`tail +$N index.txt|grep -nE "\("|head -n 1`
				if ! echo "$PM"|grep "ignored">/dev/null; then
					M=`echo $PM|cut -f1 -d':'`
					echo "Found in:"
					cat index.txt | tail "+$N" | head -n "$M" | grep -E "[0-9]* " | cut -f2 -d' '
				else
					echo "keyword ignored (too many hits)"
				fi
					fi
		else
			echo "not found"
		fi

	fi
done

