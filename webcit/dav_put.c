/*
 * Handles GroupDAV PUT requests.
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
 * This function is for uploading an ENTIRE calendar, not just one
 * component.  This would be for webcal:// 'publish' operations, not
 * for GroupDAV.
 */
void dav_put_bigics(void)
{
	wcsession *WCC = WC;
	char buf[1024];

	/*
	 * Tell the server that when we save a calendar event, we
	 * do *not* want the server to generate invitations. 
	 */
	serv_puts("ICAL sgi|0");
	serv_getln(buf, sizeof buf);

	serv_puts("ICAL putics");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '4') {
		hprintf("HTTP/1.1 502 Bad Gateway\r\n");
		dav_common_headers();
		hprintf("Content-type: text/plain\r\n");
		begin_burst();
		wc_printf("%s\r\n", &buf[4]);
		end_burst();
		return;
	}

	serv_putbuf(WCC->upload);
	serv_printf("\n000");

	/* Report success and not much else. */
	hprintf("HTTP/1.1 204 No Content\r\n");
	syslog(LOG_DEBUG, "HTTP/1.1 204 No Content\r\n");
	dav_common_headers();
	begin_burst();
	end_burst();
}



/*
 * The pathname is always going to take one of two formats:
 * [/groupdav/]room_name/euid	(GroupDAV)
 * [/groupdav/]room_name		(webcal)
 */
void dav_put(void) 
{
	wcsession *WCC = WC;
	StrBuf *dav_roomname;
	StrBuf *dav_uid;
	long new_msgnum = (-2L);
	long old_msgnum = (-1L);
	char buf[SIZ];
	int n = 0;

	if (StrBufNum_tokens(WCC->Hdr->HR.ReqLine, '/') < 2) {
		hprintf("HTTP/1.1 404 not found\r\n");
		dav_common_headers();
		hprintf("Content-Type: text/plain\r\n");
		begin_burst();
		wc_printf("The object you requested was not found.\r\n");
		end_burst();
		return;
	}

	dav_roomname = NewStrBuf();;
	dav_uid = NewStrBuf();;
	StrBufExtract_token(dav_roomname, WCC->Hdr->HR.ReqLine, 0, '/');
	StrBufExtract_token(dav_uid, WCC->Hdr->HR.ReqLine, 1, '/');
	if ((!strcasecmp(ChrPtr(dav_uid), "ics")) || 
	    (!strcasecmp(ChrPtr(dav_uid), "calendar.ics"))) {
		FlushStrBuf(dav_uid);
	}

	/* Go to the correct room. */
	if (strcasecmp(ChrPtr(WC->CurRoom.name), ChrPtr(dav_roomname))) {
		gotoroom(dav_roomname);
	}
	if (strcasecmp(ChrPtr(WC->CurRoom.name), ChrPtr(dav_roomname))) {
		hprintf("HTTP/1.1 404 not found\r\n");
		dav_common_headers();
		hprintf("Content-Type: text/plain\r\n");
		begin_burst();
		wc_printf("There is no folder called \"%s\" on this server.\r\n",
			ChrPtr(dav_roomname));
		end_burst();
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);		
		return;
	}

	/*
	 * If an HTTP If-Match: header is present, the client is attempting
	 * to replace an existing item.  We have to check to see if the
	 * message number associated with the supplied uid matches what the
	 * client is expecting.  If not, the server probably contains a newer
	 * version, so we fail...
	 */
	if (StrLength(WCC->Hdr->HR.dav_ifmatch) > 0) {
		syslog(LOG_DEBUG, "dav_ifmatch: %s\n", ChrPtr(WCC->Hdr->HR.dav_ifmatch));
		old_msgnum = locate_message_by_uid(ChrPtr(dav_uid));
		syslog(LOG_DEBUG, "old_msgnum:  %ld\n", old_msgnum);
		if (StrTol(WCC->Hdr->HR.dav_ifmatch) != old_msgnum) {
			hprintf("HTTP/1.1 412 Precondition Failed\r\n");
			syslog(LOG_INFO, "HTTP/1.1 412 Precondition Failed (ifmatch=%ld, old_msgnum=%ld)\r\n",
				StrTol(WCC->Hdr->HR.dav_ifmatch), old_msgnum);
			dav_common_headers();
			
			end_burst();
			FreeStrBuf(&dav_roomname);
			FreeStrBuf(&dav_uid);
			return;
		}
	}

	/** PUT on the collection itself uploads an ICS of the entire collection.
	 */
	if (StrLength(dav_uid) == 0) {
		dav_put_bigics();
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);
		return;
	}

	/*
	 * We are cleared for upload!  We use the new calling syntax for ENT0
	 * which allows a confirmation to be sent back to us.  That's how we
	 * extract the message ID.
	 */
	serv_puts("ENT0 1|||4|||1|");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '8') {
		hprintf("HTTP/1.1 502 Bad Gateway\r\n");
		dav_common_headers();
		hprintf("Content-type: text/plain\r\n");
		begin_burst();
		wc_printf("%s\r\n", &buf[4]);
		end_burst();
		return;
	}

	/* Send the content to the Citadel server */
	//serv_printf("Content-type: %s\n\n", WCC->upload_content_type);
	serv_putbuf(WCC->upload);
	serv_puts("\n000");

	/* Fetch the reply from the Citadel server */
	n = 0;
	FlushStrBuf(dav_uid);
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		switch(n++) {
		case 0: 
			new_msgnum = atol(buf);
			break;
		case 1:	
			syslog(LOG_DEBUG, "new_msgnum=%ld (%s)\n", new_msgnum, buf);
			break;
		case 2: 
			StrBufAppendBufPlain(dav_uid, buf, -1, 0);
			break;
		default:
			break;
		}
	}

	/* Tell the client what happened. */

	/* Citadel failed in some way? */
	if (new_msgnum < 0L) {
		hprintf("HTTP/1.1 502 Bad Gateway\r\n");
		dav_common_headers();
		hprintf("Content-type: text/plain\r\n");
		begin_burst();
		wc_printf("new_msgnum is %ld\r\n"
			"\r\n", new_msgnum);
		end_burst();
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);
		return;
	}

	/* We created this item for the first time. */
	if (old_msgnum < 0L) {
	        char escaped_uid[1024];
		hprintf("HTTP/1.1 201 Created\r\n");
		syslog(LOG_DEBUG, "HTTP/1.1 201 Created\r\n");
		dav_common_headers();
		hprintf("etag: \"%ld\"\r\n", new_msgnum);
		hprintf("Location: ");
		dav_identify_hosthdr();
		hprintf("/groupdav/");/* TODO */
		hurlescputs(ChrPtr(dav_roomname));
	        euid_escapize(escaped_uid, ChrPtr(dav_uid));
	        hprintf("/%s\r\n", escaped_uid);
		end_burst();
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);
		return;
	}

	/* We modified an existing item. */
	hprintf("HTTP/1.1 204 No Content\r\n");
	syslog(LOG_DEBUG, "HTTP/1.1 204 No Content\r\n");
	dav_common_headers();
	hprintf("Etag: \"%ld\"\r\n", new_msgnum);
	/* The item we replaced has probably already been deleted by
	 * the Citadel server, but we'll do this anyway, just in case.
	 */
	serv_printf("DELE %ld", old_msgnum);
	serv_getln(buf, sizeof buf);
	begin_burst();
	end_burst();
	FreeStrBuf(&dav_roomname);
	FreeStrBuf(&dav_uid);
	return;
}
