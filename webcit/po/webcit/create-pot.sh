#!/bin/bash

CSOURCES=`find ../.. -name \*.c`
HSOURCES=`find ../.. -name \*.html`

echo Updating webcit.pot from strings in the source code ...
xgettext \
	--copyright-holder='The Citadel Project - http://www.citadel.org' \
        --from-code='utf-8' \
	-k_ \
	-o webcit.pot \
	--add-comments \
	$CSOURCES $HSOURCES

for x in *.po
do
	echo Merging webcit.pot into $x ...
	msgmerge $x webcit.pot -o $x
done
