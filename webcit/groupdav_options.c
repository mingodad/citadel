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
void groupdav_options(StrBuf *dav_pathname) {
	StrBuf *dav_roomname;
	StrBuf *dav_uid;
	long dav_msgnum = (-1);
	char datestring[256];
	time_t now;

	now = time(NULL);
	http_datestring(datestring, sizeof datestring, now);

	dav_roomname = NewStrBuf();
	dav_uid = NewStrBuf();
	StrBufExtract_token(dav_roomname, dav_pathname, 2, '/');
	StrBufExtract_token(dav_uid, dav_pathname, 3, '/');

	/*
	 * If the room name is blank, the client is doing a top-level OPTIONS.
	 */
	if (StrLength(dav_roomname) == 0) {
		hprintf("HTTP/1.1 200 OK\r\n");
		groupdav_common_headers();
		hprintf("Date: %s\r\n", datestring);
		hprintf("DAV: 1\r\n");
		hprintf("Allow: OPTIONS, PROPFIND\r\n");
		hprintf("\r\n");
		begin_burst();
		end_burst();
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);
		return;
	}

	/* Go to the correct room. */
	if (strcasecmp(ChrPtr(WC->wc_roomname), ChrPtr(dav_roomname))) {
		gotoroom(dav_roomname);
	}

	if (strcasecmp(ChrPtr(WC->wc_roomname), ChrPtr(dav_roomname))) {
		hprintf("HTTP/1.1 404 not found\r\n");
		groupdav_common_headers();
		hprintf("Date: %s\r\n", datestring);
		hprintf(
			"Content-Type: text/plain\r\n"
			"\r\n"
			"There is no folder called \"%s\" on this server.\r\n",
			ChrPtr(dav_roomname)
		);
		begin_burst();
		end_burst();
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);
		return;
	}

	/* If dav_uid is non-empty, client is requesting an OPTIONS on
	 * a specific item in the room.
	 */
	if (StrLength(dav_uid) != 0) {

		dav_msgnum = locate_message_by_uid(ChrPtr(dav_uid));
		if (dav_msgnum < 0) {
			hprintf("HTTP/1.1 404 not found\r\n");
			groupdav_common_headers();
			hprintf(
				"Content-Type: text/plain\r\n"
				"\r\n"
				"Object \"%s\" was not found in the \"%s\" folder.\r\n",
				ChrPtr(dav_uid),
				ChrPtr(dav_roomname)
			);
			FreeStrBuf(&dav_roomname);
			FreeStrBuf(&dav_uid);
			begin_burst();end_burst();return;
		}

		hprintf("HTTP/1.1 200 OK\r\n");
		groupdav_common_headers();
		hprintf("Date: %s\r\n", datestring);
		hprintf("DAV: 1\r\n");
		hprintf("Allow: OPTIONS, PROPFIND, GET, PUT, DELETE\r\n");
		hprintf("\r\n");
		begin_burst();
		end_burst();
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);
		return;
	}

	FreeStrBuf(&dav_roomname);
	FreeStrBuf(&dav_uid);

	/*
	 * We got to this point, which means that the client is requesting
	 * an OPTIONS on the room itself.
	 */
	hprintf("HTTP/1.1 200 OK\r\n");
	groupdav_common_headers();
	hprintf("Date: %s\r\n", datestring);
	hprintf("DAV: 1\r\n");
	hprintf("Allow: OPTIONS, PROPFIND, GET, PUT\r\n");
	hprintf("\r\n");
	begin_burst();
	end_burst();
}
