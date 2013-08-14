/*
 * Handles GroupDAV and CalDAV PROPFIND requests.
 *
 * A few notes about our XML output:
 *
 * --> Yes, we are spewing tags directly instead of using an XML library.
 *     Whining about it will be summarily ignored.
 *
 * --> XML is deliberately output with no whitespace/newlines between tags.
 *     This makes it difficult to read, but we have discovered clients which
 *     crash when you try to pretty it up.
 *
 * References:
 * http://www.ietf.org/rfc/rfc4791.txt
 * http://blogs.nologin.es/rickyepoderi/index.php?/archives/14-Introducing-CalDAV-Part-I.html

Sample query:

PROPFIND /groupdav/calendar/ HTTP/1.1
Content-type: text/xml; charset=utf-8
Content-length: 166

<?xml version="1.0" encoding="UTF-8"?>
<D:propfind xmlns:D="DAV:">
  <D:prop>
    <D:getcontenttype/>
    <D:resourcetype/>
    <D:getetag/>
  </D:prop>
</D:propfind>

 *
 * Copyright (c) 2005-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 3.
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
 * IgnoreFloor: set to 0 or 1 _nothing else_
 * Subfolders: direct child floors will be put here.
 */
const folder *GetRESTFolder(int IgnoreFloor, HashList *Subfolders)
{
	wcsession  *WCC = WC;
	void *vFolder;
	const folder *ThisFolder = NULL;
	const folder *FoundFolder = NULL;
	const folder *BestGuess = NULL;
	int nBestGuess = 0;
	HashPos    *itd, *itfl;
	StrBuf     * Dir;
	void       *vDir;
	long        len;
        const char *Key;
	int iRoom, jURL, urlp;
	int delta;

/*
 * Guess room: if the full URL matches a room, list thats it. We also need to remember direct sub rooms.
 * if the URL is longer, we need to find the "best guess" so we can find the room we're in, and the rest
 * of the URL will be uids and so on.
 */
	itfl = GetNewHashPos(WCC->Floors, 0);
	urlp = GetCount(WCC->Directory);

	while (GetNextHashPos(WCC->Floors, itfl, &len, &Key, &vFolder) && 
	       (ThisFolder == NULL))
	{
		ThisFolder = vFolder;
		if (!IgnoreFloor && /* so we can handle legacy URLS... */
		    (ThisFolder->Floor != WCC->CurrentFloor))
			continue;

		if (ThisFolder->nRoomNameParts > 1) 
		{
			/*TODO: is that number all right? */
//			if (urlp - ThisFolder->nRoomNameParts != 2) {
//				if (BestGuess != NULL)
//					continue;
//ThisFolder->name
//				itd  = GetNewHashPos(WCC->Directory, 0);
//				GetNextHashPos(WCC->Directory, itd, &len, &Key, &vDir); //TODO: how many to fast forward?
//			}
			itd  = GetNewHashPos(WCC->Directory, 0);
			GetNextHashPos(WCC->Directory, itd, &len, &Key, &vDir); //TODO: how many to fast forward?
	
			for (iRoom = 0, /* Fast forward the floorname as we checked it above: */ jURL = IgnoreFloor; 

			     (iRoom <= ThisFolder->nRoomNameParts) && (jURL <= urlp); 

			     iRoom++, jURL++, GetNextHashPos(WCC->Directory, itd, &len, &Key, &vDir))
			{
				Dir = (StrBuf*)vDir;
				if (strcmp(ChrPtr(ThisFolder->RoomNameParts[iRoom]), 
					   ChrPtr(Dir)) != 0)
				{
					DeleteHashPos(&itd);
					continue;
				}
			}
			DeleteHashPos(&itd);
			/* Gotcha? */
			if ((iRoom == ThisFolder->nRoomNameParts) && (jURL == urlp))
			{
				FoundFolder = ThisFolder;
			}
			/* URL got more parts then this room, so we remember it for the best guess*/
			else if ((jURL <= urlp) &&
				 (ThisFolder->nRoomNameParts <= nBestGuess))
			{
				BestGuess = ThisFolder;
				nBestGuess = jURL - 1;
			}
			/* Room has more parts than the URL, it might be a sub-room? */
			else if (iRoom <ThisFolder->nRoomNameParts) 
			{//// TODO: ThisFolder->nRoomNameParts == urlp - IgnoreFloor???
				Put(Subfolders, SKEY(ThisFolder->name), 
				    /* Cast away const, its a reference. */
				    (void*)ThisFolder, reference_free_handler);
			}
		}
		else {
			delta = GetCount(WCC->Directory) - ThisFolder->nRoomNameParts;
			if ((delta != 2) && (nBestGuess > 1))
			    continue;
			
			itd  = GetNewHashPos(WCC->Directory, 0);
						
			if (!GetNextHashPos(WCC->Directory, 
					    itd, &len, &Key, &vDir) ||
			    (vDir == NULL))
			{
				DeleteHashPos(&itd);
				
				syslog(LOG_DEBUG, "5\n");
				continue;
			}
			DeleteHashPos(&itd);
			Dir = (StrBuf*) vDir;
			if (strcmp(ChrPtr(ThisFolder->name), 
					       ChrPtr(Dir))
			    != 0)
			{
				DeleteHashPos(&itd);
				
				syslog(LOG_DEBUG, "5\n");
				continue;
			}
			DeleteHashPos(&itfl);
			DeleteHashPos(&itd);
			if (delta != 2) {
				nBestGuess = 1;
				BestGuess = ThisFolder;
			}
			else 
				FoundFolder = ThisFolder;
		}
	}

/* TODO: Subfolders: remove patterns not matching the best guess or thisfolder */
	DeleteHashPos(&itfl);
	if (FoundFolder != NULL)
		return FoundFolder;
	else
		return BestGuess;
}




long GotoRestRoom(HashList *SubRooms)
{
	int IgnoreFloor = 0; /* deprecated... */
	wcsession *WCC = WC;
	long Count;
	long State;
	const folder *ThisFolder;

	State = REST_TOPLEVEL;

	if (WCC->Hdr->HR.Handler != NULL) 
		State |= REST_IN_NAMESPACE;

	Count = GetCount(WCC->Directory);
	
	if (Count == 0) return State;

	if (Count >= 1) State |=REST_IN_FLOOR;
	if (Count == 1) return State;
	
	/* 
	 * More than 3 params and no floor found? 
	 * -> fall back to old non-floored notation
	 */
	if ((Count >= 3) && (WCC->CurrentFloor == NULL))
		IgnoreFloor = 1;
	if (Count >= 3)
	{
		IgnoreFloor = 0;
		State |= REST_IN_FLOOR;

		ThisFolder = GetRESTFolder(IgnoreFloor, SubRooms);
		if (ThisFolder != NULL)
		{
			if (WCC->ThisRoom != NULL)
				if (CompareRooms(WCC->ThisRoom, ThisFolder) != 0)
					gotoroom(ThisFolder->name);
			State |= REST_IN_ROOM;
			
		}
		if (GetCount(SubRooms) > 0)
			State |= REST_HAVE_SUB_ROOMS;
	}
	if ((WCC->ThisRoom != NULL) && 
	    (Count + IgnoreFloor > 3))
	{
		if (WCC->Hdr->HR.Handler->RID(ExistsID, IgnoreFloor))
		{
			State |= REST_GOT_LOCAL_PART;
		}
		else {
			/// WHOOPS, not there???
			State |= REST_NONEXIST;
		}


	}
	return State;
}



/*
 * List rooms (or "collections" in DAV terminology) which contain
 * interesting groupware objects.
 */
void dav_collection_list(void)
{
	wcsession *WCC = WC;
	char buf[256];
	char roomname[256];
	int view;
	char datestring[256];
	time_t now;
	time_t mtime;
	int is_groupware_collection = 0;
	int starting_point = 1;		/**< 0 for /, 1 for /groupdav/ */

	if (WCC->Hdr->HR.Handler == NULL) {
		starting_point = 0;
	}
	else if (StrLength(WCC->Hdr->HR.ReqLine) == 0) {
		starting_point = 1;
	}
	else {
		starting_point = 2;
	}

	now = time(NULL);
	http_datestring(datestring, sizeof datestring, now);

	/*
	 * Be rude.  Completely ignore the XML request and simply send them
	 * everything we know about.  Let the client sort it out.
	 */
	hprintf("HTTP/1.0 207 Multi-Status\r\n");
	dav_common_headers();
	hprintf("Date: %s\r\n", datestring);
	hprintf("Content-type: text/xml\r\n");
	if (DisableGzip || (!WCC->Hdr->HR.gzip_ok))	
		hprintf("Content-encoding: identity\r\n");

	begin_burst();

	wc_printf("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
     		"<multistatus xmlns=\"DAV:\" xmlns:G=\"http://groupdav.org/\">"
	);

	/*
	 * If the client is requesting the root, show a root node.
	 */
	if (starting_point == 0) {
		wc_printf("<response>");
			wc_printf("<href>");
				dav_identify_host();
				wc_printf("/");
			wc_printf("</href>");
			wc_printf("<propstat>");
				wc_printf("<status>HTTP/1.1 200 OK</status>");
				wc_printf("<prop>");
					wc_printf("<displayname>/</displayname>");
					wc_printf("<resourcetype><collection/></resourcetype>");
					wc_printf("<getlastmodified>");
						escputs(datestring);
					wc_printf("</getlastmodified>");
				wc_printf("</prop>");
			wc_printf("</propstat>");
		wc_printf("</response>");
	}

	/*
	 * If the client is requesting "/groupdav", show a /groupdav subdirectory.
	 */
	if ((starting_point + WCC->Hdr->HR.dav_depth) >= 1) {
		wc_printf("<response>");
			wc_printf("<href>");
				dav_identify_host();
				wc_printf("/groupdav");
			wc_printf("</href>");
			wc_printf("<propstat>");
				wc_printf("<status>HTTP/1.1 200 OK</status>");
				wc_printf("<prop>");
					wc_printf("<displayname>GroupDAV</displayname>");
					wc_printf("<resourcetype><collection/></resourcetype>");
					wc_printf("<getlastmodified>");
						escputs(datestring);
					wc_printf("</getlastmodified>");
				wc_printf("</prop>");
			wc_printf("</propstat>");
		wc_printf("</response>");
	}

	/*
	 * Now go through the list and make it look like a DAV collection
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
		if (	(view == VIEW_CALENDAR) || 
			(view == VIEW_TASKS) || 
			(view == VIEW_ADDRESSBOOK) ||
			(view == VIEW_NOTES) ||
			(view == VIEW_JOURNAL) ||
			(view == VIEW_WIKI)
		) {
			is_groupware_collection = 1;
		}
		else {
			is_groupware_collection = 0;
		}

		if ( (is_groupware_collection) && ((starting_point + WCC->Hdr->HR.dav_depth) >= 2) ) {
			wc_printf("<response>");

			wc_printf("<href>");
			dav_identify_host();
			wc_printf("/groupdav/");
			urlescputs(roomname);
			wc_printf("/</href>");

			wc_printf("<propstat>");
			wc_printf("<status>HTTP/1.1 200 OK</status>");
			wc_printf("<prop>");
			wc_printf("<displayname>");
			escputs(roomname);
			wc_printf("</displayname>");
			wc_printf("<resourcetype><collection/>");

			switch(view) {
			case VIEW_CALENDAR:
				wc_printf("<G:vevent-collection />");
				break;
			case VIEW_TASKS:
				wc_printf("<G:vtodo-collection />");
				break;
			case VIEW_ADDRESSBOOK:
				wc_printf("<G:vcard-collection />");
				break;
			case VIEW_NOTES:
				wc_printf("<G:vnotes-collection />");
				break;
			case VIEW_JOURNAL:
				wc_printf("<G:vjournal-collection />");
				break;
			case VIEW_WIKI:
				wc_printf("<G:wiki-collection />");
				break;
			}

			wc_printf("</resourcetype>");
			wc_printf("<getlastmodified>");
				escputs(datestring);
			wc_printf("</getlastmodified>");
			wc_printf("</prop>");
			wc_printf("</propstat>");
			wc_printf("</response>");
		}
	}
	wc_printf("</multistatus>\n");

	end_burst();
}


void propfind_xml_start(void *data, const char *supplied_el, const char **attr) {
	// syslog(LOG_DEBUG, "<%s>", supplied_el);
}

void propfind_xml_end(void *data, const char *supplied_el) {
	// syslog(LOG_DEBUG, "</%s>", supplied_el);
}



/*
 * The pathname is always going to be /groupdav/room_name/msg_num
 */
void dav_propfind(void) 
{
	wcsession *WCC = WC;
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

	int parse_success = 0;
	XML_Parser xp = XML_ParserCreateNS(NULL, '|');
	if (xp) {
		// XML_SetUserData(xp, XXX);
		XML_SetElementHandler(xp, propfind_xml_start, propfind_xml_end);
		// XML_SetCharacterDataHandler(xp, xrds_xml_chardata);

		const char *req = ChrPtr(WCC->upload);
		if (req) {
			req = strchr(req, '<');			/* hunt for the first tag */
		}
		if (!req) {
			req = "ERROR";				/* force it to barf */
		}

		i = XML_Parse(xp, req, strlen(req), 1);
		if (!i) {
			syslog(LOG_DEBUG, "XML_Parse() failed: %s", XML_ErrorString(XML_GetErrorCode(xp)));
			XML_ParserFree(xp);
			parse_success = 0;
		}
		else {
			parse_success = 1;
		}
	}

	if (!parse_success) {
		hprintf("HTTP/1.1 500 Internal Server Error\r\n");
		dav_common_headers();
		hprintf("Date: %s\r\n", datestring);
		hprintf("Content-Type: text/plain\r\n");
		wc_printf("An internal error has occurred at %s:%d.\r\n", __FILE__ , __LINE__ );
		end_burst();
		return;
	}

	dav_roomname = NewStrBuf();
	dav_uid = NewStrBuf();
	StrBufExtract_token(dav_roomname, WCC->Hdr->HR.ReqLine, 0, '/');
	StrBufExtract_token(dav_uid, WCC->Hdr->HR.ReqLine, 1, '/');

	syslog(LOG_DEBUG, "PROPFIND requested for '%s' at depth %d",
		ChrPtr(dav_roomname), WCC->Hdr->HR.dav_depth
	);

	/*
	 * If the room name is blank, the client is requesting a folder list.
	 */
	if (StrLength(dav_roomname) == 0) {
		dav_collection_list();
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);
		return;
	}

	/* Go to the correct room. */
	if (strcasecmp(ChrPtr(WCC->CurRoom.name), ChrPtr(dav_roomname))) {
		gotoroom(dav_roomname);
	}
	if (strcasecmp(ChrPtr(WCC->CurRoom.name), ChrPtr(dav_roomname))) {
		hprintf("HTTP/1.1 404 not found\r\n");
		dav_common_headers();
		hprintf("Date: %s\r\n", datestring);
		hprintf("Content-Type: text/plain\r\n");
		wc_printf("There is no folder called \"%s\" on this server.\r\n", ChrPtr(dav_roomname));
		end_burst();
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);
		return;
	}

	/* If dav_uid is non-empty, client is requesting a PROPFIND on
	 * a specific item in the room.  This is not valid GroupDAV, but
	 * it is valid WebDAV (and probably CalDAV too).
	 */
	if (StrLength(dav_uid) != 0) {

		dav_msgnum = locate_message_by_uid(ChrPtr(dav_uid));
		if (dav_msgnum < 0) {
			hprintf("HTTP/1.1 404 not found\r\n");
			dav_common_headers();
			hprintf("Content-Type: text/plain\r\n");
			wc_printf("Object \"%s\" was not found in the \"%s\" folder.\r\n",
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
		dav_common_headers();
		hprintf("Date: %s\r\n", datestring);
		hprintf("Content-type: text/xml\r\n");
		if (DisableGzip || (!WCC->Hdr->HR.gzip_ok))	
			hprintf("Content-encoding: identity\r\n");
	
		begin_burst();
	
		wc_printf("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
     			"<multistatus xmlns=\"DAV:\">"
		);

		wc_printf("<response>");
		
		wc_printf("<href>");
		dav_identify_host();
		wc_printf("/groupdav/");
		urlescputs(ChrPtr(WCC->CurRoom.name));
		euid_escapize(encoded_uid, ChrPtr(dav_uid));
		wc_printf("/%s", encoded_uid);
		wc_printf("</href>");
		wc_printf("<propstat>");
		wc_printf("<status>HTTP/1.1 200 OK</status>");
		wc_printf("<prop>");
		wc_printf("<getetag>\"%ld\"</getetag>", dav_msgnum);
		wc_printf("<getlastmodified>");
		escputs(datestring);
		wc_printf("</getlastmodified>");
		wc_printf("</prop>");
		wc_printf("</propstat>");

		wc_printf("</response>\n");
		wc_printf("</multistatus>\n");
		end_burst();
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);
		return;
	}
	FreeStrBuf(&dav_roomname);
	FreeStrBuf(&dav_uid);


	/*
	 * If we get to this point the client is performing a PROPFIND on the room itself.
	 *
	 * We call it a room; DAV calls it a "collection."  We have to give it some properties
	 * of the room itself and then offer a list of all items contained therein.
	 *
	 * Be rude.  Completely ignore the XML request and simply send them
	 * everything we know about (which is going to simply be the ETag and
	 * nothing else).  Let the client-side parser sort it out.
	 */
	//syslog(LOG_DEBUG, "BE RUDE AND IGNORE: \033[31m%s\033[0m", ChrPtr(WC->upload) );
	hprintf("HTTP/1.0 207 Multi-Status\r\n");
	dav_common_headers();
	hprintf("Date: %s\r\n", datestring);
	hprintf("Content-type: text/xml\r\n");
	if (DisableGzip || (!WCC->Hdr->HR.gzip_ok)) {
		hprintf("Content-encoding: identity\r\n");
	}
	begin_burst();

	wc_printf("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
     		"<D:multistatus "
			"xmlns:D=\"DAV:\" "
			"xmlns:G=\"http://groupdav.org/\" "
			"xmlns:C=\"urn:ietf:params:xml:ns:caldav\""
		">"
	);

	/* Transmit the collection resource */
	wc_printf("<D:response>");

	wc_printf("<D:href>");
	dav_identify_host();
	wc_printf("/groupdav/");
	urlescputs(ChrPtr(WCC->CurRoom.name));
	wc_printf("</D:href>");

	wc_printf("<D:propstat>");
	wc_printf("<D:status>HTTP/1.1 200 OK</D:status>");
	wc_printf("<D:prop>");
	wc_printf("<D:displayname>");
	escputs(ChrPtr(WCC->CurRoom.name));
	wc_printf("</D:displayname>");

	wc_printf("<D:owner/>");		/* empty owner ought to be legal; see rfc3744 section 5.1 */

	wc_printf("<D:resourcetype><D:collection/>");
	switch(WCC->CurRoom.defview) {
		case VIEW_CALENDAR:
			wc_printf("<G:vevent-collection />");
			wc_printf("<C:calendar />");
			break;
		case VIEW_TASKS:
			wc_printf("<G:vtodo-collection />");
			break;
		case VIEW_ADDRESSBOOK:
			wc_printf("<G:vcard-collection />");
			break;
	}
	wc_printf("</D:resourcetype>");

	/* FIXME get the mtime
	wc_printf("<D:getlastmodified>");
		escputs(datestring);
	wc_printf("</D:getlastmodified>");
	*/
	wc_printf("</D:prop>");
	wc_printf("</D:propstat>");
	wc_printf("</D:response>");

	/* If a depth greater than zero was specified, transmit the collection listing */

	if (WCC->Hdr->HR.dav_depth > 0) {
		MsgNum = NewStrBuf();
		serv_puts("MSGS ALL");
	
		StrBuf_ServGetln(MsgNum);
		if (GetServerStatus(MsgNum, NULL) == 1)
			while (BufLen = StrBuf_ServGetln(MsgNum), 
		       	((BufLen >= 0) && 
				((BufLen != 3) || strcmp(ChrPtr(MsgNum), "000"))  ))
			{
				msgs = realloc(msgs, ++num_msgs * sizeof(long));
				msgs[num_msgs-1] = StrTol(MsgNum);
			}
	
		if (num_msgs > 0) for (i=0; i<num_msgs; ++i) {
	
			syslog(LOG_DEBUG, "PROPFIND enumerating message # %ld", msgs[i]);
			strcpy(uid, "");
			now = (-1);
			serv_printf("MSG0 %ld|3", msgs[i]);
			StrBuf_ServGetln(MsgNum);
			if (GetServerStatus(MsgNum, NULL) == 1)
				while (BufLen = StrBuf_ServGetln(MsgNum), 
			       	((BufLen >= 0) && 
					((BufLen != 3) || strcmp(ChrPtr(MsgNum), "000")) ))
				{
					if (!strncasecmp(ChrPtr(MsgNum), "exti=", 5)) {
						strcpy(uid, &ChrPtr(MsgNum)[5]);
					}
					else if (!strncasecmp(ChrPtr(MsgNum), "time=", 5)) {
						now = atol(&ChrPtr(MsgNum)[5]);
				}
			}
	
			if (!IsEmptyStr(uid)) {
				wc_printf("<D:response>");
					wc_printf("<D:href>");
						dav_identify_host();
						wc_printf("/groupdav/");
						urlescputs(ChrPtr(WCC->CurRoom.name));
						euid_escapize(encoded_uid, uid);
						wc_printf("/%s", encoded_uid);
					wc_printf("</D:href>");
					switch(WCC->CurRoom.defview) {
					case VIEW_CALENDAR:
						wc_printf("<D:getcontenttype>text/x-ical</D:getcontenttype>");
						break;
					case VIEW_TASKS:
						wc_printf("<D:getcontenttype>text/x-ical</D:getcontenttype>");
						break;
					case VIEW_ADDRESSBOOK:
						wc_printf("<D:getcontenttype>text/x-vcard</D:getcontenttype>");
						break;
					}
					wc_printf("<D:propstat>");
						wc_printf("<D:status>HTTP/1.1 200 OK</D:status>");
						wc_printf("<D:prop>");
							wc_printf("<D:getetag>\"%ld\"</D:getetag>", msgs[i]);
						if (now > 0L) {
							http_datestring(datestring, sizeof datestring, now);
							wc_printf("<D:getlastmodified>");
							escputs(datestring);
							wc_printf("</D:getlastmodified>");
						}
						wc_printf("</D:prop>");
					wc_printf("</D:propstat>");
				wc_printf("</D:response>");
			}
		}
		FreeStrBuf(&MsgNum);
	}

	wc_printf("</D:multistatus>\n");
	end_burst();

	if (msgs != NULL) {
		free(msgs);
	}
}



int ParseMessageListHeaders_EUID(StrBuf *Line, 
				 const char **pos, 
				 message_summary *Msg, 
				 StrBuf *ConversionBuffer)
{
	Msg->euid = NewStrBuf();
	StrBufExtract_NextToken(Msg->euid,  Line, pos, '|');
	Msg->date = StrBufExtractNext_long(Line, pos, '|');
	
	return StrLength(Msg->euid) > 0;
}

int DavUIDL_GetParamsGetServerCall(SharedMessageStatus *Stat, 
				   void **ViewSpecific, 
				   long oper, 
				   char *cmd, 
				   long len,
				   char *filter,
				   long flen)
{
	Stat->defaultsortorder = 0;
	Stat->sortit = 0;
	Stat->load_seen = 0;
	Stat->maxmsgs  = 9999999;

	snprintf(cmd, len, "MSGS ALL|||2");
	return 200;
}

int DavUIDL_RenderView_or_Tail(SharedMessageStatus *Stat, 
				void **ViewSpecific, 
				long oper)
{
	
	DoTemplate(HKEY("msg_listview"),NULL,&NoCtx);
	
	return 0;
}

int DavUIDL_Cleanup(void **ViewSpecific)
{
	/* Note: wDumpContent() will output one additional </div> tag. */
	/* We ought to move this out into template */
	wDumpContent(1);

	return 0;
}




void 
InitModule_PROPFIND
(void)
{
	RegisterReadLoopHandlerset(
		eReadEUIDS,
		DavUIDL_GetParamsGetServerCall,
		NULL,
		NULL, /// TODO: is this right?
		ParseMessageListHeaders_EUID,
		NULL, //// ""
		DavUIDL_RenderView_or_Tail,
		DavUIDL_Cleanup);

}
