#!/bin/bash
#

# Change this to download files
if false; then
echo "mget ftp://ftp.unicode.org/Public/MAPPINGS/ISO8859/8859-*.TXT" | lftp
echo "mget ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP*.TXT" | lftp
echo "mget ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP*.TXT" | lftp
echo "mget ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/EBCDIC/CP*.TXT" | lftp
echo "mget ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/MISC/CP*.TXT" | lftp
echo "mget ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/MISC/KOI8*.TXT" | lftp
rm -f CP932.TXT CP936.TXT CP949.TXT CP950.TXT
fi

# Produce code
printf "/** GENERATED FILE ($0), DO NOT EDIT **/\n\n"
for i in *.TXT ; do
  echo "processing $i" >&2
  grep -vE "^(#|$)" $i | grep -E "^0x" | sed -e 's/[[:space:]]/ /g' | cut -f1,2 -d' ' | \
  (
    unset arr
    while read LINE ; do
      from=$[$(echo $LINE | cut -f1 -d' ')]
      if ! test -n "$from"; then
        echo "error with $i" >&2
        exit 1
      elif test $from -ge 256; then
        echo "out-of-range ($LINE) with $i" >&2
        exit 1
      fi
      to=$(echo $LINE | cut -f2 -d' ') 
      arr[$from]=$to
    done
    name=$(echo $i | tr 'A-Z' 'a-z' | tr '-' '_' | sed -e 's/\.txt//' -e 's/8859/iso_8859/')
    printf "/* Table for $i */\nstatic const hts_UCS4 table_${name}[256] = {\n  "
    i=0
    while test "$i" -lt 256; do
      if test "$i" -gt 0; then
        printf ", "
        if test $[${i}%8] -eq 0; then
          printf "\n  "
        fi
      fi
      value=${arr[$i]:-0}
      printf "0x%04x" $value
      i=$[${i}+1]
    done
    printf " };\n\n"
  )
  echo "processed $i" >&2
done

# Indexes
printf "static const struct {\n  const char *name;\n  const hts_UCS4 *table;\n} table_mappings[] = {\n"
for i in *.TXT ; do
  name=$(echo $i | tr 'A-Z' 'a-z' | tr '-' '_' | sed -e 's/\.txt//' -e 's/8859/iso_8859/')
  printf "  { \"$(echo $name | tr -d '_')\", table_${name} },\n"
done
printf "  { NULL, NULL }\n};\n"
