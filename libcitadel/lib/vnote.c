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
	char *uid;
	char *summary;
	char *body;
	int pos_left;
	int pos_top;
	int pos_width;
	int pos_height;
	int color_red;
	int color_green;
	int color_blue;
};


struct vnote *vnote_new(void) {
	struct vnote *v;

	v = (struct vnote *) malloc(sizeof(struct vnote));
	if (v) {
		memset(v, 0, sizeof(struct vnote));
		v->magic = CTDL_VNOTE_MAGIC;
		v->pos_left = rand() % 256;
		v->pos_top = rand() % 256;
		v->pos_width = 200;
		v->pos_height = 150;
		v->color_red = 0xFF;
		v->color_green = 0xFF;
		v->color_blue = 0x00;
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

	if (v->uid) free(v->uid);
	if (v->summary) free(v->summary);
	if (v->body) free(v->body);
	
	memset(v, 0, sizeof(struct vnote));
	free(v);
}

char *vnote_serialize(struct vnote *v) {
	char *s;
	int bytes_needed = 0;

	if (!v) return NULL;
	if (v->magic != CTDL_VNOTE_MAGIC) return NULL;

	bytes_needed = 1024;
	if (v->summary) bytes_needed += strlen(v->summary);
	if (v->body) bytes_needed += strlen(v->body);
	s = malloc(bytes_needed);
	if (!s) return NULL;

	strcpy(s, "BEGIN:vnote\r\n"
		"VERSION:1.1\r\n"
		"PRODID://Citadel//vNote handler library//EN\r\n"
		"CLASS:PUBLIC\r\n"
	);
	if (v->uid) {
		strcat(s, "UID:");
		strcat(s, v->uid);
		strcat(s, "\r\n");
	}

	strcat(s, "END:vnote\r\n");
	return(s);
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
	char *s;
	struct vnote *v;

	v = vnote_new_from_str(bynari_sample);
	s = vnote_serialize(v);
	vnote_free(v);
	if (s) {
		printf("%s\n", s);
		free(s);
	}

	exit(0);
}
#endif
