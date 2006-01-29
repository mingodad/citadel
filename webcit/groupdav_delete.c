/*
 * $Id$
 */
/**
 * \defgroup GroupdavDel Handle GroupDAV DELETE requests.
 * \ingroup WebcitHttpServerGDav
 */
/*@{*/
#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"


/**
 * \brief The pathname is always going to be /groupdav/room_name/euid
 * \param dav_pathname the groupdav pathname
 * \param dav_ifmatch item to delete ????
 */
void groupdav_delete(char *dav_pathname, char *dav_ifmatch) {
	char dav_roomname[SIZ];
	char dav_uid[SIZ];
	long dav_msgnum = (-1);
	char buf[SIZ];
	int n = 0;

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
		wprintf("Content-Length: 0\r\n\r\n");
		return;
	}

	dav_msgnum = locate_message_by_uid(dav_uid);

	/**
	 * If no item exists with the requested uid ... simple error.
	 */
	if (dav_msgnum < 0L) {
		wprintf("HTTP/1.1 404 Not Found\r\n");
		groupdav_common_headers();
		wprintf("Content-Length: 0\r\n\r\n");
		return;
	}

	/**
	 * It's there ... check the ETag and make sure it matches
	 * the message number.
	 */
	if (strlen(dav_ifmatch) > 0) {
		if (atol(dav_ifmatch) != dav_msgnum) {
			wprintf("HTTP/1.1 412 Precondition Failed\r\n");
			groupdav_common_headers();
			wprintf("Content-Length: 0\r\n\r\n");
			return;
		}
	}

	/**
	 * Ok, attempt to delete the item.
	 */
	serv_printf("DELE %ld", dav_msgnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		wprintf("HTTP/1.1 204 No Content\r\n");	/* success */
		groupdav_common_headers();
		wprintf("Content-Length: 0\r\n\r\n");
	}
	else {
		wprintf("HTTP/1.1 403 Forbidden\r\n");	/* access denied */
		groupdav_common_headers();
		wprintf("Content-Length: 0\r\n\r\n");
	}
	return;
}


/*@}*/
