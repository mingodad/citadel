/*
 * $Id$
 *
 * Handles GroupDAV PROPFIND requests.
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
	int i, j;
	int ch;
	long retval = (-1L);

	/* Decode the uid */
	j=0;
	for (i=0; i<strlen(uid); i=i+2) {
		ch = 0;
		sscanf(&uid[i], "%02x", &ch);
		decoded_uid[j] = ch;
		decoded_uid[j+1] = 0;
		++j;
	}

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
	/*
	 * Be rude.  Completely ignore the XML request and simply send them
	 * everything we know about.  Let the client sort it out.
	 */
	wprintf("HTTP/1.0 207 Multi-Status\n");
	groupdav_common_headers();
	wprintf("Content-type: text/xml\n"
		"\n"
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
     		"<D:multistatus xmlns:D=\"DAV:\">\n"
	);

	wprintf("        <D:response>\n");
	wprintf("          <D:href>http://splorph.xand.com/groupdav/Calendar/</D:href>\n");
	wprintf("          <D:propstat>\n");
	wprintf("            <D:status>HTTP/1.1 200 OK</D:status>\n");
	wprintf("            <D:prop>\n");
	wprintf("             <D:displayname>Calendar</D:displayname>\n");
	wprintf("             <resourcetype xmlns=\"DAV:\" xmlns:G=\"http://groupdav.org/\">\n");
	wprintf("               <collection />\n");
	wprintf("               <G:vevent-collection />\n");
	wprintf("             <resourcetype>\n");
	wprintf("            </D:prop>\n");
	wprintf("          </D:propstat>\n");
	wprintf("        </D:response>\n");

	wprintf("</D:multistatus>\n");

}



/*
 * The pathname is always going to be /groupdav/room_name/msg_num
 */
void groupdav_propfind(char *dav_pathname) {
	char dav_roomname[SIZ];
	char msgnum[SIZ];
	char buf[SIZ];
	char uid[SIZ];
	long *msgs = NULL;
	int num_msgs;
	int i, j;

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
		wprintf("HTTP/1.1 404 not found\n");
		groupdav_common_headers();
		wprintf(
			"Content-Type: text/plain\n"
			"\n"
			"There is no folder called \"%s\" on this server.\n",
			dav_roomname
		);
		return;
	}

	/*
	 * Be rude.  Completely ignore the XML request and simply send them
	 * everything we know about (which is going to simply be the ETag and
	 * nothing else).  Let the client-side parser sort it out.
	 */
	wprintf("HTTP/1.0 207 Multi-Status\n");
	groupdav_common_headers();
	wprintf("Content-type: text/xml\n"
		"\n"
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
     		"<D:multistatus xmlns:D=\"DAV:\">\n"
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
			wprintf(" <D:response>\n");
			wprintf("  <D:href>");
			if (strlen(WC->http_host) > 0) {
				wprintf("%s://%s",
					(is_https ? "https" : "http"),
					WC->http_host);
			}
			wprintf("/groupdav/");
			urlescputs(WC->wc_roomname);
			wprintf("/");
			for (j=0; j<strlen(uid); ++j) {
				wprintf("%02X", uid[j]);
			}
			wprintf("</D:href>\n");
			wprintf("   <D:propstat>\n");
			wprintf("    <D:status>HTTP/1.1 200 OK</D:status>\n");
			wprintf("    <D:prop><D:getetag>\"%ld\"</D:getetag></D:prop>\n", msgs[i]);
			wprintf("   </D:propstat>\n");
			wprintf(" </D:response>\n");
		}
	}

	wprintf("</D:multistatus>\n");
	if (msgs != NULL) {
		free(msgs);
	}
}
