#!/bin/bash
#

# Convert httrack lang/*;txt files into android xml resources

# usage: $0 path-to/lang path-to/res 
# example:
#	rm -rf res/values-*
#	bash tools/build_strings.sh /temp/httrack-3.47.21/lang res/

function norm_key {
	echo "$1" | sed -e 's/[^a-zA-Z0-9]/_/g'
}

test -d "$1" || ! echo "lang directory not found: $1" || exit 1
test -d "$2" || ! echo "res directory not found: $2" || exit 1

for i in $1/*.txt ; do
echo -n "processing $i .. " >&2

if test $(head -5 $i | tail -1 | tr -d '\r') != "LANGUAGE_ISO"; then
echo "bad file $i: LANGUAGE_ISO expected"
exit 1
fi
if test $(head -9 $i | tail -1 | tr -d '\r') != "LANGUAGE_CHARSET"; then
echo "bad file $i: LANGUAGE_CHARSET expected"
exit 1
fi
iso=$(head -6 $i | tail -1 | tr -d '\r' | tr '-' '_' | sed -e 's/_/_r/')
cp=$(head -10 $i | tail -1 | tr -d '\r')
if test "$iso" = "en"; then
echo "skipped"
continue;
fi
dest=$2/values-${iso}
mkdir -p $dest

# read strings
cat "$i" | iconv -f "$cp" -t "utf-8" | tr -d '\r' | (
unset arr
declare -A arr
while IFS= read -r rk; do
IFS= read -r v
k="$(norm_key "$rk")"
if test -n "$k" -a -n "$v"; then
arr["$k"]="$v"
fi

# hack for multiple \n-separated lines
if echo  "$rk" | grep -q '\\n'; then
n=1
while test -n "$n"; do
sv=$(echo "$v" | sed -e 's/\\n/"/g' | cut -f${n} -d'"' | sed -e 's/\\r//g')
sk=$(echo "$rk" | sed -e 's/\\n/"/g' | cut -f${n} -d'"' | sed -e 's/\\r//g')
sk="$(norm_key "$sk")"
if test -n "$sk" -a -n "$sv"; then
arr["$sk"]="$sv"
n=$[$n+1]
else
n=
fi
done
fi

done

# map xml
cat $2/values/strings.xml | tr -d '\r' | (

# <string name="options">Options...</string>
while IFS= read -r l; do
if $(echo "$l" | grep -q "<string"); then
k="$(echo "$l" | cut -f2 -d'>' | cut -f1 -d'<')"
k="$(norm_key "$k")"
v="${arr[$k]}"
# try harder without punct
if ! test -n "$v"; then
k="$(echo "$k" | sed -e 's/[_]*$//')"
v="${arr[$k]}"
fi
if test -n "$v"; then
echo -n "$l" | sed -e "s/\(.*\)>.*<\(.*\)/\1>/"
echo -n "$v" | sed -e "s/'/\\\\'/g"
echo -n "$l" | sed -e "s/\(.*\)>.*<\(.*\)/<\2/"
echo
else
echo "    <!-- $(echo "$l" | tr '<>' '[]' | tr -d '\r\n' | sed -e 's/--/- -/g') -->"
fi
else
if $(echo "$l" | grep -q "<resources>"); then
echo "<!-- Generated file ($0), do NOT edit! Edit lang/*.txt files and re-run the script to regenerate XML files. -->"
fi
echo "$l"
fi
done
) > $dest/string.xml

)

echo "done (${iso})" >&2
done


