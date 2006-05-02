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
 * The pathname is always going to take one of two formats:
 * /groupdav/room_name/euid	(GroupDAV)
 * /groupdav/room_name		(webcal)
 */
void groupdav_get(char *dav_pathname) {
	char dav_roomname[1024];
	char dav_uid[1024];
	long dav_msgnum = (-1);
	char buf[1024];
	int in_body = 0;
	int found_content_type = 0;

	if (num_tokens(dav_pathname, '/') < 3) {
		wprintf("HTTP/1.1 404 not found\r\n");
		groupdav_common_headers();
		wprintf(
			"Content-Type: text/plain\r\n"
			"\r\n"
			"The object you requested was not found.\r\n"
		);
		return;
	}

	extract_token(dav_roomname, dav_pathname, 2, '/', sizeof dav_roomname);
	extract_token(dav_uid, dav_pathname, 3, '/', sizeof dav_uid);
	if ((!strcasecmp(dav_uid, "ics")) || (!strcasecmp(dav_uid, "calendar.ics"))) {
		strcpy(dav_uid, "");
	}

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

	/** GET on the collection itself returns an ICS of the entire collection.
	 */
	if (!strcasecmp(dav_uid, "")) {
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
			wprintf("%s", buf);
			if (bmstrcasestr(buf, "charset=")) {
				wprintf("%s\r\n", buf);
			}
			else {
				wprintf("%s;charset=UTF-8\r\n", buf);
			}
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
