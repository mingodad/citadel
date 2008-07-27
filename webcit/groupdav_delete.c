/*
 * $Id$
 *
 * Handles GroupDAV DELETE requests.
 *
 */

#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"


/*
 * The pathname is always going to be /groupdav/room_name/euid
 */
void groupdav_delete(char *dav_pathname, char *dav_ifmatch) {
	char dav_roomname[SIZ];
	char dav_uid[SIZ];
	long dav_msgnum = (-1);
	char buf[SIZ];
	int n = 0;
	int len;

	/* First, break off the "/groupdav/" prefix */
	remove_token(dav_pathname, 0, '/');
	remove_token(dav_pathname, 0, '/');

	/* Now extract the message euid */
	n = num_tokens(dav_pathname, '/');
	extract_token(dav_uid, dav_pathname, n-1, '/', sizeof dav_uid);
	remove_token(dav_pathname, n-1, '/');

	/* What's left is the room name.  Remove trailing slashes. */
	len = strlen(dav_pathname);
	if (dav_pathname[len-1] == '/') {
		dav_pathname[len-1] = 0;
	}
	strcpy(dav_roomname, dav_pathname);

	/* Go to the correct room. */
	if (strcasecmp(WC->wc_roomname, dav_roomname)) {
		gotoroom(dav_roomname);
	}
	if (strcasecmp(WC->wc_roomname, dav_roomname)) {
		hprintf("HTTP/1.1 404 not found\r\n");
		groupdav_common_headers();
		hprintf("Content-Length: 0\r\n\r\n");
		return;
	}

	dav_msgnum = locate_message_by_uid(dav_uid);

	/*
	 * If no item exists with the requested uid ... simple error.
	 */
	if (dav_msgnum < 0L) {
		hprintf("HTTP/1.1 404 Not Found\r\n");
		groupdav_common_headers();
		hprintf("Content-Length: 0\r\n\r\n");
		return;
	}

	/*
	 * It's there ... check the ETag and make sure it matches
	 * the message number.
	 */
	if (!IsEmptyStr(dav_ifmatch)) {
		if (atol(dav_ifmatch) != dav_msgnum) {
			hprintf("HTTP/1.1 412 Precondition Failed\r\n");
			groupdav_common_headers();
			hprintf("Content-Length: 0\r\n\r\n");
			return;
		}
	}

	/*
	 * Ok, attempt to delete the item.
	 */
	serv_printf("DELE %ld", dav_msgnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		hprintf("HTTP/1.1 204 No Content\r\n");	/* success */
		groupdav_common_headers();
		hprintf("Content-Length: 0\r\n\r\n");
	}
	else {
		hprintf("HTTP/1.1 403 Forbidden\r\n");	/* access denied */
		groupdav_common_headers();
		hprintf("Content-Length: 0\r\n\r\n");
	}
	return;
}
