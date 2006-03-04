/*
 * $Id$
 *
 * Handles GroupDAV GET requests.
 *
 */

#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"


/*
 * Fetch the entire contents of the room as one big ics file.
 * This is for "webcal://" type access.
 */	
void groupdav_get_big_ics(void) {
	char buf[1024];

	serv_puts("ICAL getics");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		wprintf("HTTP/1.1 404 not found\r\n");
		groupdav_common_headers();
		wprintf(
			"Content-Type: text/plain\r\n"
			"\r\n"
			"%s\r\n",
			&buf[4]
		);
		return;
	}

	wprintf("HTTP/1.1 200 OK\r\n");
	groupdav_common_headers();
	wprintf("Content-type: text/calendar; charset=UTF-8\r\n");
	begin_burst();
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		wprintf("%s\r\n", buf);
	}
	end_burst();
}


/*
 * The pathname is always going to be /groupdav/room_name/euid
 */
void groupdav_get(char *dav_pathname) {
	char dav_roomname[1024];
	char dav_uid[1024];
	long dav_msgnum = (-1);
	char buf[1024];
	int n = 0;
	int in_body = 0;
	int found_content_type = 0;

	/* First, break off the "/groupdav/" prefix */
	remove_token(dav_pathname, 0, '/');
	remove_token(dav_pathname, 0, '/');

	/* Now extract the message euid */
	n = num_tokens(dav_pathname, '/');
	extract_token(dav_uid, dav_pathname, n-1, '/', sizeof dav_uid);
	remove_token(dav_pathname, n-1, '/');

	/* What's left is the room name.  Remove trailing slashes. */
	if (dav_pathname[strlen(dav_pathname)-1] == '/') {
		dav_pathname[strlen(dav_pathname)-1] = 0;
	}
	strcpy(dav_roomname, dav_pathname);

	/* Go to the correct room. */
	if (strcasecmp(WC->wc_roomname, dav_roomname)) {
		gotoroom(dav_roomname);
	}
	if (strcasecmp(WC->wc_roomname, dav_roomname)) {
		wprintf("HTTP/1.1 404 not found\r\n");
		groupdav_common_headers();
		wprintf(
			"Content-Type: text/plain\r\n"
			"\r\n"
			"There is no folder called \"%s\" on this server.\r\n",
			dav_roomname
		);
		return;
	}

	/** The reserved item name 'ics' returns the entire calendar.
	 *  FIXME this name may not be what we choose permanently.
	 */
	if (!strcasecmp(dav_uid, "ics")) {
		groupdav_get_big_ics();
		return;
	}

	dav_msgnum = locate_message_by_uid(dav_uid);
	serv_printf("MSG2 %ld", dav_msgnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		wprintf("HTTP/1.1 404 not found\r\n");
		groupdav_common_headers();
		wprintf(
			"Content-Type: text/plain\r\n"
			"\r\n"
			"Object \"%s\" was not found in the \"%s\" folder.\r\n",
			dav_uid,
			dav_roomname
		);
		return;
	}

	wprintf("HTTP/1.1 200 OK\r\n");
	groupdav_common_headers();
	wprintf("etag: \"%ld\"\r\n", dav_msgnum);
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		if (in_body) {
			wprintf("%s\r\n", buf);
		}
		else if (!strncasecmp(buf, "Date: ", 6)) {
			wprintf("%s\r\n", buf);
		}
		else if (!strncasecmp(buf, "Content-type: ", 14)) {
			wprintf("%s\r\n", buf);
			found_content_type = 1;
		}
		else if ((strlen(buf) == 0) && (in_body == 0)) {
			if (!found_content_type) {
				wprintf("Content-type: text/plain\r\n");
			}
			in_body = 1;
			begin_burst();
		}
	}
	end_burst();
}
