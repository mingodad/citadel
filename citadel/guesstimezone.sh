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

convert_timezone()
{
    case "$1" in
	(right/*|posix/*)
	    convert_timezone "${1#*/}"
	    ;;
	("Africa/Asmera")
	    echo "Africa/Asmara"
	    ;;
	("America/Argentina/ComodRivadavia"|"America/Catamarca")
	    echo "America/Argentina/Catamarca"
	    ;;
	("America/Buenos_Aires")
	    echo "America/Argentina/Buenos_Aires"
	    ;;
	("America/Cordoba"|"America/Rosario")
	    echo "America/Argentina/Cordoba"
	    ;;
	("America/Jujuy")
	    echo "America/Argentina/Jujuy"
	    ;;
	("America/Mendoza")
	    echo "America/Argentina/Mendoza"
	    ;;
	("Antarctica/South_Pole")
	    echo "Antarctica/McMurdo"
	    ;;
        "Asia/Ashkhabad")
            echo "Asia/Ashgabat"
            ;;
        ("Asia/Calcutta")
            echo "Asia/Kolkata"
            ;;
        "Asia/Chungking")
            echo "Asia/Chongqing"
            ;;
        "Asia/Dacca")
            echo "Asia/Dhaka"
            ;;
        ("Asia/Katmandu")
            echo "Asia/Kathmandu"
            ;;
        "Asia/Macao")
            echo "Asia/Macau"
            ;;
        ("Asia/Saigon")
            echo "Asia/Ho_Chi_Minh"
            ;;
        "Asia/Thimbu")
            echo "Asia/Thimphu"
            ;;
        "Asia/Ulan_Bator")
            echo "Asia/Ulaanbaatar"
            ;;
        "Atlantic/Faeroe")
            echo "Atlantic/Faroe"
            ;;
        "Australia/ACT" | "Australia/NSW")
            echo "Australia/Sydney"
            ;;
        "Australia/LHI")
            echo "Australia/Lord_Howe"
            ;;
        "Australia/North")
            echo "Australia/Darwin"
            ;;
        "Australia/Queensland")
            echo "Australia/Brisbane"
            ;;
        "Australia/South")
            echo "Australia/Adelaide"
            ;;
        "Australia/Tasmania")
            echo "Australia/Hobart"
            ;;
        "Australia/Victoria")
            echo "Australia/Melbourne"
            ;;
        "Australia/West")
            echo "Australia/Perth"
            ;;
        "Brazil/Acre")
            echo "America/Rio_Branco"
            ;;
        "Brazil/DeNoronha")
            echo "America/Noronha"
            ;;
        "Brazil/East")
            echo "America/Sao_Paulo"
            ;;
        "Brazil/West")
            echo "America/Manaus"
            ;;
        "Canada/Atlantic")
            echo "America/Halifax"
            ;;
        "Canada/Central")
            echo "America/Winnipeg"
            ;;
        "Canada/East-Saskatchewan")
            echo "America/Regina"
            ;;
        "Canada/Eastern")
            echo "America/Toronto"
            ;;
        "Canada/Mountain")
            echo "America/Edmonton"
            ;;
        "Canada/Newfoundland")
            echo "America/St_Johns"
            ;;
        "Canada/Pacific")
            echo "America/Vancouver"
            ;;
        "Canada/Saskatchewan")
            echo "America/Regina"
            ;;
        "Canada/Yukon")
            echo "America/Whitehorse"
            ;;
        "CET")
            echo "Europe/Paris"
            ;;
        "Chile/Continental")
            echo "America/Santiago"
            ;;
        "Chile/EasterIsland")
            echo "Pacific/Easter"
            ;;
        "CST6CDT")
            echo "SystemV/CST6CDT"
            ;;
        "Cuba")
            echo "America/Havana"
            ;;
        "EET")
            echo "Europe/Helsinki"
            ;;
        "Egypt")
            echo "Africa/Cairo"
            ;;
        "Eire")
            echo "Europe/Dublin"
            ;;
        "EST")
            echo "SystemV/EST5"
            ;;
        "EST5EDT")
            echo "SystemV/EST5EDT"
            ;;
        "GB")
            echo "Europe/London"
            ;;
        "GB-Eire")
            echo "Europe/London"
            ;;
        "GMT")
            echo "Etc/GMT"
            ;;
        "GMT0")
            echo "Etc/GMT0"
            ;;
        "GMT-0")
            echo "Etc/GMT-0"
            ;;
        "GMT+0")
            echo "Etc/GMT+0"
            ;;
        "Greenwich")
            echo "Etc/Greenwich"
            ;;
        "Hongkong")
            echo "Asia/Hong_Kong"
            ;;
        "HST")
            echo "Pacific/Honolulu"
            ;;
        "Iceland")
            echo "Atlantic/Reykjavik"
            ;;
        "Iran")
            echo "Asia/Tehran"
            ;;
        "Israel")
            echo "Asia/Tel_Aviv"
            ;;
        "Jamaica")
            echo "America/Jamaica"
            ;;
        "Japan")
            echo "Asia/Tokyo"
            ;;
        "Kwajalein")
            echo "Pacific/Kwajalein"
            ;;
        "Libya")
            echo "Africa/Tripoli"
            ;;
        "MET")
            echo "Europe/Paris"
            ;;
        "Mexico/BajaNorte")
            echo "America/Tijuana"
            ;;
        "Mexico/BajaSur")
            echo "America/Mazatlan"
            ;;
        "Mexico/General")
            echo "America/Mexico_City"
            ;;
        "Mideast/Riyadh87")
            echo "Asia/Riyadh87"
            ;;
        "Mideast/Riyadh88")
            echo "Asia/Riyadh88"
            ;;
        "Mideast/Riyadh89")
            echo "Asia/Riyadh89"
            ;;
        "MST")
            echo "SystemV/MST7"
            ;;
        "MST7MDT")
            echo "SystemV/MST7MDT"
            ;;
        "Navajo")
            echo "America/Denver"
            ;;
        "NZ")
            echo "Pacific/Auckland"
            ;;
        "NZ-CHAT")
            echo "Pacific/Chatham"
            ;;
        "Poland")
            echo "Europe/Warsaw"
            ;;
        "Portugal")
            echo "Europe/Lisbon"
            ;;
        "PRC")
            echo "Asia/Shanghai"
            ;;
        "PST8PDT")
            echo "SystemV/PST8PDT"
            ;;
        "ROC")
            echo "Asia/Taipei"
            ;;
        "ROK")
            echo "Asia/Seoul"
            ;;
        "Singapore")
            echo "Asia/Singapore"
            ;;
        "Turkey")
            echo "Europe/Istanbul"
            ;;
        "UCT")
            echo "Etc/UCT"
            ;;
        "Universal")
            echo "Etc/UTC"
            ;;
        "US/Alaska")
            echo "America/Anchorage"
            ;;
        "US/Aleutian")
            echo "America/Adak"
            ;;
        "US/Arizona")
            echo "America/Phoenix"
            ;;
        "US/Central")
            echo "America/Chicago"
            ;;
        "US/East-Indiana")
            echo "America/Indianapolis"
            ;;
        "US/Eastern")
            echo "America/New_York"
            ;;
        "US/Hawaii")
            echo "Pacific/Honolulu"
            ;;
        "US/Indiana-Starke")
            echo "America/Indianapolis"
            ;;
        "US/Michigan")
            echo "America/Detroit"
            ;;
        "US/Mountain")
            echo "America/Denver"
            ;;
        "US/Pacific")
            echo "America/Los_Angeles"
            ;;
        "US/Samoa")
            echo "Pacific/Pago_Pago"
            ;;
        "UTC")
            echo "Etc/UTC"
            ;;
        "WET")
            echo "Europe/Lisbon"
            ;;
        "W-SU")
            echo "Europe/Moscow"
            ;;
        "Zulu")
            echo "Etc/UTC"
            ;;
        *)
            echo "$1"
            ;;
    esac
}

md5sum </dev/null >/dev/null 2>/dev/null || exit 1

# this check will find the timezone if /etc/localtime is a link to the right timezone file
# or that file has been copied to /etc/localtime and not changed afterwards
LOCALTIMESUM=`md5sum /etc/localtime | awk ' { print $1 } ' 2>/dev/null`
find /usr/share/zoneinfo -type f -print | while read filename
do
	THISTIMESUM=`md5sum $filename | awk ' { print $1 } '`
	if [ $LOCALTIMESUM = $THISTIMESUM ] ; then
		echo $filename | cut -c21-
		exit 0
	fi
done 2>/dev/null

# seems we haven't found the timezone yet, let's see whether /etc/timezone has some info

if [ -e /etc/timezone ]; then
    TIMEZONE="$(head -n 1 /etc/timezone)"
    TIMEZONE="${TIMEZONE%% *}"
    TIMEZONE="${TIMEZONE##/}"
    TIMEZONE="${TIMEZONE%%/}"
    TIMEZONE="$(convert_timezone $TIMEZONE)"
    if [ -f "/usr/share/zoneinfo/$TIMEZONE" ] ; then
	echo $TIMEZONE 
	exit 0
    fi
fi

exit 2
