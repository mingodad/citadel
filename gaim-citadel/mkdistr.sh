#!/bin/sh
# $Id$
#
# Script to build a distribution package.

APPNAME=gaim-citadel

set -e
	
if [ "$1" = "" ]; then
	echo "Please specify a version number!"
	exit 1
fi

if [ -d ../$APPNAME-$1 ]; then
	echo "I think you've already generated version $1."
	exit 1
fi

mkdir ../$APPNAME-$1
cp -a --parents \
	pm \
	c.pm \
	pmfile \
	citadel.c \
	citadel.lua \
	gaim.pkg \
	interface.h \
	README \
	COPYING \
\
	../gaim-citadel-$1

(cd .. && tar cvf $APPNAME-$1.tar.bz2 --bzip2 $APPNAME-$1)

echo ""
echo "Done --- but did you remember to update the version number?"
