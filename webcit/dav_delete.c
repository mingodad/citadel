/*
 * Handles GroupDAV DELETE requests.
 *
 * Copyright (c) 2005-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 * 
 * 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * 
 * 
 * 
 */

#include "webcit.h"
#include "webserver.h"
#include "dav.h"


/*
 * The pathname is always going to be /groupdav/room_name/euid
 */
void dav_delete(void) 
{
	wcsession *WCC = WC;
	char dav_uid[SIZ];
	long dav_msgnum = (-1);
	char buf[SIZ];
	int n = 0;
	StrBuf *dav_roomname = NewStrBuf();
	
	/* Now extract the message euid */
	n = StrBufNum_tokens(WCC->Hdr->HR.ReqLine, '/');
	extract_token(dav_uid, ChrPtr(WCC->Hdr->HR.ReqLine), n-1, '/', sizeof dav_uid);
	StrBufExtract_token(dav_roomname, WCC->Hdr->HR.ReqLine, 0, '/');

	///* What's left is the room name.  Remove trailing slashes. */
	//len = StrLength(WCC->Hdr->HR.ReqLine);
	//if ((len > 0) && (ChrPtr(WCC->Hdr->HR.ReqLinee)[len-1] == '/')) {
	//	StrBufCutRight(WCC->Hdr->HR.ReqLine, 1);
	//}
	//StrBufCutLeft(WCC->Hdr->HR.ReqLine, 1);

	/* Go to the correct room. */
	if (strcasecmp(ChrPtr(WC->CurRoom.name), ChrPtr(dav_roomname))) {
		gotoroom(dav_roomname);
	}
	if (strcasecmp(ChrPtr(WC->CurRoom.name), ChrPtr(dav_roomname))) {
		hprintf("HTTP/1.1 404 not found\r\n");
		dav_common_headers();
		hprintf("Content-Length: 0\r\n\r\n");
		begin_burst();
		end_burst();
		FreeStrBuf(&dav_roomname);
		return;
	}

	dav_msgnum = locate_message_by_uid(dav_uid);

	/*
	 * If no item exists with the requested uid ... simple error.
	 */
	if (dav_msgnum < 0L) {
		hprintf("HTTP/1.1 404 Not Found\r\n");
		dav_common_headers();
		hprintf("Content-Length: 0\r\n\r\n");
		begin_burst();
		end_burst();
		FreeStrBuf(&dav_roomname);
		return;
	}

	/*
	 * It's there ... check the ETag and make sure it matches
	 * the message number.
	 */
	if (StrLength(WCC->Hdr->HR.dav_ifmatch) > 0) {
		if (StrTol(WCC->Hdr->HR.dav_ifmatch) != dav_msgnum) {
			hprintf("HTTP/1.1 412 Precondition Failed\r\n");
			dav_common_headers();
			hprintf("Content-Length: 0\r\n\r\n");
			begin_burst();
			end_burst();
			FreeStrBuf(&dav_roomname);
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
		dav_common_headers();
		hprintf("Content-Length: 0\r\n\r\n");
		begin_burst();
		end_burst();
	}
	else {
		hprintf("HTTP/1.1 403 Forbidden\r\n");	/* access denied */
		dav_common_headers();
		hprintf("Content-Length: 0\r\n\r\n");
		begin_burst();
		end_burst();
	}
	FreeStrBuf(&dav_roomname);
	return;
}
