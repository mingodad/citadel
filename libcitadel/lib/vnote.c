/*
 * $Id$
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
	char *ptr = s;
	char *nexteol;
	char *thisline;
	int thisline_len;
	char *encoded_value;
	char *decoded_value;
	int is_quoted_printable;
	int is_base64;

	v = vnote_new();
	if (!v) return NULL;

	while (*ptr) {		// keep going until we hit a null terminator
		thisline = NULL;
		nexteol = strchr(ptr, '\n');
		if (nexteol) {
			thisline = malloc((nexteol - ptr) + 2);
			strncpy(thisline, ptr, (nexteol-ptr));
			thisline_len = (nexteol-ptr);
			thisline[thisline_len] = 0;
			ptr = nexteol + 1;
		}
		else {
			thisline = strdup(ptr);
			thisline_len = strlen(thisline);
			ptr += thisline_len;
		}

		if (thisline) {
			if (thisline_len > 1) {
				if (thisline[thisline_len - 1] == '\r') {
					thisline[thisline_len - 1] = 0;
					--thisline_len;
				}
			}

			/* locate the colon separator */
			encoded_value = strchr(thisline, ':');
			if (encoded_value) {
				*encoded_value++ = 0;

				/* any qualifiers?  (look for a semicolon) */
				is_base64 = 0;
				is_quoted_printable = 0;


				decoded_value = malloc(thisline_len);
				if (is_base64) {
				}
				else if (is_quoted_printable) {
				}
				else {
					strcpy(decoded_value, thisline_len);
				}

				if (0) {
				}
				else {
					free(decoded_value);	// throw it away
				}


			}
			free(thisline);
		}
	}

	return(v);
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


/* helper function for vnote_serialize() */
void vnote_serialize_output_field(char *append_to, char *field, char *label) {

	char *mydup;
	int output_len = 0;
	int is_qp = 0;
	char *ptr = field;
	unsigned char ch;
	int pos = 0;

	if (!append_to) return;
	if (!field) return;
	if (!label) return;

	mydup = malloc((strlen(field) * 3) + 1);
	if (!mydup) return;
	strcpy(mydup, "");

	while (ptr[pos] != 0) {
		ch = (unsigned char)(ptr[pos++]);

		if (ch == 9) {
			mydup[output_len++] = ch;
		}
		else if ( (ch >= 32) && (ch <= 60) ) {
			mydup[output_len++] = ch;
		}
		else if ( (ch >= 62) && (ch <= 126) ) {
			mydup[output_len++] = ch;
		}
		else {
			sprintf((char *)&mydup[output_len], "=%02X", ch);
			output_len += 3;
			is_qp = 1;
		}
	}
	mydup[output_len] = 0;

	sprintf(&append_to[strlen(append_to)], "%s%s:%s\r\n",
		label,
		(is_qp ? ";ENCODING=QUOTED-PRINTABLE" : ""),
		mydup);
	free(mydup);
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

	strcpy(s, "");
	vnote_serialize_output_field(s, "vnote", "BEGIN");
	vnote_serialize_output_field(s, "//Citadel//vNote handler library//EN", "PRODID");
	vnote_serialize_output_field(s, "1.1", "VERSION");
	vnote_serialize_output_field(s, "PUBLIC", "CLASS");
	vnote_serialize_output_field(s, v->uid, "UID");
	vnote_serialize_output_field(s, v->body, "BODY");
	vnote_serialize_output_field(s, v->body, "NOTE");
	sprintf(&s[strlen(s)], "X-OUTLOOK-COLOR:#%02X%02X%02X\r\n",
		v->color_red, v->color_green, v->color_blue);
	sprintf(&s[strlen(s)], "X-OUTLOOK-LEFT:%d\r\n", v->pos_left);
	sprintf(&s[strlen(s)], "X-OUTLOOK-TOP:%d\r\n", v->pos_top);
	sprintf(&s[strlen(s)], "X-OUTLOOK-WIDTH:%d\r\n", v->pos_width);
	sprintf(&s[strlen(s)], "X-OUTLOOK-HEIGHT:%d\r\n", v->pos_height);
	vnote_serialize_output_field(s, "vnote", "END");
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
	"END:vnote"
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

	printf("Before:\n-------------\n%s-------------\nAfter:\n-----------\n", bynari_sample);
	v = vnote_new_from_str(bynari_sample);
	s = vnote_serialize(v);
	vnote_free(v);
	if (s) {
		printf("%s", s);
		free(s);
	}

	exit(0);
}
#endif





