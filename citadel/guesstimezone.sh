#!/bin/sh

# guesstimezone.sh - an ugly hack of a script to try to guess the time
# zone currently in use on the host system, and output its name.

# Copyright (c) by Art Cancro

# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 3 of the License, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
# more details.

# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 675 Mass Ave, Cambridge, MA 02139, USA. 

md5sum </dev/null >/dev/null 2>/dev/null || exit 1
	
LOCALTIMESUM=`md5sum /etc/localtime | awk ' { print $1 } ' 2>/dev/null`
find /usr/share/zoneinfo -type f -print | while read filename
do
	THISTIMESUM=`md5sum $filename | awk ' { print $1 } '`
	if [ $LOCALTIMESUM = $THISTIMESUM ] ; then
		echo $filename | cut -c21-
		exit 0
	fi
done 2>/dev/null
exit 2
