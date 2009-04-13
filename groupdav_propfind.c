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
 */
long locate_message_by_uid(const char *uid) {
	char buf[256];
	char decoded_uid[1024];
	long retval = (-1L);

	/* decode the UID */
	euid_unescapize(decoded_uid, uid);

	/* ask Citadel if we have this one */
	serv_printf("EUID %s", decoded_uid);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		retval = atol(&buf[4]);
	}

	return(retval);
}



/*
 * List rooms (or "collections" in DAV terminology) which contain
 * interesting groupware objects.
 */
void groupdav_collection_list(const char *dav_pathname, int dav_depth)
{
	char buf[256];
	char roomname[256];
	int view;
	char datestring[256];
	time_t now;
	time_t mtime;
	int is_groupware_collection = 0;
	int starting_point = 1;		/**< 0 for /, 1 for /groupdav/ */

	if (!strcmp(dav_pathname, "/")) {
		starting_point = 0;
	}
	else if (!strcasecmp(dav_pathname, "/groupdav")) {
		starting_point = 1;
	}
	else if (!strcasecmp(dav_pathname, "/groupdav/")) {
		starting_point = 1;
	}
	else if ( (!strncasecmp(dav_pathname, "/groupdav/", 10)) && (strlen(dav_pathname) > 10) ) {
		starting_point = 2;
	}

	now = time(NULL);
	http_datestring(datestring, sizeof datestring, now);

	/**
	 * Be rude.  Completely ignore the XML request and simply send them
	 * everything we know about.  Let the client sort it out.
	 */
	hprintf("HTTP/1.0 207 Multi-Status\r\n");
	groupdav_common_headers();
	hprintf("Date: %s\r\n", datestring);
	hprintf("Content-type: text/xml\r\n");
	hprintf("Content-encoding: identity\r\n");

	begin_burst();

	wprintf("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
     		"<multistatus xmlns=\"DAV:\" xmlns:G=\"http://groupdav.org/\">"
	);

	/**
	 *	If the client is requesting the root, show a root node.
	 */
	if (starting_point == 0) {
		wprintf("<response>");
			wprintf("<href>");
				groupdav_identify_host();
				wprintf("/");
			wprintf("</href>");
			wprintf("<propstat>");
				wprintf("<status>HTTP/1.1 200 OK</status>");
				wprintf("<prop>");
					wprintf("<displayname>/</displayname>");
					wprintf("<resourcetype><collection/></resourcetype>");
					wprintf("<getlastmodified>");
						escputs(datestring);
					wprintf("</getlastmodified>");
				wprintf("</prop>");
			wprintf("</propstat>");
		wprintf("</response>");
	}

	/**
	 *	If the client is requesting "/groupdav", show a /groupdav subdirectory.
	 */
	if ((starting_point + dav_depth) >= 1) {
		wprintf("<response>");
			wprintf("<href>");
				groupdav_identify_host();
				wprintf("/groupdav");
			wprintf("</href>");
			wprintf("<propstat>");
				wprintf("<status>HTTP/1.1 200 OK</status>");
				wprintf("<prop>");
					wprintf("<displayname>GroupDAV</displayname>");
					wprintf("<resourcetype><collection/></resourcetype>");
					wprintf("<getlastmodified>");
						escputs(datestring);
					wprintf("</getlastmodified>");
				wprintf("</prop>");
			wprintf("</propstat>");
		wprintf("</response>");
	}

	/**
	 *	Now go through the list and make it look like a DAV collection
	 */
	serv_puts("LKRA");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {

		extract_token(roomname, buf, 0, '|', sizeof roomname);
		view = extract_int(buf, 7);
		mtime = extract_long(buf, 8);
		http_datestring(datestring, sizeof datestring, mtime);

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

		if ( (is_groupware_collection) && ((starting_point + dav_depth) >= 2) ) {
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
			wprintf("<getlastmodified>");
				escputs(datestring);
			wprintf("</getlastmodified>");
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
void groupdav_propfind(StrBuf *dav_pathname, int dav_depth, StrBuf *dav_content_type, StrBuf *dav_content, int offset) {
	StrBuf *dav_roomname;
	StrBuf *dav_uid;
	StrBuf *MsgNum;
	long BufLen;
	long dav_msgnum = (-1);
	char uid[256];
	char encoded_uid[256];
	long *msgs = NULL;
	int num_msgs = 0;
	int i;
	char datestring[256];
	time_t now;

	now = time(NULL);
	http_datestring(datestring, sizeof datestring, now);

	dav_roomname = NewStrBuf();
	dav_uid = NewStrBuf();
	StrBufExtract_token(dav_roomname, dav_pathname, 2, '/');
	StrBufExtract_token(dav_uid, dav_pathname, 3, '/');

	/*
	 * If the room name is blank, the client is requesting a
	 * folder list.
	 */
	if (StrLength(dav_roomname) == 0) {
		groupdav_collection_list(ChrPtr(dav_pathname), dav_depth);
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);
		return;
	}

	/* Go to the correct room. */
	if (strcasecmp(ChrPtr(WC->wc_roomname), ChrPtr(dav_roomname))) {
		gotoroom(dav_roomname);
	}
	if (strcasecmp(ChrPtr(WC->wc_roomname), ChrPtr(dav_roomname))) {
		hprintf("HTTP/1.1 404 not found\r\n");
		groupdav_common_headers();
		hprintf("Date: %s\r\n", datestring);
		hprintf("Content-Type: text/plain\r\n");
		wprintf("There is no folder called \"%s\" on this server.\r\n",
			ChrPtr(dav_roomname)
		);
		end_burst();
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);
		return;
	}

	/* If dav_uid is non-empty, client is requesting a PROPFIND on
	 * a specific item in the room.  This is not valid GroupDAV, but
	 * it is valid WebDAV.
	 */
	if (StrLength(dav_uid) != 0) {

		dav_msgnum = locate_message_by_uid(ChrPtr(dav_uid));
		if (dav_msgnum < 0) {
			hprintf("HTTP/1.1 404 not found\r\n");
			groupdav_common_headers();
			hprintf("Content-Type: text/plain\r\n");
			wprintf("Object \"%s\" was not found in the \"%s\" folder.\r\n",
				ChrPtr(dav_uid),
				ChrPtr(dav_roomname)
			);
			end_burst();
			FreeStrBuf(&dav_roomname);
			FreeStrBuf(&dav_uid);
			return;
		}

	 	/* Be rude.  Completely ignore the XML request and simply send them
		 * everything we know about (which is going to simply be the ETag and
		 * nothing else).  Let the client-side parser sort it out.
		 */
		hprintf("HTTP/1.0 207 Multi-Status\r\n");
		groupdav_common_headers();
		hprintf("Date: %s\r\n", datestring);
		hprintf("Content-type: text/xml\r\n");
		hprintf("Content-encoding: identity\r\n");
	
		begin_burst();
	
		wprintf("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
     			"<multistatus xmlns=\"DAV:\">"
		);

		wprintf("<response>");
		
		wprintf("<href>");
		groupdav_identify_host();
		wprintf("/groupdav/");
		urlescputs(ChrPtr(WC->wc_roomname));
		euid_escapize(encoded_uid, ChrPtr(dav_uid));
		wprintf("/%s", encoded_uid);
		wprintf("</href>");
		wprintf("<propstat>");
		wprintf("<status>HTTP/1.1 200 OK</status>");
		wprintf("<prop>");
		wprintf("<getetag>\"%ld\"</getetag>", dav_msgnum);
		wprintf("<getlastmodified>");
		escputs(datestring);
		wprintf("</getlastmodified>");
		wprintf("</prop>");
		wprintf("</propstat>");

		wprintf("</response>\n");
		wprintf("</multistatus>\n");
		end_burst();
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);
		return;
	}
	FreeStrBuf(&dav_roomname);
	FreeStrBuf(&dav_uid);


	/*
	 * We got to this point, which means that the client is requesting
	 * a 'collection' (i.e. a list of all items in the room).
	 *
	 * Be rude.  Completely ignore the XML request and simply send them
	 * everything we know about (which is going to simply be the ETag and
	 * nothing else).  Let the client-side parser sort it out.
	 */
	hprintf("HTTP/1.0 207 Multi-Status\r\n");
	groupdav_common_headers();
	hprintf("Date: %s\r\n", datestring);
	hprintf("Content-type: text/xml\r\n");
	hprintf("Content-encoding: identity\r\n");

	begin_burst();

	wprintf("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
     		"<multistatus xmlns=\"DAV:\" xmlns:G=\"http://groupdav.org/\">"
	);


	/** Transmit the collection resource (FIXME check depth and starting point) */
	wprintf("<response>");

	wprintf("<href>");
		groupdav_identify_host();
		wprintf("/groupdav/");
		urlescputs(ChrPtr(WC->wc_roomname));
	wprintf("</href>");

	wprintf("<propstat>");
	wprintf("<status>HTTP/1.1 200 OK</status>");
	wprintf("<prop>");
	wprintf("<displayname>");
	escputs(ChrPtr(WC->wc_roomname));
	wprintf("</displayname>");
	wprintf("<resourcetype><collection/>");

	switch(WC->wc_default_view) {
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
	/* FIXME get the mtime
	wprintf("<getlastmodified>");
		escputs(datestring);
	wprintf("</getlastmodified>");
	*/
	wprintf("</prop>");
	wprintf("</propstat>");
	wprintf("</response>");

	/** Transmit the collection listing (FIXME check depth and starting point) */

	MsgNum = NewStrBuf ();
	serv_puts("MSGS ALL");

	StrBuf_ServGetln(MsgNum);
	if (GetServerStatus(MsgNum, NULL) == 1)
		while (BufLen = StrBuf_ServGetlnBuffered(MsgNum), strcmp(ChrPtr(MsgNum), "000"))  {
			msgs = realloc(msgs, ++num_msgs * sizeof(long));
			msgs[num_msgs-1] = StrTol(MsgNum);
		}

	if (num_msgs > 0) for (i=0; i<num_msgs; ++i) {

		strcpy(uid, "");
		now = (-1);
		serv_printf("MSG0 %ld|3", msgs[i]);
		StrBuf_ServGetln(MsgNum);
		if (GetServerStatus(MsgNum, NULL) == 1)
			while (BufLen = StrBuf_ServGetlnBuffered(MsgNum), strcmp(ChrPtr(MsgNum), "000")) 
			{
				if (!strncasecmp(ChrPtr(MsgNum), "exti=", 5)) {
					strcpy(uid, &ChrPtr(MsgNum)[5]);
				}
				else if (!strncasecmp(ChrPtr(MsgNum), "time=", 5)) {
					now = atol(&ChrPtr(MsgNum)[5]);
			}
		}

		if (!IsEmptyStr(uid)) {
			wprintf("<response>");
				wprintf("<href>");
					groupdav_identify_host();
					wprintf("/groupdav/");
					urlescputs(ChrPtr(WC->wc_roomname));
					euid_escapize(encoded_uid, uid);
					wprintf("/%s", encoded_uid);
				wprintf("</href>");
				switch(WC->wc_default_view) {
				case VIEW_CALENDAR:
					wprintf("<getcontenttype>text/x-ical</getcontenttype>");
					break;
				case VIEW_TASKS:
					wprintf("<getcontenttype>text/x-ical</getcontenttype>");
					break;
				case VIEW_ADDRESSBOOK:
					wprintf("<getcontenttype>text/x-vcard</getcontenttype>");
					break;
				}
				wprintf("<propstat>");
					wprintf("<status>HTTP/1.1 200 OK</status>");
					wprintf("<prop>");
						wprintf("<getetag>\"%ld\"</getetag>", msgs[i]);
					if (now > 0L) {
						http_datestring(datestring, sizeof datestring, now);
						wprintf("<getlastmodified>");
						escputs(datestring);
						wprintf("</getlastmodified>");
					}
					wprintf("</prop>");
				wprintf("</propstat>");
			wprintf("</response>");
		}
	}
	FreeStrBuf(&MsgNum);

	wprintf("</multistatus>\n");
	end_burst();

	if (msgs != NULL) {
		free(msgs);
	}
}
