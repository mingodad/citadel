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


/* move this into the header file when we're done */
#define CTDL_VNOTE_MAGIC	0xa1fa
struct vnote {
	int magic;
};


struct vnote *vnote_new(void) {
	struct vnote *v;

	v = (struct vnote *) malloc(sizeof(struct vnote));
	if (v) {
		memset(v, 0, sizeof(struct vnote));
		v->magic = CTDL_VNOTE_MAGIC;
	}
	return v;
}

struct vnote *vnote_new_from_str(char *s) {
	struct vnote *v;

	v = vnote_new();
	if (!v) return NULL;

	/* FIXME finish this */
}

void vnote_free(struct vnote *v) {
	if (!v) return;
	if (v->magic != CTDL_VNOTE_MAGIC) return;
	
	memset(v, 0, sizeof(struct vnote));
	free(v);
}


#ifdef VNOTE_TEST_HARNESS

char *bynari_sample =
	"BEGIN:vnote\n"
	"VERSION:1.1\n"
	"PRODID://Bynari Insight Connector 3.1.3-0605191//Import from Outlook//EN\n"
	"CLASS:PUBLIC\n"
	"UID:040000008200E00074C5B7101A82E00800000000000000000000000000820425CE8571864B8D141CB3FB8CAC62\n"
	"NOTE;ENCODING=QUOTED-PRINTABLE:blah blah blah=0D=0A=0D=0A\n"
	"SUMMARY:blah blah blah=0D=0A=0D=0A\n"
	"X-OUTLOOK-COLOR:#FFFF00\n"
	"X-OUTLOOK-WIDTH:200\n"
	"X-OUTLOOK-HEIGHT:166\n"
	"X-OUTLOOK-LEFT:80\n"
	"X-OUTLOOK-TOP:80\n"
	"X-OUTLOOK-CREATE-TIME:20070611T204615Z\n"
	"REV:20070611T204621Z\n"
	"END:vnote\n"
;

char *horde_sample =
	"BEGIN:VNOTE\n"
	"VERSION:1.1\n"
	"UID:20061129111109.7chx73xdok1s at 172.16.45.2\n"
	"BODY:HORDE_1\n"
	"DCREATED:20061129T101109Z\n"
	"END:VNOTE\n"
;


main() {
	struct vnote *v = vnote_new_from_str(bynari_sample);
	vnote_free(v);
	exit(0);
}
#endif
