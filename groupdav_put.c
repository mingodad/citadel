/*
 * $Id$
 *
 * Handles GroupDAV PUT requests.
 *
 */

#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"


/*
 * The pathname is always going to be /groupdav/room_name/euid
 */
void groupdav_put(char *dav_pathname, char *dav_ifmatch,
		char *dav_content_type, char *dav_content
) {
	char dav_roomname[SIZ];
	char dav_uid[SIZ];
	long new_msgnum = (-2L);
	long old_msgnum = (-1L);
	char buf[SIZ];
	int n = 0;

	/* First, break off the "/groupdav/" prefix */
	remove_token(dav_pathname, 0, '/');
	remove_token(dav_pathname, 0, '/');

	/* Now extract the message euid */
	n = num_tokens(dav_pathname, '/');
	extract_token(dav_uid, dav_pathname, n-1, '/', sizeof dav_uid);
	remove_token(dav_pathname, n-1, '/');

	/* What's left is the room name.  Remove trailing slashes. */
	if (dav_pathname[strlen(dav_pathname)-1] == '/') {
		dav_pathname[strlen(dav_pathname)-1] = 0;
	}
	strcpy(dav_roomname, dav_pathname);

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

	/*
	 * If an HTTP If-Match: header is present, the client is attempting
	 * to replace an existing item.  We have to check to see if the
	 * message number associated with the supplied uid matches what the
	 * client is expecting.  If not, the server probably contains a newer
	 * version, so we fail...
	 */
	if (strlen(dav_ifmatch) > 0) {
		old_msgnum = locate_message_by_uid(dav_uid);
		if (atol(dav_ifmatch) != old_msgnum) {
			wprintf("HTTP/1.1 412 Precondition Failed\r\n");
			lprintf(9, "HTTP/1.1 412 Precondition Failed (ifmatch=%ld, old_msgnum=%ld)\r\n",
				atol(dav_ifmatch), old_msgnum);
			groupdav_common_headers();
			wprintf("Content-Length: 0\r\n\r\n");
			return;
		}
	}

	/*
	 * We are cleared for upload!  We use the new calling syntax for ENT0
	 * which allows a confirmation to be sent back to us.  That's how we
	 * extract the message ID.
	 */
	serv_puts("ENT0 1|||4|||1|");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '8') {
		wprintf("HTTP/1.1 502 Bad Gateway\r\n");
		groupdav_common_headers();
		wprintf("Content-type: text/plain\r\n"
			"\r\n"
			"%s\r\n", &buf[4]
		);
		return;
	}

	/* Send the content to the Citadel server */
	serv_printf("Content-type: %s\n\n", dav_content_type);
	serv_puts(dav_content);
	serv_puts("\n000");

	/* Fetch the reply from the Citadel server */
	n = 0;
	strcpy(dav_uid, "");
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		switch(n++) {
			case 0: new_msgnum = atol(buf);
				break;
			case 1:	lprintf(9, "new_msgnum=%ld (%s)\n", new_msgnum, buf);
				break;
			case 2: strcpy(dav_uid, buf);
				break;
			default:
				break;
		}
	}

	/* Tell the client what happened. */

	/* Citadel failed in some way? */
	if (new_msgnum < 0L) {
		wprintf("HTTP/1.1 502 Bad Gateway\r\n");
		groupdav_common_headers();
		wprintf("Content-type: text/plain\r\n"
			"\r\n"
			"new_msgnum is %ld\r\n"
			"\r\n", new_msgnum
		);
		return;
	}

	/* We created this item for the first time. */
	if (old_msgnum < 0L) {
		wprintf("HTTP/1.1 201 Created\r\n");
		lprintf(9, "HTTP/1.1 201 Created\r\n");
		groupdav_common_headers();
		wprintf("etag: \"%ld\"\r\n", new_msgnum);
		wprintf("Content-Length: 0\r\n");
		wprintf("Location: ");
		if (strlen(WC->http_host) > 0) {
			wprintf("%s://%s",
				(is_https ? "https" : "http"),
				WC->http_host);
		}
		wprintf("/groupdav/");
		urlescputs(dav_roomname);
		wprintf("/%s\r\n", dav_uid);
		wprintf("\r\n");
		return;
	}

	/* We modified an existing item. */
	wprintf("HTTP/1.1 204 No Content\r\n");
	lprintf(9, "HTTP/1.1 204 No Content\r\n");
	groupdav_common_headers();
	wprintf("etag: \"%ld\"\r\n", new_msgnum);
	wprintf("Content-Length: 0\r\n\r\n");

	/* The item we replaced has probably already been deleted by
	 * the Citadel server, but we'll do this anyway, just in case.
	 */
	serv_printf("DELE %ld", old_msgnum);
	serv_getln(buf, sizeof buf);

	return;
}
