/*
 * Handles GroupDAV GET requests.
 *
 * Copyright (c) 2005-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
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
 * Fetch the entire contents of the room as one big ics file.
 * This is for "webcal://" type access.
 */	
void dav_get_big_ics(void) {
	char buf[1024];

	serv_puts("ICAL getics");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		hprintf("HTTP/1.1 404 not found\r\n");
		dav_common_headers();
		hprintf("Content-Type: text/plain\r\n");
		begin_burst();
		wc_printf("%s\r\n",
			&buf[4]
			);
		end_burst();
		return;
	}

	hprintf("HTTP/1.1 200 OK\r\n");
	dav_common_headers();
	hprintf("Content-type: text/calendar; charset=UTF-8\r\n");
	begin_burst();
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		wc_printf("%s\r\n", buf);
	}
	end_burst();
}


/* 
 * MIME parser callback function for dav_get()
 * Helps identify the relevant section of a multipart message
 */
void extract_preferred(char *name, char *filename, char *partnum, char *disp,
			void *content, char *cbtype, char *cbcharset,
			size_t length, char *encoding, char *cbid, void *userdata)
{
	struct epdata *epdata = (struct epdata *)userdata;
	int hit = 0;

	/* We only want the first one that we found */
	if (!IsEmptyStr(epdata->found_section)) return;

	/* Check for a content type match */
	if (strlen(epdata->desired_content_type_1) > 0) {
		if (!strcasecmp(epdata->desired_content_type_1, cbtype)) {
			hit = 1;
		}
	}
	if (!IsEmptyStr(epdata->desired_content_type_2)) {
		if (!strcasecmp(epdata->desired_content_type_2, cbtype)) {
			hit = 1;
		}
	}

	/* Is this the one?  If so, output it. */
	if (hit) {
		safestrncpy(epdata->found_section, partnum, sizeof epdata->found_section);
		if (!IsEmptyStr(cbcharset)) {
			safestrncpy(epdata->charset, cbcharset, sizeof epdata->charset);
		}
		hprintf("Content-type: %s; charset=%s\r\n", cbtype, epdata->charset);
		begin_burst();
		StrBufAppendBufPlain(WC->WBuf, content, length, 0);
		end_burst();
	}
}



/*
 * The pathname is always going to take one of two formats:
 * /groupdav/room_name/euid	(GroupDAV)
 * /groupdav/room_name		(webcal)
 */
void dav_get(void)
{
	wcsession *WCC = WC;
	StrBuf *dav_roomname;
	StrBuf *dav_uid;
	long dav_msgnum = (-1);
	char buf[1024];
	int in_body = 0;
	char *ptr;
	char *endptr;
	char *msgtext = NULL;
	size_t msglen = 0;
	size_t msgalloc = 0;
	int linelen;
	char content_type[128];
	char charset[128];
	char date[128];
	struct epdata epdata;

	if (StrBufNum_tokens(WCC->Hdr->HR.ReqLine, '/') < 2) {
		hprintf("HTTP/1.1 404 not found\r\n");
		dav_common_headers();
		hprintf("Content-Type: text/plain\r\n");
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
	if (strcasecmp(ChrPtr(WCC->CurRoom.name), ChrPtr(dav_roomname))) {
		gotoroom(dav_roomname);
	}
	if (strcasecmp(ChrPtr(WCC->CurRoom.name), ChrPtr(dav_roomname))) {
		hprintf("HTTP/1.1 404 not found\r\n");
		dav_common_headers();
		hprintf("Content-Type: text/plain\r\n");
		wc_printf("There is no folder called \"%s\" on this server.\r\n",
			ChrPtr(dav_roomname));
		end_burst();
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);
		return;
	}

	/** GET on the collection itself returns an ICS of the entire collection.
	 */
	if (StrLength(dav_uid) == 0) {
		dav_get_big_ics();
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);
		return;
	}

	dav_msgnum = locate_message_by_uid(ChrPtr(dav_uid));
	serv_printf("MSG2 %ld", dav_msgnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		hprintf("HTTP/1.1 404 not found\r\n");
		dav_common_headers();
		hprintf("Content-Type: text/plain\r\n");
		wc_printf("Object \"%s\" was not found in the \"%s\" folder.\r\n",
			ChrPtr(dav_uid),
			ChrPtr(dav_roomname));
		end_burst();
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);
		return;
	}
	FreeStrBuf(&dav_roomname);
	FreeStrBuf(&dav_uid);

	/* We got it; a message is now arriving from the server.  Read it in. */

	in_body = 0;
	strcpy(charset, "UTF-8");
	strcpy(content_type, "text/plain");
	strcpy(date, "");
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		linelen = strlen(buf);

		/* Append it to the buffer */
		if ((msglen + linelen + 3) > msgalloc) {
			msgalloc = ( (msgalloc > 0) ? (msgalloc * 2) : 1024 );
			msgtext = realloc(msgtext, msgalloc);
		}
		strcpy(&msgtext[msglen], buf);
		msglen += linelen;
		strcpy(&msgtext[msglen], "\n");
		msglen += 1;

		/* Also learn some things about the message */
		if (linelen == 0) {
			in_body = 1;
		}
		if (!in_body) {
			if (!strncasecmp(buf, "Date:", 5)) {
				safestrncpy(date, &buf[5], sizeof date);
				striplt(date);
			}
			if (!strncasecmp(buf, "Content-type:", 13)) {
				safestrncpy(content_type, &buf[13], sizeof content_type);
				striplt(content_type);
				ptr = bmstrcasestr(&buf[13], "charset=");
				if (ptr) {
					safestrncpy(charset, ptr+8, sizeof charset);
					striplt(charset);
					endptr = strchr(charset, ';');
					if (endptr != NULL) strcpy(endptr, "");
				}
				endptr = strchr(content_type, ';');
				if (endptr != NULL) strcpy(endptr, "");
			}
		}
	}
	msgtext[msglen] = 0;

	/* Output headers common to single or multi part messages */

	hprintf("HTTP/1.1 200 OK\r\n");
	dav_common_headers();
	hprintf("etag: \"%ld\"\r\n", dav_msgnum);
	hprintf("Date: %s\r\n", date);

	memset(&epdata, 0, sizeof(struct epdata));
	safestrncpy(epdata.charset, charset, sizeof epdata.charset);

	/* If we have a multipart message on our hands, and we are in a groupware room,
	 * strip it down to only the relevant part.
	 */
	if (!strncasecmp(content_type, "multipart/", 10)) {

		if ( (WCC->CurRoom.defview == VIEW_CALENDAR) || (WCC->CurRoom.defview == VIEW_TASKS) ) {
			strcpy(epdata.desired_content_type_1, "text/calendar");
		}

		else if (WCC->CurRoom.defview == VIEW_ADDRESSBOOK) {
			strcpy(epdata.desired_content_type_1, "text/vcard");
			strcpy(epdata.desired_content_type_2, "text/x-vcard");
		}

		mime_parser(msgtext, &msgtext[msglen], extract_preferred, NULL, NULL, (void *)&epdata, 0);
	}

	/* If epdata.found_section is empty, we haven't output anything yet, so output the whole thing */

	if (IsEmptyStr(epdata.found_section)) {
		ptr = msgtext;
		endptr = &msgtext[msglen];
	
		hprintf("Content-type: %s; charset=%s\r\n", content_type, charset);
	
		in_body = 0;
		do {
			ptr = memreadline(ptr, buf, sizeof buf);
	
			if (in_body) {
				wc_printf("%s\r\n", buf);
			}
			else if ((buf[0] == 0) && (in_body == 0)) {
				in_body = 1;
				begin_burst();
			}
		} while (ptr < endptr);
	
		end_burst();
	}

	free(msgtext);
}
