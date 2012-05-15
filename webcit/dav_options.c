/*
 * Handles DAV OPTIONS requests (experimental -- not required by GroupDAV)
 *
 * Copyright (c) 2005-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"
#include "webserver.h"
#include "dav.h"

/*
 * The pathname is always going to be /groupdav/room_name/msg_num
 */
void dav_options(void)
{
	wcsession *WCC = WC;
	StrBuf *dav_roomname;
	StrBuf *dav_uid;
	long dav_msgnum = (-1);
	char datestring[256];
	time_t now;

	now = time(NULL);
	http_datestring(datestring, sizeof datestring, now);

	dav_roomname = NewStrBuf();
	dav_uid = NewStrBuf();
	StrBufExtract_token(dav_roomname, WCC->Hdr->HR.ReqLine, 0, '/');
	StrBufExtract_token(dav_uid, WCC->Hdr->HR.ReqLine, 1, '/');

	syslog(LOG_DEBUG, "\033[35m%s (logged_in=%d)\033[0m", ChrPtr(WCC->Hdr->HR.ReqLine), WC->logged_in);
	/*
	 * If the room name is blank, the client is doing an OPTIONS on the root.
	 */
	if (StrLength(dav_roomname) == 0) {
		syslog(LOG_DEBUG, "\033[36mOPTIONS requested for root\033[0m");
		hprintf("HTTP/1.1 200 OK\r\n");
		dav_common_headers();
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
	if (strcasecmp(ChrPtr(WC->CurRoom.name), ChrPtr(dav_roomname))) {
		gotoroom(dav_roomname);
	}

	if (strcasecmp(ChrPtr(WC->CurRoom.name), ChrPtr(dav_roomname))) {
		syslog(LOG_DEBUG, "\033[36mOPTIONS requested for invalid item\033[0m");
		hprintf("HTTP/1.1 404 not found\r\n");
		dav_common_headers();
		hprintf("Date: %s\r\n", datestring);
		hprintf(
			"Content-Type: text/plain\r\n");
		begin_burst();
		wc_printf(
			"There is no folder called \"%s\" on this server.\r\n",
			ChrPtr(dav_roomname)
		);
		end_burst();
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);
		return;
	}

	/* If dav_uid is non-empty, client is requesting an OPTIONS on
	 * a specific item in the room.
	 */
	if (StrLength(dav_uid) != 0) {
		syslog(LOG_DEBUG, "\033[36mOPTIONS requested for specific item\033[0m");
		dav_msgnum = locate_message_by_uid(ChrPtr(dav_uid));
		if (dav_msgnum < 0) {
			hprintf("HTTP/1.1 404 not found\r\n");
			dav_common_headers();
			hprintf("Content-Type: text/plain\r\n");
			begin_burst();
			wc_printf(
				"Object \"%s\" was not found in the \"%s\" folder.\r\n",
				ChrPtr(dav_uid),
				ChrPtr(dav_roomname)
			);
			FreeStrBuf(&dav_roomname);
			FreeStrBuf(&dav_uid);
			end_burst();return;
		}

		hprintf("HTTP/1.1 200 OK\r\n");
		dav_common_headers();
		hprintf("Date: %s\r\n", datestring);
		hprintf("DAV: 1\r\n");
		hprintf("Allow: OPTIONS, PROPFIND, GET, PUT, DELETE\r\n");
		
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
	syslog(LOG_DEBUG, "\033[36mOPTIONS requested for room '%s' (%slogged in)\033[0m",
		ChrPtr(WC->CurRoom.name),
		((WC->logged_in) ? "" : "not ")
	);
	hprintf("HTTP/1.1 200 OK\r\n");
	dav_common_headers();
	hprintf("Date: %s\r\n", datestring);

	/*
	 * Offer CalDAV (RFC 4791) if this is a calendar room
	 */
	if ( (WC->CurRoom.view == VIEW_CALENDAR) || (WC->CurRoom.view == VIEW_CALBRIEF) ) {
		hprintf("DAV: 1, calendar-access\r\n");
		syslog(LOG_DEBUG, "\033[36mDAV: 1, calendar-access\033[0m");
	}
	else {
		hprintf("DAV: 1\r\n");
		syslog(LOG_DEBUG, "\033[36mDAV: 1\033[0m");
	}

	hprintf("Allow: OPTIONS, PROPFIND, GET, PUT, REPORT\r\n");
	begin_burst();
	end_burst();
}
