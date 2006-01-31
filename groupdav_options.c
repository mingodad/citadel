/*
 * $Id$
 *
 * Handles DAV OPTIONS requests (experimental -- not required by GroupDAV)
 *
 */

#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"

/*
 * The pathname is always going to be /groupdav/room_name/msg_num
 */
void groupdav_options(char *dav_pathname) {
	char dav_roomname[256];
	char dav_uid[256];
	long dav_msgnum = (-1);
	char datestring[256];
	time_t now;

	now = time(NULL);
	http_datestring(datestring, sizeof datestring, now);

	extract_token(dav_roomname, dav_pathname, 2, '/', sizeof dav_roomname);
	extract_token(dav_uid, dav_pathname, 3, '/', sizeof dav_uid);

	/*
	 * If the room name is blank, the client is doing a top-level OPTIONS.
	 */
	if (strlen(dav_roomname) == 0) {
		wprintf("HTTP/1.1 200 OK\r\n");
		groupdav_common_headers();
		wprintf("Date: %s\r\n", datestring);
		wprintf("Allow: OPTIONS, PROPFIND\r\n");
		wprintf("\r\n");
		return;
	}

	/* Go to the correct room. */
	if (strcasecmp(WC->wc_roomname, dav_roomname)) {
		gotoroom(dav_roomname);
	}
	if (strcasecmp(WC->wc_roomname, dav_roomname)) {
		wprintf("HTTP/1.1 404 not found\r\n");
		groupdav_common_headers();
		wprintf("Date: %s\r\n", datestring);
		wprintf(
			"Content-Type: text/plain\r\n"
			"\r\n"
			"There is no folder called \"%s\" on this server.\r\n",
			dav_roomname
		);
		return;
	}

	/* If dav_uid is non-empty, client is requesting an OPTIONS on
	 * a specific item in the room.
	 */
	if (strlen(dav_uid) > 0) {

		dav_msgnum = locate_message_by_uid(dav_uid);
		if (dav_msgnum < 0) {
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
		wprintf("Date: %s\r\n", datestring);
		wprintf("Allow: OPTIONS, PROPFIND, GET, PUT, DELETE\r\n");
		wprintf("\r\n");
		return;
	}


	/*
	 * We got to this point, which means that the client is requesting
	 * an OPTIONS on the room itself.
	 */
	wprintf("HTTP/1.1 200 OK\r\n");
	groupdav_common_headers();
	wprintf("Date: %s\r\n", datestring);
	wprintf("Allow: OPTIONS, PROPFIND, GET, PUT\r\n");
	wprintf("\r\n");
}
