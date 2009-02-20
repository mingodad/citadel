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
void groupdav_delete(StrBuf *dav_pathname, char *dav_ifmatch) {
	char dav_uid[SIZ];
	long dav_msgnum = (-1);
	char buf[SIZ];
	int n = 0;

	/* First, break off the "/groupdav/" prefix */
	StrBufCutLeft(dav_pathname, 9);

	/* Now extract the message euid */
	n = StrBufNum_tokens(dav_pathname, '/');
	extract_token(dav_uid, ChrPtr(dav_pathname), n-1, '/', sizeof dav_uid);
	StrBufRemove_token(dav_pathname, n-1, '/');

	/* What's left is the room name.  Remove trailing slashes. */
	//len = StrLength(dav_pathname);
	//if ((len > 0) && (ChrPtr(dav_pathname)[len-1] == '/')) {
	//	StrBufCutRight(dav_pathname, 1);
	//}
	StrBufCutLeft(dav_pathname, 1);

	/* Go to the correct room. */
	if (strcasecmp(ChrPtr(WC->wc_roomname), ChrPtr(dav_pathname))) {
		gotoroom(dav_pathname);
	}
	if (strcasecmp(ChrPtr(WC->wc_roomname), ChrPtr(dav_pathname))) {
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
