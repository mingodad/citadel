#!/bin/bash

echo Updating citadel.pot from strings in the source code ...
xgettext \
	--copyright-holder='The Citadel Project - http://www.citadel.org' \
        --from-code='utf-8' \
	-k_ \
	-o citadel-setup.pot \
	--add-comments \
        ../utils/setup.c 

#xgettext \
#	--copyright-holder='The Citadel Project - http://www.citadel.org' \
#        --from-code='utf-8' \
#	-k_ \
#	-o citadel-server.pot \
#	--add-comments \
#	../*.c \
#        `cat ../Make_sources | sed  -e "s;.*+= ;;" ` 
#
#
#xgettext \
#	--copyright-holder='The Citadel Project - http://www.citadel.org' \
#        --from-code='utf-8' \
#	-k_ \
#	-o citadel-client.pot \
#	--add-comments \
#	../*.c \
#        ../textclient/*.c

for x in *.po
do
	echo Merging citadel-setup.pot into $x ...
	msgmerge $x citadel-setup.pot -o $x
done
