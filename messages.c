/*
 * $Id$
 */
/**
 * \defgroup MsgDisp Functions which deal with the fetching and displaying of messages.
 * \ingroup WebcitDisplayItems
 *
 */
/*@{*/
#include "webcit.h"
#include "vcard.h"
#include "webserver.h"
#include "groupdav.h"

#define SUBJ_COL_WIDTH_PCT		50	/**< Mailbox view column width */
#define SENDER_COL_WIDTH_PCT		30	/**< Mailbox view column width */
#define DATE_PLUS_BUTTONS_WIDTH_PCT	20	/**< Mailbox view column width */

/**
 * Address book entry (keep it short and sweet, it's just a quickie lookup
 * which we can use to get to the real meat and bones later)
 */
struct addrbookent {
	char ab_name[64]; /**< name string */
	long ab_msgnum;   /**< message number of address book entry */
};



#ifdef HAVE_ICONV

/**
 * \brief	Wrapper around iconv_open()
 *		Our version adds aliases for non-standard Microsoft charsets
 *	      such as 'MS950', aliasing them to names like 'CP950'
 *
 * \param	tocode		Target encoding
 * \param	fromcode	Source encoding
 */
iconv_t ctdl_iconv_open(const char *tocode, const char *fromcode)
{
	iconv_t ic = (iconv_t)(-1) ;
	ic = iconv_open(tocode, fromcode);
	if (ic == (iconv_t)(-1) ) {
		char alias_fromcode[64];
		if ( (strlen(fromcode) == 5) && (!strncasecmp(fromcode, "MS", 2)) ) {
			safestrncpy(alias_fromcode, fromcode, sizeof alias_fromcode);
			alias_fromcode[0] = 'C';
			alias_fromcode[1] = 'P';
			ic = iconv_open(tocode, alias_fromcode);
		}
	}
	return(ic);
}


/**
 * \brief  Handle subjects with RFC2047 encoding
 *  such as:
 * =?koi8-r?B?78bP0s3Mxc7JxSDXz9rE1dvO2c3JINvB0sHNySDP?=
 * \param buf the stringbuffer to process
 */
void utf8ify_rfc822_string(char *buf) {
	char *start, *end;
	char newbuf[1024];
	char charset[128];
	char encoding[16];
	char istr[1024];
	iconv_t ic = (iconv_t)(-1) ;
	char *ibuf;			/**< Buffer of characters to be converted */
	char *obuf;			/**< Buffer for converted characters */
	size_t ibuflen;			/**< Length of input buffer */
	size_t obuflen;			/**< Length of output buffer */
	char *isav;			/**< Saved pointer to input buffer */
	char *osav;			/**< Saved pointer to output buffer */
	int passes = 0;
	int i, len;
	int illegal_non_rfc2047_encoding = 0;

	/** Sometimes, badly formed messages contain strings which were simply
	 *  written out directly in some foreign character set instead of
	 *  using RFC2047 encoding.  This is illegal but we will attempt to
	 *  handle it anyway by converting from a user-specified default
	 *  charset to UTF-8 if we see any nonprintable characters.
	 */
	len = strlen(buf);
	for (i=0; i<len; ++i) {
		if ((buf[i] < 32) || (buf[i] > 126)) {
			illegal_non_rfc2047_encoding = 1;
			i = len; ///< take a shortcut, it won't be more than one.
		}
	}
	if (illegal_non_rfc2047_encoding) {
		char default_header_charset[128];
		get_preference("default_header_charset", default_header_charset, sizeof default_header_charset);
		if ( (strcasecmp(default_header_charset, "UTF-8")) && (strcasecmp(default_header_charset, "us-ascii")) ) {
			ic = ctdl_iconv_open("UTF-8", default_header_charset);
			if (ic != (iconv_t)(-1) ) {
				ibuf = malloc(1024);
				isav = ibuf;
				safestrncpy(ibuf, buf, 1024);
				ibuflen = strlen(ibuf);
				obuflen = 1024;
				obuf = (char *) malloc(obuflen);
				osav = obuf;
				iconv(ic, &ibuf, &ibuflen, &obuf, &obuflen);
				osav[1024-obuflen] = 0;
				strcpy(buf, osav);
				free(osav);
				iconv_close(ic);
				free(isav);
			}
		}
	}

	/** Now we handle foreign character sets properly encoded
	 *  in RFC2047 format.
	 */
	while (start=strstr(buf, "=?"), end=strstr(buf, "?="),
		((start != NULL) && (end != NULL) && (end > start)) )
	{
		extract_token(charset, start, 1, '?', sizeof charset);
		extract_token(encoding, start, 2, '?', sizeof encoding);
		extract_token(istr, start, 3, '?', sizeof istr);

		ibuf = malloc(1024);
		isav = ibuf;
		if (!strcasecmp(encoding, "B")) {	/**< base64 */
			ibuflen = CtdlDecodeBase64(ibuf, istr, strlen(istr));
		}
		else if (!strcasecmp(encoding, "Q")) {	/**< quoted-printable */
			size_t len;
			long pos;
			
			len = strlen(istr);
			pos = 0;
			while (pos < len)
			{
				if (istr[pos] == '_') istr[pos] = ' ';
				pos++;
			}

			ibuflen = CtdlDecodeQuotedPrintable(ibuf, istr, len);
		}
		else {
			strcpy(ibuf, istr);		/**< unknown encoding */
			ibuflen = strlen(istr);
		}

		ic = ctdl_iconv_open("UTF-8", charset);
		if (ic != (iconv_t)(-1) ) {
			obuflen = 1024;
			obuf = (char *) malloc(obuflen);
			osav = obuf;
			iconv(ic, &ibuf, &ibuflen, &obuf, &obuflen);
			osav[1024-obuflen] = 0;

			end = start;
			end++;
			strcpy(start, "");
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			strcpy(end, &end[1]);

			snprintf(newbuf, sizeof newbuf, "%s%s%s", buf, osav, end);
			strcpy(buf, newbuf);
			free(osav);
			iconv_close(ic);
		}
		else {
			end = start;
			end++;
			strcpy(start, "");
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			strcpy(end, &end[1]);

			snprintf(newbuf, sizeof newbuf, "%s(unreadable)%s", buf, end);
			strcpy(buf, newbuf);
		}

		free(isav);

		/**
		 * Since spammers will go to all sorts of absurd lengths to get their
		 * messages through, there are LOTS of corrupt headers out there.
		 * So, prevent a really badly formed RFC2047 header from throwing
		 * this function into an infinite loop.
		 */
		++passes;
		if (passes > 20) return;
	}

}
#endif




/**
 * \brief	RFC2047-encode a header field if necessary.
 *		If no non-ASCII characters are found, the string
 *		will be copied verbatim without encoding.
 *
 * \param	target		Target buffer.
 * \param	maxlen		Maximum size of target buffer.
 * \param	source		Source string to be encoded.
 */
void rfc2047encode(char *target, int maxlen, char *source)
{
	int need_to_encode = 0;
	int i, len;
	unsigned char ch;

	if (target == NULL) return;
	len = strlen(source);
	for (i=0; i<len; ++i) {
		if ((source[i] < 32) || (source[i] > 126)) {
			need_to_encode = 1;
			i = len; ///< shortcut. won't become more than 1
		}
	}

	if (!need_to_encode) {
		safestrncpy(target, source, maxlen);
		return;
	}

	strcpy(target, "=?UTF-8?Q?");
	for (i=0; i<len; ++i) {
		ch = (unsigned char) source[i];
		if ((ch < 32) || (ch > 126) || (ch == 61)) {
			sprintf(&target[strlen(target)], "=%02X", ch);
		}
		else {
			sprintf(&target[strlen(target)], "%c", ch);
		}
	}
	
	strcat(target, "?=");
}




/**
 * \brief Look for URL's embedded in a buffer and make them linkable.  We use a
 * target window in order to keep the BBS session in its own window.
 * \param buf the message buffer
 */
void url(char *buf)
{
	int len;
	char *start, *end, *pos;
	char urlbuf[SIZ];
	char outbuf[1024];

	start = NULL;
	len = strlen(buf);
	end = buf + len;
	for (pos = buf; (pos < end) && (start == NULL); ++pos) {
		if (!strncasecmp(pos, "http://", 7))
			start = pos;
		if (!strncasecmp(pos, "ftp://", 6))
			start = pos;
	}

	if (start == NULL)
		return;

	for (pos = buf+len; pos > start; --pos) {
		if (  (!isprint(*pos))
		   || (isspace(*pos))
		   || (*pos == '{')
		   || (*pos == '}')
		   || (*pos == '|')
		   || (*pos == '\\')
		   || (*pos == '^')
		   || (*pos == '[')
		   || (*pos == ']')
		   || (*pos == '`')
		   || (*pos == '<')
		   || (*pos == '>')
		   || (*pos == '(')
		   || (*pos == ')')
		) {
			end = pos;
		}
	}

	strncpy(urlbuf, start, end - start);
	urlbuf[end - start] = '\0';

	if (start != buf)
		strncpy(outbuf, buf, start - buf );
	sprintf(&outbuf[start-buf], "%ca href=%c%s%c TARGET=%c%s%c%c%s%c/A%c",
		LB, QU, urlbuf, QU, QU, TARGET, QU, RB, urlbuf, LB, RB);
	strcat(outbuf, end);
	if ( strlen(outbuf) < 250 )
		strcpy(buf, outbuf);
}


/**
 * \brief Turn a vCard "n" (name) field into something displayable.
 * \param name the name field to convert
 */
void vcard_n_prettyize(char *name)
{
	char *original_name;
	int i, j, len;

	original_name = strdup(name);
	len = strlen(original_name);
	for (i=0; i<5; ++i) {
		if (len > 0) {
			if (original_name[len-1] == ' ') {
				original_name[--len] = 0;
			}
			if (original_name[len-1] == ';') {
				original_name[--len] = 0;
			}
		}
	}
	strcpy(name, "");
	j=0;
	for (i=0; i<len; ++i) {
		if (original_name[i] == ';') {
			name[j++] = ',';
			name[j++] = ' ';			
		}
		else {
			name[j++] = original_name[i];
		}
	}
	name[j] = '\0';
	free(original_name);
}




/**
 * \brief preparse a vcard name
 * display_vcard() calls this after parsing the textual vCard into
 * our 'struct vCard' data object.
 * This gets called instead of display_parsed_vcard() if we are only looking
 * to extract the person's name instead of displaying the card.
 * \param v the vcard to retrieve the name from
 * \param storename where to put the name at
 */
void fetchname_parsed_vcard(struct vCard *v, char *storename) {
	char *name;

	strcpy(storename, "");

	name = vcard_get_prop(v, "n", 1, 0, 0);
	if (name != NULL) {
		strcpy(storename, name);
		/* vcard_n_prettyize(storename); */
	}

}



/**
 * \brief html print a vcard
 * display_vcard() calls this after parsing the textual vCard into
 * our 'struct vCard' data object.
 *
 * Set 'full' to nonzero to display the full card, otherwise it will only
 * show a summary line.
 *
 * This code is a bit ugly, so perhaps an explanation is due: we do this
 * in two passes through the vCard fields.  On the first pass, we process
 * fields we understand, and then render them in a pretty fashion at the
 * end.  Then we make a second pass, outputting all the fields we don't
 * understand in a simple two-column name/value format.
 * \param v the vCard to display
 * \param full display all items of the vcard?
 */
void display_parsed_vcard(struct vCard *v, int full) {
	int i, j;
	char buf[SIZ];
	char *name;
	int is_qp = 0;
	int is_b64 = 0;
	char *thisname, *thisvalue;
	char firsttoken[SIZ];
	int pass;

	char fullname[SIZ];
	char title[SIZ];
	char org[SIZ];
	char phone[SIZ];
	char mailto[SIZ];

	strcpy(fullname, "");
	strcpy(phone, "");
	strcpy(mailto, "");
	strcpy(title, "");
	strcpy(org, "");

	if (!full) {
		wprintf("<TD>");
		name = vcard_get_prop(v, "fn", 1, 0, 0);
		if (name != NULL) {
			escputs(name);
		}
		else if (name = vcard_get_prop(v, "n", 1, 0, 0), name != NULL) {
			strcpy(fullname, name);
			vcard_n_prettyize(fullname);
			escputs(fullname);
		}
		else {
			wprintf("&nbsp;");
		}
		wprintf("</TD>");
		return;
	}

	wprintf("<div align=center>"
		"<table bgcolor=#aaaaaa width=50%%>");
	for (pass=1; pass<=2; ++pass) {

		if (v->numprops) for (i=0; i<(v->numprops); ++i) {
			int len;
			thisname = strdup(v->prop[i].name);
			extract_token(firsttoken, thisname, 0, ';', sizeof firsttoken);
	
			for (j=0; j<num_tokens(thisname, ';'); ++j) {
				extract_token(buf, thisname, j, ';', sizeof buf);
				if (!strcasecmp(buf, "encoding=quoted-printable")) {
					is_qp = 1;
					remove_token(thisname, j, ';');
				}
				if (!strcasecmp(buf, "encoding=base64")) {
					is_b64 = 1;
					remove_token(thisname, j, ';');
				}
			}
			
			len = strlen(v->prop[i].value);
	
			if (is_qp) {
				// %ff can become 6 bytes in utf8 
				thisvalue = malloc(len * 2 + 3); 
				j = CtdlDecodeQuotedPrintable(
					thisvalue, v->prop[i].value,
					len);
				thisvalue[j] = 0;
			}
			else if (is_b64) {
				// ff will become one byte..
				thisvalue = malloc(len + 50);
				CtdlDecodeBase64(
					thisvalue, v->prop[i].value,
					strlen(v->prop[i].value) );
			}
			else {
				thisvalue = strdup(v->prop[i].value);
			}
	
			/** Various fields we may encounter ***/
	
			/** N is name, but only if there's no FN already there */
			if (!strcasecmp(firsttoken, "n")) {
				if (IsEmptyStr(fullname)) {
					strcpy(fullname, thisvalue);
					vcard_n_prettyize(fullname);
				}
			}
	
			/** FN (full name) is a true 'display name' field */
			else if (!strcasecmp(firsttoken, "fn")) {
				strcpy(fullname, thisvalue);
			}

			/** title */
			else if (!strcasecmp(firsttoken, "title")) {
				strcpy(title, thisvalue);
			}
	
			/** organization */
			else if (!strcasecmp(firsttoken, "org")) {
				strcpy(org, thisvalue);
			}
	
			else if (!strcasecmp(firsttoken, "email")) {
				if (!IsEmptyStr(mailto)) strcat(mailto, "<br />");
				long len;
				strcat(mailto,
					"<a href=\"display_enter"
					"?force_room=_MAIL_?recp=");

				urlesc(&mailto[strlen(mailto)], fullname);
				urlesc(&mailto[strlen(mailto)], " <");
				urlesc(&mailto[strlen(mailto)], thisvalue);
				urlesc(&mailto[strlen(mailto)], ">");

				strcat(mailto, "\">");
				len = strlen(mailto);
				stresc(mailto+len, SIZ - len, thisvalue, 1, 1);
				strcat(mailto, "</A>");
			}
			else if (!strcasecmp(firsttoken, "tel")) {
				if (!IsEmptyStr(phone)) strcat(phone, "<br />");
				strcat(phone, thisvalue);
				for (j=0; j<num_tokens(thisname, ';'); ++j) {
					extract_token(buf, thisname, j, ';', sizeof buf);
					if (!strcasecmp(buf, "tel"))
						strcat(phone, "");
					else if (!strcasecmp(buf, "work"))
						strcat(phone, _(" (work)"));
					else if (!strcasecmp(buf, "home"))
						strcat(phone, _(" (home)"));
					else if (!strcasecmp(buf, "cell"))
						strcat(phone, _(" (cell)"));
					else {
						strcat(phone, " (");
						strcat(phone, buf);
						strcat(phone, ")");
					}
				}
			}
			else if (!strcasecmp(firsttoken, "adr")) {
				if (pass == 2) {
					wprintf("<TR><TD>");
					wprintf(_("Address:"));
					wprintf("</TD><TD>");
					for (j=0; j<num_tokens(thisvalue, ';'); ++j) {
						extract_token(buf, thisvalue, j, ';', sizeof buf);
						if (!IsEmptyStr(buf)) {
							escputs(buf);
							if (j<3) wprintf("<br />");
							else wprintf(" ");
						}
					}
					wprintf("</TD></TR>\n");
				}
			}
			else if (!strcasecmp(firsttoken, "version")) {
				/* ignore */
			}
			else if (!strcasecmp(firsttoken, "rev")) {
				/* ignore */
			}
			else if (!strcasecmp(firsttoken, "label")) {
				/* ignore */
			}
			else {

				/*** Don't show extra fields.  They're ugly.
				if (pass == 2) {
					wprintf("<TR><TD>");
					escputs(thisname);
					wprintf("</TD><TD>");
					escputs(thisvalue);
					wprintf("</TD></TR>\n");
				}
				***/
			}
	
			free(thisname);
			free(thisvalue);
		}
	
		if (pass == 1) {
			wprintf("<TR BGCOLOR=\"#AAAAAA\">"
			"<TD COLSPAN=2 BGCOLOR=\"#FFFFFF\">"
			"<IMG ALIGN=CENTER src=\"static/viewcontacts_48x.gif\">"
			"<FONT SIZE=+1><B>");
			escputs(fullname);
			wprintf("</B></FONT>");
			if (!IsEmptyStr(title)) {
				wprintf("<div align=right>");
				escputs(title);
				wprintf("</div>");
			}
			if (!IsEmptyStr(org)) {
				wprintf("<div align=right>");
				escputs(org);
				wprintf("</div>");
			}
			wprintf("</TD></TR>\n");
		
			if (!IsEmptyStr(phone)) {
				wprintf("<tr><td>");
				wprintf(_("Telephone:"));
				wprintf("</td><td>%s</td></tr>\n", phone);
			}
			if (!IsEmptyStr(mailto)) {
				wprintf("<tr><td>");
				wprintf(_("E-mail:"));
				wprintf("</td><td>%s</td></tr>\n", mailto);
			}
		}

	}

	wprintf("</table></div>\n");
}



/**
 * \brief  Display a textual vCard
 * (Converts to a vCard object and then calls the actual display function)
 * Set 'full' to nonzero to display the whole card instead of a one-liner.
 * Or, if "storename" is non-NULL, just store the person's name in that
 * buffer instead of displaying the card at all.
 * \param vcard_source the buffer containing the vcard text
 * \param alpha what???
 * \param full should we usse all lines?
 * \param storename where to store???
 */
void display_vcard(char *vcard_source, char alpha, int full, char *storename) {
	struct vCard *v;
	char *name;
	char buf[SIZ];
	char this_alpha = 0;

	v = vcard_load(vcard_source);
	if (v == NULL) return;

	name = vcard_get_prop(v, "n", 1, 0, 0);
	if (name != NULL) {
		strcpy(buf, name);
		this_alpha = buf[0];
	}

	if (storename != NULL) {
		fetchname_parsed_vcard(v, storename);
	}
	else if (	(alpha == 0)
			|| ((isalpha(alpha)) && (tolower(alpha) == tolower(this_alpha)) )
			|| ((!isalpha(alpha)) && (!isalpha(this_alpha)))
		) {
		display_parsed_vcard(v, full);
	}

	vcard_free(v);
}


struct attach_link {
	char partnum[32];
	char html[1024];
};


/**
 * \brief I wanna SEE that message!  
 * \param msgnum the citadel number of the message to display
 * \param printable_view are we doing a print view?
 * \param section Optional for encapsulated message/rfc822 submessage)
 */
void read_message(long msgnum, int printable_view, char *section) {
	char buf[SIZ];
	char mime_partnum[256];
	char mime_name[256];
	char mime_filename[256];
	char escaped_mime_filename[256];
	char mime_content_type[256];
	char mime_charset[256];
	char mime_disposition[256];
	int mime_length;
	struct attach_link *attach_links = NULL;
	int num_attach_links = 0;
	char mime_submessages[256];
	char m_subject[256];
	char m_cc[1024];
	char from[256];
	char node[256];
	char rfca[256];
	char reply_to[512];
	char reply_all[4096];
	char now[64];
	int format_type = 0;
	int nhdr = 0;
	int bq = 0;
	int i = 0;
	char vcard_partnum[256];
	char cal_partnum[256];
	char *part_source = NULL;
	char msg4_partnum[32];
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
	strcpy(reply_all, "");
	strcpy(vcard_partnum, "");
	strcpy(cal_partnum, "");
	strcpy(mime_content_type, "text/plain");
	strcpy(mime_charset, "us-ascii");
	strcpy(mime_submessages, "");

	serv_printf("MSG4 %ld|%s", msgnum, section);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		wprintf("<strong>");
		wprintf(_("ERROR:"));
		wprintf("</strong> %s<br />\n", &buf[4]);
		return;
	}

	/** begin everythingamundo table */
        if (!printable_view) {
                wprintf("<div class=\"fix_scrollbar_bug message\" ");
                wprintf("onMouseOver=document.getElementById(\"msg%ld\").style.visibility=\"visible\" ", msgnum);
                wprintf("onMouseOut=document.getElementById(\"msg%ld\").style.visibility=\"hidden\" >", msgnum);
        }

	/** begin message header table */
	wprintf("<div class=\"message_header\">");

	strcpy(m_subject, "");
	strcpy(m_cc, "");

	while (serv_getln(buf, sizeof buf), strcasecmp(buf, "text")) {
		if (!strcmp(buf, "000")) {
			wprintf("<i>");
			wprintf(_("unexpected end of message"));
			wprintf(" (1)</i><br /><br />\n");
			wprintf("</div>\n");
			return;
		}
		if (!strncasecmp(buf, "nhdr=yes", 8))
			nhdr = 1;
		if (nhdr == 1)
			buf[0] = '_';
		if (!strncasecmp(buf, "type=", 5))
			format_type = atoi(&buf[5]);
		if (!strncasecmp(buf, "from=", 5)) {
			strcpy(from, &buf[5]);
			wprintf(_("from "));
			wprintf("<a href=\"showuser?who=");
#ifdef HAVE_ICONV
			utf8ify_rfc822_string(from);
#endif
			urlescputs(from);
			wprintf("\">");
			escputs(from);
			wprintf("</a> ");
		}
		if (!strncasecmp(buf, "subj=", 5)) {
			safestrncpy(m_subject, &buf[5], sizeof m_subject);
		}
		if (!strncasecmp(buf, "cccc=", 5)) {
			int len;
			safestrncpy(m_cc, &buf[5], sizeof m_cc);
			if (!IsEmptyStr(reply_all)) {
				strcat(reply_all, ", ");
			}
			len = strlen(reply_all);
			safestrncpy(&reply_all[len], &buf[5],
				(sizeof reply_all - len) );
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
			escputs(rfca);
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
			int len;
			wprintf(_("to "));
			if (!IsEmptyStr(reply_all)) {
				strcat(reply_all, ", ");
			}
			len = strlen(reply_all);
			safestrncpy(&reply_all[len], &buf[5],
				(sizeof reply_all - len) );
#ifdef HAVE_ICONV
			utf8ify_rfc822_string(&buf[5]);
#endif
			escputs(&buf[5]);
			wprintf(" ");
		}
		if (!strncasecmp(buf, "time=", 5)) {
			fmt_date(now, atol(&buf[5]), 0);
			wprintf("<span>");
			wprintf("%s ", now);
			wprintf("</span>");
		}

		if (!strncasecmp(buf, "part=", 5)) {
			extract_token(mime_name, &buf[5], 0, '|', sizeof mime_filename);
			extract_token(mime_filename, &buf[5], 1, '|', sizeof mime_filename);
			extract_token(mime_partnum, &buf[5], 2, '|', sizeof mime_partnum);
			extract_token(mime_disposition, &buf[5], 3, '|', sizeof mime_disposition);
			extract_token(mime_content_type, &buf[5], 4, '|', sizeof mime_content_type);
			mime_length = extract_int(&buf[5], 5);

			striplt(mime_name);
			striplt(mime_filename);
			if ( (IsEmptyStr(mime_filename)) && (!IsEmptyStr(mime_name)) ) {
				strcpy(mime_filename, mime_name);
			}

			if (!strcasecmp(mime_content_type, "message/rfc822")) {
				if (!IsEmptyStr(mime_submessages)) {
					strcat(mime_submessages, "|");
				}
				strcat(mime_submessages, mime_partnum);
			}
			else if ((!strcasecmp(mime_disposition, "inline"))
			   && (!strncasecmp(mime_content_type, "image/", 6)) ){
				++num_attach_links;
				attach_links = realloc(attach_links,
					(num_attach_links*sizeof(struct attach_link)));
				safestrncpy(attach_links[num_attach_links-1].partnum, mime_partnum, 32);
				snprintf(attach_links[num_attach_links-1].html, 1024,
					"<img src=\"mimepart/%ld/%s/%s\">",
					msgnum, mime_partnum, mime_filename);
			}
			else if ( ( (!strcasecmp(mime_disposition, "attachment")) 
			     || (!strcasecmp(mime_disposition, "inline"))
			     || (!strcasecmp(mime_disposition, ""))
			     ) && (!IsEmptyStr(mime_content_type))
			) {
				++num_attach_links;
				attach_links = realloc(attach_links,
					(num_attach_links*sizeof(struct attach_link)));
				safestrncpy(attach_links[num_attach_links-1].partnum, mime_partnum, 32);
				urlesc(escaped_mime_filename, mime_filename);
				snprintf(attach_links[num_attach_links-1].html, 1024,
					"<img src=\"static/diskette_24x.gif\" "
					"border=0 align=middle>\n"
					"%s (%s, %d bytes) [ "
					"<a href=\"mimepart/%ld/%s/%s\""
					"target=\"wc.%ld.%s\">%s</a>"
					" | "
					"<a href=\"mimepart_download/%ld/%s/%s\">%s</a>"
					" ]<br />\n",
					mime_filename,
					mime_content_type, mime_length,
					msgnum, mime_partnum, escaped_mime_filename,
					msgnum, mime_partnum,
					_("View"),
					msgnum, mime_partnum, escaped_mime_filename,
					_("Download")
				);
			}

			/** begin handler prep ***/
			if (  (!strcasecmp(mime_content_type, "text/x-vcard"))
			   || (!strcasecmp(mime_content_type, "text/vcard")) ) {
				strcpy(vcard_partnum, mime_partnum);
			}

			if (!strcasecmp(mime_content_type, "text/calendar")) {
				strcpy(cal_partnum, mime_partnum);
			}

			/** end handler prep ***/

		}

	}

	/** Generate a reply-to address */
	if (!IsEmptyStr(rfca)) {
		if (!IsEmptyStr(from)) {
			snprintf(reply_to, sizeof(reply_to), "%s <%s>", from, rfca);
		}
		else {
			strcpy(reply_to, rfca);
		}
	}
	else {
	if ((!IsEmptyStr(node))
		   && (strcasecmp(node, serv_info.serv_nodename))
		   && (strcasecmp(node, serv_info.serv_humannode)) ) {
			snprintf(reply_to, sizeof(reply_to), "%s @ %s",
				from, node);
		}
		else {
			snprintf(reply_to, sizeof(reply_to), "%s", from);
		}
	}

	if (nhdr == 1) {
		wprintf("****");
	}

#ifdef HAVE_ICONV
	utf8ify_rfc822_string(m_cc);
	utf8ify_rfc822_string(m_subject);
#endif

        /** start msg buttons */

        if (!printable_view) {
                wprintf("<p id=\"msg%ld\" class=\"msgbuttons\" >\n",msgnum);

		/** Reply */
		if ( (WC->wc_view == VIEW_MAILBOX) || (WC->wc_view == VIEW_BBS) ) {
			wprintf("<a href=\"display_enter");
			if (WC->is_mailbox) {
				wprintf("?replyquote=%ld", msgnum);
			}
			wprintf("?recp=");
			urlescputs(reply_to);
			if (!IsEmptyStr(m_subject)) {
				wprintf("?subject=");
				if (strncasecmp(m_subject, "Re:", 3)) wprintf("Re:%20");
				urlescputs(m_subject);
			}
			wprintf("\"><span>[</span>%s<span>]</span></a> ", _("Reply"));
		}

		/** ReplyQuoted */
		if ( (WC->wc_view == VIEW_MAILBOX) || (WC->wc_view == VIEW_BBS) ) {
			if (!WC->is_mailbox) {
				wprintf("<a href=\"display_enter");
				wprintf("?replyquote=%ld", msgnum);
				wprintf("?recp=");
				urlescputs(reply_to);
				if (!IsEmptyStr(m_subject)) {
					wprintf("?subject=");
					if (strncasecmp(m_subject, "Re:", 3)) wprintf("Re:%20");
					urlescputs(m_subject);
				}
				wprintf("\"><span>[</span>%s<span>]</span></a> ", _("ReplyQuoted"));
			}
		}

		/** ReplyAll */
		if (WC->wc_view == VIEW_MAILBOX) {
			wprintf("<a href=\"display_enter");
			wprintf("?replyquote=%ld", msgnum);
			wprintf("?recp=");
			urlescputs(reply_to);
			wprintf("?cc=");
			urlescputs(reply_all);
			if (!IsEmptyStr(m_subject)) {
				wprintf("?subject=");
				if (strncasecmp(m_subject, "Re:", 3)) wprintf("Re:%20");
				urlescputs(m_subject);
			}
			wprintf("\"><span>[</span>%s<span>]</span></a> ", _("ReplyAll"));
		}

		/** Forward */
		if (WC->wc_view == VIEW_MAILBOX) {
			wprintf("<a href=\"display_enter?fwdquote=%ld?subject=", msgnum);
			if (strncasecmp(m_subject, "Fwd:", 4)) wprintf("Fwd:%20");
			urlescputs(m_subject);
			wprintf("\"><span>[</span>%s<span>]</span></a> ", _("Forward"));
		}

		/** If this is one of my own rooms, or if I'm an Aide or Room Aide, I can move/delete */
		if ( (WC->is_room_aide) || (WC->is_mailbox) || (WC->room_flags2 & QR2_COLLABDEL) ) {
			/** Move */
			wprintf("<a href=\"confirm_move_msg?msgid=%ld\"><span>[</span>%s<span>]</span></a> ",
				msgnum, _("Move"));
	
			/** Delete */
			wprintf("<a href=\"delete_msg?msgid=%ld\" "
				"onClick=\"return confirm('%s');\">"
				"<span>[</span>%s<span>]</span> "
				"</a> ", msgnum, _("Delete this message?"), _("Delete")
			);
		}

		/** Headers */
		wprintf("<a href=\"#\" onClick=\"window.open('msgheaders/%ld', 'headers%ld', 'toolbar=no,location=no,directories=no,copyhistory=no,status=yes,scrollbars=yes,resizable=yes,width=600,height=400'); \" >"
			"<span>[</span>%s<span>]</span></a>", msgnum, msgnum, _("Headers"));


		/** Print */
		wprintf("<a href=\"#\" onClick=\"window.open('printmsg/%ld', 'print%ld', 'toolbar=no,location=no,directories=no,copyhistory=no,status=yes,scrollbars=yes,resizable=yes,width=600,height=400'); \" >"
			"<span>[</span>%s<span>]</span></a>", msgnum, msgnum, _("Print"));

		wprintf("</p>");

	}

	if (!IsEmptyStr(m_cc)) {
		wprintf("<p>");
		wprintf(_("CC:"));
		wprintf(" ");
		escputs(m_cc);
		wprintf("</p>");
	}
	if (!IsEmptyStr(m_subject)) {
		wprintf("<p class=\"message_subject\">");
		wprintf(_("Subject:"));
		wprintf(" ");
		escputs(m_subject);
		wprintf("</p>");
	}

	wprintf("</div>");

	/** Begin body */
	wprintf("<div class=\"message_content\">");

	/**
	 * Learn the content type
	 */
	strcpy(mime_content_type, "text/plain");
	while (serv_getln(buf, sizeof buf), (!IsEmptyStr(buf))) {
		if (!strcmp(buf, "000")) {
			wprintf("<i>");
			wprintf(_("unexpected end of message"));
			wprintf(" (2)</i><br /><br />\n");
			goto ENDBODY;
		}
		if (!strncasecmp(buf, "X-Citadel-MSG4-Partnum:", 23)) {
			safestrncpy(msg4_partnum, &buf[23], sizeof(msg4_partnum));
			striplt(msg4_partnum);
		}
		if (!strncasecmp(buf, "Content-type:", 13)) {
			int len;
			safestrncpy(mime_content_type, &buf[13], sizeof(mime_content_type));
			striplt(mime_content_type);
			len = strlen(mime_content_type);
			for (i=0; i<len; ++i) {
				if (!strncasecmp(&mime_content_type[i], "charset=", 8)) {
					safestrncpy(mime_charset, &mime_content_type[i+8],
						sizeof mime_charset);
				}
			}
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
	if (strchr(mime_charset, ';')) strcpy(strchr(mime_charset, ';'), "");
	if ( (strcasecmp(mime_charset, "us-ascii"))
	   && (strcasecmp(mime_charset, "UTF-8"))
	   && (strcasecmp(mime_charset, ""))
	) {
		ic = ctdl_iconv_open("UTF-8", mime_charset);
		if (ic == (iconv_t)(-1) ) {
			lprintf(5, "%s:%d iconv_open(UTF-8, %s) failed: %s\n",
				__FILE__, __LINE__, mime_charset, strerror(errno));
		}
	}
#endif

	/** Messages in legacy Citadel variformat get handled thusly... */
	if (!strcasecmp(mime_content_type, "text/x-citadel-variformat")) {
		fmout("JUSTIFY");
	}

	/** Boring old 80-column fixed format text gets handled this way... */
	else if ( (!strcasecmp(mime_content_type, "text/plain"))
		|| (!strcasecmp(mime_content_type, "text")) ) {
		buf [0] = '\0';
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			int len;
			len = strlen(buf);
			if ((len > 0) && buf[len-1] == '\n') buf[--len] = 0;
			if ((len > 0) && buf[len-1] == '\r') buf[--len] = 0;

#ifdef HAVE_ICONV
			if (ic != (iconv_t)(-1) ) {
				ibuf = buf;
				ibuflen = strlen(ibuf);
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
			while ((!IsEmptyStr(buf)) && (isspace(buf[len-1])))
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
			url(buf);
			escputs(buf);
			wprintf("</tt><br />\n");
		}
		wprintf("</i><br />");
	}

	else /** HTML is fun, but we've got to strip it first */
	if (!strcasecmp(mime_content_type, "text/html")) {
		output_html(mime_charset, (WC->wc_view == VIEW_WIKI ? 1 : 0));
	}

	/** Unknown weirdness */
	else {
		wprintf(_("I don't know how to display %s"), mime_content_type);
		wprintf("<br />\n", mime_content_type);
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) { }
	}

	/** If there are attached submessages, display them now... */
	if ( (!IsEmptyStr(mime_submessages)) && (!section[0]) ) {
		for (i=0; i<num_tokens(mime_submessages, '|'); ++i) {
			extract_token(buf, mime_submessages, i, '|', sizeof buf);
			/** use printable_view to suppress buttons */
			wprintf("<blockquote>");
			read_message(msgnum, 1, buf);
			wprintf("</blockquote>");
		}
	}


	/** Afterwards, offer links to download attachments 'n' such */
	if ( (num_attach_links > 0) && (!section[0]) ) {
		for (i=0; i<num_attach_links; ++i) {
			if (strcasecmp(attach_links[i].partnum, msg4_partnum)) {
				wprintf("%s", attach_links[i].html);
			}
		}
	}

	/** Handler for vCard parts */
	if (!IsEmptyStr(vcard_partnum)) {
		part_source = load_mimepart(msgnum, vcard_partnum);
		if (part_source != NULL) {

			/** If it's my vCard I can edit it */
			if (	(!strcasecmp(WC->wc_roomname, USERCONFIGROOM))
				|| (!strcasecmp(&WC->wc_roomname[11], USERCONFIGROOM))
				|| (WC->wc_view == VIEW_ADDRESSBOOK)
			) {
				wprintf("<a href=\"edit_vcard?"
					"msgnum=%ld?partnum=%s\">",
					msgnum, vcard_partnum);
				wprintf("[%s]</a>", _("edit"));
			}

			/** In all cases, display the full card */
			display_vcard(part_source, 0, 1, NULL);
		}
	}

	/** Handler for calendar parts */
	if (!IsEmptyStr(cal_partnum)) {
		part_source = load_mimepart(msgnum, cal_partnum);
		if (part_source != NULL) {
			cal_process_attachment(part_source,
						msgnum, cal_partnum);
		}
	}

	if (part_source) {
		free(part_source);
		part_source = NULL;
	}

ENDBODY:
	wprintf("</div>\n");

	/** end everythingamundo table */
	if (!printable_view) {
		wprintf("</div>\n");
	}

	if (num_attach_links > 0) {
		free(attach_links);
	}

#ifdef HAVE_ICONV
	if (ic != (iconv_t)(-1) ) {
		iconv_close(ic);
	}
#endif
}



/**
 * \brief Unadorned HTML output of an individual message, suitable
 * for placing in a hidden iframe, for printing, or whatever
 *
 * \param msgnum_as_string Message number, as a string instead of as a long int
 */
void embed_message(char *msgnum_as_string) {
	long msgnum = 0L;

	msgnum = atol(msgnum_as_string);
	begin_ajax_response();
	read_message(msgnum, 0, "");
	end_ajax_response();
}


/**
 * \brief Printable view of a message
 *
 * \param msgnum_as_string Message number, as a string instead of as a long int
 */
void print_message(char *msgnum_as_string) {
	long msgnum = 0L;

	msgnum = atol(msgnum_as_string);
	output_headers(0, 0, 0, 0, 0, 0);

	wprintf("Content-type: text/html\r\n"
		"Server: %s\r\n"
		"Connection: close\r\n",
		SERVER);
	begin_burst();

	wprintf("\r\n\r\n<html>\n"
		"<head><title>Printable view</title></head>\n"
		"<body onLoad=\" window.print(); window.close(); \">\n"
	);
	
	read_message(msgnum, 1, "");

	wprintf("\n</body></html>\n\n");
	wDumpContent(0);
}



/**
 * \brief Display a message's headers
 *
 * \param msgnum_as_string Message number, as a string instead of as a long int
 */
void display_headers(char *msgnum_as_string) {
	long msgnum = 0L;
	char buf[1024];

	msgnum = atol(msgnum_as_string);
	output_headers(0, 0, 0, 0, 0, 0);

	wprintf("Content-type: text/plain\r\n"
		"Server: %s\r\n"
		"Connection: close\r\n",
		SERVER);
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
	struct wc_attachment *att, *aptr;
	char m_subject[256];
	char from[256];
	char node[256];
	char rfca[256];
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
#ifdef HAVE_ICONV
				utf8ify_rfc822_string(from);
#endif
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
				wprintf("%s ", &buf[5]);
			}
			if (!strncasecmp(buf, "time=", 5)) {
				fmt_date(now, atol(&buf[5]), 0);
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

#ifdef HAVE_ICONV
		utf8ify_rfc822_string(m_subject);
#endif
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
		ic = ctdl_iconv_open("UTF-8", mime_charset);
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
			if (buf[len-1] == '\n') buf[--len] = 0;
			if (buf[len-1] == '\r') buf[--len] = 0;

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
			url(buf);
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
	}

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
		
				/* Create an attachment struct from this mime part... */
				att = malloc(sizeof(struct wc_attachment));
				memset(att, 0, sizeof(struct wc_attachment));
				att->length = mime_length;
				strcpy(att->content_type, mime_content_type);
				strcpy(att->filename, mime_filename);
				att->next = NULL;
				att->data = load_mimepart(msgnum, mime_partnum);
		
				/* And add it to the list. */
				if (WC->first_attachment == NULL) {
					WC->first_attachment = att;
				}
				else {
					aptr = WC->first_attachment;
					while (aptr->next != NULL) aptr = aptr->next;
					aptr->next = att;
				}
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

/**
 * \brief Display one row in the mailbox summary view
 *
 * \param num The row number to be displayed
 */
void display_summarized(int num) {
	char datebuf[64];

	wprintf("<tr id=\"m%ld\" style=\"font-weight:%s;\" "
		"onMouseDown=\"CtdlMoveMsgMouseDown(event,%ld)\">",
		WC->summ[num].msgnum,
		(WC->summ[num].is_new ? "bold" : "normal"),
		WC->summ[num].msgnum
	);

	wprintf("<td width=%d%%>", SUBJ_COL_WIDTH_PCT);
	escputs(WC->summ[num].subj);
	wprintf("</td>");

	wprintf("<td width=%d%%>", SENDER_COL_WIDTH_PCT);
	escputs(WC->summ[num].from);
	wprintf("</td>");

	wprintf("<td width=%d%%>", DATE_PLUS_BUTTONS_WIDTH_PCT);
	fmt_date(datebuf, WC->summ[num].date, 1);	/* brief */
	escputs(datebuf);
	wprintf("</td>");

	wprintf("</tr>\n");
}



/**
 * \brief display the adressbook overview
 * \param msgnum the citadel message number
 * \param alpha what????
 */
void display_addressbook(long msgnum, char alpha) {
	char buf[SIZ];
	char mime_partnum[SIZ];
	char mime_filename[SIZ];
	char mime_content_type[SIZ];
	char mime_disposition[SIZ];
	int mime_length;
	char vcard_partnum[SIZ];
	char *vcard_source = NULL;
	struct message_summary summ;

	memset(&summ, 0, sizeof(summ));
	safestrncpy(summ.subj, _("(no subject)"), sizeof summ.subj);

	sprintf(buf, "MSG0 %ld|1", msgnum);	/* ask for headers only */
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
		vcard_source = load_mimepart(msgnum, vcard_partnum);
		if (vcard_source != NULL) {

			/** Display the summary line */
			display_vcard(vcard_source, alpha, 0, NULL);

			/** If it's my vCard I can edit it */
			if (	(!strcasecmp(WC->wc_roomname, USERCONFIGROOM))
				|| (!strcasecmp(&WC->wc_roomname[11], USERCONFIGROOM))
				|| (WC->wc_view == VIEW_ADDRESSBOOK)
			) {
				wprintf("<a href=\"edit_vcard?"
					"msgnum=%ld?partnum=%s\">",
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
void fetch_ab_name(long msgnum, char *namebuf) {
	char buf[SIZ];
	char mime_partnum[SIZ];
	char mime_filename[SIZ];
	char mime_content_type[SIZ];
	char mime_disposition[SIZ];
	int mime_length;
	char vcard_partnum[SIZ];
	char *vcard_source = NULL;
	int i, len;
	struct message_summary summ;

	if (namebuf == NULL) return;
	strcpy(namebuf, "");

	memset(&summ, 0, sizeof(summ));
	safestrncpy(summ.subj, "(no subject)", sizeof summ.subj);

	sprintf(buf, "MSG0 %ld|0", msgnum);	/** unfortunately we need the mime info now */
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
		vcard_source = load_mimepart(msgnum, vcard_partnum);
		if (vcard_source != NULL) {

			/* Grab the name off the card */
			display_vcard(vcard_source, 0, 0, namebuf);

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



/**
 * \brief Record compare function for sorting address book indices
 * \param ab1 adressbook one
 * \param ab2 adressbook two
 */
int abcmp(const void *ab1, const void *ab2) {
	return(strcasecmp(
		(((const struct addrbookent *)ab1)->ab_name),
		(((const struct addrbookent *)ab2)->ab_name)
	));
}


/**
 * \brief Helper function for do_addrbook_view()
 * Converts a name into a three-letter tab label
 * \param tabbuf the tabbuffer to add name to
 * \param name the name to add to the tabbuffer
 */
void nametab(char *tabbuf, long len, char *name) {
	stresc(tabbuf, len, name, 0, 0);
	tabbuf[0] = toupper(tabbuf[0]);
	tabbuf[1] = tolower(tabbuf[1]);
	tabbuf[2] = tolower(tabbuf[2]);
	tabbuf[3] = 0;
}


/**
 * \brief Render the address book using info we gathered during the scan
 * \param addrbook the addressbook to render
 * \param num_ab the number of the addressbook
 */
void do_addrbook_view(struct addrbookent *addrbook, int num_ab) {
	int i = 0;
	int displayed = 0;
	int bg = 0;
	static int NAMESPERPAGE = 60;
	int num_pages = 0;
	int tabfirst = 0;
	char tabfirst_label[64];
	int tablast = 0;
	char tablast_label[64];
	char this_tablabel[64];
	int page = 0;
	char **tablabels;

	if (num_ab == 0) {
		wprintf("<br /><br /><br /><div align=\"center\"><i>");
		wprintf(_("This address book is empty."));
		wprintf("</i></div>\n");
		return;
	}

	if (num_ab > 1) {
		qsort(addrbook, num_ab, sizeof(struct addrbookent), abcmp);
	}

	num_pages = (num_ab / NAMESPERPAGE) + 1;

	tablabels = malloc(num_pages * sizeof (char *));
	if (tablabels == NULL) {
		wprintf("<br /><br /><br /><div align=\"center\"><i>");
		wprintf(_("An internal error has occurred."));
		wprintf("</i></div>\n");
		return;
	}

	for (i=0; i<num_pages; ++i) {
		tabfirst = i * NAMESPERPAGE;
		tablast = tabfirst + NAMESPERPAGE - 1;
		if (tablast > (num_ab - 1)) tablast = (num_ab - 1);
		nametab(tabfirst_label, 64, addrbook[tabfirst].ab_name);
		nametab(tablast_label, 64, addrbook[tablast].ab_name);
		sprintf(this_tablabel, "%s&nbsp;-&nbsp;%s", tabfirst_label, tablast_label);
		tablabels[i] = strdup(this_tablabel);
	}

	tabbed_dialog(num_pages, tablabels);
	page = (-1);

	for (i=0; i<num_ab; ++i) {

		if ((i / NAMESPERPAGE) != page) {	/* New tab */
			page = (i / NAMESPERPAGE);
			if (page > 0) {
				wprintf("</tr></table>\n");
				end_tab(page-1, num_pages);
			}
			begin_tab(page, num_pages);
			wprintf("<table border=0 cellspacing=0 cellpadding=3 width=100%%>\n");
			displayed = 0;
		}

		if ((displayed % 4) == 0) {
			if (displayed > 0) {
				wprintf("</tr>\n");
			}
			bg = 1 - bg;
			wprintf("<tr bgcolor=\"#%s\">",
				(bg ? "DDDDDD" : "FFFFFF")
			);
		}
	
		wprintf("<td>");

		wprintf("<a href=\"readfwd?startmsg=%ld&is_singlecard=1",
			addrbook[i].ab_msgnum);
		wprintf("?maxmsgs=1?summary=0?alpha=%s\">", bstr("alpha"));
		vcard_n_prettyize(addrbook[i].ab_name);
		escputs(addrbook[i].ab_name);
		wprintf("</a></td>\n");
		++displayed;
	}

	wprintf("</tr></table>\n");
	end_tab((num_pages-1), num_pages);

	for (i=0; i<num_pages; ++i) {
		free(tablabels[i]);
	}
	free(tablabels);
}



/**
 * \brief load message pointers from the server
 * \param servcmd the citadel command to send to the citserver
 * \param with_headers what headers???
 */
int load_msg_ptrs(char *servcmd, int with_headers)
{
	char buf[1024];
	time_t datestamp;
	char fullname[128];
	char nodename[128];
	char inetaddr[128];
	char subject[256];
	int nummsgs;
	int maxload = 0;

	int num_summ_alloc = 0;

	if (WC->summ != NULL) {
		free(WC->summ);
		WC->num_summ = 0;
		WC->summ = NULL;
	}
	num_summ_alloc = 100;
	WC->num_summ = 0;
	WC->summ = malloc(num_summ_alloc * sizeof(struct message_summary));

	nummsgs = 0;
	maxload = sizeof(WC->msgarr) / sizeof(long) ;
	serv_puts(servcmd);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		return (nummsgs);
	}
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		if (nummsgs < maxload) {
			WC->msgarr[nummsgs] = extract_long(buf, 0);
			datestamp = extract_long(buf, 1);
			extract_token(fullname, buf, 2, '|', sizeof fullname);
			extract_token(nodename, buf, 3, '|', sizeof nodename);
			extract_token(inetaddr, buf, 4, '|', sizeof inetaddr);
			extract_token(subject, buf, 5, '|', sizeof subject);
			++nummsgs;

			if (with_headers) {
				if (nummsgs > num_summ_alloc) {
					num_summ_alloc *= 2;
					WC->summ = realloc(WC->summ,
						num_summ_alloc * sizeof(struct message_summary));
				}
				++WC->num_summ;

				memset(&WC->summ[nummsgs-1], 0, sizeof(struct message_summary));
				WC->summ[nummsgs-1].msgnum = WC->msgarr[nummsgs-1];
				safestrncpy(WC->summ[nummsgs-1].subj,
					_("(no subject)"), sizeof WC->summ[nummsgs-1].subj);
				if (!IsEmptyStr(fullname)) {
					safestrncpy(WC->summ[nummsgs-1].from,
						fullname, sizeof WC->summ[nummsgs-1].from);
				}
				if (!IsEmptyStr(subject)) {
				safestrncpy(WC->summ[nummsgs-1].subj, subject,
					sizeof WC->summ[nummsgs-1].subj);
				}
#ifdef HAVE_ICONV
				/** Handle subjects with RFC2047 encoding */
				utf8ify_rfc822_string(WC->summ[nummsgs-1].subj);
#endif
				if (strlen(WC->summ[nummsgs-1].subj) > 75) {
					strcpy(&WC->summ[nummsgs-1].subj[72], "...");
				}

				if (!IsEmptyStr(nodename)) {
					if ( ((WC->room_flags & QR_NETWORK)
					   || ((strcasecmp(nodename, serv_info.serv_nodename)
					   && (strcasecmp(nodename, serv_info.serv_fqdn)))))
					) {
						strcat(WC->summ[nummsgs-1].from, " @ ");
						strcat(WC->summ[nummsgs-1].from, nodename);
					}
				}

				WC->summ[nummsgs-1].date = datestamp;
	
#ifdef HAVE_ICONV
				/** Handle senders with RFC2047 encoding */
				utf8ify_rfc822_string(WC->summ[nummsgs-1].from);
#endif
				if (strlen(WC->summ[nummsgs-1].from) > 25) {
					strcpy(&WC->summ[nummsgs-1].from[22], "...");
				}
			}
		}
	}
	return (nummsgs);
}

/**
 * \brief qsort() compatible function to compare two longs in descending order.
 *
 * \param s1 first number to compare 
 * \param s2 second number to compare
 */
int longcmp_r(const void *s1, const void *s2) {
	long l1;
	long l2;

	l1 = *(long *)s1;
	l2 = *(long *)s2;

	if (l1 > l2) return(-1);
	if (l1 < l2) return(+1);
	return(0);
}

 
/**
 * \brief qsort() compatible function to compare two message summary structs by ascending subject.
 *
 * \param s1 first item to compare 
 * \param s2 second item to compare
 */
int summcmp_subj(const void *s1, const void *s2) {
	struct message_summary *summ1;
	struct message_summary *summ2;
	
	summ1 = (struct message_summary *)s1;
	summ2 = (struct message_summary *)s2;
	return strcasecmp(summ1->subj, summ2->subj);
}

/**
 * \brief qsort() compatible function to compare two message summary structs by descending subject.
 *
 * \param s1 first item to compare 
 * \param s2 second item to compare
 */
int summcmp_rsubj(const void *s1, const void *s2) {
	struct message_summary *summ1;
	struct message_summary *summ2;
	
	summ1 = (struct message_summary *)s1;
	summ2 = (struct message_summary *)s2;
	return strcasecmp(summ2->subj, summ1->subj);
}

/**
 * \brief qsort() compatible function to compare two message summary structs by ascending sender.
 *
 * \param s1 first item to compare 
 * \param s2 second item to compare
 */
int summcmp_sender(const void *s1, const void *s2) {
	struct message_summary *summ1;
	struct message_summary *summ2;
	
	summ1 = (struct message_summary *)s1;
	summ2 = (struct message_summary *)s2;
	return strcasecmp(summ1->from, summ2->from);
}

/**
 * \brief qsort() compatible function to compare two message summary structs by descending sender.
 *
 * \param s1 first item to compare 
 * \param s2 second item to compare
 */
int summcmp_rsender(const void *s1, const void *s2) {
	struct message_summary *summ1;
	struct message_summary *summ2;
	
	summ1 = (struct message_summary *)s1;
	summ2 = (struct message_summary *)s2;
	return strcasecmp(summ2->from, summ1->from);
}

/**
 * \brief qsort() compatible function to compare two message summary structs by ascending date.
 *
 * \param s1 first item to compare 
 * \param s2 second item to compare
 */
int summcmp_date(const void *s1, const void *s2) {
	struct message_summary *summ1;
	struct message_summary *summ2;
	
	summ1 = (struct message_summary *)s1;
	summ2 = (struct message_summary *)s2;

	if (summ1->date < summ2->date) return -1;
	else if (summ1->date > summ2->date) return +1;
	else return 0;
}

/**
 * \brief qsort() compatible function to compare two message summary structs by descending date.
 *
 * \param s1 first item to compare 
 * \param s2 second item to compare
 */
int summcmp_rdate(const void *s1, const void *s2) {
	struct message_summary *summ1;
	struct message_summary *summ2;
	
	summ1 = (struct message_summary *)s1;
	summ2 = (struct message_summary *)s2;

	if (summ1->date < summ2->date) return +1;
	else if (summ1->date > summ2->date) return -1;
	else return 0;
}



/**
 * \brief command loop for reading messages
 *
 * \param oper Set to "readnew" or "readold" or "readfwd" or "headers"
 */
void readloop(char *oper)
{
	char cmd[256];
	char buf[SIZ];
	char old_msgs[SIZ];
	int a, b;
	int nummsgs;
	long startmsg;
	int maxmsgs;
	long *displayed_msgs = NULL;
	int num_displayed = 0;
	int is_summary = 0;
	int is_addressbook = 0;
	int is_singlecard = 0;
	int is_calendar = 0;
	int is_tasks = 0;
	int is_notes = 0;
	int is_bbview = 0;
	int lo, hi;
	int lowest_displayed = (-1);
	int highest_displayed = 0;
	struct addrbookent *addrbook = NULL;
	int num_ab = 0;
	char *sortby = NULL;
	char sortpref_name[128];
	char sortpref_value[128];
	char *subjsort_button;
	char *sendsort_button;
	char *datesort_button;
	int bbs_reverse = 0;

	if (WC->wc_view == VIEW_WIKI) {
		sprintf(buf, "wiki?room=%s?page=home", WC->wc_roomname);
		http_redirect(buf);
		return;
	}

	startmsg = atol(bstr("startmsg"));
	maxmsgs = atoi(bstr("maxmsgs"));
	is_summary = atoi(bstr("summary"));
	if (maxmsgs == 0) maxmsgs = DEFAULT_MAXMSGS;

	snprintf(sortpref_name, sizeof sortpref_name, "sort %s", WC->wc_roomname);
	get_preference(sortpref_name, sortpref_value, sizeof sortpref_value);

	sortby = bstr("sortby");
	if ( (!IsEmptyStr(sortby)) && (strcasecmp(sortby, sortpref_value)) ) {
		set_preference(sortpref_name, sortby, 1);
	}
	if (IsEmptyStr(sortby)) sortby = sortpref_value;

	/** mailbox sort */
	if (IsEmptyStr(sortby)) sortby = "rdate";

	/** message board sort */
	if (!strcasecmp(sortby, "reverse")) {
		bbs_reverse = 1;
	}
	else {
		bbs_reverse = 0;
	}

	output_headers(1, 1, 1, 0, 0, 0);

	/**
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
		sprintf(cmd, "MSGS SEARCH|%s", bstr("query"));
	}
	else {
		strcpy(cmd, "MSGS ALL");
	}

	if ((WC->wc_view == VIEW_MAILBOX) && (maxmsgs > 1)) {
		is_summary = 1;
		if (!strcmp(oper, "do_search")) {
			sprintf(cmd, "MSGS SEARCH|%s", bstr("query"));
		}
		else {
			strcpy(cmd, "MSGS ALL");
		}
	}

	if ((WC->wc_view == VIEW_ADDRESSBOOK) && (maxmsgs > 1)) {
		is_addressbook = 1;
		if (!strcmp(oper, "do_search")) {
			sprintf(cmd, "MSGS SEARCH|%s", bstr("query"));
		}
		else {
			strcpy(cmd, "MSGS ALL");
		}
		maxmsgs = 9999999;
	}

	if (is_summary) {			/**< fetch header summary */
		snprintf(cmd, sizeof cmd, "MSGS %s|%s||1",
			(!strcmp(oper, "do_search") ? "SEARCH" : "ALL"),
			(!strcmp(oper, "do_search") ? bstr("query") : "")
		);
		startmsg = 1;
		maxmsgs = 9999999;
	}

	/**
	 * Are we doing a summary view?  If so, we need to know old messages
	 * and new messages, so we can do that pretty boldface thing for the
	 * new messages.
	 */
	strcpy(old_msgs, "");
	if (is_summary) {
		serv_puts("GTSN");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			strcpy(old_msgs, &buf[4]);
		}
	}

	is_singlecard = atoi(bstr("is_singlecard"));

	if (WC->wc_default_view == VIEW_CALENDAR) {		/**< calendar */
		is_calendar = 1;
		strcpy(cmd, "MSGS ALL");
		maxmsgs = 32767;
	}
	if (WC->wc_default_view == VIEW_TASKS) {		/**< tasks */
		is_tasks = 1;
		strcpy(cmd, "MSGS ALL");
		maxmsgs = 32767;
	}
	if (WC->wc_default_view == VIEW_NOTES) {		/**< notes */
		is_notes = 1;
		strcpy(cmd, "MSGS ALL");
		maxmsgs = 32767;
	}

	if (is_notes) {
		wprintf("<div align=center>%s</div>\n", _("Click on any note to edit it."));
		wprintf("<div id=\"new_notes_here\"></div>\n");
	}

	nummsgs = load_msg_ptrs(cmd, is_summary);
	if (nummsgs == 0) {

		if ((!is_tasks) && (!is_calendar) && (!is_notes) && (!is_addressbook)) {
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

	if (is_summary) {
		for (a = 0; a < nummsgs; ++a) {
			/** Are you a new message, or an old message? */
			if (is_summary) {
				if (is_msg_in_mset(old_msgs, WC->msgarr[a])) {
					WC->summ[a].is_new = 0;
				}
				else {
					WC->summ[a].is_new = 1;
				}
			}
		}
	}

	if (startmsg == 0L) {
		if (bbs_reverse) {
			startmsg = WC->msgarr[(nummsgs >= maxmsgs) ? (nummsgs - maxmsgs) : 0];
		}
		else {
			startmsg = WC->msgarr[0];
		}
	}

	if (is_summary) {
		if (!strcasecmp(sortby, "subject")) {
			qsort(WC->summ, WC->num_summ,
				sizeof(struct message_summary), summcmp_subj);
		}
		else if (!strcasecmp(sortby, "rsubject")) {
			qsort(WC->summ, WC->num_summ,
				sizeof(struct message_summary), summcmp_rsubj);
		}
		else if (!strcasecmp(sortby, "sender")) {
			qsort(WC->summ, WC->num_summ,
				sizeof(struct message_summary), summcmp_sender);
		}
		else if (!strcasecmp(sortby, "rsender")) {
			qsort(WC->summ, WC->num_summ,
				sizeof(struct message_summary), summcmp_rsender);
		}
		else if (!strcasecmp(sortby, "date")) {
			qsort(WC->summ, WC->num_summ,
				sizeof(struct message_summary), summcmp_date);
		}
		else if (!strcasecmp(sortby, "rdate")) {
			qsort(WC->summ, WC->num_summ,
				sizeof(struct message_summary), summcmp_rdate);
		}
	}

	if (!strcasecmp(sortby, "subject")) {
		subjsort_button = "<a href=\"readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=rsubject\"><img border=\"0\" src=\"static/down_pointer.gif\" /></a>" ;
	}
	else if (!strcasecmp(sortby, "rsubject")) {
		subjsort_button = "<a href=\"readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=subject\"><img border=\"0\" src=\"static/up_pointer.gif\" /></a>" ;
	}
	else {
		subjsort_button = "<a href=\"readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=subject\"><img border=\"0\" src=\"static/sort_none.gif\" /></a>" ;
	}

	if (!strcasecmp(sortby, "sender")) {
		sendsort_button = "<a href=\"readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=rsender\"><img border=\"0\" src=\"static/down_pointer.gif\" /></a>" ;
	}
	else if (!strcasecmp(sortby, "rsender")) {
		sendsort_button = "<a href=\"readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=sender\"><img border=\"0\" src=\"static/up_pointer.gif\" /></a>" ;
	}
	else {
		sendsort_button = "<a href=\"readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=sender\"><img border=\"0\" src=\"static/sort_none.gif\" /></a>" ;
	}

	if (!strcasecmp(sortby, "date")) {
		datesort_button = "<a href=\"readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=rdate\"><img border=\"0\" src=\"static/down_pointer.gif\" /></a>" ;
	}
	else if (!strcasecmp(sortby, "rdate")) {
		datesort_button = "<a href=\"readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=date\"><img border=\"0\" src=\"static/up_pointer.gif\" /></a>" ;
	}
	else {
		datesort_button = "<a href=\"readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=rdate\"><img border=\"0\" src=\"static/sort_none.gif\" /></a>" ;
	}

	if (is_summary) {

		wprintf("<script language=\"javascript\" type=\"text/javascript\">"
			" document.onkeydown = CtdlMsgListKeyPress;	"
			" if (document.layers) {			"
			"	document.captureEvents(Event.KEYPRESS);	"
			" }						"
			"</script>\n"
		);

		/** note that Date and Delete are now in the same column */
		wprintf("<div id=\"message_list_hdr\">"
			"<div class=\"fix_scrollbar_bug\">"
			"<table cellspacing=0 style=\"width:100%%\">"
			"<tr>"
		);
		wprintf("<th width=%d%%>%s %s</th>"
			"<th width=%d%%>%s %s</th>"
			"<th width=%d%%>%s %s"
			"&nbsp;"
			"<input type=\"submit\" name=\"delete_button\" id=\"delbutton\" "
			" onClick=\"CtdlDeleteSelectedMessages(event)\" "
			" value=\"%s\">"
			"</th>"
			"</tr>\n"
			,
			SUBJ_COL_WIDTH_PCT,
			_("Subject"),	subjsort_button,
			SENDER_COL_WIDTH_PCT,
			_("Sender"),	sendsort_button,
			DATE_PLUS_BUTTONS_WIDTH_PCT,
			_("Date"),	datesort_button,
			_("Delete")
		);
		wprintf("</table></div></div>\n");

		wprintf("<div id=\"message_list\">"

			"<div class=\"fix_scrollbar_bug\">\n"

			"<table class=\"mailbox_summary\" id=\"summary_headers\" "
			"cellspacing=0 style=\"width:100%%;-moz-user-select:none;\">"
		);
	}


	/**
	 * Set the "is_bbview" variable if it appears that we are looking at
	 * a classic bulletin board view.
	 */
	if ((!is_tasks) && (!is_calendar) && (!is_addressbook)
	      && (!is_notes) && (!is_singlecard) && (!is_summary)) {
		is_bbview = 1;
	}

	/**
	 * If we're not currently looking at ALL requested
	 * messages, then display the selector bar
	 */
	if (is_bbview) {
		/** begin bbview scroller */
		wprintf("<form name=\"msgomatictop\" class=\"selector_top\" >");
		wprintf(_("Reading #"), lowest_displayed, highest_displayed);

		wprintf("<select name=\"whichones\" size=\"1\" "
			"OnChange=\"location.href=msgomatictop.whichones.options"
			"[selectedIndex].value\">\n");

		if (bbs_reverse) {
			for (b=nummsgs-1; b>=0; b = b - maxmsgs) {
				hi = b + 1;
				lo = b - maxmsgs + 2;
				if (lo < 1) lo = 1;
				wprintf("<option %s value="
					"\"%s"
					"?startmsg=%ld"
					"?maxmsgs=%d"
					"?summary=%d\">"
					"%d-%d</option> \n",
					((WC->msgarr[lo-1] == startmsg) ? "selected" : ""),
					oper,
					WC->msgarr[lo-1],
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
				wprintf("<option %s value="
					"\"%s"
					"?startmsg=%ld"
					"?maxmsgs=%d"
					"?summary=%d\">"
					"%d-%d</option> \n",
					((WC->msgarr[b] == startmsg) ? "selected" : ""),
					oper,
					WC->msgarr[lo-1],
					maxmsgs,
					is_summary,
					lo, hi);
			}
		}

		wprintf("<option value=\"%s?startmsg=%ld"
			"?maxmsgs=9999999?summary=%d\">",
			oper,
			WC->msgarr[0], is_summary);
		wprintf(_("All"));
		wprintf("</option>");
		wprintf("</select> ");
		wprintf(_("of %d messages."), nummsgs);

		/** forward/reverse */
		wprintf("<input type=\"radio\" %s name=\"direction\" value=\"\""
			"OnChange=\"location.href='%s?sortby=forward'\"",  
			(bbs_reverse ? "" : "checked"),
			oper
		);
		wprintf(">");
		wprintf(_("oldest to newest"));
		wprintf("&nbsp;&nbsp;&nbsp;&nbsp;");

		wprintf("<input type=\"radio\" %s name=\"direction\" value=\"\""
			"OnChange=\"location.href='%s?sortby=reverse'\"", 
			(bbs_reverse ? "checked" : ""),
			oper
		);
		wprintf(">");
		wprintf(_("newest to oldest"));
		wprintf("\n");
	
		wprintf("</select></form>\n");
		/** end bbview scroller */
	}



	for (a = 0; a < nummsgs; ++a) {
		if ((WC->msgarr[a] >= startmsg) && (num_displayed < maxmsgs)) {

			/** Display the message */
			if (is_summary) {
				display_summarized(a);
			}
			else if (is_addressbook) {
				fetch_ab_name(WC->msgarr[a], buf);
				++num_ab;
				addrbook = realloc(addrbook,
					(sizeof(struct addrbookent) * num_ab) );
				safestrncpy(addrbook[num_ab-1].ab_name, buf,
					sizeof(addrbook[num_ab-1].ab_name));
				addrbook[num_ab-1].ab_msgnum = WC->msgarr[a];
			}
			else if (is_calendar) {
				display_calendar(WC->msgarr[a]);
			}
			else if (is_tasks) {
				display_task(WC->msgarr[a]);
			}
			else if (is_notes) {
				display_note(WC->msgarr[a]);
			}
			else {
				if (displayed_msgs == NULL) {
					displayed_msgs = malloc(sizeof(long) *
								(maxmsgs<nummsgs ? maxmsgs : nummsgs));
				}
				displayed_msgs[num_displayed] = WC->msgarr[a];
			}

			if (lowest_displayed < 0) lowest_displayed = a;
			highest_displayed = a;

			++num_displayed;
		}
	}


	/** Output loop */
	if (displayed_msgs != NULL) {
		if (bbs_reverse) {
			qsort(displayed_msgs, num_displayed, sizeof(long), longcmp_r);
		}

		/** if we do a split bbview in the future, begin messages div here */

		for (a=0; a<num_displayed; ++a) {
			read_message(displayed_msgs[a], 0, "");
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
		wprintf("<form name=\"msgomatic\" class=\"selector_bottom\" >");
		wprintf(_("Reading #"), lowest_displayed, highest_displayed);

		wprintf("<select name=\"whichones\" size=\"1\" "
			"OnChange=\"location.href=msgomatic.whichones.options"
			"[selectedIndex].value\">\n");

		if (bbs_reverse) {
			for (b=nummsgs-1; b>=0; b = b - maxmsgs) {
				hi = b + 1;
				lo = b - maxmsgs + 2;
				if (lo < 1) lo = 1;
				wprintf("<option %s value="
					"\"%s"
					"?startmsg=%ld"
					"?maxmsgs=%d"
					"?summary=%d\">"
					"%d-%d</option> \n",
					((WC->msgarr[lo-1] == startmsg) ? "selected" : ""),
					oper,
					WC->msgarr[lo-1],
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
				wprintf("<option %s value="
					"\"%s"
					"?startmsg=%ld"
					"?maxmsgs=%d"
					"?summary=%d\">"
					"%d-%d</option> \n",
					((WC->msgarr[b] == startmsg) ? "selected" : ""),
					oper,
					WC->msgarr[lo-1],
					maxmsgs,
					is_summary,
					lo, hi);
			}
		}

		wprintf("<option value=\"%s?startmsg=%ld"
			"?maxmsgs=9999999?summary=%d\">",
			oper,
			WC->msgarr[0], is_summary);
		wprintf(_("All"));
		wprintf("</option>");
		wprintf("</select> ");
		wprintf(_("of %d messages."), nummsgs);

		/** forward/reverse */
		wprintf("<input type=\"radio\" %s name=\"direction\" value=\"\""
			"OnChange=\"location.href='%s?sortby=forward'\"",  
			(bbs_reverse ? "" : "checked"),
			oper
		);
		wprintf(">");
		wprintf(_("oldest to newest"));
		wprintf("&nbsp;&nbsp;&nbsp;&nbsp;");
		wprintf("<input type=\"radio\" %s name=\"direction\" value=\"\""
			"OnChange=\"location.href='%s?sortby=reverse'\"", 
			(bbs_reverse ? "checked" : ""),
			oper
		);
		wprintf(">");
		wprintf(_("newest to oldest"));
		wprintf("\n");

		wprintf("</form>\n");
		/** end bbview scroller */
	}

DONE:
	if (is_tasks) {
		do_tasks_view();	/** Render the task list */
	}

	if (is_calendar) {
		do_calendar_view();	/** Render the calendar */
	}

	if (is_addressbook) {
		do_addrbook_view(addrbook, num_ab);	/** Render the address book */
	}

	/** Note: wDumpContent() will output one additional </div> tag. */
	wprintf("</div>\n");		/** end of 'content' div */
	wDumpContent(1);

	/** free the summary */
	if (WC->summ != NULL) {
		free(WC->summ);
		WC->num_summ = 0;
		WC->summ = NULL;
	}
	if (addrbook != NULL) free(addrbook);
}


/**
 * \brief Back end for post_message()
 * ... this is where the actual message gets transmitted to the server.
 */
void post_mime_to_server(void) {
	char boundary[SIZ];
	int is_multipart = 0;
	static int seq = 0;
	struct wc_attachment *att;
	char *encoded;
	size_t encoded_length;
	size_t encoded_strlen;

	/** RFC2045 requires this, and some clients look for it... */
	serv_puts("MIME-Version: 1.0");
	serv_puts("X-Mailer: " SERVER);

	/** If there are attachments, we have to do multipart/mixed */
	if (WC->first_attachment != NULL) {
		is_multipart = 1;
	}

	if (is_multipart) {
		sprintf(boundary, "Citadel--Multipart--%s--%04x--%04x",
			serv_info.serv_fqdn,
			getpid(),
			++seq
		);

		/** Remember, serv_printf() appends an extra newline */
		serv_printf("Content-type: multipart/mixed; "
			"boundary=\"%s\"\n", boundary);
		serv_printf("This is a multipart message in MIME format.\n");
		serv_printf("--%s", boundary);
	}

	serv_puts("Content-type: text/html; charset=utf-8");
	serv_puts("Content-Transfer-Encoding: quoted-printable");
	serv_puts("");
	serv_puts("<html><body>\r\n");
	text_to_server_qp(bstr("msgtext"));	/** Transmit message in quoted-printable encoding */
	serv_puts("</body></html>\r\n");
	
	if (is_multipart) {

		/** Add in the attachments */
		for (att = WC->first_attachment; att!=NULL; att=att->next) {

			encoded_length = ((att->length * 150) / 100);
			encoded = malloc(encoded_length);
			if (encoded == NULL) break;
			encoded_strlen = CtdlEncodeBase64(&encoded, att->data, att->length, &encoded_length, 1);

			serv_printf("--%s", boundary);
			serv_printf("Content-type: %s", att->content_type);
			serv_printf("Content-disposition: attachment; "
				"filename=\"%s\"", att->filename);
			serv_puts("Content-transfer-encoding: base64");
			serv_puts("");
			serv_write(encoded, encoded_strlen);
			serv_puts("");
			serv_puts("");
			free(encoded);
		}
		serv_printf("--%s--", boundary);
	}

	serv_puts("000");
}


/**
 * \brief Post message (or don't post message)
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
	char encoded_subject[1024];
	static long dont_post = (-1L);
	struct wc_attachment *att, *aptr;
	int is_anonymous = 0;
	char *display_name;

	display_name = bstr("display_name");
	if (!strcmp(display_name, "__ANONYMOUS__")) {
		display_name = "";
		is_anonymous = 1;
	}

	if (WC->upload_length > 0) {

		lprintf(9, "%s:%d: we are uploading %d bytes\n", __FILE__, __LINE__, WC->upload_length);
		/** There's an attachment.  Save it to this struct... */
		att = malloc(sizeof(struct wc_attachment));
		memset(att, 0, sizeof(struct wc_attachment));
		att->length = WC->upload_length;
		strcpy(att->content_type, WC->upload_content_type);
		strcpy(att->filename, WC->upload_filename);
		att->next = NULL;

		/** And add it to the list. */
		if (WC->first_attachment == NULL) {
			WC->first_attachment = att;
		}
		else {
			aptr = WC->first_attachment;
			while (aptr->next != NULL) aptr = aptr->next;
			aptr->next = att;
		}

		/**
		 * Mozilla sends a simple filename, which is what we want,
		 * but Satan's Browser sends an entire pathname.  Reduce
		 * the path to just a filename if we need to.
		 */
		while (num_tokens(att->filename, '/') > 1) {
			remove_token(att->filename, 0, '/');
		}
		while (num_tokens(att->filename, '\\') > 1) {
			remove_token(att->filename, 0, '\\');
		}

		/**
		 * Transfer control of this memory from the upload struct
		 * to the attachment struct.
		 */
		att->data = WC->upload;
		WC->upload_length = 0;
		WC->upload = NULL;
		display_enter();
		return;
	}

	if (!IsEmptyStr(bstr("cancel_button"))) {
		sprintf(WC->ImportantMessage, 
			_("Cancelled.  Message was not posted."));
	} else if (!IsEmptyStr(bstr("attach_button"))) {
		display_enter();
		return;
	} else if (atol(bstr("postseq")) == dont_post) {
		sprintf(WC->ImportantMessage, 
			_("Automatically cancelled because you have already "
			"saved this message."));
	} else {
		rfc2047encode(encoded_subject, sizeof encoded_subject, bstr("subject"));
		sprintf(buf, "ENT0 1|%s|%d|4|%s|%s||%s|%s|%s|%s",
			bstr("recp"),
			is_anonymous,
			encoded_subject,
			display_name,
			bstr("cc"),
			bstr("bcc"),
			bstr("wikipage"),
			bstr("my_email_addr")
		);
		serv_puts(buf);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '4') {
			post_mime_to_server();
			if (  (!IsEmptyStr(bstr("recp")))
			   || (!IsEmptyStr(bstr("cc"  )))
			   || (!IsEmptyStr(bstr("bcc" )))
			) {
				sprintf(WC->ImportantMessage, _("Message has been sent.\n"));
			}
			else {
				sprintf(WC->ImportantMessage, _("Message has been posted.\n"));
			}
			dont_post = atol(bstr("postseq"));
		} else {
			lprintf(9, "%s:%d: server post error: %s\n", __FILE__, __LINE__, buf);
			sprintf(WC->ImportantMessage, "%s", &buf[4]);
			display_enter();
			return;
		}
	}

	free_attachments(WC);

	/**
	 *  We may have been supplied with instructions regarding the location
	 *  to which we must return after posting.  If found, go there.
	 */
	if (!IsEmptyStr(bstr("return_to"))) {
		http_redirect(bstr("return_to"));
	}
	/**
	 *  If we were editing a page in a wiki room, go to that page now.
	 */
	else if (!IsEmptyStr(bstr("wikipage"))) {
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
	char ebuf[SIZ];
	long now;
	char *display_name;
	struct wc_attachment *att;
	int recipient_required = 0;
	int subject_required = 0;
	int recipient_bad = 0;
	int i;
	int is_anonymous = 0;
	long existing_page = (-1L);

	now = time(NULL);

	if (!IsEmptyStr(bstr("force_room"))) {
		gotoroom(bstr("force_room"));
	}

	display_name = bstr("display_name");
	if (!strcmp(display_name, "__ANONYMOUS__")) {
		display_name = "";
		is_anonymous = 1;
	}

	/** First test to see whether this is a room that requires recipients to be entered */
	serv_puts("ENT0 0");
	serv_getln(buf, sizeof buf);

	if (!strncmp(buf, "570", 3)) {		/** 570 means that we need a recipient here */
		recipient_required = 1;
	}
	else if (buf[0] != '2') {		/** Any other error means that we cannot continue */
		sprintf(WC->ImportantMessage, "%s", &buf[4]);
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
	if (WC->wc_default_view == VIEW_ADDRESSBOOK) {
		do_edit_vcard(-1, "", "");
		return;
	}

#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	/**
	 * Are we perhaps in a calendar room?  If so, then an "enter
	 * message" command really means "add new calendar item."
	 */
	if (WC->wc_default_view == VIEW_CALENDAR) {
		display_edit_event();
		return;
	}

	/**
	 * Are we perhaps in a tasks view?  If so, then an "enter
	 * message" command really means "add new task."
	 */
	if (WC->wc_default_view == VIEW_TASKS) {
		display_edit_task();
		return;
	}
#endif

	/**
	 * Otherwise proceed normally.
	 * Do a custom room banner with no navbar...
	 */
	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	embed_room_banner(NULL, navbar_none);
	wprintf("</div>\n");
	wprintf("<div id=\"content\">\n"
		"<div class=\"fix_scrollbar_bug message \">");

	/** Now check our actual recipients if there are any */
	if (recipient_required) {
		sprintf(buf, "ENT0 0|%s|%d|0||%s||%s|%s|%s",
			bstr("recp"),
			is_anonymous,
			display_name,
			bstr("cc"), bstr("bcc"), bstr("wikipage"));
		serv_puts(buf);
		serv_getln(buf, sizeof buf);

		if (!strncmp(buf, "570", 3)) {	/** 570 means we have an invalid recipient listed */
			if (!IsEmptyStr(bstr("recp")) && 
			    !IsEmptyStr(bstr("cc"  )) && 
			    !IsEmptyStr(bstr("bcc" ))) {
				recipient_bad = 1;
			}
		}
		else if (buf[0] != '2') {	/** Any other error means that we cannot continue */
			wprintf("<em>%s</em><br />\n", &buf[4]);
			goto DONE;
		}
	}

	/** If we got this far, we can display the message entry screen. */

	/** begin message entry screen */
	wprintf("<form "
		"enctype=\"multipart/form-data\" "
		"method=\"POST\" "
		"accept-charset=\"UTF-8\" "
		"action=\"post\" "
		"name=\"enterform\""
		">\n");
	wprintf("<input type=\"hidden\" name=\"postseq\" value=\"%ld\">\n", now);
	if (WC->wc_view == VIEW_WIKI) {
		wprintf("<input type=\"hidden\" name=\"wikipage\" value=\"%s\">\n", bstr("wikipage"));
	}
	wprintf("<input type=\"hidden\" name=\"return_to\" value=\"%s\">\n", bstr("return_to"));
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%ld\">\n", WC->nonce);

	/** submit or cancel buttons */
        wprintf("<p class=\"send_edit_msg\">");
        wprintf("<input type=\"submit\" name=\"send_button\" value=\"");
        if (recipient_required) {
                wprintf(_("Send message"));
        } else {
                wprintf(_("Post message"));
        }
        wprintf("\">&nbsp;"
                "<input type=\"submit\" name=\"cancel_button\" value=\"%s\">\n", _("Cancel"));
        wprintf("</p>");

	/** header bar */

	wprintf("<img src=\"static/newmess3_24x.gif\" class=\"imgedit\">");
	wprintf("  ");	/** header bar */
	fmt_date(buf, now, 0);
	wprintf("%s", buf);
	wprintf("\n");	/** header bar */

	wprintf("<div>");
	wprintf("<label for=\"from_id\" > ");
	wprintf(_(" <I>from</I> "));
	wprintf("</label>");

	/* Allow the user to select any of his valid screen names */

	wprintf("<select name=\"display_name\" size=1 id=\"from_id\">\n");

	serv_puts("GVSN");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			wprintf("<option %s value=\"",
				((!strcasecmp(bstr("display_name"), buf)) ? "selected" : "")
			);
			escputs(buf);
			wprintf("\">");
			escputs(buf);
			wprintf("</option>\n");
		}
	}

	if (WC->room_flags & QR_ANONOPT) {
		wprintf("<option %s value=\"__ANONYMOUS__\">%s</option>\n",
			((!strcasecmp(bstr("__ANONYMOUS__"), WC->wc_fullname)) ? "selected" : ""),
			_("Anonymous")
		);
	}

	wprintf("</select>\n");

	/* If this is an email (not a post), allow the user to select any of his
	 * valid email addresses.
	 */
	if (recipient_required) {
		serv_puts("GVEA");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1') {
			wprintf("<select name=\"my_email_addr\" size=1>\n");
			while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				wprintf("<option value=\"");
				escputs(buf);
				wprintf("\">&lt;");
				escputs(buf);
				wprintf("&gt;</option>\n");
			}
			wprintf("</select>\n");
		}
	}

	wprintf(_(" <I>in</I> "));
	escputs(WC->wc_roomname);
	wprintf("</div>");

	if (recipient_required) {

		wprintf("<div style=\"float: left;\"><label for=\"recp_id\"> ");
		wprintf(_("To:"));
		wprintf("</label>"
			"<input autocomplete=\"off\" type=\"text\" name=\"recp\" id=\"recp_id\" value=\"");
		escputs(bstr("recp"));
		wprintf("\" size=45 maxlength=1000 />");
		wprintf("<div class=\"auto_complete\" id=\"recp_name_choices\"></div>");


		wprintf("<br/><label for=\"cc_id\"> ");
		wprintf(_("CC:"));
		wprintf("</label>"
			"<input autocomplete=\"off\" type=\"text\" name=\"cc\" id=\"cc_id\" value=\"");
		escputs(bstr("cc"));
		wprintf("\" size=45 maxlength=1000 />");
		wprintf("<div class=\"auto_complete\" id=\"cc_name_choices\"></div>");
		wprintf("<br/><label for=\"bcc_id\"> ");
		wprintf(_("BCC:"));
		wprintf("</label>"
			"<input autocomplete=\"off\" type=\"text\" name=\"bcc\" id=\"bcc_id\" value=\"");
		escputs(bstr("bcc"));
		wprintf("\" size=45 maxlength=1000 />");
		wprintf("<div class=\"auto_complete\" id=\"bcc_name_choices\"></div>");

		/** Initialize the autocomplete ajax helpers (found in wclib.js) */
		wprintf("<script type=\"text/javascript\">	\n"
			" activate_entmsg_autocompleters();	\n"
			"</script>				\n"
		);
		wprintf("</div>");

		/** Pop open an address book -- begin **/
		wprintf(
			"<a href=\"javascript:PopOpenAddressBook('recp_id|%s|cc_id|%s|bcc_id|%s');\" "
			"title=\"%s\">"
			"<img align=middle border=0 width=24 height=24 src=\"static/viewcontacts_24x.gif\">"
			"&nbsp;%s</a>",
			_("To:"), _("CC:"), _("BCC:"),
			_("Contacts"), _("Contacts")
		);
		/** Pop open an address book -- end **/
	}

	wprintf("<div style=\"clear: both;\"><label for=\"subject_id\" > ");
	if (recipient_required || subject_required) {
		wprintf(_("Subject:"));
	}
	else {
		wprintf(_("Subject (optional):"));
	}
	wprintf("</label>"
		"<input type=\"text\" name=\"subject\" id=\"subject_id\" value=\" ");
	escputs(bstr("subject"));
	wprintf("\" size=45 maxlength=70>\n");

	wprintf("</div>\n");

	wprintf("<textarea name=\"msgtext\" cols=\"80\" rows=\"15\">");

	/** If we're continuing from a previous edit, put our partially-composed message back... */
	msgescputs(bstr("msgtext"));

	/* If we're forwarding a message, insert it here... */
	if (atol(bstr("fwdquote")) > 0L) {
		wprintf("<br><div align=center><i>");
		wprintf(_("--- forwarded message ---"));
		wprintf("</i></div><br>");
		pullquote_message(atol(bstr("fwdquote")), 1, 1);
	}

	/** If we're replying quoted, insert the quote here... */
	else if (atol(bstr("replyquote")) > 0L) {
		wprintf("<br>"
			"<blockquote>");
		pullquote_message(atol(bstr("replyquote")), 0, 1);
		wprintf("</blockquote><br>");
	}

	/** If we're editing a wiki page, insert the existing page here... */
	else if (WC->wc_view == VIEW_WIKI) {
		safestrncpy(buf, bstr("wikipage"), sizeof buf);
		str_wiki_index(buf);
		existing_page = locate_message_by_uid(buf);
		if (existing_page >= 0L) {
			pullquote_message(existing_page, 1, 0);
		}
	}

	/** Insert our signature if appropriate... */
	if ( (WC->is_mailbox) && (strcmp(bstr("sig_inserted"), "yes")) ) {
		get_preference("use_sig", buf, sizeof buf);
		if (!strcasecmp(buf, "yes")) {
			int len;
			get_preference("signature", ebuf, sizeof ebuf);
			euid_unescapize(buf, ebuf);
			wprintf("<br>--<br>");
			len = strlen(buf);
			for (i=0; i<len; ++i) {
				if (buf[i] == '\n') {
					wprintf("<br>");
				}
				else if (buf[i] == '<') {
					wprintf("&lt;");
				}
				else if (buf[i] == '>') {
					wprintf("&gt;");
				}
				else if (buf[i] == '&') {
					wprintf("&amp;");
				}
				else if (buf[i] == '\"') {
					wprintf("&quot;");
				}
				else if (buf[i] == '\'') {
					wprintf("&#39;");
				}
				else if (isprint(buf[i])) {
					wprintf("%c", buf[i]);
				}
			}
		}
	}

	wprintf("</textarea>");

	/**
	 * The following template embeds the TinyMCE richedit control, and automatically
	 * transforms the textarea into a richedit textarea.
	 */
	do_template("richedit");

	/** Enumerate any attachments which are already in place... */
	wprintf("<div class=\"attachment buttons\"><img src=\"static/diskette_24x.gif\" class=\"imgedit\" > ");
	wprintf(_("Attachments:"));
	wprintf(" ");
	wprintf("<select name=\"which_attachment\" size=1>");
	for (att = WC->first_attachment; att != NULL; att = att->next) {
		wprintf("<option value=\"");
		urlescputs(att->filename);
		wprintf("\">");
		escputs(att->filename);
		/* wprintf(" (%s, %d bytes)",att->content_type,att->length); */
		wprintf("</option>\n");
	}
	wprintf("</select>");

	/** Now offer the ability to attach additional files... */
	wprintf("&nbsp;&nbsp;&nbsp;");
	wprintf(_("Attach file:"));
	wprintf(" <input name=\"attachfile\" class=\"attachfile\" "
		"size=16 type=\"file\">\n&nbsp;&nbsp;"
		"<input type=\"submit\" name=\"attach_button\" value=\"%s\">\n", _("Add"));
	wprintf("</div>");


	/** Make sure we only insert our signature once */
	if (strcmp(bstr("sig_inserted"), "yes")) {
		wprintf("<input type=\"hidden\" name=\"sig_inserted\" value=\"yes\">\n");
	}

	wprintf("</form>\n");
	wprintf("</div></div>\n");

DONE:	address_book_popup();
	wDumpContent(1);
}


/**
 * \brief delete a message
 */
void delete_msg(void)
{
	long msgid;
	char buf[SIZ];

	msgid = atol(bstr("msgid"));

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

	msgid = atol(bstr("msgid"));

	if (!IsEmptyStr(bstr("move_button"))) {
		sprintf(buf, "MOVE %ld|%s", msgid, bstr("target_room"));
		serv_puts(buf);
		serv_getln(buf, sizeof buf);
		sprintf(WC->ImportantMessage, "%s", &buf[4]);
	} else {
		sprintf(WC->ImportantMessage, (_("The message was not moved.")));
	}

	readloop("readnew");
}





/**
 * \brief Confirm move of a message
 */
void confirm_move_msg(void)
{
	long msgid;
	char buf[SIZ];
	char targ[SIZ];

	msgid = atol(bstr("msgid"));


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
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%ld\">\n", WC->nonce);
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


/*@}*/
