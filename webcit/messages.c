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

#define SUBJ_COL_WIDTH_PCT		50	/**< Mailbox view column width */
#define SENDER_COL_WIDTH_PCT		30	/**< Mailbox view column width */
#define DATE_PLUS_BUTTONS_WIDTH_PCT	20	/**< Mailbox view column width */



void display_enter(void);
int longcmp_r(const void *s1, const void *s2);
int summcmp_subj(const void *s1, const void *s2);
int summcmp_rsubj(const void *s1, const void *s2);
int summcmp_sender(const void *s1, const void *s2);
int summcmp_rsender(const void *s1, const void *s2);
int summcmp_date(const void *s1, const void *s2);
int summcmp_rdate(const void *s1, const void *s2);

/*----------------------------------------------------------------------------*/


typedef void (*MsgPartEvaluatorFunc)(message_summary *Sum, StrBuf *Buf);



/*----------------------------------------------------------------------------*/



/*
 * I wanna SEE that message!
 *
 * msgnum		Message number to display
 * printable_view	Nonzero to display a printable view
 * section		Optional for encapsulated message/rfc822 submessage
 */
int read_message(StrBuf *Target, const char *tmpl, long tmpllen, long msgnum, int printable_view, const StrBuf *PartNum) {
	StrBuf *Buf;
	StrBuf *HdrToken;
	StrBuf *FoundCharset;
	HashPos  *it;
	void *vMime;
	message_summary *Msg = NULL;
	headereval *Hdr;
	void *vHdr;
	char buf[SIZ];
//	char mime_submessages[256] = "";
	char reply_references[1024] = "";
	int Done = 0;
	int state=0;
	long len;
	const char *Key;

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
			
			lprintf(1, ":: [%s] = [%s]\n", ChrPtr(HdrToken), ChrPtr(Buf));
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
					lprintf(1, ":: [%s] = [%s]\n", ChrPtr(HdrToken), ChrPtr(Buf));
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
					/// todo: check next line, if not 000, append following lines
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
	Put(Msg->AllAttach, SKEY(Msg->MsgBody->PartNum), Msg->MsgBody, DestroyMime);
	
	/* strip the bare contenttype, so we ommit charset etc. */
	StrBufExtract_token(Buf, Msg->MsgBody->ContentType, 0, ';');
	StrBufTrim(Buf);
	if (GetHash(MimeRenderHandler, SKEY(Buf), &vHdr) &&
	    (vHdr != NULL)) {
		RenderMimeFunc Render;
		Render = (RenderMimeFunc)vHdr;
		Render(Msg->MsgBody, NULL, FoundCharset);
	}

	if (StrLength(Msg->reply_references)> 0) {
		/* Trim down excessively long lists of thread references.  We eliminate the
		 * second one in the list so that the thread root remains intact.
		 */
		int rrtok = num_tokens(ChrPtr(Msg->reply_references), '|');
		int rrlen = StrLength(Msg->reply_references);
		if ( ((rrtok >= 3) && (rrlen > 900)) || (rrtok > 10) ) {
			remove_token(reply_references, 1, '|');////todo
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
		    (strcasecmp(ChrPtr(Msg->OtherNode), serv_info.serv_nodename)) &&
		    (strcasecmp(ChrPtr(Msg->OtherNode), serv_info.serv_humannode)) ) 
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
	it = GetNewHashPos();
	while (GetNextHashPos(Msg->AllAttach, it, &len, &Key, &vMime) && 
	       (vMime != NULL)) {
		wc_mime_attachment *Mime = (wc_mime_attachment*) vMime;
		evaluate_mime_part(Msg, Mime);
	}
	DeleteHashPos(&it);

	DoTemplate(tmpl, tmpllen, Target, Msg, CTX_MAILSUM);

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
	const StrBuf *Tmpl = sbstr("template");

	msgnum = StrTol(WC->UrlFragment1);
	if (StrLength(Tmpl) > 0) 
		read_message(WC->WBuf, SKEY(Tmpl), msgnum, 0, NULL);
	else 
		read_message(WC->WBuf, HKEY("view_message"), msgnum, 0, NULL);
}


/*
 * Printable view of a message
 *
 * msgnum_as_string == Message number, as a string instead of as a long int
 */
void print_message(void) {
	long msgnum = 0L;

	msgnum = StrTol(WC->UrlFragment1);
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
  msgnum = StrTol(WC->UrlFragment1);
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

	msgnum = StrTol(WC->UrlFragment1);
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



/**
 * \brief Read message in simple, JavaScript-embeddable form for 'forward'
 *	or 'reply quoted' operations.
 *
 * NOTE: it is VITALLY IMPORTANT that we output no single-quotes or linebreaks
 *       in this function.  Doing so would throw a JavaScript error in the
 *       'supplied text' argument to the editor.
 *
 * \param msgnum Message number of the message we want to quote
 * \param forward_attachments Nonzero if we want attachments to be forwarded
 */
void pullquote_message(long msgnum, int forward_attachments, int include_headers) {
	struct wcsession *WCC = WC;
	char buf[SIZ];
	char mime_partnum[256];
	char mime_filename[256];
	char mime_content_type[256];
	char mime_charset[256];
	char mime_disposition[256];
	int mime_length;
	char *attachments = NULL;
	char *ptr = NULL;
	int num_attachments = 0;
	wc_attachment *att;
	char m_subject[1024];
	char from[256];
	char node[256];
	char rfca[256];
	char to[256];
	char reply_to[512];
	char now[256];
	int format_type = 0;
	int nhdr = 0;
	int bq = 0;
	int i = 0;
#ifdef HAVE_ICONV
	iconv_t ic = (iconv_t)(-1) ;
	char *ibuf;		   /**< Buffer of characters to be converted */
	char *obuf;		   /**< Buffer for converted characters      */
	size_t ibuflen;	   /**< Length of input buffer	       */
	size_t obuflen;	   /**< Length of output buffer	      */
	char *osav;		   /**< Saved pointer to output buffer       */
#endif

	strcpy(from, "");
	strcpy(node, "");
	strcpy(rfca, "");
	strcpy(reply_to, "");
	strcpy(mime_content_type, "text/plain");
	strcpy(mime_charset, "us-ascii");

	serv_printf("MSG4 %ld", msgnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		wprintf(_("ERROR:"));
		wprintf("%s<br />", &buf[4]);
		return;
	}

	strcpy(m_subject, "");

	while (serv_getln(buf, sizeof buf), strcasecmp(buf, "text")) {
		if (!strcmp(buf, "000")) {
			wprintf("%s (3)", _("unexpected end of message"));
			return;
		}
		if (include_headers) {
			if (!strncasecmp(buf, "nhdr=yes", 8))
				nhdr = 1;
			if (nhdr == 1)
				buf[0] = '_';
			if (!strncasecmp(buf, "type=", 5))
				format_type = atoi(&buf[5]);
			if (!strncasecmp(buf, "from=", 5)) {
				strcpy(from, &buf[5]);
				wprintf(_("from "));
				utf8ify_rfc822_string(from);
				msgescputs(from);
			}
			if (!strncasecmp(buf, "subj=", 5)) {
				strcpy(m_subject, &buf[5]);
			}
			if ((!strncasecmp(buf, "hnod=", 5))
			    && (strcasecmp(&buf[5], serv_info.serv_humannode))) {
				wprintf("(%s) ", &buf[5]);
			}
			if ((!strncasecmp(buf, "room=", 5))
			    && (strcasecmp(&buf[5], WC->wc_roomname))
			    && (!IsEmptyStr(&buf[5])) ) {
				wprintf(_("in "));
				wprintf("%s&gt; ", &buf[5]);
			}
			if (!strncasecmp(buf, "rfca=", 5)) {
				strcpy(rfca, &buf[5]);
				wprintf("&lt;");
				msgescputs(rfca);
				wprintf("&gt; ");
			}
			if (!strncasecmp(buf, "node=", 5)) {
				strcpy(node, &buf[5]);
				if ( ((WC->room_flags & QR_NETWORK)
				|| ((strcasecmp(&buf[5], serv_info.serv_nodename)
				&& (strcasecmp(&buf[5], serv_info.serv_fqdn)))))
				&& (IsEmptyStr(rfca))
				) {
					wprintf("@%s ", &buf[5]);
				}
			}
			if (!strncasecmp(buf, "rcpt=", 5)) {
				wprintf(_("to "));
				strcpy(to, &buf[5]);
				utf8ify_rfc822_string(to);
				wprintf("%s ", to);
			}
			if (!strncasecmp(buf, "time=", 5)) {
				webcit_fmt_date(now, atol(&buf[5]), 0);
				wprintf("%s ", now);
			}
		}

		/**
		 * Save attachment info for later.  We can't start downloading them
		 * yet because we're in the middle of a server transaction.
		 */
		if (!strncasecmp(buf, "part=", 5)) {
			ptr = malloc( (strlen(buf) + ((attachments != NULL) ? strlen(attachments) : 0)) ) ;
			if (ptr != NULL) {
				++num_attachments;
				sprintf(ptr, "%s%s\n",
					((attachments != NULL) ? attachments : ""),
					&buf[5]
				);
				free(attachments);
				attachments = ptr;
				lprintf(9, "attachments=<%s>\n", attachments);
			}
		}

	}

	if (include_headers) {
		wprintf("<br>");

		utf8ify_rfc822_string(m_subject);
		if (!IsEmptyStr(m_subject)) {
			wprintf(_("Subject:"));
			wprintf(" ");
			msgescputs(m_subject);
			wprintf("<br />");
		}

		/**
	 	 * Begin body
	 	 */
		wprintf("<br />");
	}

	/**
	 * Learn the content type
	 */
	strcpy(mime_content_type, "text/plain");
	while (serv_getln(buf, sizeof buf), (!IsEmptyStr(buf))) {
		if (!strcmp(buf, "000")) {
			wprintf("%s (4)", _("unexpected end of message"));
			goto ENDBODY;
		}
		if (!strncasecmp(buf, "Content-type: ", 14)) {
			int len;
			safestrncpy(mime_content_type, &buf[14],
				sizeof(mime_content_type));
			for (i=0; i<strlen(mime_content_type); ++i) {
				if (!strncasecmp(&mime_content_type[i], "charset=", 8)) {
					safestrncpy(mime_charset, &mime_content_type[i+8],
						sizeof mime_charset);
				}
			}
			len = strlen(mime_content_type);
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
		}
	}

	/** Set up a character set conversion if we need to (and if we can) */
#ifdef HAVE_ICONV
	if ( (strcasecmp(mime_charset, "us-ascii"))
	   && (strcasecmp(mime_charset, "UTF-8"))
	   && (strcasecmp(mime_charset, ""))
	) {
		ctdl_iconv_open("UTF-8", mime_charset, &ic);
		if (ic == (iconv_t)(-1) ) {
			lprintf(5, "%s:%d iconv_open(%s, %s) failed: %s\n",
				__FILE__, __LINE__, "UTF-8", mime_charset, strerror(errno));
		}
	}
#endif

	/** Messages in legacy Citadel variformat get handled thusly... */
	if (!strcasecmp(mime_content_type, "text/x-citadel-variformat")) {
		pullquote_fmout();
	}

	/* Boring old 80-column fixed format text gets handled this way... */
	else if (!strcasecmp(mime_content_type, "text/plain")) {
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			int len;
			len = strlen(buf);
			if ((len > 0) && (buf[len-1] == '\n')) buf[--len] = 0;
			if ((len > 0) && (buf[len-1] == '\r')) buf[--len] = 0;

#ifdef HAVE_ICONV
			if (ic != (iconv_t)(-1) ) {
				ibuf = buf;
				ibuflen = len;
				obuflen = SIZ;
				obuf = (char *) malloc(obuflen);
				osav = obuf;
				iconv(ic, &ibuf, &ibuflen, &obuf, &obuflen);
				osav[SIZ-obuflen] = 0;
				safestrncpy(buf, osav, sizeof buf);
				free(osav);
			}
#endif

			len = strlen(buf);
			while ((!IsEmptyStr(buf)) && (isspace(buf[len - 1]))) 
				buf[--len] = 0;
			if ((bq == 0) &&
		    	((!strncmp(buf, ">", 1)) || (!strncmp(buf, " >", 2)) )) {
				wprintf("<blockquote>");
				bq = 1;
			} else if ((bq == 1) &&
			   	(strncmp(buf, ">", 1)) && (strncmp(buf, " >", 2)) ) {
				wprintf("</blockquote>");
				bq = 0;
			}
			wprintf("<tt>");
			url(buf, sizeof(buf));
			msgescputs1(buf);
			wprintf("</tt><br />");
		}
		wprintf("</i><br />");
	}

	/** HTML just gets escaped and stuffed back into the editor */
	else if (!strcasecmp(mime_content_type, "text/html")) {
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			strcat(buf, "\n");
			msgescputs(buf);
		}
	}//// TODO: charset? utf8?

	/** Unknown weirdness ... don't know how to handle this content type */
	else {
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) { }
	}

ENDBODY:
	/** end of body handler */

	/*
	 * If there were attachments, we have to download them and insert them
	 * into the attachment chain for the forwarded message we are composing.
	 */
	if ( (forward_attachments) && (num_attachments) ) {
		for (i=0; i<num_attachments; ++i) {
			extract_token(buf, attachments, i, '\n', sizeof buf);
			extract_token(mime_filename, buf, 1, '|', sizeof mime_filename);
			extract_token(mime_partnum, buf, 2, '|', sizeof mime_partnum);
			extract_token(mime_disposition, buf, 3, '|', sizeof mime_disposition);
			extract_token(mime_content_type, buf, 4, '|', sizeof mime_content_type);
			mime_length = extract_int(buf, 5);

			/*
			 * tracing  ... uncomment if necessary
			 *
			 */
			lprintf(9, "fwd filename: %s\n", mime_filename);
			lprintf(9, "fwd partnum : %s\n", mime_partnum);
			lprintf(9, "fwd conttype: %s\n", mime_content_type);
			lprintf(9, "fwd dispose : %s\n", mime_disposition);
			lprintf(9, "fwd length  : %d\n", mime_length);

			if ( (!strcasecmp(mime_disposition, "inline"))
			   || (!strcasecmp(mime_disposition, "attachment")) ) {
		
				int n;
				char N[64];
				/* Create an attachment struct from this mime part... */
				att = malloc(sizeof(wc_attachment));
				memset(att, 0, sizeof(wc_attachment));
				att->length = mime_length;
				att->content_type = NewStrBufPlain(mime_content_type, -1);
				att->filename = NewStrBufPlain(mime_filename, -1);
				att->data = load_mimepart(msgnum, mime_partnum);
		
				if (WCC->attachments == NULL)
					WCC->attachments = NewHash(1, NULL);
				/* And add it to the list. */
				n = snprintf(N, sizeof N, "%d", GetCount(WCC->attachments) + 1);
				Put(WCC->attachments, N, n, att, free_attachment);
			}

		}
	}

#ifdef HAVE_ICONV
	if (ic != (iconv_t)(-1) ) {
		iconv_close(ic);
	}
#endif

	if (attachments != NULL) {
		free(attachments);
	}
}




message_summary *ReadOneMessageSummary(StrBuf *RawMessage, const char *DefaultSubject, long MsgNum) 
{
	void                 *vEval;
	MsgPartEvaluatorFunc  Eval;
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
			Eval = (MsgPartEvaluatorFunc) vEval;
			StrBufCutLeft(Buf, nBuf + 1);
			Eval(Msg, Buf);
		}
		else lprintf(1, "Don't know how to handle Message Headerline [%s]", ChrPtr(Buf));
	}
	return Msg;
}


/**
 * \brief display the adressbook overview
 * \param msgnum the citadel message number
 * \param alpha what????
 */
void display_addressbook(long msgnum, char alpha) {
	//char buf[SIZ];
	/* char mime_partnum[SIZ]; */
/* 	char mime_filename[SIZ]; */
/* 	char mime_content_type[SIZ]; */
	///char mime_disposition[SIZ];
	//int mime_length;
	char vcard_partnum[SIZ];
	char *vcard_source = NULL;
	message_summary summ;////TODO: this will leak

	memset(&summ, 0, sizeof(summ));
	///safestrncpy(summ.subj, _("(no subject)"), sizeof summ.subj);
///Load Message headers
//	Msg = 
	if (!IsEmptyStr(vcard_partnum)) {
		vcard_source = load_mimepart(msgnum, vcard_partnum);
		if (vcard_source != NULL) {

			/** Display the summary line */
			display_vcard(WC->WBuf, vcard_source, alpha, 0, NULL,msgnum);

			/** If it's my vCard I can edit it */
			if (	(!strcasecmp(WC->wc_roomname, USERCONFIGROOM))
				|| (!strcasecmp(&WC->wc_roomname[11], USERCONFIGROOM))
				|| (WC->wc_view == VIEW_ADDRESSBOOK)
			) {
				wprintf("<a href=\"edit_vcard?"
					"msgnum=%ld&partnum=%s\">",
					msgnum, vcard_partnum);
				wprintf("[%s]</a>", _("edit"));
			}

			free(vcard_source);
		}
	}

}



/**
 * \brief  If it's an old "Firstname Lastname" style record, try to convert it.
 * \param namebuf name to analyze, reverse if nescessary
 */
void lastfirst_firstlast(char *namebuf) {
	char firstname[SIZ];
	char lastname[SIZ];
	int i;

	if (namebuf == NULL) return;
	if (strchr(namebuf, ';') != NULL) return;

	i = num_tokens(namebuf, ' ');
	if (i < 2) return;

	extract_token(lastname, namebuf, i-1, ' ', sizeof lastname);
	remove_token(namebuf, i-1, ' ');
	strcpy(firstname, namebuf);
	sprintf(namebuf, "%s; %s", lastname, firstname);
}

/**
 * \brief fetch what??? name
 * \param msgnum the citadel message number
 * \param namebuf where to put the name in???
 */
void fetch_ab_name(message_summary *Msg, char *namebuf) {
	char buf[SIZ];
	char mime_partnum[SIZ];
	char mime_filename[SIZ];
	char mime_content_type[SIZ];
	char mime_disposition[SIZ];
	int mime_length;
	char vcard_partnum[SIZ];
	char *vcard_source = NULL;
	int i, len;
	message_summary summ;/// TODO this will lak

	if (namebuf == NULL) return;
	strcpy(namebuf, "");

	memset(&summ, 0, sizeof(summ));
	//////safestrncpy(summ.subj, "(no subject)", sizeof summ.subj);

	sprintf(buf, "MSG0 %ld|0", Msg->msgnum);	/** unfortunately we need the mime info now */
	serv_puts(buf);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') return;

	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		if (!strncasecmp(buf, "part=", 5)) {
			extract_token(mime_filename, &buf[5], 1, '|', sizeof mime_filename);
			extract_token(mime_partnum, &buf[5], 2, '|', sizeof mime_partnum);
			extract_token(mime_disposition, &buf[5], 3, '|', sizeof mime_disposition);
			extract_token(mime_content_type, &buf[5], 4, '|', sizeof mime_content_type);
			mime_length = extract_int(&buf[5], 5);

			if (  (!strcasecmp(mime_content_type, "text/x-vcard"))
			   || (!strcasecmp(mime_content_type, "text/vcard")) ) {
				strcpy(vcard_partnum, mime_partnum);
			}

		}
	}

	if (!IsEmptyStr(vcard_partnum)) {
		vcard_source = load_mimepart(Msg->msgnum, vcard_partnum);
		if (vcard_source != NULL) {

			/* Grab the name off the card */
			display_vcard(WC->WBuf, vcard_source, 0, 0, namebuf, Msg->msgnum);

			free(vcard_source);
		}
	}

	lastfirst_firstlast(namebuf);
	striplt(namebuf);
	len = strlen(namebuf);
	for (i=0; i<len; ++i) {
		if (namebuf[i] != ';') return;
	}
	strcpy(namebuf, _("(no name)"));
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
        struct wcsession *WCC = WC;
	message_summary *Msg;
	StrBuf *Buf, *Buf2;
	///char buf[1024];
	///time_t datestamp;
	//char fullname[128];
	//char nodename[128];
	//char inetaddr[128];
	//char subject[1024];
	///char *ptr;
	int nummsgs;
	////int sbjlen;
	int maxload = 0;
	long len;
	int n;
	////int num_summ_alloc = 0;

	if (WCC->summ != NULL) {
		if (WCC->summ != NULL)
			DeleteHash(&WCC->summ);
	}
	WCC->summ = NewHash(1, Flathash);
	nummsgs = 0;
	maxload = 10000;
	
	Buf = NewStrBuf();
	serv_puts(servcmd);
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 1) {
		FreeStrBuf(&Buf);
		return (nummsgs);
	}
// TODO 			if (with_headers) { //// TODO: Have Attachments?
	Buf2 = NewStrBuf();
	while (len = StrBuf_ServGetln(Buf),
	       ((len != 3)  ||
		strcmp(ChrPtr(Buf), "000")!= 0))
	{
		if (nummsgs < maxload) {
			Msg = (message_summary*)malloc(sizeof(message_summary));
			memset(Msg, 0, sizeof(message_summary));

			Msg->msgnum = StrBufExtract_long(Buf, 0, '|');
			Msg->date = StrBufExtract_long(Buf, 1, '|');

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
			       || ((strcasecmp(ChrPtr(Buf2), serv_info.serv_nodename)
				    && (strcasecmp(ChrPtr(Buf2), serv_info.serv_fqdn)))))))
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
				StrBufAppendBufPlain(Msg->subj, _("(no subj)"), 0, -1);
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
	void *vMsg;
	if (Summ == NULL)
		return NULL;
	GetHash(Summ, (const char*)&n, sizeof(n), &vMsg);
	return (message_summary*) vMsg;
}


/*
 * command loop for reading messages
 *
 * Set oper to "readnew" or "readold" or "readfwd" or "headers"
 */
void readloop(char *oper)
{
	StrBuf *BBViewToolBar = NULL;
	void *vMsg;
	message_summary *Msg;
	char cmd[256] = "";
	char buf[SIZ];
	char old_msgs[SIZ];
	int a = 0;
	int b = 0;
	int n;
	int nummsgs;
	long startmsg;
	int maxmsgs;
	long *displayed_msgs = NULL;
	int num_displayed = 0;
	int is_summary = 0;
	int is_addressbook = 0;
	int is_singlecard = 0;
	int is_calendar = 0;
	struct calview calv;
	int is_tasks = 0;
	int is_notes = 0;
	int is_bbview = 0;
	int lo, hi;
	int lowest_displayed = (-1);
	int highest_displayed = 0;
	addrbookent *addrbook = NULL;
	int num_ab = 0;
	int bbs_reverse = 0;
	struct wcsession *WCC = WC;     /* This is done to make it run faster; WC is a function */
	HashPos *at;
	const char *HashKey;
	long HKLen;
	int care_for_empty_list = 0;
	int load_seen = 0;
	int sortit = 0;

	switch (WCC->wc_view) {
	case VIEW_WIKI:
		sprintf(buf, "wiki?room=%s&page=home", WCC->wc_roomname);
		http_redirect(buf);
		return;
	case VIEW_CALENDAR:
		load_seen = 1;
		is_calendar = 1;
		strcpy(cmd, "MSGS ALL|||1");
		maxmsgs = 32767;
		parse_calendar_view_request(&calv);
		break;
	case VIEW_TASKS:
		is_tasks = 1;
		strcpy(cmd, "MSGS ALL");
		maxmsgs = 32767;
		break;
	case VIEW_NOTES:
		is_notes = 1;
		strcpy(cmd, "MSGS ALL");
		maxmsgs = 32767;
		wprintf("<div id=\"new_notes_here\"></div>\n");
		break;
	case VIEW_ADDRESSBOOK:
		is_singlecard = ibstr("is_singlecard");
		if (maxmsgs > 1) {
			is_addressbook = 1;
			if (!strcmp(oper, "do_search")) {
				snprintf(cmd, sizeof(cmd), "MSGS SEARCH|%s", bstr("query"));
			}
			else {
				strcpy(cmd, "MSGS ALL");
			}
			maxmsgs = 9999999;
			break;
		}

	default:
		care_for_empty_list = 1;
		startmsg = lbstr("startmsg");
		maxmsgs = ibstr("maxmsgs");
		is_summary = (ibstr("is_summary") && !WCC->is_mobile);
		if (maxmsgs == 0) maxmsgs = DEFAULT_MAXMSGS;
		

		
                /*
		 * When in summary mode, always show ALL messages instead of just
		 * new or old.  Otherwise, show what the user asked for.
		 */
		if (!strcmp(oper, "readnew")) {
			strcpy(cmd, "MSGS NEW");
		}
		else if (!strcmp(oper, "readold")) {
			strcpy(cmd, "MSGS OLD");
		}
		else if (!strcmp(oper, "do_search")) {
			snprintf(cmd, sizeof(cmd), "MSGS SEARCH|%s", bstr("query"));
		}
		else {
			strcpy(cmd, "MSGS ALL");
		}
		
		if ((WCC->wc_view == VIEW_MAILBOX) && (maxmsgs > 1) && !WCC->is_mobile) {
			is_summary = 1;
			if (!strcmp(oper, "do_search")) {
				snprintf(cmd, sizeof(cmd), "MSGS SEARCH|%s", bstr("query"));
			}
			else {
				strcpy(cmd, "MSGS ALL");
			}
		}

		is_bbview = !is_summary;
		if (is_summary) {			/**< fetch header summary */
			load_seen = 1;
			snprintf(cmd, sizeof(cmd), "MSGS %s|%s||1",
				 (!strcmp(oper, "do_search") ? "SEARCH" : "ALL"),
				 (!strcmp(oper, "do_search") ? bstr("query") : "")
				);
			startmsg = 1;
			maxmsgs = 9999999;
		} 
		if (startmsg == 0L) {
			if (bbs_reverse) {
				Msg = GetMessagePtrAt((nummsgs >= maxmsgs) ? (nummsgs - maxmsgs) : 0, WCC->summ);
				startmsg = (Msg==NULL)? 0 : Msg->msgnum;
			}
			else {
				Msg = GetMessagePtrAt(0, WCC->summ);
				startmsg = (Msg==NULL)? 0 : Msg->msgnum;
			}
		}
		sortit = is_summary || WCC->is_mobile;
	}







	output_headers(1, 1, 1, 0, 0, 0);

	/*
	if (WCC->is_mobile) {
		maxmsgs = 20;
		snprintf(cmd, sizeof(cmd), "MSGS %s|%s||1",
			(!strcmp(oper, "do_search") ? "SEARCH" : "ALL"),
			(!strcmp(oper, "do_search") ? bstr("query") : "")
		);
		SortBy =  eRDate;
	}

	/*
	 * Are we doing a summary view?  If so, we need to know old messages
	 * and new messages, so we can do that pretty boldface thing for the
	 * new messages.
	 */


	nummsgs = load_msg_ptrs(cmd, (is_summary || WCC->is_mobile));
	if (nummsgs == 0) {
		if (care_for_empty_list) {
			wprintf("<div align=\"center\"><br /><em>");
			if (!strcmp(oper, "readnew")) {
				wprintf(_("No new messages."));
			} else if (!strcmp(oper, "readold")) {
				wprintf(_("No old messages."));
			} else {
				wprintf(_("No messages here."));
			}
			wprintf("</em><br /></div>\n");
		}

		goto DONE;
	}

	if (load_seen){
		void *vMsg;

		strcpy(old_msgs, "");
		serv_puts("GTSN");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			strcpy(old_msgs, &buf[4]);
		}
		at = GetNewHashPos();
		while (GetNextHashPos(WCC->summ, at, &HKLen, &HashKey, &vMsg)) {
			/** Are you a new message, or an old message? */
			Msg = (message_summary*) vMsg;
			if (is_summary) {
				if (is_msg_in_mset(old_msgs, Msg->msgnum)) {
					Msg->is_new = 0;
				}
				else {
					Msg->is_new = 1;
				}
			}
		}
		DeleteHashPos(&at);
	}

	if (sortit) {
		CompareFunc SortIt;
		SortIt =  RetrieveSort(CTX_MAILSUM, NULL, 
				       HKEY("date"), 2);
		if (SortIt != NULL)
			SortByPayload(WCC->summ, SortIt);
	}

	if (is_summary) {
		do_template("summary_header", NULL);
	} else if (WCC->is_mobile) {
		wprintf("<div id=\"message_list\">");
	}


	/**
	 * If we're not currently looking at ALL requested
	 * messages, then display the selector bar
	 */
	if (is_bbview) {
		const char *selected;
		StrBuf *Selector = NewStrBuf();
		BBViewToolBar = NewStrBuf();
/////		DoTemplate("bbview_scrollbar");
		/** begin bbview scroller */
		StrBufAppendPrintf(BBViewToolBar, "<form name=\"msgomatictop\" class=\"selector_top\" > \n <p>");
		StrBufAppendPrintf(BBViewToolBar, _("Reading #"));//// TODO this isn't used, should it? : , lowest_displayed, highest_displayed);

		StrBufAppendPrintf(BBViewToolBar, "<select name=\"whichones\" size=\"1\" "
				   "OnChange=\"location.href=msgomatictop.whichones.options"
				   "[selectedIndex].value\">\n");

		if (bbs_reverse) {
			for (b=nummsgs-1; b>=0; b = b - maxmsgs) {
				hi = b + 1;
				lo = b - maxmsgs + 2;
				if (lo < 1) lo = 1;
				
				Msg = GetMessagePtrAt(lo-1, WCC->summ);
				n = (Msg==NULL)? 0 : Msg->msgnum;
				selected = ((n == startmsg) ? "selected" : "");
				
				StrBufAppendPrintf(Selector, 
						   "<option %s value="
						   "\"%s"
						   "&startmsg=%ld"
						   "&maxmsgs=%d"
						   "&is_summary=%d\">"
						   "%d-%d</option> \n",
						   selected,
						   oper,


						   maxmsgs,
						   is_summary,
						   hi, lo);
			}
		}
		else {
			for (b=0; b<nummsgs; b = b + maxmsgs) {
				lo = b + 1;
				hi = b + maxmsgs + 1;
				if (hi > nummsgs) hi = nummsgs;

				Msg = GetMessagePtrAt(b, WCC->summ);
				n = (Msg==NULL)? 0 : Msg->msgnum;
				selected = ((n == startmsg) ? "selected" : "");
				Msg = GetMessagePtrAt(lo-1,  WCC->summ);
				StrBufAppendPrintf(Selector, 
						   "<option %s value="
						   "\"%s"
						   "&startmsg=%ld"
						   "&maxmsgs=%d"
						   "&is_summary=%d\">"
						   "%d-%d</option> \n",
						   selected,
						   oper,
						   (Msg==NULL)? 0 : Msg->msgnum,
						   maxmsgs,
						   is_summary,
						   lo, hi);
			}
		}

		StrBufAppendBuf(BBViewToolBar, Selector, 0);

		Msg = GetMessagePtrAt(0,  WCC->summ);

		StrBufAppendPrintf(BBViewToolBar, "<option value=\"%s?startmsg=%ld"
			"&maxmsgs=9999999&is_summary=0\">",
			oper,
			(Msg==NULL)? 0 : Msg->msgnum);
		StrBufAppendPrintf(BBViewToolBar, _("All"));

		StrBufAppendPrintf(BBViewToolBar, "</option>");
		StrBufAppendPrintf(BBViewToolBar, "</select> ");
		StrBufAppendPrintf(BBViewToolBar, _("of %d messages."), nummsgs);

		/** forward/reverse */
		StrBufAppendPrintf(BBViewToolBar, "<input type=\"radio\" %s name=\"direction\" value=\"\""
			"OnChange=\"location.href='%s?sortby=forward'\"",  
			(bbs_reverse ? "" : "checked"),
			oper
		);
		StrBufAppendPrintf(BBViewToolBar, ">");
		StrBufAppendPrintf(BBViewToolBar, _("oldest to newest"));
		StrBufAppendPrintf(BBViewToolBar, "&nbsp;&nbsp;&nbsp;&nbsp;");

		StrBufAppendPrintf(BBViewToolBar, "<input type=\"radio\" %s name=\"direction\" value=\"\""
			"OnChange=\"location.href='%s?sortby=reverse'\"", 
			(bbs_reverse ? "checked" : ""),
			oper
		);
		StrBufAppendPrintf(BBViewToolBar, ">");
		StrBufAppendPrintf(BBViewToolBar, _("newest to oldest"));
		StrBufAppendPrintf(BBViewToolBar, "\n");
	
		StrBufAppendPrintf(BBViewToolBar, "</p></form>\n");
		StrBufAppendBuf(WCC->WBuf, BBViewToolBar, 0);
		/** end bbview scroller */
	}
			
	at = GetNewHashPos();
	while (GetNextHashPos(WCC->summ, at, &HKLen, &HashKey, &vMsg)) {
		Msg = (message_summary*) vMsg;		
		if ((Msg->msgnum >= startmsg) && (num_displayed < maxmsgs)) {
				
			/** Display the message */
			if (is_summary) {
				DoTemplate(HKEY("section_mailsummary"), NULL, Msg, CTX_MAILSUM);
			}
			else if (is_addressbook) {
				fetch_ab_name(Msg, buf);
				++num_ab;
				addrbook = realloc(addrbook,
						   (sizeof(addrbookent) * num_ab) );
				safestrncpy(addrbook[num_ab-1].ab_name, buf,
					    sizeof(addrbook[num_ab-1].ab_name));
				addrbook[num_ab-1].ab_msgnum = Msg->msgnum;
			}
			else if (is_calendar) {
				load_calendar_item(Msg, Msg->is_new, &calv);
			}
			else if (is_tasks) {
				display_task(Msg, Msg->is_new);
			}
			else if (is_notes) {
				display_note(Msg, Msg->is_new);
			} else if (WCC->is_mobile) {
				DoTemplate(HKEY("section_mailsummary"), NULL, Msg, CTX_MAILSUM);
			}
			else {
				if (displayed_msgs == NULL) {
					displayed_msgs = malloc(sizeof(long) *
								(maxmsgs<nummsgs ? maxmsgs : nummsgs));
				}
				displayed_msgs[num_displayed] = Msg->msgnum;
			}
			
			if (lowest_displayed < 0) lowest_displayed = a;
			highest_displayed = a;
			
			++num_displayed;
		}
	}
	DeleteHashPos(&at);

	/** Output loop */
	if (displayed_msgs != NULL) {
		if (bbs_reverse) {
			////TODOqsort(displayed_msgs, num_displayed, sizeof(long), qlongcmp_r);
		}

		/** if we do a split bbview in the future, begin messages div here */

		for (a=0; a<num_displayed; ++a) {
			read_message(WC->WBuf, HKEY("view_message"), displayed_msgs[a], 0, NULL);
		}

		/** if we do a split bbview in the future, end messages div here */

		free(displayed_msgs);
		displayed_msgs = NULL;
	}

	if (is_summary) {
		wprintf("</table>"
			"</div>\n");			/**< end of 'fix_scrollbar_bug' div */
		wprintf("</div>");			/**< end of 'message_list' div */
		
		/** Here's the grab-it-to-resize-the-message-list widget */
		wprintf("<div id=\"resize_msglist\" "
			"onMouseDown=\"CtdlResizeMsgListMouseDown(event)\">"
			"<div class=\"fix_scrollbar_bug\"> <hr>"
			"</div></div>\n"
		);

		wprintf("<div id=\"preview_pane\">");	/**< The preview pane will initially be empty */
	} else if (WCC->is_mobile) {
		wprintf("</div>");
	}

	/**
	 * Bump these because although we're thinking in zero base, the user
	 * is a drooling idiot and is thinking in one base.
	 */
	++lowest_displayed;
	++highest_displayed;

	/**
	 * If we're not currently looking at ALL requested
	 * messages, then display the selector bar
	 */
	if (is_bbview) {
		/** begin bbview scroller */
		StrBufAppendBuf(WCC->WBuf, BBViewToolBar, 0);
	}
	
DONE:
	if (is_tasks) {
		do_tasks_view();	/** Render the task list */
	}

	if (is_calendar) {
		render_calendar_view(&calv);
	}

	if (is_addressbook) {
		do_addrbook_view(addrbook, num_ab);	/** Render the address book */
	}

	/** Note: wDumpContent() will output one additional </div> tag. */
	wprintf("</div>\n");		/** end of 'content' div */
	wDumpContent(1);

	/** free the summary */
	if (WCC->summ != NULL) {
		DeleteHash(&WCC->summ);
	}
	if (addrbook != NULL) free(addrbook);
}


/*
 * Back end for post_message()
 * ... this is where the actual message gets transmitted to the server.
 */
void post_mime_to_server(void) {
	struct wcsession *WCC = WC;
	char top_boundary[SIZ];
	char alt_boundary[SIZ];
	int is_multipart = 0;
	static int seq = 0;
	wc_attachment *att;
	char *encoded;
	size_t encoded_length;
	size_t encoded_strlen;
	char *txtmail = NULL;

	sprintf(top_boundary, "Citadel--Multipart--%s--%04x--%04x",
		serv_info.serv_fqdn,
		getpid(),
		++seq
	);
	sprintf(alt_boundary, "Citadel--Multipart--%s--%04x--%04x",
		serv_info.serv_fqdn,
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
		it = GetNewHashPos();
		while (GetNextHashPos(WCC->attachments, it, &len, &Key, &vAtt)) {
			att = (wc_attachment*)vAtt;
			encoded_length = ((att->length * 150) / 100);
			encoded = malloc(encoded_length);
			if (encoded == NULL) break;
			encoded_strlen = CtdlEncodeBase64(encoded, att->data, att->length, 1);

			serv_printf("--%s", top_boundary);
			serv_printf("Content-type: %s", ChrPtr(att->content_type));
			serv_printf("Content-disposition: attachment; filename=\"%s\"", ChrPtr(att->filename));
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
	wc_attachment *att;
	int is_anonymous = 0;
	const StrBuf *display_name = NULL;
	struct wcsession *WCC = WC;
	
	if (havebstr("force_room")) {
		gotoroom(bstr("force_room"));
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
		att = malloc(sizeof(wc_attachment));
		memset(att, 0, sizeof(wc_attachment));
		att->length = WCC->upload_length;
		att->content_type = NewStrBufPlain(WCC->upload_content_type, -1);
		att->filename = NewStrBufPlain(WCC->upload_filename, -1);
		
		
		if (WCC->attachments == NULL)
			WCC->attachments = NewHash(1, NULL);
		/* And add it to the list. */
		n = snprintf(N, sizeof N, "%d", GetCount(WCC->attachments) + 1);
		Put(WCC->attachments, N, n, att, free_attachment);

		/**
		 * Mozilla sends a simple filename, which is what we want,
		 * but Satan's Browser sends an entire pathname.  Reduce
		 * the path to just a filename if we need to.
		 */
		pch = strrchr(ChrPtr(att->filename), '/');
		if (pch != NULL) {
			StrBufCutLeft(att->filename, pch - ChrPtr(att->filename));
		}
		pch = strrchr(ChrPtr(att->filename), '\\');
		if (pch != NULL) {
			StrBufCutLeft(att->filename, pch - ChrPtr(att->filename));
		}

		/**
		 * Transfer control of this memory from the upload struct
		 * to the attachment struct.
		 */
		att->data = WCC->upload;
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
		StrBuf *CmdBuf = NULL;;
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

		lprintf(9, "%s\n", CmdBuf);
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
		readloop("readnew");
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
	/////wc_attachment *att;
	int recipient_required = 0;
	int subject_required = 0;
	int recipient_bad = 0;
	int is_anonymous = 0;

      	struct wcsession *WCC = WC;

	now = time(NULL);

	if (havebstr("force_room")) {
		gotoroom(bstr("force_room"));
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
		readloop("readnew");
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
		do_edit_vcard(-1, "", "", WCC->wc_roomname);
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
		StrBuf *CmdBuf = NULL;;
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
			wprintf("<em>%s</em><br />\n", &buf[4]);/// -> important message
			return;
		}
	}
	svputlong("RCPTREQUIRED", recipient_required);
	svputlong("SUBJREQUIRED", recipient_required || subject_required);

	begin_burst();
	output_headers(1, 0, 0, 0, 1, 0);
	DoTemplate(HKEY("edit_message"), NULL, NULL, CTX_NONE);
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

	readloop("readnew");
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

	readloop("readnew");
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

void readnew(void) { readloop("readnew");}
void readold(void) { readloop("readold");}
void readfwd(void) { readloop("readfwd");}
void headers(void) { readloop("headers");}
void do_search(void) { readloop("do_search");}






void 
InitModule_MSG
(void)
{
	WebcitAddUrlHandler(HKEY("readnew"), readnew, 0);
	WebcitAddUrlHandler(HKEY("readold"), readold, 0);
	WebcitAddUrlHandler(HKEY("readfwd"), readfwd, 0);
	WebcitAddUrlHandler(HKEY("headers"), headers, 0);
	WebcitAddUrlHandler(HKEY("do_search"), do_search, 0);
	WebcitAddUrlHandler(HKEY("display_enter"), display_enter, 0);
	WebcitAddUrlHandler(HKEY("post"), post_message, 0);
	WebcitAddUrlHandler(HKEY("move_msg"), move_msg, 0);
	WebcitAddUrlHandler(HKEY("delete_msg"), delete_msg, 0);
	WebcitAddUrlHandler(HKEY("confirm_move_msg"), confirm_move_msg, 0);
	WebcitAddUrlHandler(HKEY("msg"), embed_message, NEED_URL|AJAX);
	WebcitAddUrlHandler(HKEY("printmsg"), print_message, NEED_URL);
	WebcitAddUrlHandler(HKEY("mobilemsg"), mobile_message_view, NEED_URL);
	WebcitAddUrlHandler(HKEY("msgheaders"), display_headers, NEED_URL);


	return ;
}
