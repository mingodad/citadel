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
		hprintf("HTTP/1.1 404 not found\r\n");
		groupdav_common_headers();
		hprintf("Content-Type: text/plain\r\n");
		wprintf("%s\r\n",
			&buf[4]
			);/// TODO: do we need to end-burst here?
		return;
	}

	hprintf("HTTP/1.1 200 OK\r\n");
	groupdav_common_headers();
	hprintf("Content-type: text/calendar; charset=UTF-8\r\n");
	begin_burst();
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		wprintf("%s\r\n", buf);
	}
	end_burst();
}


/* 
 * MIME parser callback function for groupdav_get()
 * Helps identify the relevant section of a multipart message
 */
void extract_preferred(char *name, char *filename, char *partnum, char *disp,
			void *content, char *cbtype, char *cbcharset,
			size_t length, char *encoding, void *userdata)
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
	struct epdata epdata;

	if (num_tokens(dav_pathname, '/') < 3) {
		hprintf("HTTP/1.1 404 not found\r\n");
		groupdav_common_headers();
		hprintf("Content-Type: text/plain\r\n");
		wprintf("The object you requested was not found.\r\n");
		end_burst();
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
		hprintf("HTTP/1.1 404 not found\r\n");
		groupdav_common_headers();
		hprintf("Content-Type: text/plain\r\n");
		wprintf("There is no folder called \"%s\" on this server.\r\n",
			dav_roomname);
		end_burst();
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
		hprintf("HTTP/1.1 404 not found\r\n");
		groupdav_common_headers();
		hprintf("Content-Type: text/plain\r\n");
		wprintf("Object \"%s\" was not found in the \"%s\" folder.\r\n",
			dav_uid,
			dav_roomname);
		end_burst();
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

	/* Output headers common to single or multi part messages */

	hprintf("HTTP/1.1 200 OK\r\n");
	groupdav_common_headers();
	hprintf("etag: \"%ld\"\r\n", dav_msgnum);
	hprintf("Date: %s\r\n", date);

	memset(&epdata, 0, sizeof(struct epdata));
	safestrncpy(epdata.charset, charset, sizeof epdata.charset);

	/* If we have a multipart message on our hands, and we are in a groupware room,
	 * strip it down to only the relevant part.
	 */
	if (!strncasecmp(content_type, "multipart/", 10)) {

		if ( (WC->wc_default_view == VIEW_CALENDAR) || (WC->wc_default_view == VIEW_TASKS) ) {
			strcpy(epdata.desired_content_type_1, "text/calendar");
		}

		else if (WC->wc_default_view == VIEW_ADDRESSBOOK) {
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
				wprintf("%s\r\n", buf);
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
