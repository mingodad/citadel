/*
 * vNote implementation for Citadel
 *
 * Copyright (C) 1999-2007 by the citadel.org development team.
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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
			thisline_len = (nexteol-ptr);
			thisline = malloc(thisline_len + 2);
			memcpy(thisline, ptr, thisline_len);
			thisline[thisline_len] = '\0';
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
				is_base64 = bmstrcasestr(thisline, "encoding=base64") ? 1 : 0;
				is_quoted_printable = bmstrcasestr(thisline, "encoding=quoted-printable") ? 1 : 0;

				char *semicolon_pos = strchr(thisline, ';');
				if (semicolon_pos) {
					*semicolon_pos = 0;
				}

				decoded_value = malloc(thisline_len);
				if (is_base64) {
					CtdlDecodeBase64(decoded_value,
							encoded_value,
							strlen(encoded_value));
				}
				else if (is_quoted_printable) {
					CtdlDecodeQuotedPrintable(decoded_value,
							encoded_value,
							strlen(encoded_value));
				}
				else {
					strcpy(decoded_value, encoded_value);
				}

				if (!strcasecmp(thisline, "UID")) {
					if (v->uid) free(v->uid);
					v->uid = decoded_value;
				}
				else if (!strcasecmp(thisline, "SUMMARY")) {
					if (v->summary) free(v->summary);
					v->summary = decoded_value;
				}
				else if ( (!strcasecmp(thisline, "NOTE"))
				     || (!strcasecmp(thisline, "BODY")) ) {
					if (v->body) free(v->body);
					v->body = decoded_value;
				}
				else if (!strcasecmp(thisline, "X-OUTLOOK-WIDTH")) {
					v->pos_width = atoi(decoded_value);
					free(decoded_value);
				}
				else if (!strcasecmp(thisline, "X-OUTLOOK-HEIGHT")) {
					v->pos_height = atoi(decoded_value);
					free(decoded_value);
				}
				else if (!strcasecmp(thisline, "X-OUTLOOK-LEFT")) {
					v->pos_left = atoi(decoded_value);
					free(decoded_value);
				}
				else if (!strcasecmp(thisline, "X-OUTLOOK-TOP")) {
					v->pos_top = atoi(decoded_value);
					free(decoded_value);
				}
				else if ( (!strcasecmp(thisline, "X-OUTLOOK-COLOR"))
				     && (strlen(decoded_value) == 7)
				     && (decoded_value[0] == '#') ) {
					sscanf(&decoded_value[1], "%2x%2x%2x",
						&v->color_red,
						&v->color_green,
						&v->color_blue);
					free(decoded_value);
				}
				else {
					free(decoded_value);	// throw it away
				}

				/* FIXME still need to handle these:
				 * X-OUTLOOK-CREATE-TIME:20070611T204615Z
				 * REV:20070611T204621Z
				 */
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
	*mydup = '\0';

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

	*s = '\0';
	vnote_serialize_output_field(s, "vnote", "BEGIN");
	vnote_serialize_output_field(s, "//Citadel//vNote handler library//EN", "PRODID");
	vnote_serialize_output_field(s, "1.1", "VERSION");
	vnote_serialize_output_field(s, "PUBLIC", "CLASS");
	vnote_serialize_output_field(s, v->uid, "UID");
	vnote_serialize_output_field(s, v->summary, "SUMMARY");
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
