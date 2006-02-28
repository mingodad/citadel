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

#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"


/*
 * Given an encoded UID, translate that to an unencoded Citadel EUID and
 * then search for it in the current room.  Return a message number or -1
 * if not found.
 *
 * FIXME there's an indexing facility for this in Citadel.  Use it!!!!
 */
long locate_message_by_uid(char *uid) {
	char buf[256];
	char decoded_uid[1024];
	long retval = (-1L);

	/* Decode the uid */
	euid_unescapize(decoded_uid, uid);

/**************  THE NEW WAY ***********************/
	serv_printf("EUID %s", decoded_uid);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		retval = atol(&buf[4]);
	}
/***************************************************/

/**************  THE OLD WAY ***********************
	serv_puts("MSGS ALL|0|1");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '8') {
		serv_printf("exti|%s", decoded_uid);
		serv_puts("000");
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			retval = atol(buf);
		}
	}
 ***************************************************/


	return(retval);
}


/*
 * List rooms (or "collections" in DAV terminology) which contain
 * interesting groupware objects.
 */
void groupdav_collection_list(void) {
	char buf[256];
	char roomname[256];
	int view;
	char datestring[256];
	time_t now;
	int is_groupware_collection = 0;

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
     		"<multistatus xmlns=\"DAV:\" xmlns:G=\"http://groupdav.org/\">"
	);

	serv_puts("LKRA");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {

		extract_token(roomname, buf, 0, '|', sizeof roomname);
		view = extract_int(buf, 7);

		/*
		 * For now, only list rooms that we know a GroupDAV client
		 * might be interested in.  In the future we may add
		 * the rest.
		 *
		 * We determine the type of objects which are stored in each
		 * room by looking at the *default* view for the room.  This
		 * allows, for example, a Calendar room to appear as a
		 * GroupDAV calendar even if the user has switched it to a
		 * Calendar List view.
		 */
		if ((view == VIEW_CALENDAR) || (view == VIEW_TASKS) || (view == VIEW_ADDRESSBOOK) ) {
			is_groupware_collection = 1;
		}
		else {
			is_groupware_collection = 0;
		}

		if (is_groupware_collection) {
			wprintf("<response>");

			wprintf("<href>");
			groupdav_identify_host();
			wprintf("/groupdav/");
			urlescputs(roomname);
			wprintf("/</href>");

			wprintf("<propstat>");
			wprintf("<status>HTTP/1.1 200 OK</status>");
			wprintf("<prop>");
			wprintf("<displayname>");
			escputs(roomname);
			wprintf("</displayname>");
			wprintf("<resourcetype><collection/>");

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

			wprintf("</resourcetype>");
			wprintf("</prop>");
			wprintf("</propstat>");
			wprintf("</response>");
		}
	}
	wprintf("</multistatus>\n");

	end_burst();
}



/*
 * The pathname is always going to be /groupdav/room_name/msg_num
 */
void groupdav_propfind(char *dav_pathname, char *dav_depth, char *dav_content_type, char *dav_content) {
	char dav_roomname[256];
	char dav_uid[256];
	char msgnum[256];
	long dav_msgnum = (-1);
	char buf[256];
	char uid[256];
	char encoded_uid[256];
	long *msgs = NULL;
	int num_msgs = 0;
	int i;
	char datestring[256];
	time_t now;

	now = time(NULL);
	http_datestring(datestring, sizeof datestring, now);

	extract_token(dav_roomname, dav_pathname, 2, '/', sizeof dav_roomname);
	extract_token(dav_uid, dav_pathname, 3, '/', sizeof dav_uid);

	lprintf(9, "dav_pathname: %s\n", dav_pathname);
	lprintf(9, "dav_roomname: %s\n", dav_roomname);
	lprintf(9, "     dav_uid: %s\n", dav_uid);
	lprintf(9, "   dav_depth: %s\n", dav_depth);

	/*
	 * If the room name is blank, the client is requesting a
	 * folder list.
	 */
	if (strlen(dav_roomname) == 0) {
		groupdav_collection_list();
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

	/* If dav_uid is non-empty, client is requesting a PROPFIND on
	 * a specific item in the room.  This is not valid GroupDAV, but
	 * we try to honor it anyway because some clients are expecting
	 * it to work...
	 */
	if (strlen(dav_uid) > 0) {

		dav_msgnum = locate_message_by_uid(dav_uid);
		if (dav_msgnum < 0) {
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

	 	/* Be rude.  Completely ignore the XML request and simply send them
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
     			"<multistatus xmlns=\"DAV:\">"
		);

		wprintf("<response>");

		wprintf("<href>");
		groupdav_identify_host();
		wprintf("/groupdav/");
		urlescputs(WC->wc_roomname);
		euid_escapize(encoded_uid, dav_uid);
		wprintf("/%s", encoded_uid);
		wprintf("</href>");
		wprintf("<propstat>");
		wprintf("<status>HTTP/1.1 200 OK</status>");
		wprintf("<prop><getetag>\"%ld\"</getetag></prop>", dav_msgnum);
		wprintf("</propstat>");

		wprintf("</response>\n");
		wprintf("</multistatus>\n");
		end_burst();
		return;
	}


	/*
	 * We got to this point, which means that the client is requesting
	 * a 'collection' (i.e. a list of all items in the room).
	 *
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
     		"<multistatus xmlns=\"DAV:\" xmlns:G=\"http://groupdav.org/\">"
	);

	serv_puts("MSGS ALL");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') while (serv_getln(msgnum, sizeof msgnum), strcmp(msgnum, "000")) {
		msgs = realloc(msgs, ++num_msgs * sizeof(long));
		msgs[num_msgs-1] = atol(msgnum);
	}

	if (num_msgs > 0) for (i=0; i<num_msgs; ++i) {

		strcpy(uid, "");
		serv_printf("MSG0 %ld|3", msgs[i]);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			if (!strncasecmp(buf, "exti=", 5)) {
				strcpy(uid, &buf[5]);
			}
		}

		if (strlen(uid) > 0) {
			wprintf("<response>");
			wprintf("<href>");
			groupdav_identify_host();
			wprintf("/groupdav/");
			urlescputs(WC->wc_roomname);
			euid_escapize(encoded_uid, uid);
			wprintf("/%s", encoded_uid);
			wprintf("</href>");
			wprintf("<propstat>");
			wprintf("<status>HTTP/1.1 200 OK</status>");
			wprintf("<prop><getetag>\"%ld\"</getetag></prop>", msgs[i]);
			wprintf("</propstat>");
			wprintf("</response>");
		}
	}

	wprintf("</multistatus>\n");
	end_burst();

	if (msgs != NULL) {
		free(msgs);
	}
}
