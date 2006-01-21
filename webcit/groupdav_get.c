/*
 * $Id$
 */
/**
 * \defgraup GroupdavGet Handle GroupDAV GET requests.
 *
 */
/*@{*/
#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"


/**
 * \briefThe pathname is always going to be /groupdav/room_name/euid
 * \param dav_pathname the pathname to print
 */
void groupdav_get(char *dav_pathname) {
	char dav_roomname[SIZ];
	char dav_uid[SIZ];
	long dav_msgnum = (-1);
	char buf[SIZ];
	int n = 0;
	int in_body = 0;
	int found_content_type = 0;

	/** First, break off the "/groupdav/" prefix */
	remove_token(dav_pathname, 0, '/');
	remove_token(dav_pathname, 0, '/');

	/** Now extract the message euid */
	n = num_tokens(dav_pathname, '/');
	extract_token(dav_uid, dav_pathname, n-1, '/', sizeof dav_uid);
	remove_token(dav_pathname, n-1, '/');

	/** What's left is the room name.  Remove trailing slashes. */
	if (dav_pathname[strlen(dav_pathname)-1] == '/') {
		dav_pathname[strlen(dav_pathname)-1] = 0;
	}
	strcpy(dav_roomname, dav_pathname);

	/** Go to the correct room. */
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

	wprintf("HTTP/1.1 200 OK\r\n");
	groupdav_common_headers();
	wprintf("etag: \"%ld\"\r\n", dav_msgnum);
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		if (!strncasecmp(buf, "Date: ", 6)) {
			wprintf("%s\r\n", buf);
		}
		if (!strncasecmp(buf, "Content-type: ", 14)) {
			wprintf("%s\r\n", buf);
			found_content_type = 1;
		}
		if ((strlen(buf) == 0) && (in_body == 0)) {
			if (!found_content_type) {
				wprintf("Content-type: text/plain\r\n");
			}
			in_body = 1;
		}
		if (in_body) {
			wprintf("%s\r\n", buf);
		}
	}
}


/*@}*/
