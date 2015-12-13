/*
 * Functions which deal with the fetching and displaying of messages.
 *
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"
#include "webserver.h"
#include "dav.h"
#include "calendar.h"

HashList *MsgHeaderHandler = NULL;
HashList *MimeRenderHandler = NULL;
HashList *ReadLoopHandler = NULL;
int dbg_analyze_msg = 0;

#define SUBJ_COL_WIDTH_PCT		50	/* Mailbox view column width */
#define SENDER_COL_WIDTH_PCT		30	/* Mailbox view column width */
#define DATE_PLUS_BUTTONS_WIDTH_PCT	20	/* Mailbox view column width */

void jsonMessageListHdr(void);

void display_enter(void);

void fixview()
{
	/* workaround for json listview; its not useable directly */
	if (WC->CurRoom.view == VIEW_JSON_LIST) {
		StrBuf *View = NewStrBuf();
		StrBufPrintf(View, "%d", VIEW_MAILBOX);
		putbstr("view", View);;
	}
}
int load_message(message_summary *Msg, 
		 StrBuf *FoundCharset,
		 StrBuf **Error)
{
	StrBuf *Buf;
	StrBuf *HdrToken;
	headereval *Hdr;
	void *vHdr;
	char buf[SIZ];
	int Done = 0;
	int state=0;
	
	Buf = NewStrBuf();
	if (Msg->PartNum != NULL) {
		serv_printf("MSG4 %ld|%s", Msg->msgnum, ChrPtr(Msg->PartNum));
	}
	else {
		serv_printf("MSG4 %ld", Msg->msgnum);
	}

	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 1) {
		*Error = NewStrBuf();
		StrBufAppendPrintf(*Error, "<strong>");
		StrBufAppendPrintf(*Error, _("ERROR:"));
		StrBufAppendPrintf(*Error, "</strong> %s<br>\n", &buf[4]);
		FreeStrBuf(&Buf);
		return 0;
	}

	/* begin everythingamundo table */
	HdrToken = NewStrBuf();
	while (!Done && StrBuf_ServGetln(Buf)>=0) {
		if ( (StrLength(Buf)==3) && 
		    !strcmp(ChrPtr(Buf), "000")) 
		{
			Done = 1;
			if (state < 2) {
				if (Msg->MsgBody->Data == NULL)
					Msg->MsgBody->Data = NewStrBuf();
				Msg->MsgBody->ContentType = NewStrBufPlain(HKEY("text/html"));
				StrBufAppendPrintf(Msg->MsgBody->Data, "<div><i>");
				StrBufAppendPrintf(Msg->MsgBody->Data, _("Empty message"));
				StrBufAppendPrintf(Msg->MsgBody->Data, "</i><br><br>\n");
				StrBufAppendPrintf(Msg->MsgBody->Data, "</div>\n");
			}
			break;
		}
		switch (state) {
		case 0:/* Citadel Message Headers */
			if (StrLength(Buf) == 0) {
				state ++;
				break;
			}
			StrBufExtract_token(HdrToken, Buf, 0, '=');
			StrBufCutLeft(Buf, StrLength(HdrToken) + 1);
			
			/* look up one of the examine_* functions to parse the content */
			if (GetHash(MsgHeaderHandler, SKEY(HdrToken), &vHdr) &&
			    (vHdr != NULL)) {
				Hdr = (headereval*)vHdr;
				Hdr->evaluator(Msg, Buf, FoundCharset);
				if (Hdr->Type == 1) {
					state++;
				}
			}/* TODO: 
			else LogError(Target, 
				      __FUNCTION__,  
				      "don't know how to handle message header[%s]\n", 
				      ChrPtr(HdrToken));
			 */
			break;
		case 1:/* Message Mime Header */
			if (StrLength(Buf) == 0) {
				state++;
				if (Msg->MsgBody->ContentType == NULL)
                			/* end of header or no header? */
					Msg->MsgBody->ContentType = NewStrBufPlain(HKEY("text/plain"));
				 /* usual end of mime header */
			}
			else
			{
				StrBufExtract_token(HdrToken, Buf, 0, ':');
				if (StrLength(HdrToken) > 0) {
					StrBufCutLeft(Buf, StrLength(HdrToken) + 1);
					/* the examine*'s know how to do with mime headers too... */
					if (GetHash(MsgHeaderHandler, SKEY(HdrToken), &vHdr) &&
					    (vHdr != NULL)) {
						Hdr = (headereval*)vHdr;
						Hdr->evaluator(Msg, Buf, FoundCharset);
					}
					break;
				}
			}
		case 2: /* Message Body */
			
			if (Msg->MsgBody->size_known > 0) {
				StrBuf_ServGetBLOBBuffered(Msg->MsgBody->Data, Msg->MsgBody->length);
				state ++;
				/*/ todo: check next line, if not 000, append following lines */
			}
			else if (1){
				if (StrLength(Msg->MsgBody->Data) > 0)
					StrBufAppendBufPlain(Msg->MsgBody->Data, "\n", 1, 0);
				StrBufAppendBuf(Msg->MsgBody->Data, Buf, 0);
			}
			break;
		case 3:
			StrBufAppendBuf(Msg->MsgBody->Data, Buf, 0);
			break;
		}
	}

	if (Msg->AllAttach == NULL)
		Msg->AllAttach = NewHash(1,NULL);
	/* now we put the body mimepart we read above into the mimelist */
	Put(Msg->AllAttach, SKEY(Msg->MsgBody->PartNum), Msg->MsgBody, DestroyMime);
	
	FreeStrBuf(&Buf);
	FreeStrBuf(&HdrToken);
	return 1;
}



/*
 * I wanna SEE that message!
 *
 * msgnum		Message number to display
 * printable_view	Nonzero to display a printable view
 * section		Optional for encapsulated message/rfc822 submessage
 */
int read_message(StrBuf *Target, const char *tmpl, long tmpllen, long msgnum, const StrBuf *PartNum, const StrBuf **OutMime, WCTemplputParams *TP) 
{
	StrBuf *Buf;
	StrBuf *FoundCharset;
	HashPos  *it;
	void *vMime;
	message_summary *Msg = NULL;
	void *vHdr;
	long len;
	const char *Key;
	WCTemplputParams SuperTP;
	WCTemplputParams SubTP;
	StrBuf *Error = NULL;

	memset(&SuperTP, 0, sizeof(WCTemplputParams));
	memset(&SubTP, 0, sizeof(WCTemplputParams));

	Buf = NewStrBuf();
	FoundCharset = NewStrBuf();
	Msg = (message_summary *)malloc(sizeof(message_summary));
	memset(Msg, 0, sizeof(message_summary));
	Msg->msgnum = msgnum;
	Msg->PartNum = PartNum;
	Msg->MsgBody =  (wc_mime_attachment*) malloc(sizeof(wc_mime_attachment));
	memset(Msg->MsgBody, 0, sizeof(wc_mime_attachment));
	Msg->MsgBody->msgnum = msgnum;

	if (!load_message(Msg, FoundCharset, &Error)) {
		StrBufAppendBuf(Target, Error, 0);
		FreeStrBuf(&Error);
	}

	/* Extract just the content-type (omit attributes such as "charset") */
	StrBufExtract_token(Buf, Msg->MsgBody->ContentType, 0, ';');
	StrBufTrim(Buf);
	StrBufLowerCase(Buf);

	StackContext(TP, &SuperTP, Msg, CTX_MAILSUM, 0, NULL);
	{
		/* Locate a renderer capable of converting this MIME part into HTML */
		if (GetHash(MimeRenderHandler, SKEY(Buf), &vHdr) &&
		    (vHdr != NULL)) {
			RenderMimeFuncStruct *Render;
			
			StackContext(&SuperTP, &SubTP, Msg->MsgBody, CTX_MIME_ATACH, 0, NULL);
			{
				Render = (RenderMimeFuncStruct*)vHdr;
				Render->f(Target, &SubTP, FoundCharset);
			}
			UnStackContext(&SubTP);
		}
		
		if (StrLength(Msg->reply_references)> 0) {
			/* Trim down excessively long lists of thread references.  We eliminate the
			 * second one in the list so that the thread root remains intact.
			 */
			int rrtok = num_tokens(ChrPtr(Msg->reply_references), '|');
			int rrlen = StrLength(Msg->reply_references);
			if ( ((rrtok >= 3) && (rrlen > 900)) || (rrtok > 10) ) {
				StrBufRemove_token(Msg->reply_references, 1, '|');
			}
		}

		/* now check if we need to translate some mimeparts, and remove the duplicate */
		it = GetNewHashPos(Msg->AllAttach, 0);
		while (GetNextHashPos(Msg->AllAttach, it, &len, &Key, &vMime) && 
		       (vMime != NULL)) {
			StackContext(&SuperTP, &SubTP, vMime, CTX_MIME_ATACH, 0, NULL);
			{
				evaluate_mime_part(Target, &SubTP);
			}
			UnStackContext(&SubTP);
		}
		DeleteHashPos(&it);
		*OutMime = DoTemplate(tmpl, tmpllen, Target, &SuperTP);
	}
	UnStackContext(&SuperTP);

	DestroyMessageSummary(Msg);
	FreeStrBuf(&FoundCharset);
	FreeStrBuf(&Buf);
	return 1;
}


long
HttpStatus(long CitadelStatus)
{
	long httpstatus = 502;
	
	switch (MAJORCODE(CitadelStatus))
	{
	case LISTING_FOLLOWS:
	case CIT_OK:
		httpstatus = 201;
		break;
	case ERROR:
		switch (MINORCODE(CitadelStatus))
		{
		case INTERNAL_ERROR:
			httpstatus = 403;
			break;
			
		case TOO_BIG:
		case ILLEGAL_VALUE:
		case HIGHER_ACCESS_REQUIRED:
		case MAX_SESSIONS_EXCEEDED:
		case RESOURCE_BUSY:
		case RESOURCE_NOT_OPEN:
		case NOT_HERE:
		case INVALID_FLOOR_OPERATION:
		case FILE_NOT_FOUND:
		case ROOM_NOT_FOUND:
			httpstatus = 409;
			break;

		case MESSAGE_NOT_FOUND:
		case ALREADY_EXISTS:
			httpstatus = 403;
			break;

		case NO_SUCH_SYSTEM:
			httpstatus = 502;
			break;

		default:
		case CMD_NOT_SUPPORTED:
		case PASSWORD_REQUIRED:
		case ALREADY_LOGGED_IN:
		case USERNAME_REQUIRED:
		case NOT_LOGGED_IN:
		case SERVER_SHUTTING_DOWN:
		case NO_SUCH_USER:
		case ASYNC_GEXP:
			httpstatus = 502;
			break;
		}
		break;

	default:
	case BINARY_FOLLOWS:
	case SEND_BINARY:
	case START_CHAT_MODE:
	case ASYNC_MSG:
	case MORE_DATA:
	case SEND_LISTING:
		httpstatus = 502; /* aeh... whut? */
		break;
	}

	return httpstatus;
}

/*
 * Unadorned HTML output of an individual message, suitable
 * for placing in a hidden iframe, for printing, or whatever
 */
void handle_one_message(void) 
{
	long CitStatus = ERROR + NOT_HERE;
	int CopyMessage = 0;
	const StrBuf *Destination;
	void *vLine;
	const StrBuf *Mime;
	long msgnum = 0L;
	wcsession *WCC = WC;
	const StrBuf *Tmpl;
	StrBuf *CmdBuf = NULL;
	const char *pMsg;


	pMsg = strchr(ChrPtr(WCC->Hdr->HR.ReqLine), '/');
	if (pMsg == NULL) {
		HttpStatus(CitStatus);
		return;
	}

	msgnum = atol(pMsg + 1);
	StrBufCutAt(WCC->Hdr->HR.ReqLine, 0, pMsg);
	gotoroom(WCC->Hdr->HR.ReqLine);
	switch (WCC->Hdr->HR.eReqType)
	{
	case eGET:
	case ePOST:
		Tmpl = sbstr("template");
		if (StrLength(Tmpl) > 0) 
			read_message(WCC->WBuf, SKEY(Tmpl), msgnum, NULL, &Mime, NULL);
		else 
			read_message(WCC->WBuf, HKEY("view_message"), msgnum, NULL, &Mime, NULL);
		http_transmit_thing(ChrPtr(Mime), 0);
		break;
	case eDELETE:
		CmdBuf = NewStrBuf ();
		if ((WCC->CurRoom.RAFlags & UA_ISTRASH) != 0) {	/* Delete from Trash is a real delete */
			serv_printf("DELE %ld", msgnum);	
		}
		else {			/* Otherwise move it to Trash */
			serv_printf("MOVE %ld|_TRASH_|0", msgnum);
		}
		StrBuf_ServGetln(CmdBuf);
		GetServerStatusMsg(CmdBuf, &CitStatus, 1, 0);
		HttpStatus(CitStatus);
		break;
	case eCOPY:
		CopyMessage = 1;
	case eMOVE:
		if (GetHash(WCC->Hdr->HTTPHeaders, HKEY("DESTINATION"), &vLine) &&
		    (vLine!=NULL)) {
			Destination = (StrBuf*) vLine;
			serv_printf("MOVE %ld|%s|%d", msgnum, ChrPtr(Destination), CopyMessage);
			StrBuf_ServGetln(CmdBuf);
			GetServerStatusMsg(CmdBuf, NULL, 1, 0);
			GetServerStatus(CmdBuf, &CitStatus);
			HttpStatus(CitStatus);
		}
		else
			HttpStatus(500);
		break;
	default:
		break;

	}
}


/*
 * Unadorned HTML output of an individual message, suitable
 * for placing in a hidden iframe, for printing, or whatever
 */
void embed_message(void) {
	const StrBuf *Mime;
	long msgnum = 0L;
	wcsession *WCC = WC;
	const StrBuf *Tmpl;
	StrBuf *CmdBuf = NULL;

	msgnum = StrBufExtract_long(WCC->Hdr->HR.ReqLine, 0, '/');
	if (msgnum <= 0) return;

	switch (WCC->Hdr->HR.eReqType)
	{
	case eGET:
	case ePOST:
		Tmpl = sbstr("template");
		if (StrLength(Tmpl) > 0) 
			read_message(WCC->WBuf, SKEY(Tmpl), msgnum, NULL, &Mime, NULL);
		else 
			read_message(WCC->WBuf, HKEY("view_message"), msgnum, NULL, &Mime, NULL);
		http_transmit_thing(ChrPtr(Mime), 0);
		break;
	case eDELETE:
		CmdBuf = NewStrBuf ();
		if ((WCC->CurRoom.RAFlags & UA_ISTRASH) != 0) {	/* Delete from Trash is a real delete */
			serv_printf("DELE %ld", msgnum);	
		}
		else {			/* Otherwise move it to Trash */
			serv_printf("MOVE %ld|_TRASH_|0", msgnum);
		}
		StrBuf_ServGetln(CmdBuf);
		GetServerStatusMsg(CmdBuf, NULL, 1, 0);
		break;
	default:
		break;

	}
}


/*
 * Printable view of a message
 */
void print_message(void) {
	long msgnum = 0L;
	const StrBuf *Mime;

	msgnum = StrBufExtract_long(WC->Hdr->HR.ReqLine, 0, '/');
	output_headers(0, 0, 0, 0, 0, 0);

	hprintf("Content-type: text/html\r\n"
		"Server: " PACKAGE_STRING "\r\n"
		"Connection: close\r\n");

	begin_burst();

	read_message(WC->WBuf, HKEY("view_message_print"), msgnum, NULL, &Mime, NULL);

	wDumpContent(0);
}

/*
 * Display a message's headers
 */
void display_headers(void) {
	long msgnum = 0L;
	char buf[1024];

	msgnum = StrBufExtract_long(WC->Hdr->HR.ReqLine, 0, '/');
	output_headers(0, 0, 0, 0, 0, 0);

	hprintf("Content-type: text/plain\r\n"
		"Server: %s\r\n"
		"Connection: close\r\n",
		PACKAGE_STRING);
	begin_burst();

	serv_printf("MSG2 %ld|1", msgnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			wc_printf("%s\n", buf);
		}
	}

	wDumpContent(0);
}



/*
 * load message pointers from the server for a "read messages" operation
 *
 * servcmd:		the citadel command to send to the citserver
 */
int load_msg_ptrs(const char *servcmd,
		  const char *filter,
		  SharedMessageStatus *Stat, 
		  load_msg_ptrs_detailheaders LH)
{
        wcsession *WCC = WC;
	message_summary *Msg;
	StrBuf *Buf, *Buf2;
	long len;
	int n;
	int skipit;
	const char *Ptr = NULL;
	int StatMajor;

	Stat->lowest_found = LONG_MAX;
	Stat->highest_found = LONG_MIN;

	if (WCC->summ != NULL) {
		DeleteHash(&WCC->summ);
	}
	WCC->summ = NewHash(1, Flathash);
	
	Buf = NewStrBuf();
	serv_puts(servcmd);
	StrBuf_ServGetln(Buf);
	StatMajor = GetServerStatus(Buf, NULL);
	switch (StatMajor) {
	case 1:
		break;
	case 8:
		if (filter != NULL) {
			serv_puts(filter);
                        serv_puts("000");
			break;
		}
		/* fall back to empty filter in case of we were fooled... */
		serv_puts("");
		serv_puts("000");
		break;
	default:
		FreeStrBuf(&Buf);
		return (Stat->nummsgs);
	}
	Buf2 = NewStrBuf();
	while (len = StrBuf_ServGetln(Buf), 
	       ((len >= 0) &&
		((len != 3) || 
		 strcmp(ChrPtr(Buf), "000")!= 0)))
	{
		if (Stat->nummsgs < Stat->maxload) {
			skipit = 0;
			Ptr = NULL;
			Msg = (message_summary*)malloc(sizeof(message_summary));
			memset(Msg, 0, sizeof(message_summary));

			Msg->msgnum = StrBufExtractNext_long(Buf, &Ptr, '|');
			Msg->date = StrBufExtractNext_long(Buf, &Ptr, '|');

			if (Stat->nummsgs == 0) {
				if (Msg->msgnum < Stat->lowest_found) {
					Stat->lowest_found = Msg->msgnum;
				}
				if (Msg->msgnum > Stat->highest_found) {
					Stat->highest_found = Msg->msgnum;
				}
			}

			if ((Msg->msgnum == 0) && (StrLength(Buf) < 32)) {
				free(Msg);
				continue;
			}

			/* 
			 * as citserver probably gives us messages in forward date sorting
			 * nummsgs should be the same order as the message date.
			 */
			if (Msg->date == 0) {
				Msg->date = Stat->nummsgs;
				if (StrLength(Buf) < 32) 
					skipit = 1;
			}
			if ((!skipit) && (LH != NULL)) {
				if (!LH(Buf, &Ptr, Msg, Buf2)){
					free(Msg);
					continue;
				}					
			}
			n = Msg->msgnum;
			Put(WCC->summ, (const char *)&n, sizeof(n), Msg, DestroyMessageSummary);
		}
		Stat->nummsgs++;
	}
	FreeStrBuf(&Buf2);
	FreeStrBuf(&Buf);
	return (Stat->nummsgs);
}



/**
 * \brief sets FlagToSet for each of ScanMe that appears in MatchMSet
 * \param ScanMe List of BasicMsgStruct to be searched it MatchSet
 * \param MatchMSet MSet we want to flag
 * \param FlagToSet Flag to set on each BasicMsgStruct->Flags if in MSet
 */
long SetFlagsFromMSet(HashList *ScanMe, MSet *MatchMSet, int FlagToSet, int Reverse)
{
	const char *HashKey;
	long HKLen;
	long count = 0;
	HashPos *at;
	void *vMsg;
	message_summary *Msg;

	at = GetNewHashPos(ScanMe, 0);
	while (GetNextHashPos(ScanMe, at, &HKLen, &HashKey, &vMsg)) {
		/* Are you a new message, or an old message? */
		Msg = (message_summary*) vMsg;
		if (Reverse && IsInMSetList(MatchMSet, Msg->msgnum)) {
			Msg->Flags = Msg->Flags | FlagToSet;
			count++;
		}
		else if (!Reverse && !IsInMSetList(MatchMSet, Msg->msgnum)) {
			Msg->Flags = Msg->Flags | FlagToSet;
			count++;
		}
	}
	DeleteHashPos(&at);
	return count;
}


long load_seen_flags(void)
{
	long count = 0;
	StrBuf *OldMsg;
	wcsession *WCC = WC;
	MSet *MatchMSet;

	OldMsg = NewStrBuf();
	serv_puts("GTSN");
	StrBuf_ServGetln(OldMsg);
	if (GetServerStatus(OldMsg, NULL) == 2) {
		StrBufCutLeft(OldMsg, 4);
	}
	else {
		FreeStrBuf(&OldMsg);
		return 0;
	}

	if (ParseMSet(&MatchMSet, OldMsg))
	{
		count = SetFlagsFromMSet(WCC->summ, MatchMSet, MSGFLAG_READ, 0);
	}
	DeleteMSet(&MatchMSet);
	FreeStrBuf(&OldMsg);
	return count;
}

extern readloop_struct rlid[];

typedef struct _RoomRenderer{
	int RoomType;

	GetParamsGetServerCall_func GetParamsGetServerCall;
	
	PrintViewHeader_func PrintPageHeader;
	PrintViewHeader_func PrintViewHeader;
	LoadMsgFromServer_func LoadMsgFromServer;
	RenderView_or_Tail_func RenderView_or_Tail;
	View_Cleanup_func ViewCleanup;
	load_msg_ptrs_detailheaders LHParse;
} RoomRenderer;


/*
 * command loop for reading messages
 *
 * Set oper to "readnew" or "readold" or "readfwd" or "headers" or "readgt" or "readlt" or "do_search"
 */
void readloop(long oper, eCustomRoomRenderer ForceRenderer)
{
	RoomRenderer *ViewMsg;
	void *vViewMsg;
	void *vMsg;
	message_summary *Msg;
	char cmd[256] = "";
	char filter[256] = "";
	int i, r;
	wcsession *WCC = WC;
	HashPos *at;
	const char *HashKey;
	long HKLen;
	WCTemplputParams SubTP;
	SharedMessageStatus Stat;
	void *ViewSpecific = NULL;

	if (havebstr("is_summary") && (1 == (ibstr("is_summary")))) {
		WCC->CurRoom.view = VIEW_MAILBOX;
	}

	if (havebstr("view")) {
		WCC->CurRoom.view = ibstr("view");
	}

	memset(&Stat, 0, sizeof(SharedMessageStatus));
	Stat.maxload = 10000;
	Stat.lowest_found = (-1);
	Stat.highest_found = (-1);
	if (ForceRenderer == eUseDefault)
		GetHash(ReadLoopHandler, IKEY(WCC->CurRoom.view), &vViewMsg);
	else 
		GetHash(ReadLoopHandler, IKEY(ForceRenderer), &vViewMsg);
	if (vViewMsg == NULL) {
		WCC->CurRoom.view = VIEW_BBS;
		GetHash(ReadLoopHandler, IKEY(WCC->CurRoom.view), &vViewMsg);
	}
	if (vViewMsg == NULL) {
		return;			/* TODO: print message */
	}

	ViewMsg = (RoomRenderer*) vViewMsg;
	if (ViewMsg->PrintPageHeader == NULL)
		output_headers(1, 1, 1, 0, 0, 0);
	else 
		ViewMsg->PrintPageHeader(&Stat, ViewSpecific);

	if (ViewMsg->GetParamsGetServerCall != NULL) {
		r = ViewMsg->GetParamsGetServerCall(
		       &Stat,
		       &ViewSpecific,
		       oper,
		       cmd, sizeof(cmd),
		       filter, sizeof(filter)
		);
	} else {
		r = 0;
	}

	switch(r)
	{
	case 400:
	case 404:

		return;
	case 300: /* the callback hook should do the work for us here, since he knows what to do. */
		return;
	case 200:
	default:
		break;
	}
	if (!IsEmptyStr(cmd)) {
		const char *p = NULL;
		if (!IsEmptyStr(filter))
			p = filter;
		Stat.nummsgs = load_msg_ptrs(cmd, p, &Stat, ViewMsg->LHParse);
	}

	if (Stat.sortit) {
		CompareFunc SortIt;
		StackContext(NULL, &SubTP, NULL, CTX_MAILSUM, 0, NULL);
		{
			SortIt =  RetrieveSort(&SubTP,
					       NULL, 0,
					       HKEY("date"),
					       Stat.defaultsortorder);
		}
		UnStackContext(&SubTP);
		if (SortIt != NULL)
			SortByPayload(WCC->summ, SortIt);
	}
	if (Stat.startmsg < 0) {
		Stat.startmsg =  0;
	}

	if (Stat.load_seen) Stat.numNewmsgs = load_seen_flags();
	
        /*
	 * Print any inforation above the message list...
	 */
	if (ViewMsg->PrintViewHeader != NULL)
		ViewMsg->PrintViewHeader(&Stat, &ViewSpecific);

	WCC->startmsg =  Stat.startmsg;
	WCC->maxmsgs = Stat.maxmsgs;
	WCC->num_displayed = 0;

	/* Put some helpful data in vars for mailsummary_json */
	{
		StrBuf *Foo;
		
		Foo = NewStrBuf ();
		StrBufPrintf(Foo, "%ld", Stat.nummsgs);
		PutBstr(HKEY("__READLOOP:TOTALMSGS"), NewStrBufDup(Foo)); // keep Foo!

		StrBufPrintf(Foo, "%ld", Stat.numNewmsgs);
		PutBstr(HKEY("__READLOOP:NEWMSGS"), NewStrBufDup(Foo)); // keep Foo!

		StrBufPrintf(Foo, "%ld", Stat.startmsg);
		PutBstr(HKEY("__READLOOP:STARTMSG"), Foo); // store Foo elsewhere, descope it here.
	}

	/*
	 * iterate over each message. if we need to load an attachment, do it here. 
	 */

	if ((ViewMsg->LoadMsgFromServer != NULL) && 
	    (!IsEmptyStr(cmd)))
	{
		at = GetNewHashPos(WCC->summ, 0);
		Stat.num_displayed = i = 0;
		while (	GetNextHashPos(WCC->summ, at, &HKLen, &HashKey, &vMsg)) {
			Msg = (message_summary*) vMsg;		
			if ((Msg->msgnum >= Stat.startmsg) && (Stat.num_displayed <= Stat.maxmsgs)) {
				ViewMsg->LoadMsgFromServer(&Stat, 
							   &ViewSpecific, 
							   Msg, 
							   (Msg->Flags & MSGFLAG_READ) != 0, 
							   i);
			} 
			i++;
		}
		DeleteHashPos(&at);
	}

	/*
	 * Done iterating the message list. now tasks we want to do after.
	 */
	if (ViewMsg->RenderView_or_Tail != NULL)
		ViewMsg->RenderView_or_Tail(&Stat, &ViewSpecific, oper);

	if (ViewMsg->ViewCleanup != NULL)
		ViewMsg->ViewCleanup(&ViewSpecific);

	WCC->startmsg = 0;
	WCC->maxmsgs = 0;
	if (WCC->summ != NULL) {
		DeleteHash(&WCC->summ);
	}
}


/*
 * Back end for post_message()
 * ... this is where the actual message gets transmitted to the server.
 */
void post_mime_to_server(void) {
	wcsession *WCC = WC;
	char top_boundary[SIZ];
	char alt_boundary[SIZ];
	int is_multipart = 0;
	static int seq = 0;
	wc_mime_attachment *att;
	char *encoded;
	size_t encoded_length;
	size_t encoded_strlen;
	char *txtmail = NULL;
	int include_text_alt = 0;	/* Set to nonzero to include multipart/alternative text/plain */

	sprintf(top_boundary, "Citadel--Multipart--%s--%04x--%04x",
		ChrPtr(WCC->serv_info->serv_fqdn),
		getpid(),
		++seq
	);
	sprintf(alt_boundary, "Citadel--Multipart--%s--%04x--%04x",
		ChrPtr(WCC->serv_info->serv_fqdn),
		getpid(),
		++seq
	);

	/* RFC2045 requires this, and some clients look for it... */
	serv_puts("MIME-Version: 1.0");
	serv_puts("X-Mailer: " PACKAGE_STRING);

	/* If there are attachments, we have to do multipart/mixed */
	if (GetCount(WCC->attachments) > 0) {
		is_multipart = 1;
	}

	/* Only do multipart/alternative for mailboxes.  BBS and Wiki rooms don't need it. */
	if ((WCC->CurRoom.view == VIEW_MAILBOX) ||
	    (WCC->CurRoom.view == VIEW_JSON_LIST))
	{
		include_text_alt = 1;
	}

	if (is_multipart) {
		/* Remember, serv_printf() appends an extra newline */
		serv_printf("Content-type: multipart/mixed; boundary=\"%s\"\n", top_boundary);
		serv_printf("This is a multipart message in MIME format.\n");
		serv_printf("--%s", top_boundary);
	}

	/* Remember, serv_printf() appends an extra newline */
	if (include_text_alt) {
		StrBuf *Buf;
		serv_printf("Content-type: multipart/alternative; "
			"boundary=\"%s\"\n", alt_boundary);
		serv_printf("This is a multipart message in MIME format.\n");
		serv_printf("--%s", alt_boundary);

		serv_puts("Content-type: text/plain; charset=utf-8");
		serv_puts("Content-Transfer-Encoding: quoted-printable");
		serv_puts("");
		txtmail = html_to_ascii(bstr("msgtext"), 0, 80, 0);
		Buf = NewStrBufPlain(txtmail, -1);
        	free(txtmail);

        	text_to_server_qp(Buf);     /* Transmit message in quoted-printable encoding */
		FreeStrBuf(&Buf);
		serv_printf("\n--%s", alt_boundary);
	}

	if (havebstr("markdown"))
	{
		serv_puts("Content-type: text/x-markdown; charset=utf-8");
		serv_puts("Content-Transfer-Encoding: quoted-printable");
		serv_puts("");
		text_to_server_qp(sbstr("msgtext"));	/* Transmit message in quoted-printable encoding */
	}
	else
	{
		serv_puts("Content-type: text/html; charset=utf-8");
		serv_puts("Content-Transfer-Encoding: quoted-printable");
		serv_puts("");
		serv_puts("<html><body>\r\n");
		text_to_server_qp(sbstr("msgtext"));	/* Transmit message in quoted-printable encoding */
		serv_puts("</body></html>\r\n");
	}

	if (include_text_alt) {
		serv_printf("--%s--", alt_boundary);
	}
	
	if (is_multipart) {
		long len;
		const char *Key; 
		void *vAtt;
		HashPos  *it;

		/* Add in the attachments */
		it = GetNewHashPos(WCC->attachments, 0);
		while (GetNextHashPos(WCC->attachments, it, &len, &Key, &vAtt)) {
			att = (wc_mime_attachment *)vAtt;
			if (att->length == 0)
				continue;
			encoded_length = ((att->length * 150) / 100);
			encoded = malloc(encoded_length);
			if (encoded == NULL) break;
			encoded_strlen = CtdlEncodeBase64(encoded, ChrPtr(att->Data), StrLength(att->Data), 1);

			serv_printf("--%s", top_boundary);
			serv_printf("Content-type: %s", ChrPtr(att->ContentType));
			serv_printf("Content-disposition: attachment; filename=\"%s\"", ChrPtr(att->FileName));
			serv_puts("Content-transfer-encoding: base64");
			serv_puts("");
			serv_write(encoded, encoded_strlen);
			serv_puts("");
			serv_puts("");
			free(encoded);
		}
		serv_printf("--%s--", top_boundary);
		DeleteHashPos(&it);
	}

	serv_puts("000");
}


/*
 * Post message (or don't post message)
 *
 * Note regarding the "dont_post" variable:
 * A random value (actually, it's just a timestamp) is inserted as a hidden
 * field called "postseq" when the display_enter page is generated.  This
 * value is checked when posting, using the static variable dont_post.  If a
 * user attempts to post twice using the same dont_post value, the message is
 * discarded.  This prevents the accidental double-saving of the same message
 * if the user happens to click the browser "back" button.
 */
void post_message(void)
{
	StrBuf *UserName;
	StrBuf *EmailAddress;
	StrBuf *EncBuf;
	char buf[1024];
	StrBuf *encoded_subject = NULL;
	static long dont_post = (-1L);
	int is_anonymous = 0;
	const StrBuf *display_name = NULL;
	wcsession *WCC = WC;
	StrBuf *Buf;
	
	if (havebstr("force_room")) {
		gotoroom(sbstr("force_room"));
	}

	if (havebstr("display_name")) {
		display_name = sbstr("display_name");
		if (!strcmp(ChrPtr(display_name), "__ANONYMOUS__")) {
			display_name = NULL;
			is_anonymous = 1;
		}
	}

	if (!strcasecmp(bstr("submit_action"), "cancel")) {
		AppendImportantMessage(_("Cancelled.  Message was not posted."), -1);
	} else if (lbstr("postseq") == dont_post) {
		AppendImportantMessage(
			_("Automatically cancelled because you have already "
			  "saved this message."), -1);
	} else {
		const char CMD[] = "ENT0 1|%s|%d|4|%s|%s||%s|%s|%s|%s|%s";
		StrBuf *Recp = NULL; 
		StrBuf *Cc = NULL;
		StrBuf *Bcc = NULL;
		char *wikipage = NULL;
		const StrBuf *my_email_addr = NULL;
		StrBuf *CmdBuf = NULL;
		StrBuf *references = NULL;
		int saving_to_drafts = 0;
		long HeaderLen = 0;

		saving_to_drafts = !strcasecmp(bstr("submit_action"), "draft");
		Buf = NewStrBuf();

		if (saving_to_drafts) {
		        /* temporarily change to the drafts room */
		        serv_puts("GOTO _DRAFTS_");
			StrBuf_ServGetln(Buf);
			if (GetServerStatusMsg(Buf, NULL, 1, 2) != 2) {
				/* You probably don't even have a dumb Drafts folder */
				syslog(LOG_DEBUG, "%s:%d: server save to drafts error: %s\n", __FILE__, __LINE__, ChrPtr(Buf) + 4);
				AppendImportantMessage(_("Saved to Drafts failed: "), -1);
				display_enter();
				FreeStrBuf(&Buf);
				return;
			}
		}

		if (havebstr("references"))
		{
			const StrBuf *ref = sbstr("references");
			references = NewStrBufDup(ref);
			if (*ChrPtr(references) == '|') {	/* remove leading '|' if present */
				StrBufCutLeft(references, 1);
			}
			StrBufReplaceChars(references, '|', '!');
		}
		if (havebstr("subject")) {
			const StrBuf *Subj;
			/*
			 * make enough room for the encoded string; 
			 * plus the QP header 
			 */
			Subj = sbstr("subject");
			
			StrBufRFC2047encode(&encoded_subject, Subj);
		}
		UserName = NewStrBuf();
		EmailAddress = NewStrBuf();
		EncBuf = NewStrBuf();

		Recp = StrBufSanitizeEmailRecipientVector(sbstr("recp"), UserName, EmailAddress, EncBuf);
		Cc = StrBufSanitizeEmailRecipientVector(sbstr("cc"), UserName, EmailAddress, EncBuf);
		Bcc = StrBufSanitizeEmailRecipientVector(sbstr("bcc"), UserName, EmailAddress, EncBuf);

		FreeStrBuf(&UserName);
		FreeStrBuf(&EmailAddress);
		FreeStrBuf(&EncBuf);

		wikipage = strdup(bstr("page"));
		str_wiki_index(wikipage);
		my_email_addr = sbstr("my_email_addr");
		
		HeaderLen = StrLength(Recp) + 
			StrLength(encoded_subject) +
			StrLength(Cc) +
			StrLength(Bcc) + 
			strlen(wikipage) +
			StrLength(my_email_addr) + 
			StrLength(references);
		CmdBuf = NewStrBufPlain(NULL, sizeof (CMD) + HeaderLen);
		StrBufPrintf(CmdBuf, 
			     CMD,
			     saving_to_drafts?"":ChrPtr(Recp),
			     is_anonymous,
			     ChrPtr(encoded_subject),
			     ChrPtr(display_name),
			     saving_to_drafts?"":ChrPtr(Cc),
			     saving_to_drafts?"":ChrPtr(Bcc),
			     wikipage,
			     ChrPtr(my_email_addr),
			     ChrPtr(references));
		FreeStrBuf(&references);
		FreeStrBuf(&encoded_subject);
		free(wikipage);

		if ((HeaderLen + StrLength(sbstr("msgtext")) < 10) && 
		    (GetCount(WCC->attachments) == 0)){
			AppendImportantMessage(_("Refusing to post empty message.\n"), -1);
			FreeStrBuf(&CmdBuf);
				
		}
		else 
		{
			syslog(LOG_DEBUG, "%s\n", ChrPtr(CmdBuf));
			serv_puts(ChrPtr(CmdBuf));
			FreeStrBuf(&CmdBuf);

			StrBuf_ServGetln(Buf);
			if (GetServerStatus(Buf, NULL) == 4) {
				if (saving_to_drafts) {
					if (  (havebstr("recp"))
					      || (havebstr("cc"  ))
					      || (havebstr("bcc" )) ) {
						/* save recipient headers or room to post to */
						serv_printf("To: %s", ChrPtr(Recp));
						serv_printf("Cc: %s", ChrPtr(Cc));
						serv_printf("Bcc: %s", ChrPtr(Bcc));
					} else {
						serv_printf("X-Citadel-Room: %s", ChrPtr(WCC->CurRoom.name));
					}
				}
				post_mime_to_server();
				if (saving_to_drafts) {
					AppendImportantMessage(_("Message has been saved to Drafts.\n"), -1);
					gotoroom(WCC->CurRoom.name);
					fixview();
					readloop(readnew, eUseDefault);
					FreeStrBuf(&Buf);
					return;
				} else if (  (havebstr("recp"))
					     || (havebstr("cc"  ))
					     || (havebstr("bcc" ))
					) {
					AppendImportantMessage(_("Message has been sent.\n"), -1);
				}
				else {
					AppendImportantMessage(_("Message has been posted.\n"), -1);
				}
				dont_post = lbstr("postseq");
			} else {
				syslog(LOG_DEBUG, "%s:%d: server post error: %s", __FILE__, __LINE__, ChrPtr(Buf) + 4);
				AppendImportantMessage(ChrPtr(Buf) + 4, StrLength(Buf) - 4);
				display_enter();
				if (saving_to_drafts) gotoroom(WCC->CurRoom.name);
				FreeStrBuf(&Recp);
				FreeStrBuf(&Buf);
				FreeStrBuf(&Cc);
				FreeStrBuf(&Bcc);
				return;
			}
		}
		FreeStrBuf(&Recp);
		FreeStrBuf(&Buf);
		FreeStrBuf(&Cc);
		FreeStrBuf(&Bcc);
	}

	DeleteHash(&WCC->attachments);

	/*
	 *  We may have been supplied with instructions regarding the location
	 *  to which we must return after posting.  If found, go there.
	 */
	if (havebstr("return_to")) {
		http_redirect(bstr("return_to"));
	}
	/*
	 *  If we were editing a page in a wiki room, go to that page now.
	 */
	else if (havebstr("page")) {
		snprintf(buf, sizeof buf, "wiki?page=%s", bstr("page"));
		http_redirect(buf);
	}
	/*
	 *  Otherwise, just go to the "read messages" loop.
	 */
	else {
		fixview();
		readloop(readnew, eUseDefault);
	}
}


/*
 * Client is uploading an attachment
 */
void upload_attachment(void) {
	wcsession *WCC = WC;
	const char *pch;
	int n;
	const char *newn;
	long newnlen;
	void *v;
	wc_mime_attachment *att;
	const StrBuf *Tmpl = sbstr("template");
	const StrBuf *MimeType = NULL;
	const StrBuf *UID;

	begin_burst();
	syslog(LOG_DEBUG, "upload_attachment()\n");
	if (!Tmpl) wc_printf("upload_attachment()<br>\n");

	if (WCC->upload_length <= 0) {
		syslog(LOG_DEBUG, "ERROR no attachment was uploaded\n");
		if (Tmpl)
		{
			putlbstr("UPLOAD_ERROR", 1);
			MimeType = DoTemplate(SKEY(Tmpl), NULL, &NoCtx);
		}
		else      wc_printf("ERROR no attachment was uploaded<br>\n");
		http_transmit_thing(ChrPtr(MimeType), 0);

		return;
	}

	syslog(LOG_DEBUG, "Client is uploading %d bytes\n", WCC->upload_length);
	if (Tmpl) putlbstr("UPLOAD_LENGTH", WCC->upload_length);
	else wc_printf("Client is uploading %d bytes<br>\n", WCC->upload_length);

	att = (wc_mime_attachment*)malloc(sizeof(wc_mime_attachment));
	memset(att, 0, sizeof(wc_mime_attachment ));
	att->length = WCC->upload_length;
	att->ContentType = NewStrBufPlain(WCC->upload_content_type, -1);
	att->FileName = NewStrBufDup(WCC->upload_filename);
	UID = sbstr("qquuid");
	if (UID)
		att->PartNum = NewStrBufDup(UID);

	if (WCC->attachments == NULL) {
		WCC->attachments = NewHash(1, Flathash);
	}

	/* And add it to the list. */
	n = 0;
	if ((GetCount(WCC->attachments) > 0) && 
	    GetHashAt(WCC->attachments, 
		      GetCount(WCC->attachments) -1, 
		      &newnlen, &newn, &v))
	    n = *((int*) newn) + 1;
	Put(WCC->attachments, IKEY(n), att, DestroyMime);

	/*
	 * Mozilla sends a simple filename, which is what we want,
	 * but Satan's Browser sends an entire pathname.  Reduce
	 * the path to just a filename if we need to.
	 */
	pch = strrchr(ChrPtr(att->FileName), '/');
	if (pch != NULL) {
		StrBufCutLeft(att->FileName, pch - ChrPtr(att->FileName) + 1);
	}
	pch = strrchr(ChrPtr(att->FileName), '\\');
	if (pch != NULL) {
		StrBufCutLeft(att->FileName, pch - ChrPtr(att->FileName) + 1);
	}

	/*
	 * Transfer control of this memory from the upload struct
	 * to the attachment struct.
	 */
	att->Data = WCC->upload;
	WCC->upload = NULL;
	WCC->upload_length = 0;
	
	if (Tmpl) MimeType = DoTemplate(SKEY(Tmpl), NULL, &NoCtx);
	http_transmit_thing(ChrPtr(MimeType), 0);
}


/*
 * Remove an attachment from the message currently being composed.
 *
 * Currently we identify the attachment to be removed by its filename.
 * There is probably a better way to do this.
 */
void remove_attachment(void) {
	wcsession *WCC = WC;
	wc_mime_attachment *att;
	void *vAtt;
	StrBuf *WhichAttachment;
	HashPos *at;
	long len;
	int found=0;
	const char *key;

	WhichAttachment = NewStrBufDup(sbstr("which_attachment"));
	if (ChrPtr(WhichAttachment)[0] == '/')
		StrBufCutLeft(WhichAttachment, 1);
	StrBufUnescape(WhichAttachment, 0);
	at = GetNewHashPos(WCC->attachments, 0);
	do {
		vAtt = NULL;
		GetHashPos(WCC->attachments, at, &len, &key, &vAtt);

		att = (wc_mime_attachment*) vAtt;
		if ((att != NULL) &&
		    (
			    !strcmp(ChrPtr(WhichAttachment), ChrPtr(att->FileName)) ||
		    ((att->PartNum != NULL) &&
		     !strcmp(ChrPtr(WhichAttachment), ChrPtr(att->PartNum)))
			    ))
		{
			DeleteEntryFromHash(WCC->attachments, at);
			found=1;
			break;
		}
	}
	while (NextHashPos(WCC->attachments, at));

	FreeStrBuf(&WhichAttachment);
	wc_printf("remove_attachment(%d) completed\n", found);
}


long FourHash(const char *key, long length) 
{
        int i;
        long ret = 0;
        const unsigned char *ptr = (const unsigned char*)key;

        for (i = 0; i < 4; i++, ptr ++) 
                ret = (ret << 8) | 
                        ( ((*ptr >= 'a') &&
                           (*ptr <= 'z'))? 
                          *ptr - 'a' + 'A': 
                          *ptr);

        return ret;
}

long l_subj;
long l_wefw;
long l_msgn;
long l_from;
long l_rcpt;
long l_cccc;
long l_replyto;
long l_node;
long l_rfca;
long l_nvto;

const char *ReplyToModeStrings [3] = {
	"reply",
	"replyall",
	"forward"
};
typedef enum _eReplyToNodes {
	eReply,
	eReplyAll,
	eForward
}eReplyToNodes;
	
/*
 * display the message entry screen
 */
void display_enter(void)
{
	const char *ReplyingModeStr;
	eReplyToNodes ReplyMode = eReply;
	StrBuf *Line;
	long Result;
	int rc;
	const StrBuf *display_name = NULL;
	int recipient_required = 0;
	int subject_required = 0;
	int is_anonymous = 0;
      	wcsession *WCC = WC;
	int i = 0;
	long replying_to;

	if (havebstr("force_room")) {
		gotoroom(sbstr("force_room"));
	}

	display_name = sbstr("display_name");
	if (!strcmp(ChrPtr(display_name), "__ANONYMOUS__")) {
		display_name = NULL;
		is_anonymous = 1;
	}

	/*
	 * First, do we have permission to enter messages in this room at all?
	 */
	Line = NewStrBuf();
	serv_puts("ENT0 0");
	StrBuf_ServGetln(Line);
	rc = GetServerStatusMsg(Line, &Result, 0, 2);

	if (Result == 570) {		/* 570 means that we need a recipient here */
		recipient_required = 1;
	}
	else if (rc != 2) {		/* Any other error means that we cannot continue */
		rc = GetServerStatusMsg(Line, &Result, 0, 2);
		fixview();
		readloop(readnew, eUseDefault);
		FreeStrBuf(&Line);
		return;
	}

	/* Is the server strongly recommending that the user enter a message subject? */
	if (StrLength(Line) > 4) {
		subject_required = extract_int(ChrPtr(Line) + 4, 1);
	}

	/*
	 * Are we perhaps in an address book view?  If so, then an "enter
	 * message" command really means "add new entry."
	 */
	if (WCC->CurRoom.defview == VIEW_ADDRESSBOOK) {
		do_edit_vcard(-1, "", NULL, NULL, "",  ChrPtr(WCC->CurRoom.name));
		FreeStrBuf(&Line);
		return;
	}

	/*
	 * Are we perhaps in a calendar room?  If so, then an "enter
	 * message" command really means "add new calendar item."
	 */
	if (WCC->CurRoom.defview == VIEW_CALENDAR) {
		display_edit_event();
		FreeStrBuf(&Line);
		return;
	}

	/*
	 * Are we perhaps in a tasks view?  If so, then an "enter
	 * message" command really means "add new task."
	 */
	if (WCC->CurRoom.defview == VIEW_TASKS) {
		display_edit_task();
		FreeStrBuf(&Line);
		return;
	}


	ReplyingModeStr = bstr("replying_mode");
	if (ReplyingModeStr != NULL) for (i = 0; i < 3; i++) {
			if (strcmp(ReplyingModeStr, ReplyToModeStrings[i]) == 0) {
				ReplyMode = (eReplyToNodes) i;
				break;
			}
		}
		

	/*
	 * If the "replying_to" variable is set, it refers to a message
	 * number from which we must extract some header fields...
	 */
	replying_to = lbstr("replying_to");
	if (replying_to > 0) {
		long len;
		StrBuf *wefw = NULL;
		StrBuf *msgn = NULL;
		StrBuf *from = NULL;
		StrBuf *node = NULL;
		StrBuf *rfca = NULL;
		StrBuf *rcpt = NULL;
		StrBuf *cccc = NULL;
		StrBuf *replyto = NULL;
		StrBuf *nvto = NULL;
		serv_printf("MSG0 %ld|1", replying_to);	

		StrBuf_ServGetln(Line);
		if (GetServerStatusMsg(Line, NULL, 0, 0) == 1)
			while (len = StrBuf_ServGetln(Line),
			       (len >= 0) && 
			       ((len != 3)  ||
				strcmp(ChrPtr(Line), "000")))
			{
				long which = 0;
				if ((StrLength(Line) > 4) && 
				    (ChrPtr(Line)[4] == '='))
					which = FourHash(ChrPtr(Line), 4);

				if (which == l_subj)
				{
					StrBuf *subj = NewStrBuf();
					StrBuf *FlatSubject;

					if (ReplyMode == eForward) {
						if (strncasecmp(ChrPtr(Line) + 5, "Fw:", 3)) {
							StrBufAppendBufPlain(subj, HKEY("Fw: "), 0);
						}
					}
					else {
						if (strncasecmp(ChrPtr(Line) + 5, "Re:", 3)) {
							StrBufAppendBufPlain(subj, HKEY("Re: "), 0);
						}
					}
					StrBufAppendBufPlain(subj, 
							     ChrPtr(Line) + 5, 
							     StrLength(Line) - 5, 0);
					FlatSubject = NewStrBufPlain(NULL, StrLength(subj));
					StrBuf_RFC822_to_Utf8(FlatSubject, subj, NULL, NULL);

					PutBstr(HKEY("subject"), FlatSubject);
				}

				else if (which == l_wefw)
				{
					int rrtok;
					int rrlen;

					wefw = NewStrBufPlain(ChrPtr(Line) + 5, StrLength(Line) - 5);
					
					/* Trim down excessively long lists of thread references.  We eliminate the
					 * second one in the list so that the thread root remains intact.
					 */
					rrtok = num_tokens(ChrPtr(wefw), '|');
					rrlen = StrLength(wefw);
					if ( ((rrtok >= 3) && (rrlen > 900)) || (rrtok > 10) ) {
						StrBufRemove_token(wefw, 1, '|');
					}
				}

				else if (which == l_msgn) {
					msgn = NewStrBufPlain(ChrPtr(Line) + 5, StrLength(Line) - 5);
				}

				else if (which == l_from) {
					StrBuf *FlatFrom;
					from = NewStrBufPlain(ChrPtr(Line) + 5, StrLength(Line) - 5);
					FlatFrom = NewStrBufPlain(NULL, StrLength(from));
					StrBuf_RFC822_to_Utf8(FlatFrom, from, NULL, NULL);
					FreeStrBuf(&from);
					from = FlatFrom;
					for (i=0; i<StrLength(from); ++i) {
						if (ChrPtr(from)[i] == ',')
							StrBufPeek(from, NULL, i, ' ');
					}
				}
				
				else if (which == l_rcpt) {
					rcpt = NewStrBufPlain(ChrPtr(Line) + 5, StrLength(Line) - 5);
				}
				
				else if (which == l_cccc) {
					cccc = NewStrBufPlain(ChrPtr(Line) + 5, StrLength(Line) - 5);
				}
				
				else if (which == l_node) {
					node = NewStrBufPlain(ChrPtr(Line) + 5, StrLength(Line) - 5);
				}
				else if (which == l_replyto) {
					replyto = NewStrBufPlain(ChrPtr(Line) + 5, StrLength(Line) - 5);
				}
				else if (which == l_rfca) {
					StrBuf *FlatRFCA;
					rfca = NewStrBufPlain(ChrPtr(Line) + 5, StrLength(Line) - 5);
					FlatRFCA = NewStrBufPlain(NULL, StrLength(rfca));
					StrBuf_RFC822_to_Utf8(FlatRFCA, rfca, NULL, NULL);
					FreeStrBuf(&rfca);
					rfca = FlatRFCA;
				}
				else if (which == l_nvto) {
					nvto = NewStrBufPlain(ChrPtr(Line) + 5, StrLength(Line) - 5);
					putbstr("nvto", nvto);
				}
			}


		if (StrLength(wefw) + StrLength(msgn) > 0) {
			StrBuf *refs = NewStrBuf();
			if (StrLength(wefw) > 0) {
				StrBufAppendBuf(refs, wefw, 0);
			}
			if ( (StrLength(wefw) > 0) && 
			     (StrLength(msgn) > 0) ) 
			{
				StrBufAppendBufPlain(refs, HKEY("|"), 0);
			}
			if (StrLength(msgn) > 0) {
				StrBufAppendBuf(refs, msgn, 0);
			}
			PutBstr(HKEY("references"), refs);
		}

		/*
		 * If this is a Reply or a ReplyAll, copy the sender's email into the To: field
		 */
		if ((ReplyMode == eReply) || (ReplyMode == eReplyAll))
		{
			StrBuf *to_rcpt;
			if ((StrLength(replyto) > 0) && (ReplyMode == eReplyAll)) {
				to_rcpt = NewStrBuf();
				StrBufAppendBuf(to_rcpt, replyto, 0);
			}
			else if (StrLength(rfca) > 0) {
				to_rcpt = NewStrBuf();
				StrBufAppendBuf(to_rcpt, from, 0);
				StrBufAppendBufPlain(to_rcpt, HKEY(" <"), 0);
				StrBufAppendBuf(to_rcpt, rfca, 0);
				StrBufAppendBufPlain(to_rcpt, HKEY(">"), 0);
			}
			else {
				to_rcpt =  from;
				from = NULL;
				if (	(StrLength(node) > 0)
					&& (strcasecmp(ChrPtr(node), ChrPtr(WCC->serv_info->serv_nodename)))
				) {
					StrBufAppendBufPlain(to_rcpt, HKEY(" @ "), 0);
					StrBufAppendBuf(to_rcpt, node, 0);
				}
			}
			PutBstr(HKEY("recp"), to_rcpt);
		}

		/*
		 * Only if this is a ReplyAll, copy all recipients into the Cc: field
		 */
		if (ReplyMode == eReplyAll)
		{
			StrBuf *cc_rcpt = rcpt;
			rcpt = NULL;
			if ((StrLength(cccc) > 0) && (StrLength(replyto) == 0))
			{
				if (cc_rcpt != NULL)  {
					StrBufAppendPrintf(cc_rcpt, ", ");
					StrBufAppendBuf(cc_rcpt, cccc, 0);
				} else {
					cc_rcpt = cccc;
					cccc = NULL;
				}
			}
			if (cc_rcpt != NULL)
				PutBstr(HKEY("cc"), cc_rcpt);
		}
		FreeStrBuf(&wefw);
		FreeStrBuf(&msgn);
		FreeStrBuf(&from);
		FreeStrBuf(&node);
		FreeStrBuf(&rfca);
		FreeStrBuf(&rcpt);
		FreeStrBuf(&cccc);
	}
	FreeStrBuf(&Line);
	/*
	 * Otherwise proceed normally.
	 * Do a custom room banner with no navbar...
	 */

	if (recipient_required) {
		const StrBuf *Recp = NULL; 
		const StrBuf *Cc = NULL;
		const StrBuf *Bcc = NULL;
		char *wikipage = NULL;
		StrBuf *CmdBuf = NULL;
		const char CMD[] = "ENT0 0|%s|%d|0||%s||%s|%s|%s";
		
		Recp = sbstr("recp");
		Cc = sbstr("cc");
		Bcc = sbstr("bcc");
		wikipage = strdup(bstr("page"));
		str_wiki_index(wikipage);
		
		CmdBuf = NewStrBufPlain(NULL, 
					sizeof (CMD) + 
					StrLength(Recp) + 
					StrLength(display_name) +
					StrLength(Cc) +
					StrLength(Bcc) + 
					strlen(wikipage));

		StrBufPrintf(CmdBuf, 
			     CMD,
			     ChrPtr(Recp), 
			     is_anonymous,
			     ChrPtr(display_name),
			     ChrPtr(Cc), 
			     ChrPtr(Bcc), 
			     wikipage
		);
		serv_puts(ChrPtr(CmdBuf));
		StrBuf_ServGetln(CmdBuf);
		free(wikipage);

		rc = GetServerStatusMsg(CmdBuf, &Result, 0, 0);

		if (	(Result == 570)		/* invalid or missing recipient(s) */
			|| (Result == 550)	/* higher access required to send Internet mail */
		) {
			/* These errors will have been displayed and are excusable */
		}
		else if (rc != 2) {	/* Any other error means that we cannot continue */
			AppendImportantMessage(ChrPtr(CmdBuf) + 4, StrLength(CmdBuf) - 4);
			FreeStrBuf(&CmdBuf);
			fixview();
			readloop(readnew, eUseDefault);
			return;
		}
		FreeStrBuf(&CmdBuf);
	}
	if (recipient_required)
		PutBstr(HKEY("__RCPTREQUIRED"), NewStrBufPlain(HKEY("1")));
	if (recipient_required || subject_required)
		PutBstr(HKEY("__SUBJREQUIRED"), NewStrBufPlain(HKEY("1")));

	begin_burst();
	output_headers(1, 0, 0, 0, 1, 0);
	if (WCC->CurRoom.defview == VIEW_WIKIMD) 
		DoTemplate(HKEY("edit_markdown_epic"), NULL, &NoCtx);
	else
		DoTemplate(HKEY("edit_message"), NULL, &NoCtx);
	end_burst();

	return;
}

/*
 * delete a message
 */
void delete_msg(void)
{
	long msgid;
	StrBuf *Line;
	
	msgid = lbstr("msgid");
	Line = NewStrBuf();
	if ((WC->CurRoom.RAFlags & UA_ISTRASH) != 0) {	/* Delete from Trash is a real delete */
		serv_printf("DELE %ld", msgid);	
	}
	else {			/* Otherwise move it to Trash */
		serv_printf("MOVE %ld|_TRASH_|0", msgid);
	}

	StrBuf_ServGetln(Line);
	GetServerStatusMsg(Line, NULL, 1, 0);

	fixview();

	readloop(readnew, eUseDefault);
}


/*
 * move a message to another room
 */
void move_msg(void)
{
	long msgid;

	msgid = lbstr("msgid");

	if (havebstr("move_button")) {
		StrBuf *Line;
		serv_printf("MOVE %ld|%s", msgid, bstr("target_room"));
		Line = NewStrBuf();
		StrBuf_ServGetln(Line);
		GetServerStatusMsg(Line, NULL, 1, 0);
		FreeStrBuf(&Line);
	} else {
		AppendImportantMessage(_("The message was not moved."), -1);
	}

	fixview();
	readloop(readnew, eUseDefault);
}



/*
 * Generic function to output an arbitrary MIME attachment from
 * message being composed
 *
 * partnum		The MIME part to be output
 * filename		Fake filename to give
 * force_download	Nonzero to force set the Content-Type: header to "application/octet-stream"
 */
void postpart(StrBuf *partnum, StrBuf *filename, int force_download)
{
	void *vPart;
	StrBuf *content_type;
	wc_mime_attachment *part;
	int i;

	i = StrToi(partnum);
	if (GetHash(WC->attachments, IKEY(i), &vPart) &&
	    (vPart != NULL)) {
		part = (wc_mime_attachment*) vPart;
		if (force_download) {
			content_type = NewStrBufPlain(HKEY("application/octet-stream"));
		}
		else {
			content_type = NewStrBufDup(part->ContentType);
		}
		StrBufAppendBuf(WC->WBuf, part->Data, 0);
		http_transmit_thing(ChrPtr(content_type), 0);
	} else {
		hprintf("HTTP/1.1 404 %s\n", ChrPtr(partnum));
		output_headers(0, 0, 0, 0, 0, 0);
		hprintf("Content-Type: text/plain\r\n");
		begin_burst();
		wc_printf(_("An error occurred while retrieving this part: %s/%s\n"), 
			ChrPtr(partnum), ChrPtr(filename));
		end_burst();
	}
	FreeStrBuf(&content_type);
}


/*
 * Generic function to output an arbitrary MIME part from an arbitrary
 * message number on the server.
 *
 * msgnum		Number of the item on the citadel server
 * partnum		The MIME part to be output
 * force_download	Nonzero to force set the Content-Type: header to "application/octet-stream"
 */
void mimepart(int force_download)
{
	int detect_mime = 0;
	long msgnum;
	long ErrorDetail;
	StrBuf *att;
	wcsession *WCC = WC;
	StrBuf *Buf;
	off_t bytes;
	StrBuf *ContentType = NewStrBufPlain(HKEY("application/octet-stream"));
	const char *CT;

	att = Buf = NewStrBuf();
	msgnum = StrBufExtract_long(WCC->Hdr->HR.ReqLine, 0, '/');
	StrBufExtract_token(att, WCC->Hdr->HR.ReqLine, 1, '/');

	serv_printf("OPNA %ld|%s", msgnum, ChrPtr(att));
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, &ErrorDetail) == 2) {
		StrBufCutLeft(Buf, 4);
		bytes = StrBufExtract_long(Buf, 0, '|');
		StrBufExtract_token(ContentType, Buf, 3, '|');
		CheckGZipCompressionAllowed (SKEY(ContentType));
		if (force_download)
		{
			FlushStrBuf(ContentType);
			detect_mime = 0;
		}
		else
		{
			if (!strcasecmp(ChrPtr(ContentType), "application/octet-stream"))
			{
				StrBufExtract_token(Buf, WCC->Hdr->HR.ReqLine, 2, '/');
				CT = GuessMimeByFilename(SKEY(Buf));
				StrBufPlain(ContentType, CT, -1);
			}
			if (!strcasecmp(ChrPtr(ContentType), "application/octet-stream"))
			{
				detect_mime = 1;
			}
		}
		serv_read_binary_to_http(ContentType, bytes, 0, detect_mime);

		serv_read_binary(WCC->WBuf, bytes, Buf);
		serv_puts("CLOS");
		StrBuf_ServGetln(Buf);
		CT = ChrPtr(ContentType);
	} else {
		StrBufCutLeft(Buf, 4);
		switch (ErrorDetail) {
		default:
		case ERROR + MESSAGE_NOT_FOUND:
			hprintf("HTTP/1.1 404 %s\n", ChrPtr(Buf));
			break;
		case ERROR + NOT_LOGGED_IN:
			hprintf("HTTP/1.1 401 %s\n", ChrPtr(Buf));
			break;

		case ERROR + HIGHER_ACCESS_REQUIRED:
			hprintf("HTTP/1.1 403 %s\n", ChrPtr(Buf));
			break;
		case ERROR + INTERNAL_ERROR:
		case ERROR + TOO_BIG:
			hprintf("HTTP/1.1 500 %s\n", ChrPtr(Buf));
			break;
		}

		hprintf("Pragma: no-cache\r\n"
			"Cache-Control: no-store\r\n"
			"Expires: -1\r\n"
		);

		hprintf("Content-Type: text/plain\r\n");
		begin_burst();
		wc_printf(_("An error occurred while retrieving this part: %s\n"), 
			ChrPtr(Buf));
		end_burst();
	}
	FreeStrBuf(&ContentType);
	FreeStrBuf(&Buf);
}


/*
 * Read any MIME part of a message, from the server, into memory.
 */
StrBuf *load_mimepart(long msgnum, char *partnum)
{
	off_t bytes;
	StrBuf *Buf;
	
	Buf = NewStrBuf();
	serv_printf("DLAT %ld|%s", msgnum, partnum);
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 6) {
		StrBufCutLeft(Buf, 4);
		bytes = StrBufExtract_long(Buf, 0, '|');
		FreeStrBuf(&Buf);
		Buf = NewStrBuf();
		StrBuf_ServGetBLOBBuffered(Buf, bytes);
		return(Buf);
	}
	else {
		FreeStrBuf(&Buf);
		return(NULL);
	}
}

/*
 * Read any MIME part of a message, from the server, into memory.
 */
void MimeLoadData(wc_mime_attachment *Mime)
{
	StrBuf *Buf;
	const char *Ptr;
	off_t bytes;
	/* TODO: is there a chance the content type is different from the one we know? */

	serv_printf("DLAT %ld|%s", Mime->msgnum, ChrPtr(Mime->PartNum));
	Buf = NewStrBuf();
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 6) {
		Ptr = &(ChrPtr(Buf)[4]);
		bytes = StrBufExtractNext_long(Buf, &Ptr, '|');
		StrBufSkip_NTokenS(Buf, &Ptr, '|', 3);  /* filename, cbtype, mimetype */
		if (Mime->Charset == NULL) Mime->Charset = NewStrBuf();
		StrBufExtract_NextToken(Mime->Charset, Buf, &Ptr, '|');
		
		if (Mime->Data == NULL)
			Mime->Data = NewStrBufPlain(NULL, bytes);
		StrBuf_ServGetBLOBBuffered(Mime->Data, bytes);
	}
	else {
		FlushStrBuf(Mime->Data);
		/* TODO XImportant message */
	}
	FreeStrBuf(&Buf);
}


void view_mimepart(void) {
	mimepart(0);
}

void download_mimepart(void) {
	mimepart(1);
}

void view_postpart(void) {
	StrBuf *filename = NewStrBuf();
	StrBuf *partnum = NewStrBuf();

	StrBufExtract_token(partnum, WC->Hdr->HR.ReqLine, 0, '/');
	StrBufExtract_token(filename, WC->Hdr->HR.ReqLine, 1, '/');

	postpart(partnum, filename, 0);

	FreeStrBuf(&filename);
	FreeStrBuf(&partnum);
}

void download_postpart(void) {
	StrBuf *filename = NewStrBuf();
	StrBuf *partnum = NewStrBuf();

	StrBufExtract_token(partnum, WC->Hdr->HR.ReqLine, 0, '/');
	StrBufExtract_token(filename, WC->Hdr->HR.ReqLine, 1, '/');

	postpart(partnum, filename, 1);

	FreeStrBuf(&filename);
	FreeStrBuf(&partnum);
}



void show_num_attachments(void) {
	wc_printf("%d", GetCount(WC->attachments));
}


void h_readnew(void) { readloop(readnew, eUseDefault);}
void h_readold(void) { readloop(readold, eUseDefault);}
void h_readfwd(void) { readloop(readfwd, eUseDefault);}
void h_headers(void) { readloop(headers, eUseDefault);}
void h_do_search(void) { readloop(do_search, eUseDefault);}
void h_readgt(void) { readloop(readgt, eUseDefault);}
void h_readlt(void) { readloop(readlt, eUseDefault);}



/* Output message list in JSON format */
void jsonMessageList(void) {
	StrBuf *View = NewStrBuf();
	const StrBuf *room = sbstr("room");
	long oper = (havebstr("query")) ? do_search : readnew;
	StrBufPrintf(View, "%d", VIEW_JSON_LIST);
	putbstr("view", View);; 
	gotoroom(room);
	readloop(oper, eUseDefault);
}

void RegisterReadLoopHandlerset(
	int RoomType,
	GetParamsGetServerCall_func GetParamsGetServerCall,
	PrintViewHeader_func PrintPageHeader,
	PrintViewHeader_func PrintViewHeader,
	load_msg_ptrs_detailheaders LH,
	LoadMsgFromServer_func LoadMsgFromServer,
	RenderView_or_Tail_func RenderView_or_Tail,
	View_Cleanup_func ViewCleanup
	)
{
	RoomRenderer *Handler;

	Handler = (RoomRenderer*) malloc(sizeof(RoomRenderer));

	Handler->RoomType = RoomType;
	Handler->GetParamsGetServerCall = GetParamsGetServerCall;
	Handler->PrintPageHeader = PrintPageHeader;
	Handler->PrintViewHeader = PrintViewHeader;
	Handler->LoadMsgFromServer = LoadMsgFromServer;
	Handler->RenderView_or_Tail = RenderView_or_Tail;
	Handler->ViewCleanup = ViewCleanup;
	Handler->LHParse = LH;

	Put(ReadLoopHandler, IKEY(RoomType), Handler, NULL);
}

void 
InitModule_MSG
(void)
{
	RegisterPreference("use_sig",
			   _("Attach signature to email messages?"), 
			   PRF_YESNO, 
			   NULL);
	RegisterPreference("signature", _("Use this signature:"), PRF_QP_STRING, NULL);
	RegisterPreference("default_header_charset", 
			   _("Default character set for email headers:"), 
			   PRF_STRING, 
			   NULL);
	RegisterPreference("defaultfrom", _("Preferred email address"), PRF_STRING, NULL);
	RegisterPreference("defaultname", 
			   _("Preferred display name for email messages"), 
			   PRF_STRING, 
			   NULL);
	RegisterPreference("defaulthandle", 
			   _("Preferred display name for bulletin board posts"), 
			   PRF_STRING, 
			   NULL);
	RegisterPreference("mailbox",_("Mailbox view mode"), PRF_STRING, NULL);

	WebcitAddUrlHandler(HKEY("readnew"), "", 0, h_readnew, ANONYMOUS|NEED_URL);
	WebcitAddUrlHandler(HKEY("readold"), "", 0, h_readold, ANONYMOUS|NEED_URL);
	WebcitAddUrlHandler(HKEY("readfwd"), "", 0, h_readfwd, ANONYMOUS|NEED_URL);
	WebcitAddUrlHandler(HKEY("headers"), "", 0, h_headers, NEED_URL);
	WebcitAddUrlHandler(HKEY("readgt"), "", 0, h_readgt, ANONYMOUS|NEED_URL);
	WebcitAddUrlHandler(HKEY("readlt"), "", 0, h_readlt, ANONYMOUS|NEED_URL);
	WebcitAddUrlHandler(HKEY("do_search"), "", 0, h_do_search, 0);
	WebcitAddUrlHandler(HKEY("display_enter"), "", 0, display_enter, 0);
	WebcitAddUrlHandler(HKEY("post"), "", 0, post_message, PROHIBIT_STARTPAGE);
	WebcitAddUrlHandler(HKEY("move_msg"), "", 0, move_msg, PROHIBIT_STARTPAGE);
	WebcitAddUrlHandler(HKEY("delete_msg"), "", 0, delete_msg, PROHIBIT_STARTPAGE);
	WebcitAddUrlHandler(HKEY("msg"), "", 0, embed_message, NEED_URL);
	WebcitAddUrlHandler(HKEY("message"), "", 0, handle_one_message, NEED_URL|XHTTP_COMMANDS|COOKIEUNNEEDED|FORCE_SESSIONCLOSE);
	WebcitAddUrlHandler(HKEY("printmsg"), "", 0, print_message, NEED_URL);
	WebcitAddUrlHandler(HKEY("msgheaders"), "", 0, display_headers, NEED_URL);

	WebcitAddUrlHandler(HKEY("mimepart"), "", 0, view_mimepart, NEED_URL);
	WebcitAddUrlHandler(HKEY("mimepart_download"), "", 0, download_mimepart, NEED_URL);
	WebcitAddUrlHandler(HKEY("postpart"), "", 0, view_postpart, NEED_URL|PROHIBIT_STARTPAGE);
	WebcitAddUrlHandler(HKEY("postpart_download"), "", 0, download_postpart, NEED_URL|PROHIBIT_STARTPAGE);
	WebcitAddUrlHandler(HKEY("upload_attachment"), "", 0, upload_attachment, AJAX);
	WebcitAddUrlHandler(HKEY("remove_attachment"), "", 0, remove_attachment, AJAX);
	WebcitAddUrlHandler(HKEY("show_num_attachments"), "", 0, show_num_attachments, AJAX);

	/* json */
	WebcitAddUrlHandler(HKEY("roommsgs"), "", 0, jsonMessageList,0);

	l_subj = FourHash("subj", 4);
	l_wefw = FourHash("wefw", 4);
	l_msgn = FourHash("msgn", 4);
	l_from = FourHash("from", 4);
	l_rcpt = FourHash("rcpt", 4);
	l_cccc = FourHash("cccc", 4);
	l_replyto = FourHash("rep2", 4);
	l_node = FourHash("node", 4);
	l_rfca = FourHash("rfca", 4);
	l_nvto = FourHash("nvto", 4);

	return ;
}

void
SessionDetachModule_MSG
(wcsession *sess)
{
	DeleteHash(&sess->summ);
}
