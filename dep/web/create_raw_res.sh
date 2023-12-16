#/bin/bash

oh="include/$2.raw.h"
oc="raw/$2.raw.c"
n="__$2"

if [ ! -d "raw" ]; then
    mkdir "raw"
fi

if [ ! -d "include" ]; then
    mkdir "include"
fi

# source

printf "const unsigned char %s[] = {\n" "$n" > "$oc"

tmp=$(mktemp)
gzip -9 -c -k "./$1" > $tmp
size=$(stat --printf="%s" $tmp)
cat $tmp | xxd -i >> "$oc"
rm $tmp

printf "};\n" >> "$oc"

# header

printf "#pragma once\n\n" > "$oh"

printf "extern const unsigned char %s[];\n" "$n" >> "$oh"
printf "#define %s_size %s\n" "$n" "$size" >> "$oh"
