/*
 * $Id$
 *
 * Functions which deal with the fetching and displaying of messages.
 *
 */

#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"

HashList *MsgHeaderHandler = NULL;
HashList *MsgEvaluators = NULL;
HashList *MimeRenderHandler = NULL;
int dbg_analyze_msg = 0;

#define SUBJ_COL_WIDTH_PCT		50	/**< Mailbox view column width */
#define SENDER_COL_WIDTH_PCT		30	/**< Mailbox view column width */
#define DATE_PLUS_BUTTONS_WIDTH_PCT	20	/**< Mailbox view column width */

void jsonMessageListHdr(void);

void display_enter(void);

/*----------------------------------------------------------------------------*/


typedef void (*MsgPartEvaluatorFunc)(message_summary *Sum, StrBuf *Buf);

typedef struct _MsgPartEvaluatorStruct {
	MsgPartEvaluatorFunc f;
}MsgPartEvaluatorStruct;


/*----------------------------------------------------------------------------*/



/*
 * I wanna SEE that message!
 *
 * msgnum		Message number to display
 * printable_view	Nonzero to display a printable view
 * section		Optional for encapsulated message/rfc822 submessage
 */
int read_message(StrBuf *Target, const char *tmpl, long tmpllen, long msgnum, int printable_view, const StrBuf *PartNum) 
{
	wcsession *WCC = WC;
	StrBuf *Buf;
	StrBuf *HdrToken;
	StrBuf *FoundCharset;
	HashPos  *it;
	void *vMime;
	message_summary *Msg = NULL;
	headereval *Hdr;
	void *vHdr;
	char buf[SIZ];
	int Done = 0;
	int state=0;
	long len;
	const char *Key;
	WCTemplputParams SubTP;

	Buf = NewStrBuf();
	lprintf(1, "----------%s---------MSG4 %ld|%s--------------\n", tmpl, msgnum, ChrPtr(PartNum));
	serv_printf("MSG4 %ld|%s", msgnum, ChrPtr(PartNum));
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 1) {
		StrBufAppendPrintf(Target, "<strong>");
		StrBufAppendPrintf(Target, _("ERROR:"));
		StrBufAppendPrintf(Target, "</strong> %s<br />\n", &buf[4]);
		FreeStrBuf(&Buf);
		return 0;
	}

	/** begin everythingamundo table */


	HdrToken = NewStrBuf();
	Msg = (message_summary *)malloc(sizeof(message_summary));
	memset(Msg, 0, sizeof(message_summary));
	Msg->msgnum = msgnum;
	Msg->PartNum = PartNum;
	Msg->MsgBody =  (wc_mime_attachment*) malloc(sizeof(wc_mime_attachment));
	memset(Msg->MsgBody, 0, sizeof(wc_mime_attachment));
	Msg->MsgBody->msgnum = msgnum;
	FoundCharset = NewStrBuf();
	while ((StrBuf_ServGetln(Buf)>=0) && !Done) {
		if ( (StrLength(Buf)==3) && 
		    !strcmp(ChrPtr(Buf), "000")) 
		{
			Done = 1;
			if (state < 2) {
				lprintf(1, _("unexpected end of message"));
				
				Msg->MsgBody->ContentType = NewStrBufPlain(HKEY("text/html"));
				StrBufAppendPrintf(Msg->MsgBody->Data, "<div><i>");
				StrBufAppendPrintf(Msg->MsgBody->Data, _("unexpected end of message"));
				StrBufAppendPrintf(Msg->MsgBody->Data, " (1)</i><br /><br />\n");
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
			
#ifdef TECH_PREVIEW
			if (dbg_analyze_msg) lprintf(1, ":: [%s] = [%s]\n", ChrPtr(HdrToken), ChrPtr(Buf));
#endif
			/* look up one of the examine_* functions to parse the content */
			if (GetHash(MsgHeaderHandler, SKEY(HdrToken), &vHdr) &&
			    (vHdr != NULL)) {
				Hdr = (headereval*)vHdr;
				Hdr->evaluator(Msg, Buf, FoundCharset);
				if (Hdr->Type == 1) {
					state++;
				}
			}
			else lprintf(1, "don't know how to handle message header[%s]\n", ChrPtr(HdrToken));
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
#ifdef TECH_PREVIEW
					if (dbg_analyze_msg) lprintf(1, ":: [%s] = [%s]\n", ChrPtr(HdrToken), ChrPtr(Buf));
#endif
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
				StrBuf_ServGetBLOB(Msg->MsgBody->Data, Msg->MsgBody->length);
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
	
	/* strip the bare contenttype, so we ommit charset etc. */
	StrBufExtract_token(Buf, Msg->MsgBody->ContentType, 0, ';');
	StrBufTrim(Buf);
	/* look up the renderer, that will convert this mimeitem into the htmlized form */
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
	DoTemplate(tmpl, tmpllen, Target, &SubTP);

	DestroyMessageSummary(Msg);
	FreeStrBuf(&FoundCharset);
	FreeStrBuf(&HdrToken);
	FreeStrBuf(&Buf);
	return 1;
}



/*
 * Unadorned HTML output of an individual message, suitable
 * for placing in a hidden iframe, for printing, or whatever
 *
 * msgnum_as_string == Message number, as a string instead of as a long int
 */
void embed_message(void) {
	long msgnum = 0L;
	wcsession *WCC = WC;
	const StrBuf *Tmpl = sbstr("template");

	msgnum = StrTol(WCC->UrlFragment2);
	if (StrLength(Tmpl) > 0) 
		read_message(WCC->WBuf, SKEY(Tmpl), msgnum, 0, NULL);
	else 
		read_message(WCC->WBuf, HKEY("view_message"), msgnum, 0, NULL);
}


/*
 * Printable view of a message
 *
 * msgnum_as_string == Message number, as a string instead of as a long int
 */
void print_message(void) {
	long msgnum = 0L;

	msgnum = StrTol(WC->UrlFragment2);
	output_headers(0, 0, 0, 0, 0, 0);

	hprintf("Content-type: text/html\r\n"
		"Server: " PACKAGE_STRING "\r\n"
		"Connection: close\r\n");

	begin_burst();

	read_message(WC->WBuf, HKEY("view_message_print"), msgnum, 1, NULL);

	wDumpContent(0);
}

/* 
 * Mobile browser view of message
 *
 * @param msg_num_as_string Message number as a string instead of as a long int 
 */
void mobile_message_view(void) {
  long msgnum = 0L;
  msgnum = StrTol(WC->UrlFragment2);
  output_headers(1, 0, 0, 0, 0, 1);
  begin_burst();
  do_template("msgcontrols", NULL);
  read_message(WC->WBuf, HKEY("view_message"), msgnum,1, NULL);
  wDumpContent(0);
}

/**
 * \brief Display a message's headers
 *
 * \param msgnum_as_string Message number, as a string instead of as a long int
 */
void display_headers(void) {
	long msgnum = 0L;
	char buf[1024];

	msgnum = StrTol(WC->UrlFragment2);
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
			wprintf("%s\n", buf);
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
 * with_headers:	also include some of the headers with the message numbers (more expensive)
 */
int load_msg_ptrs(char *servcmd, int with_headers)
{
	StrBuf* FoundCharset = NULL;
        wcsession *WCC = WC;
	message_summary *Msg;
	StrBuf *Buf, *Buf2;
	int nummsgs = 0;
	int maxload = 0;
	long len;
	int n;
	int skipit;

	if (WCC->summ != NULL) {
		DeleteHash(&WCC->summ);
	}
	WCC->summ = NewHash(1, Flathash);
	maxload = 10000;
	
	Buf = NewStrBuf();
	serv_puts(servcmd);
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 1) {
		FreeStrBuf(&Buf);
		return (nummsgs);
	}
	Buf2 = NewStrBuf();
	while (len = StrBuf_ServGetln(Buf),
	       ((len != 3)  ||
		strcmp(ChrPtr(Buf), "000")!= 0))
	{
		if (nummsgs < maxload) {
			skipit = 0;
			Msg = (message_summary*)malloc(sizeof(message_summary));
			memset(Msg, 0, sizeof(message_summary));

			Msg->msgnum = StrBufExtract_long(Buf, 0, '|');
			Msg->date = StrBufExtract_long(Buf, 1, '|');
			/* 
			 * as citserver probably gives us messages in forward date sorting
			 * nummsgs should be the same order as the message date.
			 */
			if (Msg->date == 0) {
				Msg->date = nummsgs;
				if (StrLength(Buf) < 32) 
					skipit = 1;
			}
			if (!skipit) {
				Msg->from = NewStrBufPlain(NULL, StrLength(Buf));
				StrBufExtract_token(Buf2, Buf, 2, '|');
				if (StrLength(Buf2) != 0) {
					/** Handle senders with RFC2047 encoding */
					StrBuf_RFC822_to_Utf8(Msg->from, Buf2, WCC->DefaultCharset, FoundCharset);
				}
			
				/** Nodename */
				StrBufExtract_token(Buf2, Buf, 3, '|');
				if ((StrLength(Buf2) !=0 ) &&
				    ( ((WCC->room_flags & QR_NETWORK)
				       || ((strcasecmp(ChrPtr(Buf2), ChrPtr(WCC->serv_info->serv_nodename))
					    && (strcasecmp(ChrPtr(Buf2), ChrPtr(WCC->serv_info->serv_fqdn))))))))
				{
					StrBufAppendBufPlain(Msg->from, HKEY(" @ "), 0);
					StrBufAppendBuf(Msg->from, Buf2, 0);
				}

				/** Not used:
				    StrBufExtract_token(Msg->inetaddr, Buf, 4, '|');
				*/

				Msg->subj = NewStrBufPlain(NULL, StrLength(Buf));
				StrBufExtract_token(Buf2,  Buf, 5, '|');
				if (StrLength(Buf2) == 0)
					StrBufAppendBufPlain(Msg->subj, _("(no subject)"), -1,0);
				else {
					StrBuf_RFC822_to_Utf8(Msg->subj, Buf2, WCC->DefaultCharset, FoundCharset);
					if ((StrLength(Msg->subj) > 75) && 
					    (StrBuf_Utf8StrLen(Msg->subj) > 75)) {
						StrBuf_Utf8StrCut(Msg->subj, 72);
						StrBufAppendBufPlain(Msg->subj, HKEY("..."), 0);
					}
				}


				if ((StrLength(Msg->from) > 25) && 
				    (StrBuf_Utf8StrLen(Msg->from) > 25)) {
					StrBuf_Utf8StrCut(Msg->from, 23);
					StrBufAppendBufPlain(Msg->from, HKEY("..."), 0);
				}
			}
			n = Msg->msgnum;
			Put(WCC->summ, (const char *)&n, sizeof(n), Msg, DestroyMessageSummary);
		}
		nummsgs++;
	}
	FreeStrBuf(&Buf2);
	FreeStrBuf(&Buf);
	return (nummsgs);
}


inline message_summary* GetMessagePtrAt(int n, HashList *Summ)
{
	const char *Key;
	long HKLen;
	void *vMsg;

	if (Summ == NULL)
		return NULL;
	GetHashAt(Summ, n, &HKLen, &Key, &vMsg);
	return (message_summary*) vMsg;
}


long DrawMessageDropdown(StrBuf *Selector, long maxmsgs, long startmsg, int nMessages)
{
	StrBuf *TmpBuf;
	wcsession *WCC = WC;
	void *vMsg;
	int lo, hi;
	long ret;
	long hklen;
	const char *key;
	int done = 0;
	int nItems;
	HashPos *At;
	long vector[16];
	WCTemplputParams SubTP;

	memset(&SubTP, 0, sizeof(WCTemplputParams));
	SubTP.Filter.ContextType = CTX_LONGVECTOR;
	SubTP.Context = &vector;
	TmpBuf = NewStrBuf();
	At = GetNewHashPos(WCC->summ, nMessages);
	nItems = GetCount(WCC->summ);
	ret = nMessages;
	vector[0] = 7;
	vector[2] = 1;
	vector[1] = startmsg;
	vector[3] = 0;

	while (!done) {
		vector[3] = abs(nMessages);
		lo = GetHashPosCounter(At);
		if (nMessages > 0) {
			if (lo + nMessages >= nItems) {
				hi = nItems - 1;
				vector[3] = nItems - lo;
				if (startmsg == lo) 
					ret = vector[3];
			}
			else {
				hi = lo + nMessages - 1;
			}
		} else {
			if (lo + nMessages < -1) {
				hi = 0;
			}
			else {
				if ((lo % abs(nMessages)) != 0) {
					int offset = (lo % abs(nMessages) *
						      (nMessages / abs(nMessages)));
					hi = lo + offset;
					vector[3] = abs(offset);
					if (startmsg == lo)
						 ret = offset;
				}
				else
					hi = lo + nMessages;
			}
		}
		done = !GetNextHashPos(WCC->summ, At, &hklen, &key, &vMsg);
		
		/**
		 * Bump these because although we're thinking in zero base, the user
		 * is a drooling idiot and is thinking in one base.
		 */
		vector[4] = lo + 1;
		vector[5] = hi + 1;
		vector[6] = lo;
		FlushStrBuf(TmpBuf);
		dbg_print_longvector(vector);
		DoTemplate(HKEY("select_messageindex"), TmpBuf, &SubTP);
		StrBufAppendBuf(Selector, TmpBuf, 0);
	}
	vector[6] = 0;
	FlushStrBuf(TmpBuf);
	if (maxmsgs == 9999999) {
		vector[1] = 1;
		ret = maxmsgs;
	}
	else
		vector[1] = 0;		
	vector[2] = 0;
	dbg_print_longvector(vector);
	DoTemplate(HKEY("select_messageindex_all"), TmpBuf, &SubTP);
	StrBufAppendBuf(Selector, TmpBuf, 0);
	FreeStrBuf(&TmpBuf);
	DeleteHashPos(&At);
	return ret;
}

void load_seen_flags(void)
{
	message_summary *Msg;
	const char *HashKey;
	long HKLen;
	HashPos *at;
	void *vMsg;
	StrBuf *OldMsg;
	wcsession *WCC = WC;

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
	at = GetNewHashPos(WCC->summ, 0);
	while (GetNextHashPos(WCC->summ, at, &HKLen, &HashKey, &vMsg)) {
		/** Are you a new message, or an old message? */
		Msg = (message_summary*) vMsg;
		if (is_msg_in_mset(ChrPtr(OldMsg), Msg->msgnum)) {
			Msg->is_new = 0;
		}
		else {
			Msg->is_new = 1;
		}
	}
	FreeStrBuf(&OldMsg);
	DeleteHashPos(&at);
}

extern readloop_struct rlid[];

/*
 * command loop for reading messages
 *
 * Set oper to "readnew" or "readold" or "readfwd" or "headers"
 */
void readloop(long oper)
{
	StrBuf *MessageDropdown = NULL;
	StrBuf *BBViewToolBar = NULL;
	void *vMsg;
	message_summary *Msg;
	char cmd[256] = "";
	char buf[SIZ];
	int a = 0;
	int with_headers = 0;
	int nummsgs;
	long startmsg = 0;
	int maxmsgs = 0;
	long *displayed_msgs = NULL;
	int num_displayed = 0;
	int is_singlecard = 0;
	struct calview calv;
	int i;
	int lowest_displayed = (-1);
	int highest_displayed = 0;
	addrbookent *addrbook = NULL;
	int num_ab = 0;
	int bbs_reverse = 0;
	wcsession *WCC = WC;
	HashPos *at;
	const char *HashKey;
	long HKLen;
	int care_for_empty_list = 0;
	int load_seen = 0;
	int sortit = 0;
	int defaultsortorder = 0;
	WCTemplputParams SubTP;

	if (havebstr("is_summary") && (1 == (ibstr("is_summary"))))
		WCC->wc_view = VIEW_MAILBOX;

	if (!WCC->is_ajax) {
	output_headers(1, 1, 1, 0, 0, 0);
	} else if (WCC->wc_view == VIEW_MAILBOX) {
	  jsonMessageListHdr();
	}

	switch (WCC->wc_view) {
	case VIEW_WIKI:
		sprintf(buf, "wiki?room=%s&page=home", ChrPtr(WCC->wc_roomname));
		http_redirect(buf);
		return;
	case VIEW_CALBRIEF:
	case VIEW_CALENDAR:
		load_seen = 1;
		strcpy(cmd, "MSGS ALL|||1");
		maxmsgs = 32767;
		parse_calendar_view_request(&calv);
		break;
	case VIEW_TASKS:
		strcpy(cmd, "MSGS ALL");
		maxmsgs = 32767;
		break;
	case VIEW_NOTES:
		strcpy(cmd, "MSGS ALL");
		maxmsgs = 32767;
		wprintf("<div id=\"new_notes_here\"></div>\n");
		break;
	case VIEW_ADDRESSBOOK:
		is_singlecard = ibstr("is_singlecard");
		if (is_singlecard != 1) {
			if (oper == do_search) {
				snprintf(cmd, sizeof(cmd), "MSGS SEARCH|%s", bstr("query"));
			}
			else {
				strcpy(cmd, "MSGS ALL");
			}
			maxmsgs = 9999999;
			break;
		}
		break;
	case VIEW_MAILBOX: 
	  if (!WCC->is_ajax) {
	    new_summary_view();
	    return;
	  } else {
		defaultsortorder = 2;
		sortit = 1;
		load_seen = 1;
		care_for_empty_list = 0;
		with_headers = 1;
		/* Generally using maxmsgs|startmsg is not required
		   in mailbox view, but we have a 'safemode' for clients
		   (*cough* Exploder) that simply can't handle too many */
		if (havebstr("maxmsgs")) maxmsgs = ibstr("maxmsgs");
		else maxmsgs = 9999999;
		if (havebstr("startmsg")) startmsg = lbstr("startmsg");
		snprintf(cmd, sizeof(cmd), "MSGS %s|%s||1",
			 (oper == do_search) ? "SEARCH" : "ALL",
			 (oper == do_search) ? bstr("query") : ""
			);
	  }
		break;
	case VIEW_BBS:
	default:
		defaultsortorder = 1;
		startmsg = -1;
		sortit = 1;
		care_for_empty_list = 1;

		rlid[oper].cmd(cmd, sizeof(cmd));
		SetAccessCommand(oper);

		if (havebstr("maxmsgs"))
			maxmsgs = ibstr("maxmsgs");
		if (maxmsgs == 0) maxmsgs = DEFAULT_MAXMSGS;

		if (havebstr("startmsg")) {
			startmsg = lbstr("startmsg");
		}
		
	}

	nummsgs = load_msg_ptrs(cmd, with_headers);
	if (nummsgs == 0) {
		if (care_for_empty_list) {
			wprintf("<div class=\"nomsgs\"><br><em>");
			switch (oper) {
			case readnew:
				wprintf(_("No new messages."));
				break;
			case readold:
				wprintf(_("No old messages."));
				break;
			default:
				wprintf(_("No messages here."));
			}
			wprintf("</em><br></div>\n");
			goto DONE;
		}

	}

	if (sortit) {
		CompareFunc SortIt;
		memset(&SubTP, 0, sizeof(WCTemplputParams));
		SubTP.Filter.ContextType = CTX_NONE;
		SubTP.Context = NULL;
		SortIt =  RetrieveSort(&SubTP, NULL, 0,
				       HKEY("date"), defaultsortorder);
		if (SortIt != NULL)
			SortByPayload(WCC->summ, SortIt);
		if (WCC->wc_view == VIEW_BBS) {
			if (lbstr("SortOrder") == 2) {
				bbs_reverse = 1;
				num_displayed = -DEFAULT_MAXMSGS;
			}
			else {
				bbs_reverse = 0;
				num_displayed = DEFAULT_MAXMSGS;
			}
		}
	}
	if (startmsg < 0) startmsg = (bbs_reverse) ? nummsgs - 1 : 0;

	if (load_seen) load_seen_flags();
	
        /**
	 * If we're to print s.th. above the message list...
	 */
	switch (WCC->wc_view) {
	case VIEW_BBS:
		BBViewToolBar = NewStrBuf();
		MessageDropdown = NewStrBuf();

		maxmsgs = DrawMessageDropdown(MessageDropdown, maxmsgs, startmsg, num_displayed);
		if (num_displayed < 0) {
			startmsg += maxmsgs;
			if (num_displayed != maxmsgs)				
				maxmsgs = abs(maxmsgs) + 1;
			else
				maxmsgs = abs(maxmsgs);

		}
		memset(&SubTP, 0, sizeof(WCTemplputParams));
		SubTP.Filter.ContextType = CTX_STRBUF;
		SubTP.Context = MessageDropdown;
		DoTemplate(HKEY("msg_listselector_top"), BBViewToolBar, &SubTP);
		StrBufAppendBuf(WCC->WBuf, BBViewToolBar, 0);
		FlushStrBuf(BBViewToolBar);
		break;
	}
	WCC->startmsg =  startmsg;
	WCC->maxmsgs = maxmsgs;
	WCC->num_displayed = 0;

	/* Put some helpful data in vars for mailsummary_json */
	svputlong("READLOOP:TOTALMSGS", nummsgs);
	svputlong("READLOOP:STARTMSG", startmsg);
	svputlong("WCVIEW", WCC->wc_view);
	/*
	 * iterate over each message. if we need to load an attachment, do it here. 
	 */
	if (WCC->wc_view == VIEW_MAILBOX) goto NO_MSG_LOOP;
	/*
	 * iterate over each message. if we need to load an attachment, do it here. 
	 */
	at = GetNewHashPos(WCC->summ, 0);
	num_displayed = i = 0;
	while (GetNextHashPos(WCC->summ, at, &HKLen, &HashKey, &vMsg)) {
		Msg = (message_summary*) vMsg;		
		if ((Msg->msgnum >= startmsg) && (num_displayed <= maxmsgs)) {			
			switch (WCC->wc_view) {
			case VIEW_WIKI:
				break;
			case VIEW_CALBRIEF: /* load the mime attachments for special tasks... */
			case VIEW_CALENDAR:
				load_calendar_item(Msg, Msg->is_new, &calv);
				break;
			case VIEW_TASKS:
				display_task(Msg, Msg->is_new);
				break;
			case VIEW_NOTES:
				display_note(Msg, Msg->is_new);
				break;
			case VIEW_ADDRESSBOOK:
				fetch_ab_name(Msg, buf);
				++num_ab;
				addrbook = realloc(addrbook,
						   (sizeof(addrbookent) * num_ab) );
				safestrncpy(addrbook[num_ab-1].ab_name, buf,
					    sizeof(addrbook[num_ab-1].ab_name));
				addrbook[num_ab-1].ab_msgnum = Msg->msgnum;
				break;
			case VIEW_BBS: /* Tag the mails we want to show in bbview... */
			default:
				if (displayed_msgs == NULL) {
					displayed_msgs = malloc(sizeof(long) *
								(maxmsgs<nummsgs ? maxmsgs + 1 : nummsgs + 1));
				}
				if ((i >= startmsg) && (i < startmsg + maxmsgs)) {
					displayed_msgs[num_displayed] = Msg->msgnum;
					if (lowest_displayed < 0) lowest_displayed = a;
					highest_displayed = a;
			
					num_displayed++;
				}
			}
		} 
		i++;
	}
	DeleteHashPos(&at);

 NO_MSG_LOOP:	
	/*
	 * Done iterating the message list. now tasks we want to do after.
	 */
	switch (WCC->wc_view) {
	case VIEW_MAILBOX:
	  DoTemplate(HKEY("mailsummary_json"),NULL, &SubTP);
	  break;
	case VIEW_BBS:
		if (displayed_msgs != NULL) {
			/** if we do a split bbview in the future, begin messages div here */
			
			for (a=0; a<num_displayed; ++a) {
				read_message(WCC->WBuf, HKEY("view_message"), displayed_msgs[a], 0, NULL);
			}
			
			/** if we do a split bbview in the future, end messages div here */
			
			free(displayed_msgs);
			displayed_msgs = NULL;
		}
		memset(&SubTP, 0, sizeof(WCTemplputParams));
		SubTP.Filter.ContextType = CTX_STRBUF;
		SubTP.Context = MessageDropdown;
		DoTemplate(HKEY("msg_listselector_bottom"), BBViewToolBar, &SubTP);
		StrBufAppendBuf(WCC->WBuf, BBViewToolBar, 0);

		FreeStrBuf(&BBViewToolBar);
		FreeStrBuf(&MessageDropdown);
	}

	
DONE:
	switch (WCC->wc_view) {
	case VIEW_WIKI:
		break;
	case VIEW_CALBRIEF:
	case VIEW_CALENDAR:
		render_calendar_view(&calv);
		break;
	case VIEW_TASKS:
		do_tasks_view();	/** Render the task list */
		break;
	case VIEW_NOTES:
		break;
	case VIEW_ADDRESSBOOK:
		if (is_singlecard)
			read_message(WC->WBuf, HKEY("view_message"), lbstr("startmsg"), 0, NULL);
		else
			do_addrbook_view(addrbook, num_ab);	/** Render the address book */
		break;
	case VIEW_MAILBOX: 
	case VIEW_BBS:
	default:
		break;
	}
	/** Note: wDumpContent() will output one additional </div> tag. */
	if (WCC->wc_view != VIEW_MAILBOX) {
		/* We ought to move this out into template */
		wDumpContent(1);
	} else {
		end_burst();
	}
	WCC->startmsg = 0;
	WCC->maxmsgs = 0;
	if (WCC->summ != NULL) {
		DeleteHash(&WCC->summ);
	}
	if (addrbook != NULL) free(addrbook);
	FreeStrBuf(&BBViewToolBar);
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

	if (is_multipart) {
		/* Remember, serv_printf() appends an extra newline */
		serv_printf("Content-type: multipart/mixed; boundary=\"%s\"\n", top_boundary);
		serv_printf("This is a multipart message in MIME format.\n");
		serv_printf("--%s", top_boundary);
	}

	/* Remember, serv_printf() appends an extra newline */
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

	serv_puts("Content-type: text/html; charset=utf-8");
	serv_puts("Content-Transfer-Encoding: quoted-printable");
	serv_puts("");
	serv_puts("<html><body>\r\n");
	text_to_server_qp(bstr("msgtext"));	/* Transmit message in quoted-printable encoding */
	serv_puts("</body></html>\r\n");

	serv_printf("--%s--", alt_boundary);
	
	if (is_multipart) {
		long len;
		const char *Key; 
		void *vAtt;
		HashPos  *it;

		/* Add in the attachments */
		it = GetNewHashPos(WCC->attachments, 0);
		while (GetNextHashPos(WCC->attachments, it, &len, &Key, &vAtt)) {
			att = (wc_mime_attachment *)vAtt;
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
	char buf[1024];
	StrBuf *encoded_subject = NULL;
	static long dont_post = (-1L);
	wc_mime_attachment  *att;
	int is_anonymous = 0;
	const StrBuf *display_name = NULL;
	wcsession *WCC = WC;
	
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

	if (WCC->upload_length > 0) {
		const char *pch;
		int n;
		char N[64];

		lprintf(9, "%s:%d: we are uploading %d bytes\n", __FILE__, __LINE__, WCC->upload_length);
		/** There's an attachment.  Save it to this struct... */
		att = malloc(sizeof(wc_mime_attachment));
		memset(att, 0, sizeof(wc_mime_attachment ));
		att->length = WCC->upload_length;
		att->ContentType = NewStrBufPlain(WCC->upload_content_type, -1);
		att->FileName = NewStrBufPlain(WCC->upload_filename, -1);
		
		
		if (WCC->attachments == NULL)
			WCC->attachments = NewHash(1, NULL);
		/* And add it to the list. */
		n = snprintf(N, sizeof N, "%d", GetCount(WCC->attachments) + 1);
		Put(WCC->attachments, N, n, att, DestroyMime);

		/**
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

		/**
		 * Transfer control of this memory from the upload struct
		 * to the attachment struct.
		 */
		att->Data = NewStrBufPlain(WCC->upload, WCC->upload_length);
		free(WCC->upload);
		WCC->upload_length = 0;
		WCC->upload = NULL;
		display_enter();
		return;
	}

	if (havebstr("cancel_button")) {
		sprintf(WCC->ImportantMessage, 
			_("Cancelled.  Message was not posted."));
	} else if (havebstr("attach_button")) {
		display_enter();
		return;
	} else if (lbstr("postseq") == dont_post) {
		sprintf(WCC->ImportantMessage, 
			_("Automatically cancelled because you have already "
			"saved this message."));
	} else {
		const char CMD[] = "ENT0 1|%s|%d|4|%s|%s||%s|%s|%s|%s|%s";
		const StrBuf *Recp = NULL; 
		const StrBuf *Cc = NULL;
		const StrBuf *Bcc = NULL;
		const StrBuf *Wikipage = NULL;
		const StrBuf *my_email_addr = NULL;
		StrBuf *CmdBuf = NULL;
		StrBuf *references = NULL;

		if (havebstr("references"))
		{
			const StrBuf *ref = sbstr("references");
			references = NewStrBufPlain(ChrPtr(ref), StrLength(ref));
			lprintf(9, "Converting: %s\n", ChrPtr(references));
			StrBufReplaceChars(references, '|', '!');
			lprintf(9, "Converted: %s\n", ChrPtr(references));
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
		Recp = sbstr("recp");
		Cc = sbstr("cc");
		Bcc = sbstr("bcc");
		Wikipage = sbstr("wikipage");
		my_email_addr = sbstr("my_email_addr");
		
		CmdBuf = NewStrBufPlain(NULL, 
					sizeof (CMD) + 
					StrLength(Recp) + 
					StrLength(encoded_subject) +
					StrLength(Cc) +
					StrLength(Bcc) + 
					StrLength(Wikipage) +
					StrLength(my_email_addr) + 
					StrLength(references));

		StrBufPrintf(CmdBuf, 
			     CMD,
			     ChrPtr(Recp),
			     is_anonymous,
			     ChrPtr(encoded_subject),
			     ChrPtr(display_name),
			     ChrPtr(Cc),
			     ChrPtr(Bcc),
			     ChrPtr(Wikipage),
			     ChrPtr(my_email_addr),
			     ChrPtr(references));
		FreeStrBuf(&references);

		lprintf(9, "%s\n", ChrPtr(CmdBuf));
		serv_puts(ChrPtr(CmdBuf));
		serv_getln(buf, sizeof buf);
		FreeStrBuf(&CmdBuf);
		FreeStrBuf(&encoded_subject);
		if (buf[0] == '4') {
			post_mime_to_server();
			if (  (havebstr("recp"))
			   || (havebstr("cc"  ))
			   || (havebstr("bcc" ))
			) {
				sprintf(WCC->ImportantMessage, _("Message has been sent.\n"));
			}
			else {
				sprintf(WC->ImportantMessage, _("Message has been posted.\n"));
			}
			dont_post = lbstr("postseq");
		} else {
			lprintf(9, "%s:%d: server post error: %s\n", __FILE__, __LINE__, buf);
			sprintf(WC->ImportantMessage, "%s", &buf[4]);
			display_enter();
			return;
		}
	}

	DeleteHash(&WCC->attachments);

	/**
	 *  We may have been supplied with instructions regarding the location
	 *  to which we must return after posting.  If found, go there.
	 */
	if (havebstr("return_to")) {
		http_redirect(bstr("return_to"));
	}
	/**
	 *  If we were editing a page in a wiki room, go to that page now.
	 */
	else if (havebstr("wikipage")) {
		snprintf(buf, sizeof buf, "wiki?page=%s", bstr("wikipage"));
		http_redirect(buf);
	}
	/**
	 *  Otherwise, just go to the "read messages" loop.
	 */
	else {
		readloop(readnew);
	}
}




/**
 * \brief display the message entry screen
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

	/** First test to see whether this is a room that requires recipients to be entered */
	serv_puts("ENT0 0");
	serv_getln(buf, sizeof buf);

	if (!strncmp(buf, "570", 3)) {		/** 570 means that we need a recipient here */
		recipient_required = 1;
	}
	else if (buf[0] != '2') {		/** Any other error means that we cannot continue */
		sprintf(WCC->ImportantMessage, "%s", &buf[4]);
		readloop(readnew);
		return;
	}

	/* Is the server strongly recommending that the user enter a message subject? */
	if ((buf[3] != '\0') && (buf[4] != '\0')) {
		subject_required = extract_int(&buf[4], 1);
	}

	/**
	 * Are we perhaps in an address book view?  If so, then an "enter
	 * message" command really means "add new entry."
	 */
	if (WCC->wc_default_view == VIEW_ADDRESSBOOK) {
		do_edit_vcard(-1, "", "", ChrPtr(WCC->wc_roomname));
		return;
	}

	/*
	 * Are we perhaps in a calendar room?  If so, then an "enter
	 * message" command really means "add new calendar item."
	 */
	if (WCC->wc_default_view == VIEW_CALENDAR) {
		display_edit_event();
		return;
	}

	/*
	 * Are we perhaps in a tasks view?  If so, then an "enter
	 * message" command really means "add new task."
	 */
	if (WCC->wc_default_view == VIEW_TASKS) {
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
		Wikipage = sbstr("wikipage");
		
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

		if (!strncmp(buf, "570", 3)) {	/** 570 means we have an invalid recipient listed */
			if (havebstr("recp") && 
			    havebstr("cc"  ) && 
			    havebstr("bcc" )) {
				recipient_bad = 1;
			}
		}
		else if (buf[0] != '2') {	/** Any other error means that we cannot continue */
			wprintf("<em>%s</em><br />\n", &buf[4]);/*TODO -> important message */
			return;
		}
	}
	svputlong("RCPTREQUIRED", recipient_required);
	svputlong("SUBJREQUIRED", recipient_required || subject_required);

	begin_burst();
	output_headers(1, 0, 0, 0, 1, 0);
	DoTemplate(HKEY("edit_message"), NULL, &NoCtx);
	end_burst();

	return;
}

/**
 * \brief delete a message
 */
void delete_msg(void)
{
	long msgid;
	char buf[SIZ];

	msgid = lbstr("msgid");

	if (WC->wc_is_trash) {	/** Delete from Trash is a real delete */
		serv_printf("DELE %ld", msgid);	
	}
	else {			/** Otherwise move it to Trash */
		serv_printf("MOVE %ld|_TRASH_|0", msgid);
	}

	serv_getln(buf, sizeof buf);
	sprintf(WC->ImportantMessage, "%s", &buf[4]);

	readloop(readnew);
}


/**
 * \brief move a message to another folder
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

	readloop(readnew);
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
	wprintf("<div id=\"banner\">\n");
	wprintf("<h1>");
	wprintf(_("Confirm move of message"));
	wprintf("</h1>");
	wprintf("</div>\n");

	wprintf("<div id=\"content\" class=\"service\">\n");

	wprintf("<CENTER>");

	wprintf(_("Move this message to:"));
	wprintf("<br />\n");

	wprintf("<form METHOD=\"POST\" action=\"move_msg\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"msgid\" VALUE=\"%s\">\n", bstr("msgid"));

	wprintf("<SELECT NAME=\"target_room\" SIZE=5>\n");
	serv_puts("LKRA");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			extract_token(targ, buf, 0, '|', sizeof targ);
			wprintf("<OPTION>");
			escputs(targ);
			wprintf("\n");
		}
	}
	wprintf("</SELECT>\n");
	wprintf("<br />\n");

	wprintf("<INPUT TYPE=\"submit\" NAME=\"move_button\" VALUE=\"%s\">", _("Move"));
	wprintf("&nbsp;");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">", _("Cancel"));
	wprintf("</form></CENTER>\n");

	wprintf("</CENTER>\n");
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
	
	if (GetHash(WC->attachments, SKEY(partnum), &vPart) &&
	    (vPart != NULL)) {
		part = (wc_mime_attachment*) vPart;
		if (force_download) {
			content_type = NewStrBufPlain(HKEY("application/octet-stream"));
		}
		else {
			content_type = NewStrBufDup(part->ContentType);
		}
		output_headers(0, 0, 0, 0, 0, 0);
		StrBufAppendBuf(WC->WBuf, part->Data, 0);
		http_transmit_thing(ChrPtr(content_type), 0);
	} else {
		hprintf("HTTP/1.1 404 %s\n", ChrPtr(partnum));
		output_headers(0, 0, 0, 0, 0, 0);
		hprintf("Content-Type: text/plain\r\n");
		wprintf(_("An error occurred while retrieving this part: %s/%s\n"), 
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
void mimepart(const char *msgnum, const char *partnum, int force_download)
{
	char buf[256];
	off_t bytes;
	char content_type[256];
	
	serv_printf("OPNA %s|%s", msgnum, partnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		bytes = extract_long(&buf[4], 0);
		if (force_download) {
			strcpy(content_type, "application/octet-stream");
		}
		else {
			extract_token(content_type, &buf[4], 3, '|', sizeof content_type);
		}
		output_headers(0, 0, 0, 0, 0, 0);

		read_server_binary(WC->WBuf, bytes);
		serv_puts("CLOS");
		serv_getln(buf, sizeof buf);
		http_transmit_thing(content_type, 0);
	} else {
		hprintf("HTTP/1.1 404 %s\n", &buf[4]);
		output_headers(0, 0, 0, 0, 0, 0);
		hprintf("Content-Type: text/plain\r\n");
		wprintf(_("An error occurred while retrieving this part: %s\n"), &buf[4]);
		end_burst();
	}
}


/*
 * Read any MIME part of a message, from the server, into memory.
 */
char *load_mimepart(long msgnum, char *partnum)
{
	char buf[SIZ];
	off_t bytes;
	char content_type[SIZ];
	char *content;
	
	serv_printf("DLAT %ld|%s", msgnum, partnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '6') {
		bytes = extract_long(&buf[4], 0);
		extract_token(content_type, &buf[4], 3, '|', sizeof content_type);

		content = malloc(bytes + 2);
		serv_read(content, bytes);

		content[bytes] = 0;	/* null terminate for good measure */
		return(content);
	}
	else {
		return(NULL);
	}
}

/*
 * Read any MIME part of a message, from the server, into memory.
 */
void MimeLoadData(wc_mime_attachment *Mime)
{
	char buf[SIZ];
	off_t bytes;
/* TODO: is there a chance the contenttype is different  to the one we know?	 */
	serv_printf("DLAT %ld|%s", Mime->msgnum, ChrPtr(Mime->PartNum));
	serv_getln(buf, sizeof buf);
	if (buf[0] == '6') {
		bytes = extract_long(&buf[4], 0);

		if (Mime->Data == NULL)
			Mime->Data = NewStrBufPlain(NULL, bytes);
		StrBuf_ServGetBLOB(Mime->Data, bytes);

	}
	else {
		FlushStrBuf(Mime->Data);
		/* TODO XImportant message */
	}
}




void view_mimepart(void) {
	mimepart(ChrPtr(WC->UrlFragment2),
		 ChrPtr(WC->UrlFragment3),
		 0);
}

void download_mimepart(void) {
	mimepart(ChrPtr(WC->UrlFragment2),
		 ChrPtr(WC->UrlFragment3),
		 1);
}

void view_postpart(void) {
	postpart(WC->UrlFragment2,
		 WC->UrlFragment3,
		 0);
}

void download_postpart(void) {
	postpart(WC->UrlFragment2,
		 WC->UrlFragment3,
		 1);
}

void h_readnew(void) { readloop(readnew);}
void h_readold(void) { readloop(readold);}
void h_readfwd(void) { readloop(readfwd);}
void h_headers(void) { readloop(headers);}
void h_do_search(void) { readloop(do_search);}

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
/* Spit out the new summary view. This is basically a static page, so clients can cache the layout, all the dirty work is javascript :) */
void new_summary_view(void) {
  begin_burst();
  DoTemplate(HKEY("msg_listview"),NULL,&NoCtx);
  DoTemplate(HKEY("trailing"),NULL,&NoCtx);
  end_burst();
}
/** Output message list in JSON-format */
void jsonMessageList(void) {
  const StrBuf *room = sbstr("room");
  long oper = (havebstr("query")) ? do_search : readnew;
  WC->is_ajax = 1; 
  gotoroom(room);
  readloop(oper);
  WC->is_ajax = 0;
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

	WebcitAddUrlHandler(HKEY("readnew"), h_readnew, NEED_URL);
	WebcitAddUrlHandler(HKEY("readold"), h_readold, NEED_URL);
	WebcitAddUrlHandler(HKEY("readfwd"), h_readfwd, NEED_URL);
	WebcitAddUrlHandler(HKEY("headers"), h_headers, NEED_URL);
	WebcitAddUrlHandler(HKEY("do_search"), h_do_search, 0);
	WebcitAddUrlHandler(HKEY("display_enter"), display_enter, 0);
	WebcitAddUrlHandler(HKEY("post"), post_message, 0);
	WebcitAddUrlHandler(HKEY("move_msg"), move_msg, 0);
	WebcitAddUrlHandler(HKEY("delete_msg"), delete_msg, 0);
	WebcitAddUrlHandler(HKEY("confirm_move_msg"), confirm_move_msg, 0);
	WebcitAddUrlHandler(HKEY("msg"), embed_message, NEED_URL|AJAX);
	WebcitAddUrlHandler(HKEY("printmsg"), print_message, NEED_URL);
	WebcitAddUrlHandler(HKEY("mobilemsg"), mobile_message_view, NEED_URL);
	WebcitAddUrlHandler(HKEY("msgheaders"), display_headers, NEED_URL);

	WebcitAddUrlHandler(HKEY("mimepart"), view_mimepart, NEED_URL);
	WebcitAddUrlHandler(HKEY("mimepart_download"), download_mimepart, NEED_URL);
	WebcitAddUrlHandler(HKEY("postpart"), view_postpart, NEED_URL);
	WebcitAddUrlHandler(HKEY("postpart_download"), download_postpart, NEED_URL);

	/* json */
	WebcitAddUrlHandler(HKEY("roommsgs"), jsonMessageList,0);
	return ;
}
