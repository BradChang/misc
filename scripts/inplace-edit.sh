#!/bin/sh

# This is an example of a Perl one liner that does in place edit
# The example one liner below uncomments commented lines

cat >input.txt <<EOF
# comment
# another comment
regular line
EOF

echo 'initial file:'
cat input.txt
echo

# the one liner
perl -pi.bak -e 's/^#\s?(.+)$/$1/' input.txt

echo 'edited file:'
cat input.txt
echo

rm input.txt

