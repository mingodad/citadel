/*----------------------------------------------------------------------------*/
#include "webcit.h"
#include "webserver.h"

/**
 * message index functions
 */

void DestroyMimeParts(wc_mime_attachment *Mime)
{
	FreeStrBuf(&Mime->Name);
	FreeStrBuf(&Mime->FileName);
	FreeStrBuf(&Mime->PartNum);
	FreeStrBuf(&Mime->Disposition);
	FreeStrBuf(&Mime->ContentType);
	FreeStrBuf(&Mime->Charset);
	FreeStrBuf(&Mime->Data);
}

void DestroyMime(void *vMime)
{
	wc_mime_attachment *Mime = (wc_mime_attachment*)vMime;
	DestroyMimeParts(Mime);
	free(Mime);
}

void DestroyMessageSummary(void *vMsg)
{
	message_summary *Msg = (message_summary*) vMsg;

	FreeStrBuf(&Msg->from);
	FreeStrBuf(&Msg->to);
	FreeStrBuf(&Msg->subj);
	FreeStrBuf(&Msg->reply_inreplyto);
	FreeStrBuf(&Msg->reply_references);
	FreeStrBuf(&Msg->cccc);
	FreeStrBuf(&Msg->hnod);
	FreeStrBuf(&Msg->AllRcpt);
	FreeStrBuf(&Msg->Room);
	FreeStrBuf(&Msg->Rfca);
	FreeStrBuf(&Msg->OtherNode);

	FreeStrBuf(&Msg->reply_to);

	DeleteHash(&Msg->Attachments);  /**< list of Accachments */
	DeleteHash(&Msg->Submessages);
	DeleteHash(&Msg->AttachLinks);
	DeleteHash(&Msg->AllAttach);

	DestroyMimeParts(&Msg->MsgBody);

	free(Msg);
}



void RegisterMsgHdr(const char *HeaderName, long HdrNLen, ExamineMsgHeaderFunc evaluator, int type)
{
	headereval *ev;
	ev = (headereval*) malloc(sizeof(headereval));
	ev->evaluator = evaluator;
	ev->Type = type;
	Put(MsgHeaderHandler, HeaderName, HdrNLen, ev, NULL);
}

void RegisterMimeRenderer(const char *HeaderName, long HdrNLen, RenderMimeFunc MimeRenderer)
{
	Put(MimeRenderHandler, HeaderName, HdrNLen, MimeRenderer, reference_free_handler);
	
}

/*----------------------------------------------------------------------------*/


void examine_nhdr(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->nhdr = 0;
	if (!strncasecmp(ChrPtr(HdrLine), "yes", 8))
		Msg->nhdr = 1;
}
int Conditional_ANONYMOUS_MESSAGE(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	return Msg->nhdr != 0;
}


void examine_type(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->format_type = StrToi(HdrLine);
			
}

void examine_from(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->from);
	Msg->from = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->from, HdrLine, WC->DefaultCharset, FoundCharset);
}
void tmplput_MAIL_SUMM_FROM(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	StrBufAppendBuf(Target, Msg->from, 0);
}



void examine_subj(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->subj);
	Msg->subj = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->subj, HdrLine, WC->DefaultCharset, FoundCharset);
}
void tmplput_MAIL_SUMM_SUBJECT(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{/////TODO: Fwd: and RE: filter!!
	message_summary *Msg = (message_summary*) Context;
	StrBufAppendBuf(Target, Msg->subj, 0);
}


void examine_msgn(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->reply_inreplyto);
	Msg->reply_inreplyto = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->reply_inreplyto, HdrLine, WC->DefaultCharset, FoundCharset);
}
void tmplput_MAIL_SUMM_INREPLYTO(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	StrBufAppendBuf(Target, Msg->reply_inreplyto, 0);
}

int Conditional_MAIL_SUMM_UNREAD(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	return Msg->is_new != 0;
}

void examine_wefw(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->reply_references);
	Msg->reply_references = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->reply_references, HdrLine, WC->DefaultCharset, FoundCharset);
}
void tmplput_MAIL_SUMM_REFIDS(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	StrBufAppendBuf(Target, Msg->reply_references, 0);
}


void examine_cccc(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->cccc);
	Msg->cccc = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->cccc, HdrLine, WC->DefaultCharset, FoundCharset);
	if (Msg->AllRcpt == NULL)
		Msg->AllRcpt = NewStrBufPlain(NULL, StrLength(HdrLine));
	if (StrLength(Msg->AllRcpt) > 0) {
		StrBufAppendBufPlain(Msg->AllRcpt, HKEY(", "), 0);
	}
	StrBufAppendBuf(Msg->AllRcpt, Msg->cccc, 0);
}
void tmplput_MAIL_SUMM_CCCC(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	StrBufAppendBuf(Target, Msg->cccc, 0);
}




void examine_room(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	if ((StrLength(HdrLine) > 0) &&
	    (strcasecmp(ChrPtr(HdrLine), WC->wc_roomname))) {
		FreeStrBuf(&Msg->Room);
		Msg->Room = NewStrBufDup(HdrLine);		
	}
}
void tmplput_MAIL_SUMM_ORGROOM(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	StrBufAppendBuf(Target, Msg->Room, 0);
}


void examine_rfca(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->Rfca);
	Msg->Rfca = NewStrBufDup(HdrLine);
}
void tmplput_MAIL_SUMM_RFCA(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	StrBufAppendBuf(Target, Msg->Rfca, 0);
}


void examine_node(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	if ( (StrLength(HdrLine) > 0) &&
	     ((WC->room_flags & QR_NETWORK)
	      || ((strcasecmp(ChrPtr(HdrLine), serv_info.serv_nodename)
		   && (strcasecmp(ChrPtr(HdrLine), serv_info.serv_fqdn)))))) {
		FreeStrBuf(&Msg->OtherNode);
		Msg->OtherNode = NewStrBufDup(HdrLine);
	}
}
void tmplput_MAIL_SUMM_OTHERNODE(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	StrBufAppendBuf(Target, Msg->OtherNode, 0);
}
int Conditional_MAIL_SUMM_OTHERNODE(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	return StrLength(Msg->OtherNode) > 0;
}


void examine_rcpt(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->to);
	Msg->to = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->to, HdrLine, WC->DefaultCharset, FoundCharset);
	if (Msg->AllRcpt == NULL)
		Msg->AllRcpt = NewStrBufPlain(NULL, StrLength(HdrLine));
	if (StrLength(Msg->AllRcpt) > 0) {
		StrBufAppendBufPlain(Msg->AllRcpt, HKEY(", "), 0);
	}
	StrBufAppendBuf(Msg->AllRcpt, Msg->to, 0);
}
void tmplput_MAIL_SUMM_TO(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	StrBufAppendBuf(Target, Msg->to, 0);
}
void tmplput_MAIL_SUMM_ALLRCPT(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	StrBufAppendBuf(Target, Msg->AllRcpt, 0);
}



void examine_time(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->date = StrTol(HdrLine);
}
void tmplput_MAIL_SUMM_DATE_STR(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	char datebuf[64];
	message_summary *Msg = (message_summary*) Context;
	webcit_fmt_date(datebuf, Msg->date, 1);
	StrBufAppendBufPlain(Target, datebuf, -1, 0);
}
void tmplput_MAIL_SUMM_DATE_NO(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	StrBufAppendPrintf(Target, "%ld", Msg->date, 0);
}



void render_MAIL(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	Mime->Data = NewStrBufPlain(NULL, Mime->length);
	read_message(Mime->Data, HKEY("view_submessage"), Mime->msgnum, 0, Mime->PartNum);
/*
	if ( (!IsEmptyStr(mime_submessages)) && (!section[0]) ) {
		for (i=0; i<num_tokens(mime_submessages, '|'); ++i) {
			extract_token(buf, mime_submessages, i, '|', sizeof buf);
			/** use printable_view to suppress buttons * /
			wprintf("<blockquote>");
			read_message(Mime->msgnum, 1, ChrPtr(Mime->Section));
			wprintf("</blockquote>");
		}
	}
*/
}

void render_MIME_VCard(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	MimeLoadData(Mime);
	if (StrLength(Mime->Data) > 0) {
		StrBuf *Buf;
		Buf = NewStrBuf();
		/** If it's my vCard I can edit it */
		if (	(!strcasecmp(WC->wc_roomname, USERCONFIGROOM))
			|| (!strcasecmp(&WC->wc_roomname[11], USERCONFIGROOM))
			|| (WC->wc_view == VIEW_ADDRESSBOOK)
			) {
			StrBufAppendPrintf(Buf, "<a href=\"edit_vcard?msgnum=%ld&partnum=%s\">",
				Mime->msgnum, ChrPtr(Mime->PartNum));
			StrBufAppendPrintf(Buf, "[%s]</a>", _("edit"));
		}

		/* In all cases, display the full card */
		display_vcard(Buf, ChrPtr(Mime->Data), 0, 1, NULL, Mime->msgnum);
		FreeStrBuf(&Mime->Data);
		Mime->Data = Buf;
	}

}
void render_MIME_ICS(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	MimeLoadData(Mime);
	if (StrLength(Mime->Data) > 0) {
		cal_process_attachment(Mime);
	}
}



void examine_mime_part(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	wc_mime_attachment *mime;
	StrBuf *Buf;
	void *vMimeRenderer;

	mime = (wc_mime_attachment*) malloc(sizeof(wc_mime_attachment));
	memset(mime, 0, sizeof(wc_mime_attachment));
	mime->msgnum = Msg->msgnum;
	Buf = NewStrBuf();

	mime->Name = NewStrBuf();
	StrBufExtract_token(mime->Name, HdrLine, 0, '|');

	StrBufExtract_token(Buf, HdrLine, 1, '|');
	mime->FileName = NewStrBuf();
	StrBuf_RFC822_to_Utf8(mime->FileName, Buf, WC->DefaultCharset, FoundCharset);

	mime->PartNum = NewStrBuf();
	StrBufExtract_token(mime->PartNum, HdrLine, 2, '|');

	mime->Disposition = NewStrBuf();
	StrBufExtract_token(mime->Disposition, HdrLine, 3, '|');

	mime->ContentType = NewStrBuf();
	StrBufExtract_token(mime->ContentType, HdrLine, 4, '|');

	mime->length = StrBufExtract_int(HdrLine, 5, '|');

	StrBufTrim(mime->Name);
	StrBufTrim(mime->FileName);

	if ( (StrLength(mime->FileName) == 0) && (StrLength(mime->Name) > 0) ) {
		StrBufAppendBuf(mime->FileName, mime->Name, 0);
	}

	if (Msg->AllAttach == NULL)
		Msg->AllAttach = NewHash(1,NULL);
	Put(Msg->AllAttach, SKEY(mime->PartNum), mime, DestroyMime);


	if (GetHash(MimeRenderHandler, SKEY(mime->ContentType), &vMimeRenderer) &&
	    vMimeRenderer != NULL)
	{
		mime->Renderer = (RenderMimeFunc) vMimeRenderer;
		if (Msg->Submessages == NULL)
			Msg->Submessages = NewHash(1,NULL);
		Put(Msg->Submessages, SKEY(mime->PartNum), mime, reference_free_handler);
	}
	else if ((!strcasecmp(ChrPtr(mime->Disposition), "inline"))
		 && (!strncasecmp(ChrPtr(mime->ContentType), "image/", 6)) ){
		if (Msg->AttachLinks == NULL)
			Msg->AttachLinks = NewHash(1,NULL);
		Put(Msg->AttachLinks, SKEY(mime->PartNum), mime, reference_free_handler);
	}
	else if ((StrLength(mime->ContentType) > 0) &&
		  ( (!strcasecmp(ChrPtr(mime->Disposition), "attachment")) 
		    || (!strcasecmp(ChrPtr(mime->Disposition), "inline"))
		    || (!strcasecmp(ChrPtr(mime->Disposition), ""))))
	{		
		if (Msg->AttachLinks == NULL)
			Msg->AttachLinks = NewHash(1,NULL);
		Put(Msg->AttachLinks, SKEY(mime->PartNum), mime, reference_free_handler);
		if (strcasecmp(ChrPtr(mime->ContentType), "application/octet-stream") == 0) {
			FlushStrBuf(mime->ContentType);
			StrBufAppendBufPlain(mime->ContentType,
					     GuessMimeByFilename(SKEY(mime->FileName)),
					     -1, 0);
		}
	}

	FreeStrBuf(&Buf);
}

void tmplput_MAIL_SUMM_NATTACH(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	StrBufAppendPrintf(Target, "%ld", GetCount(Msg->Attachments));
}







void examine_hnod(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->hnod);
	Msg->hnod = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->hnod, HdrLine, WC->DefaultCharset, FoundCharset);
}
void tmplput_MAIL_SUMM_H_NODE(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	StrBufAppendBuf(Target, Msg->hnod, 0);
}
int Conditional_MAIL_SUMM_H_NODE(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	return StrLength(Msg->hnod) > 0;
}



void examine_text(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{////TODO: read messages here
	Msg->MsgBody.Data = NewStrBuf();
}

void examine_msg4_partnum(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->MsgBody.PartNum = NewStrBufDup(HdrLine);
	StrBufTrim(Msg->MsgBody.PartNum);/////TODO: striplt == trim?
}

void examine_content_encoding(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
////TODO: do we care?
}

void examine_content_lengh(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->MsgBody.length = StrTol(HdrLine);
	Msg->MsgBody.size_known = 1;
}

void examine_content_type(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{////TODO
	int len, i;
	Msg->MsgBody.ContentType = NewStrBufDup(HdrLine);
	StrBufTrim(Msg->MsgBody.ContentType);/////todo==striplt?
	len = StrLength(Msg->MsgBody.ContentType);
	for (i=0; i<len; ++i) {
		if (!strncasecmp(ChrPtr(Msg->MsgBody.ContentType) + i, "charset=", 8)) {/// TODO: WHUT?
//			safestrncpy(mime_charset, &mime_content_type[i+8],
			///			    sizeof mime_charset);
		}
	}/****
	for (i=0; i<len; ++i) {
		if (mime_content_type[i] == ';') {
			mime_content_type[i] = 0;
			len = i - 1;
		}
	}
	len = strlen(mime_charset);
	for (i=0; i<len; ++i) {
		if (mime_charset[i] == ';') {
			mime_charset[i] = 0;
			len = i - 1;
		}
	}
	 */
}

void tmplput_MAIL_SUMM_N(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	StrBufAppendPrintf(Target, "%ld", Msg->msgnum);
}



int Conditional_MAIL_MIME_ALL(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	return GetCount(Msg->Attachments) > 0;
}

int Conditional_MAIL_MIME_SUBMESSAGES(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	return GetCount(Msg->Submessages) > 0;
}

int Conditional_MAIL_MIME_ATTACHLINKS(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	return GetCount(Msg->AttachLinks) > 0;
}

int Conditional_MAIL_MIME_ATTACH(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	return GetCount(Msg->AllAttach) > 0;
}



/*----------------------------------------------------------------------------*/
void tmplput_QUOTED_MAIL_BODY(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	long MsgNum;
	MsgNum = LBstr(Token->Params[0]->Start, Token->Params[0]->len);
	read_message(Target, HKEY("view_message_replyquote"), MsgNum, 0, NULL);
}

void tmplput_MAIL_BODY(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	StrBufAppendBuf(Target, Msg->MsgBody.Data, 0);
}


void render_MAIL_variformat(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	/* Messages in legacy Citadel variformat get handled thusly... */
	StrBuf *Target = NewStrBufPlain(NULL, StrLength(Mime->Data));
	FmOut(Target, "JUSTIFY", Mime->Data);
	FreeStrBuf(&Mime->Data);
	Mime->Data = Target;
}

void render_MAIL_text_plain(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	StrBuf *cs = NULL;
	const char *ptr, *pte;
	const char *BufPtr = NULL;
	StrBuf *Line = NewStrBuf();
	StrBuf *Line1 = NewStrBuf();
	StrBuf *Line2 = NewStrBuf();
	StrBuf *Target = NewStrBufPlain(NULL, StrLength(Mime->Data));
	int ConvertIt = 1;
	int bn = 0;
	int bq = 0;
	int i, n, done = 0;
	long len;
#ifdef HAVE_ICONV
	iconv_t ic = (iconv_t)(-1) ;
#endif

	if ((StrLength(Mime->Data) == 0) && (Mime->length > 0)) {
		FreeStrBuf(&Mime->Data);
		Mime->Data = NewStrBufPlain(NULL, Mime->length);
		read_message(Mime->Data, HKEY("view_submessage"), Mime->msgnum, 0, Mime->PartNum);
	}

	/* Boring old 80-column fixed format text gets handled this way... */
	if ((strcasecmp(ChrPtr(Mime->Charset), "us-ascii") == 0) &&
	    (strcasecmp(ChrPtr(Mime->Charset), "UTF-8") == 0))
		ConvertIt = 0;

#ifdef HAVE_ICONV
	if (ConvertIt) {
		if (StrLength(Mime->Charset) != 0)
			cs = Mime->Charset;
		else if (StrLength(FoundCharset) > 0)
			cs = FoundCharset;
		else if (StrLength(WC->DefaultCharset) > 0)
			cs = WC->DefaultCharset;
		if (cs == 0) {
			ConvertIt = 0;
		}
		else {
			ctdl_iconv_open("UTF-8", ChrPtr(cs), &ic);
			if (ic == (iconv_t)(-1) ) {
				lprintf(5, "%s:%d iconv_open(UTF-8, %s) failed: %s\n",
					__FILE__, __LINE__, ChrPtr(Mime->Charset), strerror(errno));
			}
		}
	}
#endif

	while ((n = StrBufSipLine(Line, Mime->Data, &BufPtr), n >= 0) && !done)
	{
		done = n == 0;
		bq = 0;
		i = 0;
		ptr = ChrPtr(Line);
		len = StrLength(Line);
		pte = ptr + len;
		
		while ((ptr < pte) &&
		       ((*ptr == '>') ||
			isspace(*ptr)))
		{
			if (*ptr == '>')
				bq++;
			ptr ++;
			i++;
		}
		if (i > 0) StrBufCutLeft(Line, i);
		
		if (StrLength(Line) == 0)
			continue;

		for (i = bn; i < bq; i++)				
			StrBufAppendBufPlain(Target, HKEY("<blockquote>"), 0);
		for (i = bq; i < bn; i++)				
			StrBufAppendBufPlain(Target, HKEY("</blockquote>"), 0);

		if (ConvertIt == 1) {
			StrBufConvert(Line, Line1, &ic);
		}

		StrBufAppendBufPlain(Target, HKEY("<tt>"), 0);
		UrlizeText(Line1, Line, Line2);

		StrEscAppend(Target, Line1, NULL, 0, 0);
		StrBufAppendBufPlain(Target, HKEY("</tt><br />\n"), 0);
		bn = bq;
	}

	for (i = 0; i < bn; i++)				
		StrBufAppendBufPlain(Target, HKEY("</blockquote>"), 0);

	StrBufAppendBufPlain(Target, HKEY("</i><br />"), 0);
#ifdef HAVE_ICONV
	if (ic != (iconv_t)(-1) ) {
		iconv_close(ic);
	}
#endif
	FreeStrBuf(&Mime->Data);
	Mime->Data = Target;
	FreeStrBuf(&Line);
	FreeStrBuf(&Line1);
	FreeStrBuf(&Line2);
}

void render_MAIL_html(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	StrBuf *Buf;
	/* HTML is fun, but we've got to strip it first */
	if (StrLength(Mime->Data) == 0)
		return;

	Buf = NewStrBufPlain(NULL, StrLength(Mime->Data));

	output_html(ChrPtr(Mime->Charset), 
		    (WC->wc_view == VIEW_WIKI ? 1 : 0), 
		    StrToi(Mime->PartNum), 
		    Mime->Data, Buf);
	FreeStrBuf(&Mime->Data);
	Mime->Data = Buf;
}

void render_MAIL_UNKNOWN(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	/* Unknown weirdness */
	FlushStrBuf(Mime->Data);
	StrBufAppendBufPlain(Mime->Data, _("I don't know how to display "), -1, 0);
	StrBufAppendBuf(Mime->Data, Mime->ContentType, 0);
	StrBufAppendBufPlain(Mime->Data, HKEY("<br />\n"), 0);
}






HashList *iterate_get_mime_All(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	return Msg->Attachments;
}
HashList *iterate_get_mime_Submessages(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	return Msg->Submessages;
}
HashList *iterate_get_mime_AttachLinks(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	return Msg->AttachLinks;
}
HashList *iterate_get_mime_Attachments(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	message_summary *Msg = (message_summary*) Context;
	return Msg->AllAttach;
}

void tmplput_MIME_Name(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) Context;
	StrBufAppendBuf(Target, mime->Name, 0);
}

void tmplput_MIME_FileName(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) Context;
	StrBufAppendBuf(Target, mime->FileName, 0);
}

void tmplput_MIME_PartNum(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) Context;
	StrBufAppendBuf(Target, mime->PartNum, 0);
}

void tmplput_MIME_MsgNum(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) Context;
	StrBufAppendPrintf(Target, "%ld", mime->msgnum);
}

void tmplput_MIME_Disposition(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) Context;
	StrBufAppendBuf(Target, mime->Disposition, 0);
}

void tmplput_MIME_ContentType(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) Context;
	StrBufAppendBuf(Target, mime->ContentType, 0);
}

void tmplput_MIME_Charset(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) Context;
	StrBufAppendBuf(Target, mime->Charset, 0);
}

void tmplput_MIME_Data(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) Context;
	if (mime->Renderer != NULL)
		mime->Renderer(mime, NULL, NULL);
	StrBufAppendBuf(Target, mime->Data, 0); /// TODO: check whether we need to load it now?
}

void tmplput_MIME_Length(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) Context;
	StrBufAppendPrintf(Target, "%ld", mime->length);
}

HashList *iterate_get_registered_Attachments(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	return WC->attachments;
}

void tmplput_ATT_Length(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	wc_attachment *att = (wc_attachment*) Context;
	StrBufAppendPrintf(Target, "%ld", att->length);
}

void tmplput_ATT_Contenttype(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	wc_attachment *att = (wc_attachment*) Context;
	StrBufAppendBuf(Target, att->content_type, 0);
}

void tmplput_ATT_FileName(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	wc_attachment *att = (wc_attachment*) Context;
	StrBufAppendBuf(Target, att->filename, 0);
}






void 
InitModule_MSGRENDERERS
(void)
{
	RegisterNamespace("MAIL:SUMM:DATESTR", 0, 0, tmplput_MAIL_SUMM_DATE_STR, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:DATENO",  0, 0, tmplput_MAIL_SUMM_DATE_NO,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:N",       0, 0, tmplput_MAIL_SUMM_N,        CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:FROM",    0, 2, tmplput_MAIL_SUMM_FROM,     CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:TO",      0, 2, tmplput_MAIL_SUMM_TO,       CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:SUBJECT", 0, 4, tmplput_MAIL_SUMM_SUBJECT,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:NTATACH", 0, 0, tmplput_MAIL_SUMM_NATTACH,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:CCCC", 0, 2, tmplput_MAIL_SUMM_CCCC,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:H_NODE", 0, 2, tmplput_MAIL_SUMM_H_NODE,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:ALLRCPT", 0, 2, tmplput_MAIL_SUMM_ALLRCPT,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:ORGROOM", 0, 2, tmplput_MAIL_SUMM_ORGROOM,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:RFCA", 0, 2, tmplput_MAIL_SUMM_RFCA,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:OTHERNODE", 2, 0, tmplput_MAIL_SUMM_OTHERNODE,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:REFIDS", 0, 0, tmplput_MAIL_SUMM_REFIDS,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:INREPLYTO", 0, 2, tmplput_MAIL_SUMM_INREPLYTO,  CTX_MAILSUM);
	RegisterNamespace("MAIL:BODY", 0, 2, tmplput_MAIL_BODY,  CTX_MAILSUM);
	RegisterNamespace("MAIL:QUOTETEXT", 1, 2, tmplput_QUOTED_MAIL_BODY,  CTX_NONE);
	RegisterNamespace("ATT:SIZE", 0, 1, tmplput_ATT_Length, CTX_ATT);
	RegisterNamespace("ATT:TYPE", 0, 1, tmplput_ATT_Contenttype, CTX_ATT);
	RegisterNamespace("ATT:FILENAME", 0, 1, tmplput_ATT_FileName, CTX_ATT);

	RegisterConditional(HKEY("COND:MAIL:SUMM:UNREAD"), 0, Conditional_MAIL_SUMM_UNREAD, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:SUMM:H_NODE"), 0, Conditional_MAIL_SUMM_H_NODE, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:SUMM:OTHERNODE"), 0, Conditional_MAIL_SUMM_OTHERNODE, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:ANON"), 0, Conditional_ANONYMOUS_MESSAGE, CTX_MAILSUM);

	RegisterConditional(HKEY("COND:MAIL:MIME:ATTACH"), 0, Conditional_MAIL_MIME_ALL, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:MIME:ATTACH:SUBMESSAGES"), 0, Conditional_MAIL_MIME_SUBMESSAGES, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:MIME:ATTACH:LINKS"), 0, Conditional_MAIL_MIME_ATTACHLINKS, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:MIME:ATTACH:ATT"), 0, Conditional_MAIL_MIME_ATTACH, CTX_MAILSUM);

	RegisterIterator("MAIL:MIME:ATTACH", 0, NULL, iterate_get_mime_All, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_MAILSUM);
	RegisterIterator("MAIL:MIME:ATTACH:SUBMESSAGES", 0, NULL, iterate_get_mime_Submessages, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_MAILSUM);
	RegisterIterator("MAIL:MIME:ATTACH:LINKS", 0, NULL, iterate_get_mime_AttachLinks, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_MAILSUM);
	RegisterIterator("MAIL:MIME:ATTACH:ATT", 0, NULL, iterate_get_mime_Attachments, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_MAILSUM);

	RegisterNamespace("MAIL:MIME:NAME", 0, 2, tmplput_MIME_Name, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:FILENAME", 0, 2, tmplput_MIME_FileName, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:PARTNUM", 0, 2, tmplput_MIME_PartNum, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:MSGNUM", 0, 2, tmplput_MIME_MsgNum, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:DISPOSITION", 0, 2, tmplput_MIME_Disposition, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:CONTENTTYPE", 0, 2, tmplput_MIME_ContentType, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:CHARSET", 0, 2, tmplput_MIME_Charset, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:LENGTH", 0, 2, tmplput_MIME_Length, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:DATA", 0, 2, tmplput_MIME_Data, CTX_MIME_ATACH);

	RegisterIterator("MSG:ATTACHNAMES", 0, NULL, iterate_get_registered_Attachments, 
			 NULL, NULL, CTX_ATT, CTX_NONE);

	RegisterMimeRenderer(HKEY("message/rfc822"), render_MAIL);
	RegisterMimeRenderer(HKEY("text/x-vcard"), render_MIME_VCard);
	RegisterMimeRenderer(HKEY("text/vcard"), render_MIME_VCard);
	RegisterMimeRenderer(HKEY("text/calendar"), render_MIME_ICS);
	RegisterMimeRenderer(HKEY("application/ics"), render_MIME_ICS);

	RegisterMimeRenderer(HKEY("text/x-citadel-variformat"), render_MAIL_variformat);
	RegisterMimeRenderer(HKEY("text/plain"), render_MAIL_text_plain);
	RegisterMimeRenderer(HKEY("text"), render_MAIL_text_plain);
	RegisterMimeRenderer(HKEY("text/html"), render_MAIL_html);
	RegisterMimeRenderer(HKEY(""), render_MAIL_UNKNOWN);

	RegisterMsgHdr(HKEY("nhdr"), examine_nhdr, 0);
	RegisterMsgHdr(HKEY("type"), examine_type, 0);
	RegisterMsgHdr(HKEY("from"), examine_from, 0);
	RegisterMsgHdr(HKEY("subj"), examine_subj, 0);
	RegisterMsgHdr(HKEY("msgn"), examine_msgn, 0);
	RegisterMsgHdr(HKEY("wefw"), examine_wefw, 0);
	RegisterMsgHdr(HKEY("cccc"), examine_cccc, 0);
	RegisterMsgHdr(HKEY("hnod"), examine_hnod, 0);
	RegisterMsgHdr(HKEY("room"), examine_room, 0);
	RegisterMsgHdr(HKEY("rfca"), examine_rfca, 0);
	RegisterMsgHdr(HKEY("node"), examine_node, 0);
	RegisterMsgHdr(HKEY("rcpt"), examine_rcpt, 0);
	RegisterMsgHdr(HKEY("time"), examine_time, 0);
	RegisterMsgHdr(HKEY("part"), examine_mime_part, 0);
	RegisterMsgHdr(HKEY("text"), examine_text, 1);
	RegisterMsgHdr(HKEY("X-Citadel-MSG4-Partnum"), examine_msg4_partnum, 0);
	RegisterMsgHdr(HKEY("Content-type"), examine_content_type, 0);
	RegisterMsgHdr(HKEY("Content-length"), examine_content_lengh, 0);
	RegisterMsgHdr(HKEY("Content-transfer-encoding"), examine_content_encoding, 0);
}
