/*
 * $Id: $
 *
 * vNote implementation for Citadel
 *
 * Copyright (C) 1999-2007 by the citadel.org development team.
 * This code is freely redistributable under the terms of the GNU General
 * Public License.  All other rights reserved.
 */


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <libcitadel.h>


/*

This whole file is merely a placeholder.

We need to be able to support some of these...


BEGIN:vnote
VERSION:1.1
PRODID://Bynari Insight Connector 3.1.3-0605191//Import from Outlook//EN
CLASS:PUBLIC
UID:040000008200E00074C5B7101A82E0080000000000000000000000000000000000000820425CE8571864B8D141CB3FB8CAC62
NOTE;ENCODING=QUOTED-PRINTABLE:blah blah blah=0D=0A=0D=0A
SUMMARY:blah blah blah=0D=0A=0D=0A
X-OUTLOOK-COLOR:#FFFF00
X-OUTLOOK-WIDTH:200
X-OUTLOOK-HEIGHT:166
X-OUTLOOK-LEFT:80
X-OUTLOOK-TOP:80
X-OUTLOOK-CREATE-TIME:20070611T204615Z
REV:20070611T204621Z
END:vnote

BEGIN:VNOTE^M
VERSION:1.1^M
UID:20061129111109.7chx73xdok1s at 172.16.45.2^M
BODY:HORDE_1^M
DCREATED:20061129T101109Z^M
END:VNOTE^M

*/
