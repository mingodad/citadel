/*
 * $Id$
 *
 * Handles DAV OPTIONS requests (experimental -- not required by GroupDAV)
 *
 *
 * Copyright (c) 2005-2010 by the citadel.org team
 *
 * This program is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"

/*
 * The pathname is always going to be /groupdav/room_name/msg_num
 */
void groupdav_options(void)
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
	if (strcasecmp(ChrPtr(WC->CurRoom.name), ChrPtr(dav_roomname))) {
		gotoroom(dav_roomname);
	}

	if (strcasecmp(ChrPtr(WC->CurRoom.name), ChrPtr(dav_roomname))) {
		hprintf("HTTP/1.1 404 not found\r\n");
		groupdav_common_headers();
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

		dav_msgnum = locate_message_by_uid(ChrPtr(dav_uid));
		if (dav_msgnum < 0) {
			hprintf("HTTP/1.1 404 not found\r\n");
			groupdav_common_headers();
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
		groupdav_common_headers();
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
	hprintf("HTTP/1.1 200 OK\r\n");
	groupdav_common_headers();
	hprintf("Date: %s\r\n", datestring);
	hprintf("DAV: 1\r\n");
	hprintf("Allow: OPTIONS, PROPFIND, GET, PUT\r\n");
	begin_burst();
	wc_printf("\r\n");
	end_burst();
}
