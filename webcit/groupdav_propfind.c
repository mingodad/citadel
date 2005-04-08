/*
 * $Id$
 *
 * Handles GroupDAV PROPFIND requests.
 *
 * A few notes about our XML output:
 *
 * --> Yes, we are spewing tags directly instead of using an XML library.
 *     If you would like to rewrite this using libxml2, code it up and submit
 *     a patch.  Whining will be summarily ignored.
 *
 * --> XML is deliberately output with no whitespace/newlines between tags.
 *     This makes it difficult to read, but we have discovered clients which
 *     crash when you try to pretty it up.
 *
 */

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <limits.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"


/*
 * Given an encoded UID, translate that to an unencoded Citadel EUID and
 * then search for it in the current room.  Return a message number or -1
 * if not found.
 *
 * NOTE: this function relies on the Citadel server's brute-force search.
 * There's got to be a way to optimize this better.
 */
long locate_message_by_uid(char *uid) {
	char buf[SIZ];
	char decoded_uid[SIZ];
	long retval = (-1L);

	/* Decode the uid */
	euid_unescapize(decoded_uid, uid);

	serv_puts("MSGS ALL|0|1");
	serv_gets(buf);
	if (buf[0] == '8') {
		serv_printf("exti|%s", decoded_uid);
		serv_puts("000");
		while (serv_gets(buf), strcmp(buf, "000")) {
			retval = atol(buf);
		}
	}
	return(retval);
}


/*
 * List folders containing interesting groupware objects
 */
void groupdav_folder_list(void) {
	char buf[SIZ];
	char roomname[SIZ];
	int view;
	char datestring[SIZ];
	time_t now;

	now = time(NULL);
	http_datestring(datestring, sizeof datestring, now);

	/*
	 * Be rude.  Completely ignore the XML request and simply send them
	 * everything we know about.  Let the client sort it out.
	 */
	wprintf("HTTP/1.0 207 Multi-Status\r\n");
	groupdav_common_headers();
	wprintf("Date: %s\r\n", datestring);
	wprintf("Content-type: text/xml\r\n");
	wprintf("Content-encoding: identity\r\n");

	begin_burst();

	wprintf("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
     		"<D:multistatus xmlns:D=\"DAV:\">"
	);

	serv_puts("LKRA");
	serv_gets(buf);
	if (buf[0] == '1') while (serv_gets(buf), strcmp(buf, "000")) {

		extract_token(roomname, buf, 0, '|', sizeof roomname);
		view = extract_int(buf, 6);

		/*
		 * For now, only list rooms that we know a GroupDAV client
		 * might be interested in.  In the future we may add
		 * the rest.
		 */
		if ((view == VIEW_CALENDAR)
		   || (view == VIEW_TASKS)
		   || (view == VIEW_ADDRESSBOOK) ) {

			wprintf("<D:response>");

			wprintf("<D:href>");
			if (strlen(WC->http_host) > 0) {
				wprintf("%s://%s",
					(is_https ? "https" : "http"),
					WC->http_host);
			}
			wprintf("/groupdav/");
			urlescputs(roomname);
			wprintf("/</D:href>");

			wprintf("<D:propstat>");
			wprintf("<D:status>HTTP/1.1 200 OK</D:status>");
			wprintf("<D:prop>");
			wprintf("<D:displayname>");
			escputs(roomname);
			wprintf("</D:displayname>");
			wprintf("<D:resourcetype><D:collection/>");

			switch(view) {
				case VIEW_CALENDAR:
					wprintf("<G:vevent-collection />");
					break;
				case VIEW_TASKS:
					wprintf("<G:vtodo-collection />");
					break;
				case VIEW_ADDRESSBOOK:
					wprintf("<G:vcard-collection />");
					break;
			}

			wprintf("</D:resourcetype>");
			wprintf("</D:prop>");
			wprintf("</D:propstat>");
			wprintf("</D:response>");
		}
	}
	wprintf("</D:multistatus>\n");

	end_burst();
}



/*
 * The pathname is always going to be /groupdav/room_name/msg_num
 */
void groupdav_propfind(char *dav_pathname) {
	char dav_roomname[SIZ];
	char msgnum[SIZ];
	char buf[SIZ];
	char uid[SIZ];
	char encoded_uid[SIZ];
	long *msgs = NULL;
	int num_msgs = 0;
	int i;
	char datestring[SIZ];
	time_t now;

	now = time(NULL);
	http_datestring(datestring, sizeof datestring, now);


	/* First, break off the "/groupdav/" prefix */
	remove_token(dav_pathname, 0, '/');
	remove_token(dav_pathname, 0, '/');

	/* What's left is the room name.  Remove trailing slashes. */
	if (dav_pathname[strlen(dav_pathname)-1] == '/') {
		dav_pathname[strlen(dav_pathname)-1] = 0;
	}
	strcpy(dav_roomname, dav_pathname);


	/*
	 * If the room name is blank, the client is requesting a
	 * folder list.
	 */
	if (strlen(dav_roomname) == 0) {
		groupdav_folder_list();
		return;
	}

	/* Go to the correct room. */
	if (strcasecmp(WC->wc_roomname, dav_roomname)) {
		gotoroom(dav_roomname);
	}
	if (strcasecmp(WC->wc_roomname, dav_roomname)) {
		wprintf("HTTP/1.1 404 not found\r\n");
		groupdav_common_headers();
		wprintf("Date: %s\r\n", datestring);
		wprintf(
			"Content-Type: text/plain\r\n"
			"\r\n"
			"There is no folder called \"%s\" on this server.\r\n",
			dav_roomname
		);
		return;
	}

	/*
	 * Be rude.  Completely ignore the XML request and simply send them
	 * everything we know about (which is going to simply be the ETag and
	 * nothing else).  Let the client-side parser sort it out.
	 */
	wprintf("HTTP/1.0 207 Multi-Status\r\n");
	groupdav_common_headers();
	wprintf("Date: %s\r\n", datestring);
	wprintf("Content-type: text/xml\r\n");
	wprintf("Content-encoding: identity\r\n");

	begin_burst();

	wprintf("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
     		"<D:multistatus xmlns:D=\"DAV:\">"
	);

	serv_puts("MSGS ALL");
	serv_gets(buf);
	if (buf[0] == '1') while (serv_gets(msgnum), strcmp(msgnum, "000")) {
		msgs = realloc(msgs, ++num_msgs * sizeof(long));
		msgs[num_msgs-1] = atol(msgnum);
	}

	if (num_msgs > 0) for (i=0; i<num_msgs; ++i) {

		strcpy(uid, "");
		serv_printf("MSG0 %ld|3", msgs[i]);
		serv_gets(buf);
		if (buf[0] == '1') while (serv_gets(buf), strcmp(buf, "000")) {
			if (!strncasecmp(buf, "exti=", 5)) {
				strcpy(uid, &buf[5]);
			}
		}

		if (strlen(uid) > 0) {
			wprintf("<D:response>");
			wprintf("<D:href>");
			if (strlen(WC->http_host) > 0) {
				wprintf("%s://%s",
					(is_https ? "https" : "http"),
					WC->http_host);
			}
			wprintf("/groupdav/");
			urlescputs(WC->wc_roomname);
			euid_escapize(encoded_uid, uid);
			wprintf("/%s", encoded_uid);
			wprintf("</D:href>");
			wprintf("<D:propstat>");
			wprintf("<D:status>HTTP/1.1 200 OK</D:status>");
			wprintf("<D:prop><D:getetag>\"%ld\"</D:getetag></D:prop>", msgs[i]);
			wprintf("</D:propstat>");
			wprintf("</D:response>");
		}
	}

	wprintf("</D:multistatus>\n");
	end_burst();

	if (msgs != NULL) {
		free(msgs);
	}
}
