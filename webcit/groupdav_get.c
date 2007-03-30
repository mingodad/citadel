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
	char *ptr;
	char *endptr;
	char *msgtext = NULL;
	size_t msglen = 0;
	size_t msgalloc = 0;
	int linelen;
	char content_type[128];
	char charset[128];
	char date[128];

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

	/* We got it; a message is now arriving from the server.  Read it in. */

	in_body = 0;
	found_content_type = 0;
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
	lprintf(9, "CONTENT TYPE: '%s'\n", content_type);
	lprintf(9, "CHARSET: '%s'\n", charset);
	lprintf(9, "DATE: '%s'\n", date);

	/* Now do something with it.  FIXME boil it down to only the part we need */

	ptr = msgtext;
	endptr = &msgtext[msglen];

	wprintf("HTTP/1.1 200 OK\r\n");
	groupdav_common_headers();
	wprintf("etag: \"%ld\"\r\n", dav_msgnum);
	wprintf("Content-type: %s; charset=%s\r\n", content_type, charset);
	wprintf("Date: %s\r\n", date);

	in_body = 0;
	do {
		ptr = memreadline(ptr, buf, sizeof buf);

		if (in_body) {
			wprintf("%s\r\n", buf);
		}
		else if ((strlen(buf) == 0) && (in_body == 0)) {
			in_body = 1;
			begin_burst();
		}
	} while (ptr < endptr);

	end_burst();

	free(msgtext);
}
