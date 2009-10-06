#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"

/*
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
	RenderMimeFuncStruct *f;

	f = (RenderMimeFuncStruct*) malloc(sizeof(RenderMimeFuncStruct));
	f->f = MimeRenderer;
	Put(MimeRenderHandler, HeaderName, HdrNLen, f, NULL);
	
}

/*----------------------------------------------------------------------------*/

/*
 *  comparator for two longs in descending order.
 */
int longcmp_r(const void *s1, const void *s2) {
	long l1;
	long l2;

	l1 = *(long *)GetSearchPayload(s1);
	l2 = *(long *)GetSearchPayload(s2);

	if (l1 > l2) return(-1);
	if (l1 < l2) return(+1);
	return(0);
}

/*
 *  comparator for longs; descending order.
 */
int qlongcmp_r(const void *s1, const void *s2) {
	long l1 = (long) s1;
	long l2 = (long) s2;

	if (l1 > l2) return(-1);
	if (l1 < l2) return(+1);
	return(0);
}

 
/*
 * comparator for message summary structs by ascending subject.
 */
int summcmp_subj(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)GetSearchPayload(s1);
	summ2 = (message_summary *)GetSearchPayload(s2);
	return strcasecmp(ChrPtr(summ1->subj), ChrPtr(summ2->subj));
}

/*
 * comparator for message summary structs by descending subject.
 */
int summcmp_rsubj(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)GetSearchPayload(s1);
	summ2 = (message_summary *)GetSearchPayload(s2);
	return strcasecmp(ChrPtr(summ2->subj), ChrPtr(summ1->subj));
}
/*
 * comparator for message summary structs by descending subject.
 */
int groupchange_subj(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)s1;
	summ2 = (message_summary *)s2;
	return ChrPtr(summ2->subj)[0] != ChrPtr(summ1->subj)[0];
}

/*
 * comparator for message summary structs by ascending sender.
 */
int summcmp_sender(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)GetSearchPayload(s1);
	summ2 = (message_summary *)GetSearchPayload(s2);
	return strcasecmp(ChrPtr(summ1->from), ChrPtr(summ2->from));
}

/*
 * comparator for message summary structs by descending sender.
 */
int summcmp_rsender(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)GetSearchPayload(s1);
	summ2 = (message_summary *)GetSearchPayload(s2);
	return strcasecmp(ChrPtr(summ2->from), ChrPtr(summ1->from));
}
/*
 * comparator for message summary structs by descending sender.
 */
int groupchange_sender(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)s1;
	summ2 = (message_summary *)s2;
	return strcasecmp(ChrPtr(summ2->from), ChrPtr(summ1->from)) != 0;

}

/*
 * comparator for message summary structs by ascending date.
 */
int summcmp_date(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)GetSearchPayload(s1);
	summ2 = (message_summary *)GetSearchPayload(s2);

	if (summ1->date < summ2->date) return -1;
	else if (summ1->date > summ2->date) return +1;
	else return 0;
}

/*
 * comparator for message summary structs by descending date.
 */
int summcmp_rdate(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)GetSearchPayload(s1);
	summ2 = (message_summary *)GetSearchPayload(s2);

	if (summ1->date < summ2->date) return +1;
	else if (summ1->date > summ2->date) return -1;
	else return 0;
}

/*
 * comparator for message summary structs by descending date.
 */
const long DAYSECONDS = 24 * 60 * 60;
int groupchange_date(const void *s1, const void *s2) {
	message_summary *summ1;
	message_summary *summ2;
	
	summ1 = (message_summary *)s1;
	summ2 = (message_summary *)s2;

	return (summ1->date % DAYSECONDS) != (summ2->date %DAYSECONDS);
}


/*----------------------------------------------------------------------------*/
/* Don't wanna know... or? */
void examine_pref(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset) {return;}
void examine_suff(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset) {return;}
void examine_path(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset) {return;}
void examine_content_encoding(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
/* TODO: do we care? */
}

void examine_nhdr(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->nhdr = 0;
	if (!strncasecmp(ChrPtr(HdrLine), "yes", 8))
		Msg->nhdr = 1;
}
int Conditional_ANONYMOUS_MESSAGE(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
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
void tmplput_MAIL_SUMM_FROM(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->from, 0);
}



void examine_subj(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->subj);
	Msg->subj = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->subj, HdrLine, WC->DefaultCharset, FoundCharset);
}
void tmplput_MAIL_SUMM_SUBJECT(StrBuf *Target, WCTemplputParams *TP)
{/*////TODO: Fwd: and RE: filter!!*/

	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->subj, 0);
}
int Conditional_MAIL_SUMM_SUBJECT(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return StrLength(Msg->subj) > 0;
}


void examine_msgn(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->reply_inreplyto);
	Msg->reply_inreplyto = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->reply_inreplyto, HdrLine, WC->DefaultCharset, FoundCharset);
}
void tmplput_MAIL_SUMM_INREPLYTO(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->reply_inreplyto, 0);
}

int Conditional_MAIL_SUMM_UNREAD(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return Msg->is_new != 0;
}

void examine_wefw(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->reply_references);
	Msg->reply_references = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->reply_references, HdrLine, WC->DefaultCharset, FoundCharset);
}
void tmplput_MAIL_SUMM_REFIDS(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->reply_references, 0);
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
void tmplput_MAIL_SUMM_CCCC(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->cccc, 0);
}


void examine_room(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	if ((StrLength(HdrLine) > 0) &&
	    (strcasecmp(ChrPtr(HdrLine), ChrPtr(WC->wc_roomname)))) {
		FreeStrBuf(&Msg->Room);
		Msg->Room = NewStrBufDup(HdrLine);		
	}
}
void tmplput_MAIL_SUMM_ORGROOM(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->Room, 0);
}


void examine_rfca(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->Rfca);
	Msg->Rfca = NewStrBufDup(HdrLine);
}
void tmplput_MAIL_SUMM_RFCA(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->Rfca, 0);
}
int Conditional_MAIL_SUMM_RFCA(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return StrLength(Msg->Rfca) > 0;
}
int Conditional_MAIL_SUMM_CCCC(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return StrLength(Msg->cccc) > 0;
}

void examine_node(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	wcsession *WCC = WC;

	if ( (StrLength(HdrLine) > 0) &&
	     ((WC->room_flags & QR_NETWORK)
	      || ((strcasecmp(ChrPtr(HdrLine), ChrPtr(WCC->serv_info->serv_nodename))
		   && (strcasecmp(ChrPtr(HdrLine), ChrPtr(WCC->serv_info->serv_fqdn))))))) {
		FreeStrBuf(&Msg->OtherNode);
		Msg->OtherNode = NewStrBufDup(HdrLine);
	}
}
void tmplput_MAIL_SUMM_OTHERNODE(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->OtherNode, 0);
}
int Conditional_MAIL_SUMM_OTHERNODE(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
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
void tmplput_MAIL_SUMM_TO(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->to, 0);
}
int Conditional_MAIL_SUMM_TO(StrBuf *Target, WCTemplputParams *TP) 
{
	message_summary *Msg = (message_summary*) CTX;
	return StrLength(Msg->to) != 0;
}
int Conditional_MAIL_SUMM_SUBJ(StrBuf *Target, WCTemplputParams *TP) 
{
	message_summary *Msg = (message_summary*) CTX;
	return StrLength(Msg->subj) != 0;
}
void tmplput_MAIL_SUMM_ALLRCPT(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->AllRcpt, 0);
}



void tmplput_SUMM_COUNT(StrBuf *Target, WCTemplputParams *TP)
{
	StrBufAppendPrintf(Target, "%d", GetCount( WC->summ));
}

HashList *iterate_get_mailsumm_All(StrBuf *Target, WCTemplputParams *TP)
{
	return WC->summ;
}
void examine_time(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->date = StrTol(HdrLine);
}

void tmplput_MAIL_SUMM_DATE_BRIEF(StrBuf *Target, WCTemplputParams *TP)
{
	char datebuf[64];
	message_summary *Msg = (message_summary*) CTX;
	webcit_fmt_date(datebuf, 64, Msg->date, DATEFMT_BRIEF);
	StrBufAppendBufPlain(Target, datebuf, -1, 0);
}

void tmplput_MAIL_SUMM_DATE_FULL(StrBuf *Target, WCTemplputParams *TP)
{
	char datebuf[64];
	message_summary *Msg = (message_summary*) CTX;
	webcit_fmt_date(datebuf, 64, Msg->date, DATEFMT_FULL);
	StrBufAppendBufPlain(Target, datebuf, -1, 0);
}
void tmplput_MAIL_SUMM_DATE_NO(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendPrintf(Target, "%ld", Msg->date, 0);
}



void render_MAIL(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	const StrBuf *TemplateMime;

	Mime->Data = NewStrBufPlain(NULL, Mime->length);
	read_message(Mime->Data, HKEY("view_submessage"), Mime->msgnum, Mime->PartNum, &TemplateMime);
/*
	if ( (!IsEmptyStr(mime_submessages)) && (!section[0]) ) {
		for (i=0; i<num_tokens(mime_submessages, '|'); ++i) {
			extract_token(buf, mime_submessages, i, '|', sizeof buf);
			/ ** use printable_view to suppress buttons * /
			wprintf("<blockquote>");
			read_message(Mime->msgnum, 1, ChrPtr(Mime->Section));
			wprintf("</blockquote>");
		}
	}
*/
}

void render_MIME_VCard(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	wcsession *WCC = WC;
	MimeLoadData(Mime);
	if (StrLength(Mime->Data) > 0) {
		StrBuf *Buf;
		Buf = NewStrBuf();
		/** If it's my vCard I can edit it */
		if (	(!strcasecmp(ChrPtr(WCC->wc_roomname), USERCONFIGROOM))
			|| (!strcasecmp(&(ChrPtr(WCC->wc_roomname)[11]), USERCONFIGROOM))
			|| (WC->wc_view == VIEW_ADDRESSBOOK)
			) {
			StrBufAppendPrintf(Buf, "<a href=\"edit_vcard?msgnum=%ld&partnum=%s\">",
				Mime->msgnum, ChrPtr(Mime->PartNum));
			StrBufAppendPrintf(Buf, "[%s]</a>", _("edit"));
		}

		/* In all cases, display the full card */
		display_vcard(Buf, Mime->Data, 0, 1, NULL, Mime->msgnum);
		FreeStrBuf(&Mime->Data);
		Mime->Data = Buf;
	}

}

void render_MIME_VNote(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	MimeLoadData(Mime);
	if (StrLength(Mime->Data) > 0) {
		struct vnote *v;
		StrBuf *Buf;
		char *vcard;

		Buf = NewStrBuf();
		vcard = SmashStrBuf(&Mime->Data);
		v = vnote_new_from_str(vcard);
		free (vcard);
		if (v) {
			WCTemplputParams TP;
			
			memset(&TP, 0, sizeof(WCTemplputParams));
			TP.Filter.ContextType = CTX_VNOTE;
			TP.Context = v;
			DoTemplate(HKEY("mail_vnoteitem"),
				   Buf, &TP);
			
			vnote_free(v);
			Mime->Data = Buf;
		}
		else
			Mime->Data = NewStrBuf();
	}

}

void render_MIME_ICS(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	if (StrLength(Mime->Data) == 0) {
		MimeLoadData(Mime);
	}
	if (StrLength(Mime->Data) > 0) {
		cal_process_attachment(Mime);
	}
}



void examine_mime_part(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	wc_mime_attachment *Mime;
	StrBuf *Buf;
	
	Mime = (wc_mime_attachment*) malloc(sizeof(wc_mime_attachment));
	memset(Mime, 0, sizeof(wc_mime_attachment));
	Mime->msgnum = Msg->msgnum;
	Buf = NewStrBuf();

	Mime->Name = NewStrBuf();
	StrBufExtract_token(Buf, HdrLine, 0, '|');
	StrBuf_RFC822_to_Utf8(Mime->Name, Buf, WC->DefaultCharset, FoundCharset);
	StrBufTrim(Mime->Name);

	StrBufExtract_token(Buf, HdrLine, 1, '|');
	Mime->FileName = NewStrBuf();
	StrBuf_RFC822_to_Utf8(Mime->FileName, Buf, WC->DefaultCharset, FoundCharset);
	StrBufTrim(Mime->FileName);

	Mime->PartNum = NewStrBuf();
	StrBufExtract_token(Mime->PartNum, HdrLine, 2, '|');
	StrBufTrim(Mime->PartNum);
	if (strchr(ChrPtr(Mime->PartNum), '.') != NULL) 
		Mime->level = 2;
	else
		Mime->level = 1;

	Mime->Disposition = NewStrBuf();
	StrBufExtract_token(Mime->Disposition, HdrLine, 3, '|');

	Mime->ContentType = NewStrBuf();
	StrBufExtract_token(Mime->ContentType, HdrLine, 4, '|');
	StrBufTrim(Mime->ContentType);
	StrBufLowerCase(Mime->ContentType);

	if (!strcmp(ChrPtr(Mime->ContentType), "application/octet-stream")) {
		StrBufPlain(Mime->ContentType, 
			    GuessMimeByFilename(SKEY(Mime->FileName)), -1);
	}
	Mime->length = StrBufExtract_int(HdrLine, 5, '|');

	if ( (StrLength(Mime->FileName) == 0) && (StrLength(Mime->Name) > 0) ) {
		StrBufAppendBuf(Mime->FileName, Mime->Name, 0);
	}

	if (StrLength(Msg->PartNum) > 0) {
		StrBuf *tmp;
		StrBufPrintf(Buf, "%s.%s", ChrPtr(Msg->PartNum), ChrPtr(Mime->PartNum));
		tmp = Mime->PartNum;
		Mime->PartNum = Buf;
		Buf = tmp;
	}

	if (Msg->AllAttach == NULL)
		Msg->AllAttach = NewHash(1,NULL);
	Put(Msg->AllAttach, SKEY(Mime->PartNum), Mime, DestroyMime);
	FreeStrBuf(&Buf);
}


void evaluate_mime_part(message_summary *Msg, wc_mime_attachment *Mime)
{
	void *vMimeRenderer;

	/* just print the root-node */
	if ((Mime->level == 1) &&
	    GetHash(MimeRenderHandler, SKEY(Mime->ContentType), &vMimeRenderer) &&
	    vMimeRenderer != NULL)
	{
		Mime->Renderer = (RenderMimeFuncStruct*) vMimeRenderer;
		if (Msg->Submessages == NULL)
			Msg->Submessages = NewHash(1,NULL);
		Put(Msg->Submessages, SKEY(Mime->PartNum), Mime, reference_free_handler);
	}
	else if ((Mime->level == 1) &&
		 (!strcasecmp(ChrPtr(Mime->Disposition), "inline"))
		 && (!strncasecmp(ChrPtr(Mime->ContentType), "image/", 6)) ){
		if (Msg->AttachLinks == NULL)
			Msg->AttachLinks = NewHash(1,NULL);
		Put(Msg->AttachLinks, SKEY(Mime->PartNum), Mime, reference_free_handler);
	}
	else if ((Mime->level == 1) &&
		 (StrLength(Mime->ContentType) > 0) &&
		  ( (!strcasecmp(ChrPtr(Mime->Disposition), "attachment")) 
		    || (!strcasecmp(ChrPtr(Mime->Disposition), "inline"))
		    || (!strcasecmp(ChrPtr(Mime->Disposition), ""))))
	{		
		if (Msg->AttachLinks == NULL)
			Msg->AttachLinks = NewHash(1,NULL);
		Put(Msg->AttachLinks, SKEY(Mime->PartNum), Mime, reference_free_handler);
		if ((strcasecmp(ChrPtr(Mime->ContentType), "application/octet-stream") == 0) && 
		    (StrLength(Mime->FileName) > 0)) {
			FlushStrBuf(Mime->ContentType);
			StrBufAppendBufPlain(Mime->ContentType,
					     GuessMimeByFilename(SKEY(Mime->FileName)),
					     -1, 0);
		}
	}
}

void tmplput_MAIL_SUMM_NATTACH(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendPrintf(Target, "%ld", GetCount(Msg->Attachments));
}


void examine_hnod(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	FreeStrBuf(&Msg->hnod);
	Msg->hnod = NewStrBufPlain(NULL, StrLength(HdrLine));
	StrBuf_RFC822_to_Utf8(Msg->hnod, HdrLine, WC->DefaultCharset, FoundCharset);
}
void tmplput_MAIL_SUMM_H_NODE(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->hnod, 0);
}
int Conditional_MAIL_SUMM_H_NODE(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return StrLength(Msg->hnod) > 0;
}



void examine_text(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->MsgBody->Data = NewStrBufPlain(NULL, SIZ);
}

void examine_msg4_partnum(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->MsgBody->PartNum = NewStrBufDup(HdrLine);
	StrBufTrim(Msg->MsgBody->PartNum);
}

void examine_content_lengh(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->MsgBody->length = StrTol(HdrLine);
	Msg->MsgBody->size_known = 1;
}

void examine_content_type(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	void *vHdr;
	headereval *Hdr;
	StrBuf *Token;
	StrBuf *Value;
	const char* sem;
	const char *eq;
	int len;
	StrBufTrim(HdrLine);
	Msg->MsgBody->ContentType = NewStrBufDup(HdrLine);
	sem = strchr(ChrPtr(HdrLine), ';');

	if (sem != NULL) {
		Token = NewStrBufPlain(NULL, StrLength(HdrLine));
		Value = NewStrBufPlain(NULL, StrLength(HdrLine));
		len = sem - ChrPtr(HdrLine);
		StrBufCutAt(Msg->MsgBody->ContentType, len, NULL);
		while (sem != NULL) {
			while (isspace(*(sem + 1)))
				sem ++;
			StrBufCutLeft(HdrLine, sem - ChrPtr(HdrLine));
			sem = strchr(ChrPtr(HdrLine), ';');
			if (sem != NULL)
				len = sem - ChrPtr(HdrLine);
			else
				len = StrLength(HdrLine);
			FlushStrBuf(Token);
			FlushStrBuf(Value);
			StrBufAppendBufPlain(Token, ChrPtr(HdrLine), len, 0);
			eq = strchr(ChrPtr(Token), '=');
			if (eq != NULL) {
				len = eq - ChrPtr(Token);
				StrBufAppendBufPlain(Value, eq + 1, StrLength(Token) - len - 1, 0); 
				StrBufCutAt(Token, len, NULL);
				StrBufTrim(Value);
			}
			StrBufTrim(Token);

			if (GetHash(MsgHeaderHandler, SKEY(Token), &vHdr) &&
			    (vHdr != NULL)) {
				Hdr = (headereval*)vHdr;
				Hdr->evaluator(Msg, Value, FoundCharset);
			}
			else lprintf(1, "don't know how to handle content type sub-header[%s]\n", ChrPtr(Token));
		}
		FreeStrBuf(&Token);
		FreeStrBuf(&Value);
	}
}

void tmplput_MAIL_SUMM_N(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendPrintf(Target, "%ld", Msg->msgnum);
}



int Conditional_MAIL_MIME_ALL(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return GetCount(Msg->Attachments) > 0;
}

int Conditional_MAIL_MIME_SUBMESSAGES(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return GetCount(Msg->Submessages) > 0;
}

int Conditional_MAIL_MIME_ATTACHLINKS(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return GetCount(Msg->AttachLinks) > 0;
}

int Conditional_MAIL_MIME_ATTACH(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return GetCount(Msg->AllAttach) > 0;
}

void tmplput_QUOTED_MAIL_BODY(StrBuf *Target, WCTemplputParams *TP)
{
	const StrBuf *Mime;
        long MsgNum;
	StrBuf *Buf;

	MsgNum = LBstr(TKEY(0));
	Buf = NewStrBuf();
	read_message(Buf, HKEY("view_message_replyquote"), MsgNum, NULL, &Mime);
	StrBufAppendTemplate(Target, TP, Buf, 1);
	FreeStrBuf(&Buf);
}

void tmplput_EDIT_MAIL_BODY(StrBuf *Target, WCTemplputParams *TP)
{
	const StrBuf *Mime;
        long MsgNum;
	StrBuf *Buf;

	MsgNum = LBstr(TKEY(0));
	Buf = NewStrBuf();
	read_message(Buf, HKEY("view_message_edit"), MsgNum, NULL, &Mime);
	StrBufAppendTemplate(Target, TP, Buf, 1);
	FreeStrBuf(&Buf);
}

void tmplput_EDIT_WIKI_BODY(StrBuf *Target, WCTemplputParams *TP)	// FIXME
{
	const StrBuf *Mime;
        long msgnum;
	StrBuf *Buf;

	msgnum = locate_message_by_uid(BSTR("wikipage"));
	if (msgnum >= 0L) {
		Buf = NewStrBuf();
		read_message(Buf, HKEY("view_message_wikiedit"), msgnum, NULL, &Mime);
		StrBufAppendTemplate(Target, TP, Buf, 1);
		FreeStrBuf(&Buf);
	}
}

void tmplput_MAIL_BODY(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	StrBufAppendTemplate(Target, TP, Msg->MsgBody->Data, 0);
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
	const char *ptr, *pte;
	const char *BufPtr = NULL;
	StrBuf *Line;
	StrBuf *Line1;
	StrBuf *Line2;
	StrBuf *Target;

	int bn = 0;
	int bq = 0;
	int i;
	long len;
#ifdef HAVE_ICONV
	StrBuf *cs = NULL;
	int ConvertIt = 1;
	iconv_t ic = (iconv_t)(-1) ;
#endif

	if ((StrLength(Mime->Data) == 0) && (Mime->length > 0)) {
		FreeStrBuf(&Mime->Data);
		MimeLoadData(Mime);
	}

#ifdef HAVE_ICONV
	if (ConvertIt) {
		if (StrLength(Mime->Charset) != 0)
			cs = Mime->Charset;
		else if (StrLength(FoundCharset) > 0)
			cs = FoundCharset;
		else if (StrLength(WC->DefaultCharset) > 0)
			cs = WC->DefaultCharset;
		if (cs == NULL) {
			ConvertIt = 0;
		}
		else if (!strcasecmp(ChrPtr(cs), "utf-8")) {
			ConvertIt = 0;
		}
		else if (!strcasecmp(ChrPtr(cs), "us-ascii")) {
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
	Line = NewStrBufPlain(NULL, SIZ);
	Line1 = NewStrBufPlain(NULL, SIZ);
	Line2 = NewStrBufPlain(NULL, SIZ);
	Target = NewStrBufPlain(NULL, StrLength(Mime->Data));

	if (StrLength(Mime->Data) > 0) 
		do 
		{
			StrBufSipLine(Line, Mime->Data, &BufPtr);
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
		
			if (StrLength(Line) == 0) {
				StrBufAppendBufPlain(Target, HKEY("<tt></tt><br />\n"), 0);
				continue;
			}

			for (i = bn; i < bq; i++)				
				StrBufAppendBufPlain(Target, HKEY("<blockquote>"), 0);
			for (i = bq; i < bn; i++)				
				StrBufAppendBufPlain(Target, HKEY("</blockquote>"), 0);
#ifdef HAVE_ICONV
			if (ConvertIt) {
				StrBufConvert(Line, Line1, &ic);
			}
#endif
			StrBufAppendBufPlain(Target, HKEY("<tt>"), 0);
			UrlizeText(Line1, Line, Line2);

			StrEscAppend(Target, Line1, NULL, 0, 0);
			StrBufAppendBufPlain(Target, HKEY("</tt><br />\n"), 0);
			bn = bq;
		}
	while ((BufPtr != StrBufNOTNULL) &&
	       (BufPtr != NULL));

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
	FlushStrBuf(Mime->ContentType);
	StrBufAppendBufPlain(Mime->ContentType, HKEY("text/html"), 0);
	FlushStrBuf(Mime->Charset);
	StrBufAppendBufPlain(Mime->Charset, HKEY("UTF-8"), 0);
	FreeStrBuf(&Line);
	FreeStrBuf(&Line1);
	FreeStrBuf(&Line2);
}

void render_MAIL_html(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	StrBuf *Buf;

	if (StrLength(Mime->Data) == 0)
		return;

	Buf = NewStrBufPlain(NULL, StrLength(Mime->Data));

	/* HTML is fun, but we've got to strip it first */
	output_html(ChrPtr(Mime->Charset), 
		    (WC->wc_view == VIEW_WIKI ? 1 : 0), 
		    Mime->msgnum,
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


HashList *iterate_get_mime_All(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return Msg->Attachments;
}
HashList *iterate_get_mime_Submessages(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return Msg->Submessages;
}
HashList *iterate_get_mime_AttachLinks(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return Msg->AttachLinks;
}
HashList *iterate_get_mime_Attachments(StrBuf *Target, WCTemplputParams *TP)
{
	message_summary *Msg = (message_summary*) CTX;
	return Msg->AllAttach;
}

void tmplput_MIME_Name(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	StrBufAppendTemplate(Target, TP, mime->Name, 0);
}

void tmplput_MIME_FileName(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	StrBufAppendTemplate(Target, TP, mime->FileName, 0);
}

void tmplput_MIME_PartNum(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	StrBufAppendTemplate(Target, TP, mime->PartNum, 0);
}

void tmplput_MIME_MsgNum(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	StrBufAppendPrintf(Target, "%ld", mime->msgnum);
}

void tmplput_MIME_Disposition(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	StrBufAppendTemplate(Target, TP, mime->Disposition, 0);
}

void tmplput_MIME_ContentType(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	StrBufAppendTemplate(Target, TP, mime->ContentType, 0);
}

void examine_charset(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset)
{
	Msg->MsgBody->Charset = NewStrBufDup(HdrLine);
}

void tmplput_MIME_Charset(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	StrBufAppendTemplate(Target, TP, mime->Charset, 0);
}

void tmplput_MIME_Data(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	if (mime->Renderer != NULL)
		mime->Renderer->f(mime, NULL, NULL);
	StrBufAppendTemplate(Target, TP, mime->Data, 0);
	/* TODO: check whether we need to load it now? */
}

void tmplput_MIME_LoadData(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;	
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	wc_mime_attachment *att;
	
	if ( (!strcasecmp(ChrPtr(mime->Disposition), "inline"))||
	     (!strcasecmp(ChrPtr(mime->Disposition), "attachment")) ) 
	{
 		
 		int n;
 		char N[64];
 		/* steal this mime part... */
 		att = malloc(sizeof(wc_mime_attachment));
 		memcpy(att, mime, sizeof(wc_mime_attachment));
 		memset(mime, 0, sizeof(wc_mime_attachment));

		if (att->Data == NULL) 
			MimeLoadData(att);

 		if (WCC->attachments == NULL)
 			WCC->attachments = NewHash(1, NULL);
 		/* And add it to the list. */
 		n = snprintf(N, sizeof N, "%d", GetCount(WCC->attachments) + 1);
 		Put(WCC->attachments, N, n, att, DestroyMime);
 	}
}

void tmplput_MIME_Length(StrBuf *Target, WCTemplputParams *TP)
{
	wc_mime_attachment *mime = (wc_mime_attachment*) CTX;
	StrBufAppendPrintf(Target, "%ld", mime->length);
}

/* startmsg is an index within the message list.
 * starting_from is the Citadel message number to be supplied to a "MSGS GT" operation
 */
long DrawMessageDropdown(StrBuf *Selector, long maxmsgs, long startmsg, int nMessages, long starting_from)
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
	TmpBuf = NewStrBufPlain(NULL, SIZ);
	At = GetNewHashPos(WCC->summ, nMessages);
	nItems = GetCount(WCC->summ);
	ret = nMessages;
	vector[0] = 7;
	vector[2] = 1;
	vector[1] = startmsg;
	vector[3] = 0;
	vector[7] = starting_from;

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
		
		/*
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

HashList *iterate_get_registered_Attachments(StrBuf *Target, WCTemplputParams *TP)
{
	return WC->attachments;
}

void servcmd_do_search(char *buf, long bufsize)
{
	snprintf(buf, bufsize, "MSGS SEARCH|%s", bstr("query"));
}

void servcmd_headers(char *buf, long bufsize)
{
	snprintf(buf, bufsize, "MSGS ALL");
}

void servcmd_readfwd(char *buf, long bufsize)
{
	snprintf(buf, bufsize, "MSGS ALL");
}

void servcmd_readgt(char *buf, long bufsize)
{
	snprintf(buf, bufsize, "MSGS GT|%s", bstr("gt"));
}

void servcmd_readnew(char *buf, long bufsize)
{
	snprintf(buf, bufsize, "MSGS NEW");
}

void servcmd_readold(char *buf, long bufsize)
{
	snprintf(buf, bufsize, "MSGS OLD");
}


readloop_struct rlid[] = {
	{ {HKEY("do_search")}, servcmd_do_search},
	{ {HKEY("headers")},   servcmd_headers},
	{ {HKEY("readfwd")},   servcmd_readfwd},
	{ {HKEY("readnew")},   servcmd_readnew},
	{ {HKEY("readold")},   servcmd_readold},
	{ {HKEY("readgt")},    servcmd_readgt}
};

/* Spit out the new summary view. This is basically a static page, so clients can cache the layout, all the dirty work is javascript :) */
void new_summary_view(void) {
	DoTemplate(HKEY("msg_listview"),NULL,&NoCtx);
	DoTemplate(HKEY("trailing"),NULL,&NoCtx);
}


int mailview_GetParamsGetServerCall(SharedMessageStatus *Stat, 
				    void **ViewSpecific, 
				    long oper, 
				    char *cmd, 
				    long len)
{
	if (!WC->is_ajax) {
		new_summary_view();
		return 200;
	} else {
		Stat->defaultsortorder = 2;
		Stat->sortit = 1;
		Stat->load_seen = 1;
		/* Generally using maxmsgs|startmsg is not required
		   in mailbox view, but we have a 'safemode' for clients
		   (*cough* Exploder) that simply can't handle too many */
		if (havebstr("maxmsgs"))  Stat->maxmsgs  = ibstr("maxmsgs");
		else                      Stat->maxmsgs  = 9999999;
		if (havebstr("startmsg")) Stat->startmsg = lbstr("startmsg");
		snprintf(cmd, len, "MSGS %s|%s||1",
			 (oper == do_search) ? "SEARCH" : "ALL",
			 (oper == do_search) ? bstr("query") : ""
			);
	}
	return 200;
}

int mailview_RenderView_or_Tail(SharedMessageStatus *Stat, 
				void **ViewSpecific, 
				long oper)
{
	WCTemplputParams SubTP;

	DoTemplate(HKEY("mailsummary_json"),NULL, &SubTP);
	return 0;
}

int mailview_Cleanup(void **ViewSpecific)
{
	/* Note: wDumpContent() will output one additional </div> tag. */
	/* We ought to move this out into template */
	if (WC->is_ajax) 
		end_burst();
	else
		wDumpContent(1);
	return 0;
}



typedef struct _bbsview_stuct {
	StrBuf *BBViewToolBar;
	StrBuf *MessageDropdown;
	long *displayed_msgs;
	int a;
}bbsview_struct;

int bbsview_GetParamsGetServerCall(SharedMessageStatus *Stat, 
				   void **ViewSpecific, 
				   long oper, 
				   char *cmd, 
				   long len)
{
	bbsview_struct *VS;

	VS = (bbsview_struct*) malloc(sizeof(bbsview_struct));
	memset(VS, 0, sizeof(bbsview_struct));
	*ViewSpecific = (void*)VS;
	Stat->defaultsortorder = 1;
	Stat->startmsg = -1;
	Stat->sortit = 1;
	
	rlid[oper].cmd(cmd, len);
	
	if (havebstr("maxmsgs"))
		Stat->maxmsgs = ibstr("maxmsgs");
	if (Stat->maxmsgs == 0) Stat->maxmsgs = DEFAULT_MAXMSGS;
	
	if (havebstr("startmsg")) {
		Stat->startmsg = lbstr("startmsg");
	}
	if (lbstr("SortOrder") == 2) {
		Stat->reverse = 1;
		Stat->num_displayed = -DEFAULT_MAXMSGS;
	}
	else {
		Stat->reverse = 0;
		Stat->num_displayed = DEFAULT_MAXMSGS;
	}

	return 200;
}

int bbsview_PrintViewHeader(SharedMessageStatus *Stat, void **ViewSpecific)
{
	bbsview_struct *VS;
	WCTemplputParams SubTP;

	VS = (bbsview_struct*)*ViewSpecific;

	VS->BBViewToolBar = NewStrBufPlain(NULL, SIZ);
	VS->MessageDropdown = NewStrBufPlain(NULL, SIZ);

	/*** startmsg->maxmsgs = **/DrawMessageDropdown(VS->MessageDropdown, 
							Stat->maxmsgs, 
							Stat->startmsg,
							Stat->num_displayed, 
							Stat->lowest_found-1);
	if (Stat->num_displayed < 0) {
		Stat->startmsg += Stat->maxmsgs;
		if (Stat->num_displayed != Stat->maxmsgs)				
			Stat->maxmsgs = abs(Stat->maxmsgs) + 1;
		else
			Stat->maxmsgs = abs(Stat->maxmsgs);

	}
	if (Stat->nummsgs > 0) {
		memset(&SubTP, 0, sizeof(WCTemplputParams));
		SubTP.Filter.ContextType = CTX_STRBUF;
		SubTP.Context = VS->MessageDropdown;
		DoTemplate(HKEY("msg_listselector_top"), VS->BBViewToolBar, &SubTP);
		StrBufAppendBuf(WC->WBuf, VS->BBViewToolBar, 0);
		FlushStrBuf(VS->BBViewToolBar);
	}
	return 200;
}

int bbsview_LoadMsgFromServer(SharedMessageStatus *Stat, 
			      void **ViewSpecific, 
			      message_summary* Msg, 
			      int is_new, 
			      int i)
{
	bbsview_struct *VS;

	VS = (bbsview_struct*)*ViewSpecific;
	if (VS->displayed_msgs == NULL) {
		VS->displayed_msgs = malloc(sizeof(long) *
					    ((Stat->maxmsgs < Stat->nummsgs) ? 
					     Stat->maxmsgs + 1 : 
					     Stat->nummsgs + 1));
	}
	if ((i >= Stat->startmsg) && (i < Stat->startmsg + Stat->maxmsgs)) {
		VS->displayed_msgs[Stat->num_displayed] = Msg->msgnum;
		Stat->num_displayed++;
	}
	return 200;
}


int bbsview_RenderView_or_Tail(SharedMessageStatus *Stat, 
			       void **ViewSpecific, 
			       long oper)
{
	wcsession *WCC = WC;
	bbsview_struct *VS;
	WCTemplputParams SubTP;
	const StrBuf *Mime;

	VS = (bbsview_struct*)*ViewSpecific;
	if (Stat->nummsgs == 0) {
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
	}
	else 
	{
		if (VS->displayed_msgs != NULL) {
			/* if we do a split bbview in the future, begin messages div here */
			int a;/// todo	
			for (a=0; a < Stat->num_displayed; ++a) {
				read_message(WCC->WBuf, HKEY("view_message"), VS->displayed_msgs[a], NULL, &Mime);
			}
			
			/* if we do a split bbview in the future, end messages div here */
			
			free(VS->displayed_msgs);
			VS->displayed_msgs = NULL;
		}
		memset(&SubTP, 0, sizeof(WCTemplputParams));
		SubTP.Filter.ContextType = CTX_STRBUF;
		SubTP.Context = VS->MessageDropdown;
		DoTemplate(HKEY("msg_listselector_bottom"), VS->BBViewToolBar, &SubTP);
		StrBufAppendBuf(WCC->WBuf, VS->BBViewToolBar, 0);
	}
	return 0;

}


int bbsview_Cleanup(void **ViewSpecific)
{
	bbsview_struct *VS;

	VS = (bbsview_struct*)*ViewSpecific;
	end_burst();
	FreeStrBuf(&VS->BBViewToolBar);
	FreeStrBuf(&VS->MessageDropdown);
	free(VS);
	return 0;
}

void 
InitModule_MSGRENDERERS
(void)
{
	RegisterReadLoopHandlerset(
		VIEW_MAILBOX,
		mailview_GetParamsGetServerCall,
		NULL, /// TODO: is this right?
		NULL, //// ""
		mailview_RenderView_or_Tail,
		mailview_Cleanup);

	RegisterReadLoopHandlerset(
		VIEW_BBS,
		bbsview_GetParamsGetServerCall,
		bbsview_PrintViewHeader,
		bbsview_LoadMsgFromServer,
		bbsview_RenderView_or_Tail,
		bbsview_Cleanup);

	RegisterSortFunc(HKEY("date"), 
			 NULL, 0,
			 summcmp_date,
			 summcmp_rdate,
			 groupchange_date,
			 CTX_MAILSUM);
	RegisterSortFunc(HKEY("subject"), 
			 NULL, 0,
			 summcmp_subj,
			 summcmp_rsubj,
			 groupchange_subj,
			 CTX_MAILSUM);
	RegisterSortFunc(HKEY("sender"),
			 NULL, 0,
			 summcmp_sender,
			 summcmp_rsender,
			 groupchange_sender,
			 CTX_MAILSUM);

	RegisterNamespace("SUMM:COUNT", 0, 0, tmplput_SUMM_COUNT, CTX_NONE);
	/* iterate over all known mails in WC->summ */
	RegisterIterator("MAIL:SUMM:MSGS", 0, NULL, iterate_get_mailsumm_All,
			 NULL,NULL, CTX_MAILSUM, CTX_NONE, IT_NOFLAG);

	RegisterNamespace("MAIL:SUMM:DATEBRIEF", 0, 0, tmplput_MAIL_SUMM_DATE_BRIEF, CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:DATEFULL", 0, 0, tmplput_MAIL_SUMM_DATE_FULL, CTX_MAILSUM);
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
	RegisterNamespace("MAIL:SUMM:REFIDS", 0, 1, tmplput_MAIL_SUMM_REFIDS,  CTX_MAILSUM);
	RegisterNamespace("MAIL:SUMM:INREPLYTO", 0, 2, tmplput_MAIL_SUMM_INREPLYTO,  CTX_MAILSUM);
	RegisterNamespace("MAIL:BODY", 0, 2, tmplput_MAIL_BODY,  CTX_MAILSUM);
	RegisterNamespace("MAIL:QUOTETEXT", 1, 2, tmplput_QUOTED_MAIL_BODY,  CTX_NONE);
	RegisterNamespace("MAIL:EDITTEXT", 1, 2, tmplput_EDIT_MAIL_BODY,  CTX_NONE);
	RegisterNamespace("MAIL:EDITWIKI", 1, 2, tmplput_EDIT_WIKI_BODY,  CTX_NONE);
	RegisterConditional(HKEY("COND:MAIL:SUMM:RFCA"), 0, Conditional_MAIL_SUMM_RFCA,  CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:SUMM:CCCC"), 0, Conditional_MAIL_SUMM_CCCC,  CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:SUMM:UNREAD"), 0, Conditional_MAIL_SUMM_UNREAD, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:SUMM:H_NODE"), 0, Conditional_MAIL_SUMM_H_NODE, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:SUMM:OTHERNODE"), 0, Conditional_MAIL_SUMM_OTHERNODE, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:SUMM:SUBJECT"), 0, Conditional_MAIL_SUMM_SUBJECT, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:ANON"), 0, Conditional_ANONYMOUS_MESSAGE, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:TO"), 0, Conditional_MAIL_SUMM_TO, CTX_MAILSUM);	
	RegisterConditional(HKEY("COND:MAIL:SUBJ"), 0, Conditional_MAIL_SUMM_SUBJ, CTX_MAILSUM);	

	/* do we have mimetypes to iterate over? */
	RegisterConditional(HKEY("COND:MAIL:MIME:ATTACH"), 0, Conditional_MAIL_MIME_ALL, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:MIME:ATTACH:SUBMESSAGES"), 0, Conditional_MAIL_MIME_SUBMESSAGES, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:MIME:ATTACH:LINKS"), 0, Conditional_MAIL_MIME_ATTACHLINKS, CTX_MAILSUM);
	RegisterConditional(HKEY("COND:MAIL:MIME:ATTACH:ATT"), 0, Conditional_MAIL_MIME_ATTACH, CTX_MAILSUM);
	RegisterIterator("MAIL:MIME:ATTACH", 0, NULL, iterate_get_mime_All, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_MAILSUM, IT_NOFLAG);
	RegisterIterator("MAIL:MIME:ATTACH:SUBMESSAGES", 0, NULL, iterate_get_mime_Submessages, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_MAILSUM, IT_NOFLAG);
	RegisterIterator("MAIL:MIME:ATTACH:LINKS", 0, NULL, iterate_get_mime_AttachLinks, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_MAILSUM, IT_NOFLAG);
	RegisterIterator("MAIL:MIME:ATTACH:ATT", 0, NULL, iterate_get_mime_Attachments, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_MAILSUM, IT_NOFLAG);

	/* Parts of a mime attachent */
	RegisterNamespace("MAIL:MIME:NAME", 0, 2, tmplput_MIME_Name, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:FILENAME", 0, 2, tmplput_MIME_FileName, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:PARTNUM", 0, 2, tmplput_MIME_PartNum, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:MSGNUM", 0, 2, tmplput_MIME_MsgNum, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:DISPOSITION", 0, 2, tmplput_MIME_Disposition, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:CONTENTTYPE", 0, 2, tmplput_MIME_ContentType, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:CHARSET", 0, 2, tmplput_MIME_Charset, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:LENGTH", 0, 2, tmplput_MIME_Length, CTX_MIME_ATACH);
	RegisterNamespace("MAIL:MIME:DATA", 0, 2, tmplput_MIME_Data, CTX_MIME_ATACH);
	/* load the actual attachment into WC->attachments; no output!!! */
	RegisterNamespace("MAIL:MIME:LOADDATA", 0, 0, tmplput_MIME_LoadData, CTX_MIME_ATACH);

	/* iterate the WC->attachments; use the above tokens for their contents */
	RegisterIterator("MSG:ATTACHNAMES", 0, NULL, iterate_get_registered_Attachments, 
			 NULL, NULL, CTX_MIME_ATACH, CTX_NONE, IT_NOFLAG);

	/* mime renderers translate an attachment into webcit viewable html text */
	RegisterMimeRenderer(HKEY("message/rfc822"), render_MAIL);
	RegisterMimeRenderer(HKEY("text/vnote"), render_MIME_VNote);
	RegisterMimeRenderer(HKEY("text/x-vcard"), render_MIME_VCard);
	RegisterMimeRenderer(HKEY("text/vcard"), render_MIME_VCard);
	RegisterMimeRenderer(HKEY("text/calendar"), render_MIME_ICS);
	RegisterMimeRenderer(HKEY("application/ics"), render_MIME_ICS);
	RegisterMimeRenderer(HKEY("text/x-citadel-variformat"), render_MAIL_variformat);
	RegisterMimeRenderer(HKEY("text/plain"), render_MAIL_text_plain);
	RegisterMimeRenderer(HKEY("text"), render_MAIL_text_plain);
	RegisterMimeRenderer(HKEY("text/html"), render_MAIL_html);
	RegisterMimeRenderer(HKEY(""), render_MAIL_UNKNOWN);

	/* these headers are citserver replies to MSG4 and friends. one evaluator for each */
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
	/* these are the content-type headers we get infront of a message; put it into the same hash since it doesn't clash. */
	RegisterMsgHdr(HKEY("X-Citadel-MSG4-Partnum"), examine_msg4_partnum, 0);
	RegisterMsgHdr(HKEY("Content-type"), examine_content_type, 0);
	RegisterMsgHdr(HKEY("Content-length"), examine_content_lengh, 0);
	RegisterMsgHdr(HKEY("Content-transfer-encoding"), examine_content_encoding, 0); /* do we care? */
	RegisterMsgHdr(HKEY("charset"), examine_charset, 0);

	/* Don't care about these... */
	RegisterMsgHdr(HKEY("pref"), examine_pref, 0);
	RegisterMsgHdr(HKEY("suff"), examine_suff, 0);
	RegisterMsgHdr(HKEY("path"), examine_path, 0);
}

void 
ServerStartModule_MSGRENDERERS
(void)
{
	MsgHeaderHandler = NewHash(1, NULL);
	MimeRenderHandler = NewHash(1, NULL);
	ReadLoopHandler = NewHash(1, NULL);
}

void 
ServerShutdownModule_MSGRENDERERS
(void)
{
	DeleteHash(&MsgHeaderHandler);
	DeleteHash(&MimeRenderHandler);
	DeleteHash(&ReadLoopHandler);
}



void 
SessionDestroyModule_MSGRENDERERS
(wcsession *sess)
{
	DeleteHash(&sess->attachments);
}
