/*
 * $Id$
 *
 * Handles GroupDAV PROPFIND requests.
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

const folder *GetRESTFolder(int IgnoreFloor)
{
	wcsession  *WCC = WC;
	void *vFolder;
	const folder *ThisFolder = NULL;
	HashPos    *itd, *itfl;
	StrBuf     * Dir;
	void       *vDir;
	long        len;
        const char *Key;
	int i, j, urlp;
	int delta;



	itfl = GetNewHashPos(WCC->Floors, 0);

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
			if (GetCount(WCC->Directory) - ThisFolder->nRoomNameParts != 2)
				continue;

			itd  = GetNewHashPos(WCC->Directory, 0);
			GetNextHashPos(WCC->Directory, itd, &len, &Key, &vDir); //TODO: how many to fast forward?
	/* Fast forward the floorname we checked above... */
			for (i = 0, j = 1; 
			     (i > ThisFolder->nRoomNameParts) && (j > urlp); 
			     i++, j++, GetNextHashPos(WCC->Directory, itd, &len, &Key, &vDir))
			{
				Dir = (StrBuf*)vDir;
				if (strcmp(ChrPtr(ThisFolder->RoomNameParts[i]), 
					   ChrPtr(Dir)) != 0)
				{
					DeleteHashPos(&itd);
					continue;
				}
			}
			DeleteHashPos(&itd);
			DeleteHashPos(&itfl);
			return ThisFolder;
		}
		else {
			
			if (GetCount(WCC->Directory) - ThisFolder->nRoomNameParts != 2)
				continue;
			itd  = GetNewHashPos(WCC->Directory, 0);
			
			
			if (!GetNextHashPos(WCC->Directory, 
					    itd, &len, &Key, &vDir) ||
			    (vDir == NULL))
			{
				DeleteHashPos(&itd);
				
				lprintf(0, "5\n");
				continue;
			}
			DeleteHashPos(&itd);
			Dir = (StrBuf*) vDir;
			if (strcmp(ChrPtr(ThisFolder->name), 
					       ChrPtr(Dir))
			    != 0)
			{
				DeleteHashPos(&itd);
				
				lprintf(0, "5\n");
				continue;
			}
			
			DeleteHashPos(&itfl);
			DeleteHashPos(&itd);
			
			return ThisFolder;;
		}
	}
	DeleteHashPos(&itfl);
	return NULL;
}




long GotoRestRoom()
{
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
	
	if (Count >= 3) {
		State |= REST_IN_FLOOR;
		ThisFolder = GetRESTFolder(0);
		WCC->ThisRoom = ThisFolder;
		if (ThisFolder != NULL)
		{
			gotoroom(ThisFolder->name);
			State |= REST_IN_ROOM;
			return State;
		}
		
	}


	/* 
	 * More than 3 params and no floor found? 
	 * -> fall back to old non-floored notation
	 */

	if ((Count >= 3) && (WCC->CurrentFloor == NULL))
	{
		ThisFolder = GetRESTFolder(1);
		WCC->ThisRoom = ThisFolder;
		if (ThisFolder != NULL)
		{
			gotoroom(ThisFolder->name);
			State |= REST_IN_ROOM;
			return State;
		}


	}


	if (Count == 3) return State;

	/// TODO: ID detection
	/// TODO: File detection


	return State;
}
/*
 * The pathname is always going to be /groupdav/room_name/msg_num
 */
void groupdav_propfind(void) 
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
	long State;

	now = time(NULL);
	http_datestring(datestring, sizeof datestring, now);

	dav_roomname = NewStrBuf();
	dav_uid = NewStrBuf();
	StrBufExtract_token(dav_roomname, WCC->Hdr->HR.ReqLine, 0, '/');
	StrBufExtract_token(dav_uid, WCC->Hdr->HR.ReqLine, 1, '/');

	/*
	 * If the room name is blank, the client is requesting a
	 * folder list.
	 */
	State = GotoRestRoom();
	if (((State & REST_IN_ROOM) == 0) ||
	    (((State & (REST_GOT_EUID|REST_GOT_ID|REST_GOT_FILENAME)) == 0) &&
	     (WCC->Hdr->HR.dav_depth == 0)))
	{
		now = time(NULL);
		http_datestring(datestring, sizeof datestring, now);

		/*
		 * Be rude.  Completely ignore the XML request and simply send them
		 * everything we know about.  Let the client sort it out.
		 */
		hprintf("HTTP/1.0 207 Multi-Status\r\n");
		groupdav_common_headers();
		hprintf("Date: %s\r\n", datestring);
		hprintf("Content-type: text/xml\r\n");
		hprintf("Content-encoding: identity\r\n");

		begin_burst();


		/*
		 * If the client is requesting the root, show a root node.
		 */
		do_template("dav_propfind_top", NULL);
		end_burst();
		FreeStrBuf(&dav_roomname);
		FreeStrBuf(&dav_uid);
		return;
	}

	if ((State & (REST_GOT_EUID|REST_GOT_ID|REST_GOT_FILENAME)) == 0) {
		readloop(headers, eReadEUIDS);
		return;

	}


	/* Go to the correct room. */
	if (strcasecmp(ChrPtr(WCC->wc_roomname), ChrPtr(dav_roomname))) {
		gotoroom(dav_roomname);
	}
	if (strcasecmp(ChrPtr(WCC->wc_roomname), ChrPtr(dav_roomname))) {
		hprintf("HTTP/1.1 404 not found\r\n");
		groupdav_common_headers();
		hprintf("Date: %s\r\n", datestring);
		hprintf("Content-Type: text/plain\r\n");
		wc_printf("There is no folder called \"%s\" on this server.\r\n",
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
		groupdav_common_headers();
		hprintf("Date: %s\r\n", datestring);
		hprintf("Content-type: text/xml\r\n");
		hprintf("Content-encoding: identity\r\n");
	
		begin_burst();
	
		wc_printf("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
     			"<multistatus xmlns=\"DAV:\">"
		);

		wc_printf("<response>");
		
		wc_printf("<href>");
		groupdav_identify_host();
		wc_printf("/groupdav/");
		urlescputs(ChrPtr(WCC->wc_roomname));
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

	wc_printf("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
     		"<multistatus xmlns=\"DAV:\" xmlns:G=\"http://groupdav.org/\">"
	);


	/* Transmit the collection resource (FIXME check depth and starting point) */
	wc_printf("<response>");

	wc_printf("<href>");
	groupdav_identify_host();
	wc_printf("/groupdav/");
	urlescputs(ChrPtr(WCC->wc_roomname));
	wc_printf("</href>");

	wc_printf("<propstat>");
	wc_printf("<status>HTTP/1.1 200 OK</status>");
	wc_printf("<prop>");
	wc_printf("<displayname>");
	escputs(ChrPtr(WCC->wc_roomname));
	wc_printf("</displayname>");
	wc_printf("<resourcetype><collection/>");

	switch(WCC->wc_default_view) {
		case VIEW_CALENDAR:
			wc_printf("<G:vevent-collection />");
			break;
		case VIEW_TASKS:
			wc_printf("<G:vtodo-collection />");
			break;
		case VIEW_ADDRESSBOOK:
			wc_printf("<G:vcard-collection />");
			break;
	}

	wc_printf("</resourcetype>");
	/* FIXME get the mtime
	wc_printf("<getlastmodified>");
		escputs(datestring);
	wc_printf("</getlastmodified>");
	*/
	wc_printf("</prop>");
	wc_printf("</propstat>");
	wc_printf("</response>");

	/* Transmit the collection listing (FIXME check depth and starting point) */

	MsgNum = NewStrBuf();
	serv_puts("MSGS ALL");

	StrBuf_ServGetln(MsgNum);
	if (GetServerStatus(MsgNum, NULL) == 1)
		while (BufLen = StrBuf_ServGetln(MsgNum), strcmp(ChrPtr(MsgNum), "000"))  {
			msgs = realloc(msgs, ++num_msgs * sizeof(long));
			msgs[num_msgs-1] = StrTol(MsgNum);
		}

	if (num_msgs > 0) for (i=0; i<num_msgs; ++i) {

		strcpy(uid, "");
		now = (-1);
		serv_printf("MSG0 %ld|3", msgs[i]);
		StrBuf_ServGetln(MsgNum);
		if (GetServerStatus(MsgNum, NULL) == 1)
			while (BufLen = StrBuf_ServGetln(MsgNum), strcmp(ChrPtr(MsgNum), "000")) 
			{
				if (!strncasecmp(ChrPtr(MsgNum), "exti=", 5)) {
					strcpy(uid, &ChrPtr(MsgNum)[5]);
				}
				else if (!strncasecmp(ChrPtr(MsgNum), "time=", 5)) {
					now = atol(&ChrPtr(MsgNum)[5]);
			}
		}

		if (!IsEmptyStr(uid)) {
			wc_printf("<response>");
				wc_printf("<href>");
					groupdav_identify_host();
					wc_printf("/groupdav/");
					urlescputs(ChrPtr(WCC->wc_roomname));
					euid_escapize(encoded_uid, uid);
					wc_printf("/%s", encoded_uid);
				wc_printf("</href>");
				switch(WCC->wc_default_view) {
				case VIEW_CALENDAR:
					wc_printf("<getcontenttype>text/x-ical</getcontenttype>");
					break;
				case VIEW_TASKS:
					wc_printf("<getcontenttype>text/x-ical</getcontenttype>");
					break;
				case VIEW_ADDRESSBOOK:
					wc_printf("<getcontenttype>text/x-vcard</getcontenttype>");
					break;
				}
				wc_printf("<propstat>");
					wc_printf("<status>HTTP/1.1 200 OK</status>");
					wc_printf("<prop>");
						wc_printf("<getetag>\"%ld\"</getetag>", msgs[i]);
					if (now > 0L) {
						http_datestring(datestring, sizeof datestring, now);
						wc_printf("<getlastmodified>");
						escputs(datestring);
						wc_printf("</getlastmodified>");
					}
					wc_printf("</prop>");
				wc_printf("</propstat>");
			wc_printf("</response>");
		}
	}
	FreeStrBuf(&MsgNum);

	wc_printf("</multistatus>\n");
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
	return StrLength(Msg->euid) > 0;
}

int DavUIDL_GetParamsGetServerCall(SharedMessageStatus *Stat, 
				    void **ViewSpecific, 
				    long oper, 
				    char *cmd, 
				    long len)
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
		NULL, /// TODO: is this right?
		ParseMessageListHeaders_EUID,
		NULL, //// ""
		DavUIDL_RenderView_or_Tail,
		DavUIDL_Cleanup);

}
