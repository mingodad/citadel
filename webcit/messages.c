/*
 * Functions which deal with the fetching and displaying of messages.
 *
 * Copyright (c) 1996-2011 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"

HashList *MsgHeaderHandler = NULL;
HashList *MsgEvaluators = NULL;
HashList *MimeRenderHandler = NULL;
HashList *ReadLoopHandler = NULL;
int dbg_analyze_msg = 0;

#define SUBJ_COL_WIDTH_PCT		50	/* Mailbox view column width */
#define SENDER_COL_WIDTH_PCT		30	/* Mailbox view column width */
#define DATE_PLUS_BUTTONS_WIDTH_PCT	20	/* Mailbox view column width */

void jsonMessageListHdr(void);

void display_enter(void);

typedef void (*MsgPartEvaluatorFunc)(message_summary *Sum, StrBuf *Buf);

typedef struct _MsgPartEvaluatorStruct {
	MsgPartEvaluatorFunc f;
} MsgPartEvaluatorStruct;

int load_message(message_summary *Msg, 
		 StrBuf *FoundCharset,
		 StrBuf **Error)
{
	wcsession *WCC = WC;
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
	
	/* Generate a reply-to address */
	if (StrLength(Msg->Rfca) > 0) {
		if (Msg->reply_to == NULL)
			Msg->reply_to = NewStrBuf();
		if (StrLength(Msg->from) > 0) {
			StrBufPrintf(Msg->reply_to, "%s <%s>", ChrPtr(Msg->from), ChrPtr(Msg->Rfca));
		}
		else {
			FlushStrBuf(Msg->reply_to);
			StrBufAppendBuf(Msg->reply_to, Msg->Rfca, 0);
		}
	}
	else 
	{
		if ((StrLength(Msg->OtherNode)>0) && 
		    (strcasecmp(ChrPtr(Msg->OtherNode), ChrPtr(WCC->serv_info->serv_nodename))) &&
		    (strcasecmp(ChrPtr(Msg->OtherNode), ChrPtr(WCC->serv_info->serv_humannode)) ))
		{
			if (Msg->reply_to == NULL)
				Msg->reply_to = NewStrBuf();
			StrBufPrintf(Msg->reply_to, 
				     "%s @ %s",
				     ChrPtr(Msg->from), 
				     ChrPtr(Msg->OtherNode));
		}
		else {
			if (Msg->reply_to == NULL)
				Msg->reply_to = NewStrBuf();
			FlushStrBuf(Msg->reply_to);
			StrBufAppendBuf(Msg->reply_to, Msg->from, 0);
		}
	}
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
int read_message(StrBuf *Target, const char *tmpl, long tmpllen, long msgnum, const StrBuf *PartNum, const StrBuf **OutMime) 
{
	StrBuf *Buf;
	StrBuf *FoundCharset;
	HashPos  *it;
	void *vMime;
	message_summary *Msg = NULL;
	void *vHdr;
	long len;
	const char *Key;
	WCTemplputParams SubTP;
	StrBuf *Error = NULL;

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

	/* Locate a renderer capable of converting this MIME part into HTML */
	if (GetHash(MimeRenderHandler, SKEY(Buf), &vHdr) &&
	    (vHdr != NULL)) {
		RenderMimeFuncStruct *Render;
		Render = (RenderMimeFuncStruct*)vHdr;
		Render->f(Msg->MsgBody, NULL, FoundCharset);
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
		wc_mime_attachment *Mime = (wc_mime_attachment*) vMime;
		evaluate_mime_part(Msg, Mime);
	}
	DeleteHashPos(&it);
	memset(&SubTP, 0, sizeof(WCTemplputParams));
	SubTP.Filter.ContextType = CTX_MAILSUM;
	SubTP.Context = Msg;
	*OutMime = DoTemplate(tmpl, tmpllen, Target, &SubTP);

	DestroyMessageSummary(Msg);
	FreeStrBuf(&FoundCharset);
	FreeStrBuf(&Buf);
	return 1;
}


void
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


}

/*
 * Unadorned HTML output of an individual message, suitable
 * for placing in a hidden iframe, for printing, or whatever
 */
void handle_one_message(void) 
{
	long CitStatus;
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
			read_message(WCC->WBuf, SKEY(Tmpl), msgnum, NULL, &Mime);
		else 
			read_message(WCC->WBuf, HKEY("view_message"), msgnum, NULL, &Mime);
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
		FlushStrBuf(WCC->ImportantMsg);
		StrBufAppendBuf(WCC->ImportantMsg, CmdBuf, 4);
		GetServerStatus(CmdBuf, &CitStatus);
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
			FlushStrBuf(WCC->ImportantMsg);
			StrBufAppendBuf(WCC->ImportantMsg, CmdBuf, 4);
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
	switch (WCC->Hdr->HR.eReqType)
	{
	case eGET:
	case ePOST:
		Tmpl = sbstr("template");
		if (StrLength(Tmpl) > 0) 
			read_message(WCC->WBuf, SKEY(Tmpl), msgnum, NULL, &Mime);
		else 
			read_message(WCC->WBuf, HKEY("view_message"), msgnum, NULL, &Mime);
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
		FlushStrBuf(WCC->ImportantMsg);
		StrBufAppendBuf(WCC->ImportantMsg, CmdBuf, 4);
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

	read_message(WC->WBuf, HKEY("view_message_print"), msgnum, NULL, &Mime);

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

	serv_printf("MSG2 %ld|3", msgnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			wc_printf("%s\n", buf);
		}
	}

	wDumpContent(0);
}


message_summary *ReadOneMessageSummary(StrBuf *RawMessage, const char *DefaultSubject, long MsgNum) 
{
	void                 *vEval;
	MsgPartEvaluatorStruct  *Eval;
	message_summary      *Msg;
	StrBuf *Buf;
	const char *buf;
	const char *ebuf;
	int nBuf;
	long len;
	
	Buf = NewStrBuf();

	serv_printf("MSG0 %ld|1", MsgNum);	/* ask for headers only */
	
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 1) {
		FreeStrBuf(&Buf);
		return NULL;
	}

	Msg = (message_summary*)malloc(sizeof(message_summary));
	memset(Msg, 0, sizeof(message_summary));
	while (len = StrBuf_ServGetln(Buf),
	       ((len != 3)  ||
		strcmp(ChrPtr(Buf), "000")== 0)){
		buf = ChrPtr(Buf);
		ebuf = strchr(ChrPtr(Buf), '=');
		nBuf = ebuf - buf;
		if (GetHash(MsgEvaluators, buf, nBuf, &vEval) && vEval != NULL) {
			Eval = (MsgPartEvaluatorStruct*) vEval;
			StrBufCutLeft(Buf, nBuf + 1);
			Eval->f(Msg, Buf);
		}
		else lprintf(1, "Don't know how to handle Message Headerline [%s]", ChrPtr(Buf));
	}
	return Msg;
}





/*
 * load message pointers from the server for a "read messages" operation
 *
 * servcmd:		the citadel command to send to the citserver
 */
int load_msg_ptrs(const char *servcmd, 
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

	Stat->lowest_found = LONG_MAX;
	Stat->highest_found = LONG_MIN;

	if (WCC->summ != NULL) {
		DeleteHash(&WCC->summ);
	}
	WCC->summ = NewHash(1, Flathash);
	
	Buf = NewStrBuf();
	serv_puts(servcmd);
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 1) {
		FreeStrBuf(&Buf);
		return (Stat->nummsgs);
	}
	Buf2 = NewStrBuf();
	while (len = StrBuf_ServGetln(Buf), ((len != 3) || strcmp(ChrPtr(Buf), "000")!= 0))
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
void SetFlagsFromMSet(HashList *ScanMe, MSet *MatchMSet, int FlagToSet, int Reverse)
{
	const char *HashKey;
	long HKLen;
	HashPos *at;
	void *vMsg;
	message_summary *Msg;

	at = GetNewHashPos(ScanMe, 0);
	while (GetNextHashPos(ScanMe, at, &HKLen, &HashKey, &vMsg)) {
		/* Are you a new message, or an old message? */
		Msg = (message_summary*) vMsg;
		if (Reverse && IsInMSetList(MatchMSet, Msg->msgnum)) {
			Msg->Flags = Msg->Flags | FlagToSet;
		}
		else if (!Reverse && !IsInMSetList(MatchMSet, Msg->msgnum)) {
			Msg->Flags = Msg->Flags | FlagToSet;
		}
	}
	DeleteHashPos(&at);
}


void load_seen_flags(void)
{
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
		return;
	}

	if (ParseMSet(&MatchMSet, OldMsg))
	{
		SetFlagsFromMSet(WCC->summ, MatchMSet, MSGFLAG_READ, 0);
	}
	DeleteMSet(&MatchMSet);
	FreeStrBuf(&OldMsg);
}

extern readloop_struct rlid[];

typedef struct _RoomRenderer{
	int RoomType;

	GetParamsGetServerCall_func GetParamsGetServerCall;
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
	int i, r;
	wcsession *WCC = WC;
	HashPos *at;
	const char *HashKey;
	long HKLen;
	WCTemplputParams SubTP;
	SharedMessageStatus Stat;
	void *ViewSpecific;

	if (havebstr("is_summary") && (1 == (ibstr("is_summary")))) {
		WCC->CurRoom.view = VIEW_MAILBOX;
	}

	if (havebstr("is_ajax") && (1 == (ibstr("is_ajax")))) {
		WCC->is_ajax = 1;
	}

	if ((oper == do_search) && (WCC->CurRoom.view == VIEW_WIKI)) {
		display_wiki_pagelist();
		return;
	}

	if (WCC->CurRoom.view == VIEW_WIKI) {
		http_redirect("wiki?page=home");
		return;
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
		return;			// TODO: print message
	}

	ViewMsg = (RoomRenderer*) vViewMsg;
	if (!WCC->is_ajax) {
		output_headers(1, 1, 1, 0, 0, 0);
	} else if (WCC->CurRoom.view == VIEW_MAILBOX) {
		jsonMessageListHdr();
	}

	if (ViewMsg->GetParamsGetServerCall != NULL) {
		r = ViewMsg->GetParamsGetServerCall(
		       &Stat,
		       &ViewSpecific,
		       oper,
		       cmd, sizeof(cmd)
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
	if (!IsEmptyStr(cmd))
		Stat.nummsgs = load_msg_ptrs(cmd, &Stat, ViewMsg->LHParse);

	if (Stat.sortit) {
		CompareFunc SortIt;
		memset(&SubTP, 0, sizeof(WCTemplputParams));
		SubTP.Filter.ContextType = CTX_MAILSUM;
		SubTP.Context = NULL;
		SortIt =  RetrieveSort(&SubTP, NULL, 0,
				       HKEY("date"), Stat.defaultsortorder);
		if (SortIt != NULL)
			SortByPayload(WCC->summ, SortIt);
	}
	if (Stat.startmsg < 0) {
		Stat.startmsg =  0;
	}

	if (Stat.load_seen) load_seen_flags();
	
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
		PutBstr(HKEY("__READLOOP:TOTALMSGS"), NewStrBufDup(Foo));
		StrBufPrintf(Foo, "%ld", Stat.startmsg);
		PutBstr(HKEY("__READLOOP:STARTMSG"), Foo);
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
	if (WC->CurRoom.view == VIEW_MAILBOX) {
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
		serv_printf("Content-type: multipart/alternative; "
			"boundary=\"%s\"\n", alt_boundary);
		serv_printf("This is a multipart message in MIME format.\n");
		serv_printf("--%s", alt_boundary);

		serv_puts("Content-type: text/plain; charset=utf-8");
		serv_puts("Content-Transfer-Encoding: quoted-printable");
		serv_puts("");
		txtmail = html_to_ascii(bstr("msgtext"), 0, 80, 0);
        	text_to_server_qp(txtmail);     /* Transmit message in quoted-printable encoding */
        	free(txtmail);

		serv_printf("--%s", alt_boundary);
	}

	serv_puts("Content-type: text/html; charset=utf-8");
	serv_puts("Content-Transfer-Encoding: quoted-printable");
	serv_puts("");
	serv_puts("<html><body>\r\n");
	text_to_server_qp(bstr("msgtext"));	/* Transmit message in quoted-printable encoding */
	serv_puts("</body></html>\r\n");

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
		sprintf(WCC->ImportantMessage, 
			_("Cancelled.  Message was not posted."));
	} else if (lbstr("postseq") == dont_post) {
		sprintf(WCC->ImportantMessage, 
			_("Automatically cancelled because you have already "
			  "saved this message."));
	} else {
		const char CMD[] = "ENT0 1|%s|%d|4|%s|%s||%s|%s|%s|%s|%s";
		StrBuf *Recp = NULL; 
		StrBuf *Cc = NULL;
		StrBuf *Bcc = NULL;
		const StrBuf *Wikipage = NULL;
		const StrBuf *my_email_addr = NULL;
		StrBuf *CmdBuf = NULL;
		StrBuf *references = NULL;
		int save_to_drafts;
		long HeaderLen;

		save_to_drafts = !strcasecmp(bstr("submit_action"), "drafts");
		Buf = NewStrBuf();

		if (save_to_drafts) {
		        /* temporarily change to the drafts room */
		        serv_puts("GOTO _DRAFTS_");
			StrBuf_ServGetln(Buf);
			if (GetServerStatus(Buf, NULL) != 2) {
				/* You probably don't even have a dumb Drafts folder */
				StrBufCutLeft(Buf, 4);
				lprintf(9, "%s:%d: server save to drafts error: %s\n", __FILE__, __LINE__, ChrPtr(Buf));
				StrBufAppendBufPlain(WCC->ImportantMsg, _("Saved to Drafts failed: "), -1, 0);
				StrBufAppendBuf(WCC->ImportantMsg, Buf, 0);
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

		Wikipage = sbstr("page");
		my_email_addr = sbstr("my_email_addr");
		
		HeaderLen = StrLength(Recp) + 
			StrLength(encoded_subject) +
			StrLength(Cc) +
			StrLength(Bcc) + 
			StrLength(Wikipage) +
			StrLength(my_email_addr) + 
			StrLength(references);
		CmdBuf = NewStrBufPlain(NULL, sizeof (CMD) + HeaderLen);
		StrBufPrintf(CmdBuf, 
			     CMD,
			     save_to_drafts?"":ChrPtr(Recp),
			     is_anonymous,
			     ChrPtr(encoded_subject),
			     ChrPtr(display_name),
			     save_to_drafts?"":ChrPtr(Cc),
			     save_to_drafts?"":ChrPtr(Bcc),
			     ChrPtr(Wikipage),
			     ChrPtr(my_email_addr),
			     ChrPtr(references));
		FreeStrBuf(&references);
		FreeStrBuf(&encoded_subject);

		if ((HeaderLen + StrLength(sbstr("msgtext")) < 10) && 
		    (GetCount(WCC->attachments) == 0)){
			StrBufAppendBufPlain(WCC->ImportantMsg, _("Refusing to post empty message.\n"), -1, 0);
			FreeStrBuf(&CmdBuf);
				
		}
		else 
		{
			lprintf(9, "%s\n", ChrPtr(CmdBuf));
			serv_puts(ChrPtr(CmdBuf));
			FreeStrBuf(&CmdBuf);

			StrBuf_ServGetln(Buf);
			if (GetServerStatus(Buf, NULL) == 4) {
				if (save_to_drafts) {
					if (  (havebstr("recp"))
					      || (havebstr("cc"  ))
					      || (havebstr("bcc" )) ) {
						/* save recipient headers or room to post to */
						serv_printf("To: %s", ChrPtr(Recp));
						serv_printf("Cc: %s", ChrPtr(Cc));
						serv_printf("Bcc: %s", ChrPtr(Bcc));
					} else {
						serv_printf("X-Citadel-Room: %s", ChrPtr(WC->CurRoom.name));
					}
				}
				post_mime_to_server();
				if (save_to_drafts) {
					StrBufAppendBufPlain(WCC->ImportantMsg, _("Message has been saved to Drafts.\n"), -1, 0);
					gotoroom(WCC->CurRoom.name);
					display_enter();
					FreeStrBuf(&Buf);
					return;
				} else if (  (havebstr("recp"))
					     || (havebstr("cc"  ))
					     || (havebstr("bcc" ))
					) {
					StrBufAppendBufPlain(WCC->ImportantMsg, _("Message has been sent.\n"), -1, 0);
				}
				else {
					StrBufAppendBufPlain(WCC->ImportantMsg, _("Message has been posted.\n"), -1, 0);
				}
				dont_post = lbstr("postseq");
			} else {
				StrBufCutLeft(Buf, 4);

				lprintf(9, "%s:%d: server post error: %s\n", __FILE__, __LINE__, ChrPtr(Buf));
				StrBufAppendBuf(WCC->ImportantMsg, Buf, 0);
				if (save_to_drafts) gotoroom(WCC->CurRoom.name);
				display_enter();
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

	lprintf(9, "upload_attachment()\n");
	wc_printf("upload_attachment()<br>\n");

	if (WCC->upload_length <= 0) {
		lprintf(9, "ERROR no attachment was uploaded\n");
		wc_printf("ERROR no attachment was uploaded<br>\n");
		return;
	}

	lprintf(9, "Client is uploading %d bytes\n", WCC->upload_length);
	wc_printf("Client is uploading %d bytes<br>\n", WCC->upload_length);
	att = malloc(sizeof(wc_mime_attachment));
	memset(att, 0, sizeof(wc_mime_attachment ));
	att->length = WCC->upload_length;
	att->ContentType = NewStrBufPlain(WCC->upload_content_type, -1);
	att->FileName = NewStrBufDup(WCC->upload_filename);
	
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
	const char *key;

	WhichAttachment = NewStrBufDup(sbstr("which_attachment"));
	StrBufUnescape(WhichAttachment, 0);
	at = GetNewHashPos(WCC->attachments, 0);
	do {
		GetHashPos(WCC->attachments, at, &len, &key, &vAtt);
	
		att = (wc_mime_attachment*) vAtt;
		if ((att != NULL) && 
		    (strcmp(ChrPtr(WhichAttachment), 
			    ChrPtr(att->FileName)   ) == 0))
		{
			DeleteEntryFromHash(WCC->attachments, at);
			break;
		}
	}
	while (NextHashPos(WCC->attachments, at));
	FreeStrBuf(&WhichAttachment);
	wc_printf("remove_attachment() completed\n");
}


/*
 * display the message entry screen
 */
void display_enter(void)
{
	char buf[SIZ];
	long now;
	const StrBuf *display_name = NULL;
	int recipient_required = 0;
	int subject_required = 0;
	int recipient_bad = 0;
	int is_anonymous = 0;
      	wcsession *WCC = WC;

	now = time(NULL);

	if (havebstr("force_room")) {
		gotoroom(sbstr("force_room"));
	}

	display_name = sbstr("display_name");
	if (!strcmp(ChrPtr(display_name), "__ANONYMOUS__")) {
		display_name = NULL;
		is_anonymous = 1;
	}

	/* First test to see whether this is a room that requires recipients to be entered */
	serv_puts("ENT0 0");
	serv_getln(buf, sizeof buf);

	if (!strncmp(buf, "570", 3)) {		/* 570 means that we need a recipient here */
		recipient_required = 1;
	}
	else if (buf[0] != '2') {		/* Any other error means that we cannot continue */
		sprintf(WCC->ImportantMessage, "%s", &buf[4]);
		readloop(readnew, eUseDefault);
		return;
	}

	/* Is the server strongly recommending that the user enter a message subject? */
	if ((buf[3] != '\0') && (buf[4] != '\0')) {
		subject_required = extract_int(&buf[4], 1);
	}

	/*
	 * Are we perhaps in an address book view?  If so, then an "enter
	 * message" command really means "add new entry."
	 */
	if (WCC->CurRoom.defview == VIEW_ADDRESSBOOK) {
		do_edit_vcard(-1, "", NULL, NULL, "",  ChrPtr(WCC->CurRoom.name));
		return;
	}

	/*
	 * Are we perhaps in a calendar room?  If so, then an "enter
	 * message" command really means "add new calendar item."
	 */
	if (WCC->CurRoom.defview == VIEW_CALENDAR) {
		display_edit_event();
		return;
	}

	/*
	 * Are we perhaps in a tasks view?  If so, then an "enter
	 * message" command really means "add new task."
	 */
	if (WCC->CurRoom.defview == VIEW_TASKS) {
		display_edit_task();
		return;
	}

	/*
	 * Otherwise proceed normally.
	 * Do a custom room banner with no navbar...
	 */

	if (recipient_required) {
		const StrBuf *Recp = NULL; 
		const StrBuf *Cc = NULL;
		const StrBuf *Bcc = NULL;
		const StrBuf *Wikipage = NULL;
		StrBuf *CmdBuf = NULL;
		const char CMD[] = "ENT0 0|%s|%d|0||%s||%s|%s|%s";
		
		Recp = sbstr("recp");
		Cc = sbstr("cc");
		Bcc = sbstr("bcc");
		Wikipage = sbstr("page");
		
		CmdBuf = NewStrBufPlain(NULL, 
					sizeof (CMD) + 
					StrLength(Recp) + 
					StrLength(display_name) +
					StrLength(Cc) +
					StrLength(Bcc) + 
					StrLength(Wikipage));

		StrBufPrintf(CmdBuf, 
			     CMD,
			     ChrPtr(Recp), 
			     is_anonymous,
			     ChrPtr(display_name),
			     ChrPtr(Cc), 
			     ChrPtr(Bcc), 
			     ChrPtr(Wikipage));
		serv_puts(ChrPtr(CmdBuf));
		serv_getln(buf, sizeof buf);
		FreeStrBuf(&CmdBuf);

		if (!strncmp(buf, "570", 3)) {	/* 570 means we have an invalid recipient listed */
			if (havebstr("recp") && 
			    havebstr("cc"  ) && 
			    havebstr("bcc" )) {
				recipient_bad = 1;
			}
		}
		else if (buf[0] != '2') {	/* Any other error means that we cannot continue */
			wc_printf("<em>%s</em><br>\n", &buf[4]);	/* TODO -> important message */
			return;
		}
	}
	if (recipient_required)
		PutBstr(HKEY("__RCPTREQUIRED"), NewStrBufPlain(HKEY("1")));
	if (recipient_required || subject_required)
		PutBstr(HKEY("__SUBJREQUIRED"), NewStrBufPlain(HKEY("1")));

	begin_burst();
	output_headers(1, 0, 0, 0, 1, 0);
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
	char buf[SIZ];

	msgid = lbstr("msgid");

	if ((WC->CurRoom.RAFlags & UA_ISTRASH) != 0) {	/* Delete from Trash is a real delete */
		serv_printf("DELE %ld", msgid);	
	}
	else {			/* Otherwise move it to Trash */
		serv_printf("MOVE %ld|_TRASH_|0", msgid);
	}

	serv_getln(buf, sizeof buf);
	sprintf(WC->ImportantMessage, "%s", &buf[4]);
	readloop(readnew, eUseDefault);
}


/*
 * move a message to another room
 */
void move_msg(void)
{
	long msgid;
	char buf[SIZ];

	msgid = lbstr("msgid");

	if (havebstr("move_button")) {
		sprintf(buf, "MOVE %ld|%s", msgid, bstr("target_room"));
		serv_puts(buf);
		serv_getln(buf, sizeof buf);
		sprintf(WC->ImportantMessage, "%s", &buf[4]);
	} else {
		sprintf(WC->ImportantMessage, (_("The message was not moved.")));
	}

	readloop(readnew, eUseDefault);
}


/*
 * Confirm move of a message
 */
void confirm_move_msg(void)
{
	long msgid;
	char buf[SIZ];
	char targ[SIZ];

	msgid = lbstr("msgid");


	output_headers(1, 1, 2, 0, 0, 0);
	wc_printf("<div id=\"banner\">\n");
	wc_printf("<h1>");
	wc_printf(_("Confirm move of message"));
	wc_printf("</h1>");
	wc_printf("</div>\n");

	wc_printf("<div id=\"content\" class=\"service\">\n");

	wc_printf("<CENTER>");

	wc_printf(_("Move this message to:"));
	wc_printf("<br>\n");

	wc_printf("<form METHOD=\"POST\" action=\"move_msg\">\n");
	wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
	wc_printf("<INPUT TYPE=\"hidden\" NAME=\"msgid\" VALUE=\"%s\">\n", bstr("msgid"));

	wc_printf("<SELECT NAME=\"target_room\" SIZE=5>\n");
	serv_puts("LKRA");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			extract_token(targ, buf, 0, '|', sizeof targ);
			wc_printf("<OPTION>");
			escputs(targ);
			wc_printf("\n");
		}
	}
	wc_printf("</SELECT>\n");
	wc_printf("<br>\n");

	wc_printf("<INPUT TYPE=\"submit\" NAME=\"move_button\" VALUE=\"%s\">", _("Move"));
	wc_printf("&nbsp;");
	wc_printf("<INPUT TYPE=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">", _("Cancel"));
	wc_printf("</form></CENTER>\n");

	wc_printf("</CENTER>\n");
	wDumpContent(1);
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
	long msgnum;
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
	if (GetServerStatus(Buf, NULL) == 2) {
		StrBufCutLeft(Buf, 4);
		bytes = StrBufExtract_long(Buf, 0, '|');
		if (!force_download) {
			StrBufExtract_token(ContentType, Buf, 3, '|');
		}

		serv_read_binary(WCC->WBuf, bytes, Buf);
		serv_puts("CLOS");
		StrBuf_ServGetln(Buf);
		CT = ChrPtr(ContentType);

		if (!force_download) {
			if (!strcasecmp(ChrPtr(ContentType), "application/octet-stream")) {
				StrBufExtract_token(Buf, WCC->Hdr->HR.ReqLine, 2, '/');
				CT = GuessMimeByFilename(SKEY(Buf));
			}
			if (!strcasecmp(ChrPtr(ContentType), "application/octet-stream")) {
				CT = GuessMimeType(SKEY(WCC->WBuf));
			}
		}
		http_transmit_thing(CT, 0);
	} else {
		StrBufCutLeft(Buf, 4);
		hprintf("HTTP/1.1 404 %s\n", ChrPtr(Buf));
		output_headers(0, 0, 0, 0, 0, 0);
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

void jsonMessageListHdr(void) 
{
	/* TODO: make a generic function */
	hprintf("HTTP/1.1 200 OK\r\n");
	hprintf("Content-type: application/json; charset=utf-8\r\n");
	hprintf("Server: %s / %s\r\n", PACKAGE_STRING, ChrPtr(WC->serv_info->serv_software));
	hprintf("Connection: close\r\n");
	hprintf("Pragma: no-cache\r\nCache-Control: no-store\r\nExpires:-1\r\n");
	begin_burst();
}


/* Output message list in JSON format */
void jsonMessageList(void) {
	const StrBuf *room = sbstr("room");
	long oper = (havebstr("query")) ? do_search : readnew;
	WC->is_ajax = 1; 
	gotoroom(room);
	readloop(oper, eUseDefault);
	WC->is_ajax = 0;
}

void RegisterReadLoopHandlerset(
	int RoomType,
	GetParamsGetServerCall_func GetParamsGetServerCall,
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
	WebcitAddUrlHandler(HKEY("confirm_move_msg"), "", 0, confirm_move_msg, PROHIBIT_STARTPAGE);
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
	return ;
}

void
SessionDetachModule_MSG
(wcsession *sess)
{
	DeleteHash(&sess->summ);
}
