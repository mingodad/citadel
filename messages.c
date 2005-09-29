/*
 * $Id$
 *
 * Functions which deal with the fetching and displaying of messages.
 *
 */

#include "webcit.h"
#include "vcard.h"
#include "webserver.h"
#include "groupdav.h"


/* Address book entry (keep it short and sweet, it's just a quickie lookup
 * which we can use to get to the real meat and bones later)
 */
struct addrbookent {
	char ab_name[64];
	long ab_msgnum;
};



#ifdef HAVE_ICONV
/* Handle subjects with RFC2047 encoding, such as:
 * =?koi8-r?B?78bP0s3Mxc7JxSDXz9rE1dvO2c3JINvB0sHNySDP?=
 */
void utf8ify_rfc822_string(char *buf) {
	char *start, *end;
	char newbuf[1024];
	char charset[128];
	char encoding[16];
	char istr[1024];
	iconv_t ic = (iconv_t)(-1) ;
	char *ibuf;		   /* Buffer of characters to be converted */
	char *obuf;		   /* Buffer for converted characters      */
	size_t ibuflen;	       /* Length of input buffer	       */
	size_t obuflen;	       /* Length of output buffer	      */
	char *isav;		   /* Saved pointer to input buffer	*/
	char *osav;		   /* Saved pointer to output buffer       */

	while (start=strstr(buf, "=?"), end=strstr(buf, "?="),
		((start != NULL) && (end != NULL) && (end > start)) )
	{
		extract_token(charset, start, 1, '?', sizeof charset);
		extract_token(encoding, start, 2, '?', sizeof encoding);
		extract_token(istr, start, 3, '?', sizeof istr);

		/*strcpy(start, "");
		++end;
		++end;*/

		ibuf = malloc(1024);
		isav = ibuf;
		if (!strcasecmp(encoding, "B")) {	/* base64 */
			ibuflen = CtdlDecodeBase64(ibuf, istr, strlen(istr));
		}
		else if (!strcasecmp(encoding, "Q")) {	/* quoted-printable */
			ibuflen = CtdlDecodeQuotedPrintable(ibuf, istr, strlen(istr));
		}
		else {
			strcpy(ibuf, istr);		/* huh? */
			ibuflen = strlen(istr);
		}

		ic = iconv_open("UTF-8", charset);
		if (ic != (iconv_t)(-1) ) {
			obuf = malloc(1024);
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
			snprintf(newbuf, sizeof newbuf, "%s(unreadable)%s", buf, end);
			strcpy(buf, newbuf);
		}

		free(isav);
	}

}
#endif


/*
 * Look for URL's embedded in a buffer and make them linkable.  We use a
 * target window in order to keep the BBS session in its own window.
 */
void url(buf)
char buf[];
{

	int pos;
	int start, end;
	char ench;
	char urlbuf[SIZ];
	char outbuf[1024];

	start = (-1);
	end = strlen(buf);
	ench = 0;

	for (pos = 0; pos < strlen(buf); ++pos) {
		if (!strncasecmp(&buf[pos], "http://", 7))
			start = pos;
		if (!strncasecmp(&buf[pos], "ftp://", 6))
			start = pos;
	}

	if (start < 0)
		return;

	if ((start > 0) && (buf[start - 1] == '<'))
		ench = '>';
	if ((start > 0) && (buf[start - 1] == '['))
		ench = ']';
	if ((start > 0) && (buf[start - 1] == '('))
		ench = ')';
	if ((start > 0) && (buf[start - 1] == '{'))
		ench = '}';

	for (pos = strlen(buf); pos > start; --pos) {
		if ((buf[pos] == ' ') || (buf[pos] == ench))
			end = pos;
	}

	strncpy(urlbuf, &buf[start], end - start);
	urlbuf[end - start] = 0;

	strncpy(outbuf, buf, start);
	sprintf(&outbuf[start], "%cA HREF=%c%s%c TARGET=%c%s%c%c%s%c/A%c",
		LB, QU, urlbuf, QU, QU, TARGET, QU, RB, urlbuf, LB, RB);
	strcat(outbuf, &buf[end]);
	if ( strlen(outbuf) < 250 )
		strcpy(buf, outbuf);
}


/*
 * Turn a vCard "n" (name) field into something displayable.
 */
void vcard_n_prettyize(char *name)
{
	char *original_name;
	int i;

	original_name = strdup(name);
	for (i=0; i<5; ++i) {
		if (strlen(original_name) > 0) {
			if (original_name[strlen(original_name)-1] == ' ') {
				original_name[strlen(original_name)-1] = 0;
			}
			if (original_name[strlen(original_name)-1] == ';') {
				original_name[strlen(original_name)-1] = 0;
			}
		}
	}
	strcpy(name, "");
	for (i=0; i<strlen(original_name); ++i) {
		if (original_name[i] == ';') {
			strcat(name, ", ");
		}
		else {
			name[strlen(name)+1] = 0;
			name[strlen(name)] = original_name[i];
		}
	}
	free(original_name);
}




/* display_vcard() calls this after parsing the textual vCard into
 * our 'struct vCard' data object.
 * This gets called instead of display_parsed_vcard() if we are only looking
 * to extract the person's name instead of displaying the card.
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



/* display_vcard() calls this after parsing the textual vCard into
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

	char displayname[SIZ];
	char title[SIZ];
	char org[SIZ];
	char phone[SIZ];
	char mailto[SIZ];

	strcpy(displayname, "");
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
			strcpy(displayname, name);
			vcard_n_prettyize(displayname);
			escputs(displayname);
		}
		else {
			wprintf("&nbsp;");
		}
		wprintf("</TD>");
		return;
	}

	wprintf("<div align=center><table bgcolor=#aaaaaa width=50%%>");
	for (pass=1; pass<=2; ++pass) {

		if (v->numprops) for (i=0; i<(v->numprops); ++i) {

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
	
			if (is_qp) {
				thisvalue = malloc(strlen(v->prop[i].value) + 50);
				j = CtdlDecodeQuotedPrintable(
					thisvalue, v->prop[i].value,
					strlen(v->prop[i].value) );
				thisvalue[j] = 0;
			}
			else if (is_b64) {
				thisvalue = malloc(strlen(v->prop[i].value) + 50);
				CtdlDecodeBase64(
					thisvalue, v->prop[i].value,
					strlen(v->prop[i].value) );
			}
			else {
				thisvalue = strdup(v->prop[i].value);
			}
	
			/*** Various fields we may encounter ***/
	
			/* N is name, but only if there's no FN already there */
			if (!strcasecmp(firsttoken, "n")) {
				if (strlen(displayname) == 0) {
					strcpy(displayname, thisvalue);
					vcard_n_prettyize(displayname);
				}
			}
	
			/* FN (full name) is a true 'display name' field */
			else if (!strcasecmp(firsttoken, "fn")) {
				strcpy(displayname, thisvalue);
			}

			/* title */
			else if (!strcasecmp(firsttoken, "title")) {
				strcpy(title, thisvalue);
			}
	
			/* organization */
			else if (!strcasecmp(firsttoken, "org")) {
				strcpy(org, thisvalue);
			}
	
			else if (!strcasecmp(firsttoken, "email")) {
				if (strlen(mailto) > 0) strcat(mailto, "<br />");
				strcat(mailto,
					"<A HREF=\"/display_enter"
					"?force_room=_MAIL_?recp=");

				urlesc(&mailto[strlen(mailto)], displayname);
				urlesc(&mailto[strlen(mailto)], " <");
				urlesc(&mailto[strlen(mailto)], thisvalue);
				urlesc(&mailto[strlen(mailto)], ">");

				strcat(mailto, "\">");
				stresc(&mailto[strlen(mailto)], thisvalue, 1, 1);
				strcat(mailto, "</A>");
			}
			else if (!strcasecmp(firsttoken, "tel")) {
				if (strlen(phone) > 0) strcat(phone, "<br />");
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
						if (strlen(buf) > 0) {
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
			"<IMG ALIGN=CENTER SRC=\"/static/viewcontacts_48x.gif\">"
			"<FONT SIZE=+1><B>");
			escputs(displayname);
			wprintf("</B></FONT>");
			if (strlen(title) > 0) {
				wprintf("<div align=right>");
				escputs(title);
				wprintf("</div>");
			}
			if (strlen(org) > 0) {
				wprintf("<div align=right>");
				escputs(org);
				wprintf("</div>");
			}
			wprintf("</TD></TR>\n");
		
			if (strlen(phone) > 0) {
				wprintf("<tr><td>");
				wprintf(_("Telephone:"));
				wprintf("</td><td>%s</td></tr>\n", phone);
			}
			if (strlen(mailto) > 0) {
				wprintf("<tr><td>");
				wprintf(_("E-mail:"));
				wprintf("</td><td>%s</td></tr>\n", mailto);
			}
		}

	}

	wprintf("</table></div>\n");
}



/*
 * Display a textual vCard
 * (Converts to a vCard object and then calls the actual display function)
 * Set 'full' to nonzero to display the whole card instead of a one-liner.
 * Or, if "storename" is non-NULL, just store the person's name in that
 * buffer instead of displaying the card at all.
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


/*
 * I wanna SEE that message!
 */
void read_message(long msgnum, int printable_view) {
	char buf[SIZ];
	char mime_partnum[256];
	char mime_filename[256];
	char mime_content_type[256];
	char mime_charset[256];
	char mime_disposition[256];
	int mime_length;
	char mime_http[SIZ];
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
#ifdef HAVE_ICONV
	iconv_t ic = (iconv_t)(-1) ;
	char *ibuf;		   /* Buffer of characters to be converted */
	char *obuf;		   /* Buffer for converted characters      */
	size_t ibuflen;	       /* Length of input buffer	       */
	size_t obuflen;	       /* Length of output buffer	      */
	char *osav;		   /* Saved pointer to output buffer       */
#endif

	strcpy(from, "");
	strcpy(node, "");
	strcpy(rfca, "");
	strcpy(reply_to, "");
	strcpy(reply_all, "");
	strcpy(vcard_partnum, "");
	strcpy(cal_partnum, "");
	strcpy(mime_http, "");
	strcpy(mime_content_type, "text/plain");
	strcpy(mime_charset, "us-ascii");

	serv_printf("MSG4 %ld", msgnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		wprintf("<STRONG>");
		wprintf(_("ERROR:"));
		wprintf("</STRONG> %s<br />\n", &buf[4]);
		return;
	}

	/* begin everythingamundo table */
	if (!printable_view) {
		wprintf("<div id=\"fix_scrollbar_bug\">\n");
		wprintf("<table width=100%% border=1 cellspacing=0 "
			"cellpadding=0><TR><TD>\n");
	}

	/* begin message header table */
	wprintf("<TABLE WIDTH=100%% BORDER=0 CELLSPACING=0 "
		"CELLPADDING=1 BGCOLOR=\"#CCCCCC\"><TR><TD>\n");

	wprintf("<SPAN CLASS=\"message_header\">");
	strcpy(m_subject, "");
	strcpy(m_cc, "");

	while (serv_getln(buf, sizeof buf), strcasecmp(buf, "text")) {
		if (!strcmp(buf, "000")) {
			wprintf("<I>");
			wprintf(_("unexpected end of message"));
			wprintf("</I><br /><br />\n");
			wprintf("</SPAN>\n");
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
			wprintf("<A HREF=\"/showuser?who=");
#ifdef HAVE_ICONV
			utf8ify_rfc822_string(from);
#endif
			urlescputs(from);
			wprintf("\">");
			escputs(from);
			wprintf("</A> ");
		}
		if (!strncasecmp(buf, "subj=", 5)) {
			safestrncpy(m_subject, &buf[5], sizeof m_subject);
		}
		if (!strncasecmp(buf, "cccc=", 5)) {
			safestrncpy(m_cc, &buf[5], sizeof m_cc);
			if (strlen(reply_all) > 0) {
				strcat(reply_all, ", ");
			}
			safestrncpy(&reply_all[strlen(reply_all)], &buf[5],
				(sizeof reply_all - strlen(reply_all)) );
		}
		if ((!strncasecmp(buf, "hnod=", 5))
		    && (strcasecmp(&buf[5], serv_info.serv_humannode))) {
			wprintf("(%s) ", &buf[5]);
		}
		if ((!strncasecmp(buf, "room=", 5))
		    && (strcasecmp(&buf[5], WC->wc_roomname))
		    && (strlen(&buf[5])>0) ) {
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
			&& (strlen(rfca)==0)
			) {
				wprintf("@%s ", &buf[5]);
			}
		}
		if (!strncasecmp(buf, "rcpt=", 5)) {
			wprintf(_("to "));
			escputs(&buf[5]);
			wprintf(" ");
			if (strlen(reply_all) > 0) {
				strcat(reply_all, ", ");
			}
			safestrncpy(&reply_all[strlen(reply_all)], &buf[5],
				(sizeof reply_all - strlen(reply_all)) );
		}
		if (!strncasecmp(buf, "time=", 5)) {
			fmt_date(now, atol(&buf[5]), 0);
			wprintf("%s ", now);
		}

		if (!strncasecmp(buf, "part=", 5)) {
			extract_token(mime_filename, &buf[5], 1, '|', sizeof mime_filename);
			extract_token(mime_partnum, &buf[5], 2, '|', sizeof mime_partnum);
			extract_token(mime_disposition, &buf[5], 3, '|', sizeof mime_disposition);
			extract_token(mime_content_type, &buf[5], 4, '|', sizeof mime_content_type);
			mime_length = extract_int(&buf[5], 5);

			if ((!strcasecmp(mime_disposition, "inline"))
			   && (!strncasecmp(mime_content_type, "image/", 6)) ){
				snprintf(&mime_http[strlen(mime_http)],
					(sizeof(mime_http) - strlen(mime_http) - 1),
					"<IMG SRC=\"/mimepart/%ld/%s/%s\">",
					msgnum, mime_partnum, mime_filename);
			}
			else if ( (!strcasecmp(mime_disposition, "attachment")) 
			     || (!strcasecmp(mime_disposition, "inline")) ) {
				snprintf(&mime_http[strlen(mime_http)],
					(sizeof(mime_http) - strlen(mime_http) - 1),
					"<A HREF=\"/mimepart/%ld/%s/%s\" "
					"TARGET=\"wc.%ld.%s\">"
					"<IMG SRC=\"/static/diskette_24x.gif\" "
					"BORDER=0 ALIGN=MIDDLE>\n"
					"Part %s: %s (%s, %d bytes)</A><br />\n",
					msgnum, mime_partnum, mime_filename,
					msgnum, mime_partnum,
					mime_partnum, mime_filename,
					mime_content_type, mime_length);
			}

			/*** begin handler prep ***/
			if (!strcasecmp(mime_content_type, "text/x-vcard")) {
				strcpy(vcard_partnum, mime_partnum);
			}

			if (!strcasecmp(mime_content_type, "text/calendar")) {
				strcpy(cal_partnum, mime_partnum);
			}

			/*** end handler prep ***/

		}

	}

	/* Generate a reply-to address */
	if (strlen(rfca) > 0) {
		strcpy(reply_to, rfca);
	}
	else {
		if ( (strlen(node) > 0)
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

	wprintf("</SPAN>");
#ifdef HAVE_ICONV
	utf8ify_rfc822_string(m_cc);
	utf8ify_rfc822_string(m_subject);
#endif
	if (strlen(m_cc) > 0) {
		wprintf("<br />"
			"<SPAN CLASS=\"message_subject\">");
		wprintf(_("CC:"));
		wprintf(" ");
		escputs(m_cc);
		wprintf("</SPAN>");
	}
	if (strlen(m_subject) > 0) {
		wprintf("<br />"
			"<SPAN CLASS=\"message_subject\">");
		wprintf(_("Subject:"));
		wprintf(" ");
		escputs(m_subject);
		wprintf("</SPAN>");
	}
	wprintf("</TD>\n");

	/* start msg buttons */
	if (!printable_view) {
		wprintf("<td align=right>\n");

		/* Reply */
		if ( (WC->wc_view == VIEW_MAILBOX) || (WC->wc_view == VIEW_BBS) ) {
			wprintf("<a href=\"/display_enter");
			if (WC->is_mailbox) {
				wprintf("?replyquote=%ld", msgnum);
			}
			wprintf("?recp=");
			urlescputs(reply_to);
			wprintf("?subject=");
			if (strncasecmp(m_subject, "Re:", 3)) wprintf("Re:%20");
			urlescputs(m_subject);
			wprintf("\">[%s]</a> ", _("Reply"));
		}

		/* ReplyQuoted */
		if ( (WC->wc_view == VIEW_MAILBOX) || (WC->wc_view == VIEW_BBS) ) {
			if (!WC->is_mailbox) {
				wprintf("<a href=\"/display_enter");
				wprintf("?replyquote=%ld", msgnum);
				wprintf("?recp=");
				urlescputs(reply_to);
				wprintf("?subject=");
				if (strncasecmp(m_subject, "Re:", 3)) wprintf("Re:%20");
				urlescputs(m_subject);
				wprintf("\">[%s]</a> ", _("ReplyQuoted"));
			}
		}

		/* ReplyAll */
		if (WC->wc_view == VIEW_MAILBOX) {
			wprintf("<a href=\"/display_enter");
			wprintf("?replyquote=%ld", msgnum);
			wprintf("?recp=");
			urlescputs(reply_to);
			wprintf("?cc=");
			urlescputs(reply_all);
			wprintf("?subject=");
			if (strncasecmp(m_subject, "Re:", 3)) wprintf("Re:%20");
			urlescputs(m_subject);
			wprintf("\">[%s]</a> ", _("ReplyAll"));
		}

		/* Forward */
		if (WC->wc_view == VIEW_MAILBOX) {
			wprintf("<a href=\"/display_enter?fwdquote=%ld?subject=", msgnum);
			if (strncasecmp(m_subject, "Fwd:", 4)) wprintf("Fwd:%20");
			urlescputs(m_subject);
			wprintf("\">[%s]</a> ", _("Forward"));
		}

		/* If this is one of my own rooms, or if I'm an Aide or Room Aide, I can move/delete */
		if ( (WC->is_room_aide) || (WC->is_mailbox) ) {
			/* Move */
			wprintf("<a href=\"/confirm_move_msg?msgid=%ld\">[%s]</a> ",
				msgnum, _("Move"));
	
			/* Delete */
			wprintf("<a href=\"/delete_msg?msgid=%ld\" "
				"onClick=\"return confirm('%s');\">"
				"[%s]</a> ", msgnum, _("Delete this message?"), _("Delete")
			);
		}

		wprintf("<a href=\"#\" onClick=\"window.open('/printmsg?msgnum=%ld', 'print%ld', 'toolbar=no,location=no,directories=no,copyhistory=no,status=yes,scrollbars=yes,resizable=yes,width=600,height=400'); \" >"
			"[%s]</a>", msgnum, msgnum, _("Print"));

		wprintf("</td>");
	}

	wprintf("</TR></TABLE>\n");

	/* Begin body */
	wprintf("<TABLE BORDER=0 WIDTH=100%% BGCOLOR=#FFFFFF "
		"CELLPADDING=1 CELLSPACING=0><TR><TD>");

	/* 
	 * Learn the content type
	 */
	strcpy(mime_content_type, "text/plain");
	while (serv_getln(buf, sizeof buf), (strlen(buf) > 0)) {
		if (!strcmp(buf, "000")) {
			wprintf("<I>");
			wprintf(_("unexpected end of message"));
			wprintf("</I><br /><br />\n");
			goto ENDBODY;
		}
		if (!strncasecmp(buf, "Content-type: ", 14)) {
			safestrncpy(mime_content_type, &buf[14],
				sizeof(mime_content_type));
			for (i=0; i<strlen(mime_content_type); ++i) {
				if (!strncasecmp(&mime_content_type[i], "charset=", 8)) {
					safestrncpy(mime_charset, &mime_content_type[i+8],
						sizeof mime_charset);
				}
			}
			for (i=0; i<strlen(mime_content_type); ++i) {
				if (mime_content_type[i] == ';') {
					mime_content_type[i] = 0;
				}
			}
		}
	}

	/* Set up a character set conversion if we need to (and if we can) */
#ifdef HAVE_ICONV
	if ( (strcasecmp(mime_charset, "us-ascii"))
	   && (strcasecmp(mime_charset, "UTF-8")) ) {
		ic = iconv_open("UTF-8", mime_charset);
		if (ic == (iconv_t)(-1) ) {
			lprintf(5, "iconv_open() failed: %s\n", strerror(errno));
		}
	}
#endif

	/* Messages in legacy Citadel variformat get handled thusly... */
	if (!strcasecmp(mime_content_type, "text/x-citadel-variformat")) {
		fmout("JUSTIFY");
	}

	/* Boring old 80-column fixed format text gets handled this way... */
	else if ( (!strcasecmp(mime_content_type, "text/plain"))
		|| (!strcasecmp(mime_content_type, "text")) ) {
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			if (buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = 0;
			if (buf[strlen(buf)-1] == '\r') buf[strlen(buf)-1] = 0;

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

			while ((strlen(buf) > 0) && (isspace(buf[strlen(buf) - 1])))
				buf[strlen(buf) - 1] = 0;
			if ((bq == 0) &&
		    	((!strncmp(buf, ">", 1)) || (!strncmp(buf, " >", 2)) || (!strncmp(buf, " :-)", 4)))) {
				wprintf("<BLOCKQUOTE>");
				bq = 1;
			} else if ((bq == 1) &&
			   	(strncmp(buf, ">", 1)) && (strncmp(buf, " >", 2)) && (strncmp(buf, " :-)", 4))) {
				wprintf("</BLOCKQUOTE>");
				bq = 0;
			}
			wprintf("<TT>");
			url(buf);
			escputs(buf);
			wprintf("</TT><br />\n");
		}
		wprintf("</I><br />");
	}

	else /* HTML is fun, but we've got to strip it first */
	if (!strcasecmp(mime_content_type, "text/html")) {
		output_html(mime_charset);
	}

	/* Unknown weirdness */
	else {
		wprintf(_("I don't know how to display %s"), mime_content_type);
		wprintf("<br />\n", mime_content_type);
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) { }
	}

	/* Afterwards, offer links to download attachments 'n' such */
	if (strlen(mime_http) > 0) {
		wprintf("%s", mime_http);
	}

	/* Handler for vCard parts */
	if (strlen(vcard_partnum) > 0) {
		part_source = load_mimepart(msgnum, vcard_partnum);
		if (part_source != NULL) {

			/* If it's my vCard I can edit it */
			if (	(!strcasecmp(WC->wc_roomname, USERCONFIGROOM))
				|| (!strcasecmp(&WC->wc_roomname[11], USERCONFIGROOM))
				|| (WC->wc_view == VIEW_ADDRESSBOOK)
			) {
				wprintf("<A HREF=\"/edit_vcard?"
					"msgnum=%ld?partnum=%s\">",
					msgnum, vcard_partnum);
				wprintf("[%s]</A>", _("edit"));
			}

			/* In all cases, display the full card */
			display_vcard(part_source, 0, 1, NULL);
		}
	}

	/* Handler for calendar parts */
	if (strlen(cal_partnum) > 0) {
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
	wprintf("</TD></TR></TABLE>\n");

	/* end everythingamundo table */
	if (!printable_view) {
		wprintf("</TD></TR></TABLE>\n");
		wprintf("</div><br />\n");
	}

#ifdef HAVE_ICONV
	if (ic != (iconv_t)(-1) ) {
		iconv_close(ic);
	}
#endif
}



/*
 * Unadorned HTML output of an individual message, suitable
 * for placing in a hidden iframe, for printing, or whatever
 */
void embed_message(void) {
	long msgnum = 0L;

	msgnum = atol(bstr("msgnum"));
	begin_ajax_response();
	read_message(msgnum, 0);
	end_ajax_response();
}


/*
 * Printable view of a message
 */
void print_message(void) {
	long msgnum = 0L;

	msgnum = atol(bstr("msgnum"));
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
	
	read_message(msgnum, 1);

	wprintf("\n</body></html>\n\n");
	wDumpContent(0);
}



/*
 * Read message in simple, JavaScript-embeddable form for 'forward'
 * or 'reply quoted' operations.
 *
 * NOTE: it is VITALLY IMPORTANT that we output no single-quotes or linebreaks
 *       in this function.  Doing so would throw a JavaScript error in the
 *       'supplied text' argument to the editor.
 */
void pullquote_message(long msgnum, int forward_attachments) {
	char buf[SIZ];
	char mime_partnum[256];
	char mime_filename[256];
	char mime_content_type[256];
	char mime_charset[256];
	char mime_disposition[256];
	int mime_length;
	char mime_http[SIZ];
	char *attachments = NULL;
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
	char *ibuf;		   /* Buffer of characters to be converted */
	char *obuf;		   /* Buffer for converted characters      */
	size_t ibuflen;	       /* Length of input buffer	       */
	size_t obuflen;	       /* Length of output buffer	      */
	char *osav;		   /* Saved pointer to output buffer       */
#endif

	strcpy(from, "");
	strcpy(node, "");
	strcpy(rfca, "");
	strcpy(reply_to, "");
	strcpy(mime_http, "");
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
			wprintf(_("unexpected end of message"));
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
		    && (strlen(&buf[5])>0) ) {
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
			&& (strlen(rfca)==0)
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

		/*
		 * Save attachment info for later.  We can't start downloading them
		 * yet because we're in the middle of a server transaction.
		 */
		if (!strncasecmp(buf, "part=", 5)) {
			++num_attachments;
			attachments = realloc(attachments, (num_attachments * 1024));
			strcat(attachments, &buf[5]);
			strcat(attachments, "\n");
		}

	}

	wprintf("<br>");

#ifdef HAVE_ICONV
	utf8ify_rfc822_string(m_subject);
#endif
	if (strlen(m_subject) > 0) {
		wprintf(_("Subject:"));
		wprintf(" ");
		msgescputs(m_subject);
		wprintf("<br />");
	}

	/*
	 * Begin body
	 */
	wprintf("<br>");

	/* 
	 * Learn the content type
	 */
	strcpy(mime_content_type, "text/plain");
	while (serv_getln(buf, sizeof buf), (strlen(buf) > 0)) {
		if (!strcmp(buf, "000")) {
			wprintf(_("unexpected end of message"));
			goto ENDBODY;
		}
		if (!strncasecmp(buf, "Content-type: ", 14)) {
			safestrncpy(mime_content_type, &buf[14],
				sizeof(mime_content_type));
			for (i=0; i<strlen(mime_content_type); ++i) {
				if (!strncasecmp(&mime_content_type[i], "charset=", 8)) {
					safestrncpy(mime_charset, &mime_content_type[i+8],
						sizeof mime_charset);
				}
			}
			for (i=0; i<strlen(mime_content_type); ++i) {
				if (mime_content_type[i] == ';') {
					mime_content_type[i] = 0;
				}
			}
		}
	}

	/* Set up a character set conversion if we need to (and if we can) */
#ifdef HAVE_ICONV
	if ( (strcasecmp(mime_charset, "us-ascii"))
	   && (strcasecmp(mime_charset, "UTF-8")) ) {
		ic = iconv_open("UTF-8", mime_charset);
		if (ic == (iconv_t)(-1) ) {
			lprintf(5, "iconv_open() failed: %s\n", strerror(errno));
		}
	}
#endif

	/* Messages in legacy Citadel variformat get handled thusly... */
	if (!strcasecmp(mime_content_type, "text/x-citadel-variformat")) {
		pullquote_fmout();
	}

	/* Boring old 80-column fixed format text gets handled this way... */
	else if (!strcasecmp(mime_content_type, "text/plain")) {
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			if (buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = 0;
			if (buf[strlen(buf)-1] == '\r') buf[strlen(buf)-1] = 0;

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

			while ((strlen(buf) > 0) && (isspace(buf[strlen(buf) - 1])))
				buf[strlen(buf) - 1] = 0;
			if ((bq == 0) &&
		    	((!strncmp(buf, ">", 1)) || (!strncmp(buf, " >", 2)) || (!strncmp(buf, " :-)", 4)))) {
				wprintf("<BLOCKQUOTE>");
				bq = 1;
			} else if ((bq == 1) &&
			   	(strncmp(buf, ">", 1)) && (strncmp(buf, " >", 2)) && (strncmp(buf, " :-)", 4))) {
				wprintf("</BLOCKQUOTE>");
				bq = 0;
			}
			wprintf("<TT>");
			url(buf);
			msgescputs(buf);
			wprintf("</TT><br />");
		}
		wprintf("</I><br />");
	}

	/* HTML just gets escaped and stuffed back into the editor */
	else if (!strcasecmp(mime_content_type, "text/html")) {
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			msgescputs(buf);
		}
	}

	/* Unknown weirdness ... don't know how to handle this content type */
	else {
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) { }
	}

ENDBODY:
	/* end of body handler */

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
			lprintf(9, "fwd filename: %s\n", mime_filename);
			lprintf(9, "fwd partnum : %s\n", mime_partnum);
			lprintf(9, "fwd conttype: %s\n", mime_content_type);
			lprintf(9, "fwd dispose : %s\n", mime_disposition);
			lprintf(9, "fwd length  : %d\n", mime_length);
			 */

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
		free(attachments);
	}

#ifdef HAVE_ICONV
	if (ic != (iconv_t)(-1) ) {
		iconv_close(ic);
	}
#endif
}






void display_summarized(int num) {
	char datebuf[64];

	wprintf("<TD>");
	if (WC->summ[num].is_new) wprintf("<B>");
	escputs(WC->summ[num].subj);
	if (WC->summ[num].is_new) wprintf("</B>");
	wprintf("</TD><TD>");
	if (WC->summ[num].is_new) wprintf("<B>");
	escputs(WC->summ[num].from);
	if (WC->summ[num].is_new) wprintf("</B>");
	wprintf(" </TD><TD>");
	if (WC->summ[num].is_new) wprintf("<B>");
	fmt_date(datebuf, WC->summ[num].date, 1);	/* brief */
	escputs(datebuf);
	if (WC->summ[num].is_new) wprintf("</B>");
	wprintf(" </TD>");
	wprintf("<TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"msg_%ld\" VALUE=\"yes\">"
		"</TD>\n",
		WC->summ[num].msgnum
	);
}





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

			if (!strcasecmp(mime_content_type, "text/x-vcard")) {
				strcpy(vcard_partnum, mime_partnum);
			}

		}
	}

	if (strlen(vcard_partnum) > 0) {
		vcard_source = load_mimepart(msgnum, vcard_partnum);
		if (vcard_source != NULL) {

			/* Display the summary line */
			display_vcard(vcard_source, alpha, 0, NULL);

			/* If it's my vCard I can edit it */
			if (	(!strcasecmp(WC->wc_roomname, USERCONFIGROOM))
				|| (!strcasecmp(&WC->wc_roomname[11], USERCONFIGROOM))
				|| (WC->wc_view == VIEW_ADDRESSBOOK)
			) {
				wprintf("<A HREF=\"/edit_vcard?"
					"msgnum=%ld?partnum=%s\">",
					msgnum, vcard_partnum);
				wprintf("[%s]</A>", _("edit"));
			}

			free(vcard_source);
		}
	}

}



/* If it's an old "Firstname Lastname" style record, try to
 * convert it.
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


void fetch_ab_name(long msgnum, char *namebuf) {
	char buf[SIZ];
	char mime_partnum[SIZ];
	char mime_filename[SIZ];
	char mime_content_type[SIZ];
	char mime_disposition[SIZ];
	int mime_length;
	char vcard_partnum[SIZ];
	char *vcard_source = NULL;
	int i;
	struct message_summary summ;

	if (namebuf == NULL) return;
	strcpy(namebuf, "");

	memset(&summ, 0, sizeof(summ));
	safestrncpy(summ.subj, "(no subject)", sizeof summ.subj);

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

			if (!strcasecmp(mime_content_type, "text/x-vcard")) {
				strcpy(vcard_partnum, mime_partnum);
			}

		}
	}

	if (strlen(vcard_partnum) > 0) {
		vcard_source = load_mimepart(msgnum, vcard_partnum);
		if (vcard_source != NULL) {

			/* Grab the name off the card */
			display_vcard(vcard_source, 0, 0, namebuf);

			free(vcard_source);
		}
	}

	lastfirst_firstlast(namebuf);
	striplt(namebuf);
	for (i=0; i<strlen(namebuf); ++i) {
		if (namebuf[i] != ';') return;
	}
	strcpy(namebuf, _("(no name)"));
}



/*
 * Record compare function for sorting address book indices
 */
int abcmp(const void *ab1, const void *ab2) {
	return(strcasecmp(
		(((const struct addrbookent *)ab1)->ab_name),
		(((const struct addrbookent *)ab2)->ab_name)
	));
}


/*
 * Helper function for do_addrbook_view()
 * Converts a name into a three-letter tab label
 */
void nametab(char *tabbuf, char *name) {
	stresc(tabbuf, name, 0, 0);
	tabbuf[0] = toupper(tabbuf[0]);
	tabbuf[1] = tolower(tabbuf[1]);
	tabbuf[2] = tolower(tabbuf[2]);
	tabbuf[3] = 0;
}


/*
 * Render the address book using info we gathered during the scan
 */
void do_addrbook_view(struct addrbookent *addrbook, int num_ab) {
	int i = 0;
	int displayed = 0;
	int bg = 0;
	static int NAMESPERPAGE = 60;
	int num_pages = 0;
	int page = 0;
	int tabfirst = 0;
	char tabfirst_label[SIZ];
	int tablast = 0;
	char tablast_label[SIZ];

	if (num_ab == 0) {
		wprintf("<br /><br /><br /><div align=\"center\"><i>");
		wprintf(_("This address book is empty."));
		wprintf("</i></div>\n");
		return;
	}

	if (num_ab > 1) {
		qsort(addrbook, num_ab, sizeof(struct addrbookent), abcmp);
	}

	num_pages = num_ab / NAMESPERPAGE;

	page = atoi(bstr("page"));

	wprintf("Page: ");
	for (i=0; i<=num_pages; ++i) {
		if (i != page) {
			wprintf("<A HREF=\"/readfwd?page=%d\">", i);
		}
		else {
			wprintf("<B>");
		}
		tabfirst = i * NAMESPERPAGE;
		tablast = tabfirst + NAMESPERPAGE - 1;
		if (tablast > (num_ab - 1)) tablast = (num_ab - 1);
		nametab(tabfirst_label, addrbook[tabfirst].ab_name);
		nametab(tablast_label, addrbook[tablast].ab_name);
		wprintf("[%s&nbsp;-&nbsp;%s]",
			tabfirst_label, tablast_label
		);
		if (i != page) {
			wprintf("</A>\n");
		}
		else {
			wprintf("</B>\n");
		}
	}
	wprintf("<br />\n");

	wprintf("<TABLE border=0 cellspacing=0 "
		"cellpadding=3 width=100%%>\n"
	);

	for (i=0; i<num_ab; ++i) {

		if ((i / NAMESPERPAGE) == page) {

			if ((displayed % 4) == 0) {
				if (displayed > 0) {
					wprintf("</TR>\n");
				}
				bg = 1 - bg;
				wprintf("<TR BGCOLOR=\"#%s\">",
					(bg ? "DDDDDD" : "FFFFFF")
				);
			}
	
			wprintf("<TD>");
	
			wprintf("<A HREF=\"/readfwd?startmsg=%ld&is_singlecard=1",
				addrbook[i].ab_msgnum);
			wprintf("?maxmsgs=1?summary=0?alpha=%s\">", bstr("alpha"));
			vcard_n_prettyize(addrbook[i].ab_name);
			escputs(addrbook[i].ab_name);
			wprintf("</A></TD>\n");
			++displayed;
		}
	}

	wprintf("</TR></TABLE>\n");
}



/* 
 * load message pointers from the server
 */
int load_msg_ptrs(char *servcmd, int with_headers)
{
	char buf[1024];
	time_t datestamp;
	char displayname[128];
	char nodename[128];
	char inetaddr[128];
	char subject[256];
	int nummsgs;
	int maxload = 0;

	int num_summ_alloc = 0;

	if (with_headers) {
		if (WC->num_summ != 0) {
			free(WC->summ);
			WC->num_summ = 0;
		}
	}
	num_summ_alloc = 100;
	WC->num_summ = 0;
	WC->summ = malloc(num_summ_alloc * sizeof(struct message_summary));

	nummsgs = 0;
	maxload = sizeof(WC->msgarr) / sizeof(long) ;
	serv_puts(servcmd);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		wprintf("<EM>%s</EM><br />\n", &buf[4]);
		return (nummsgs);
	}
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		if (nummsgs < maxload) {
			WC->msgarr[nummsgs] = extract_long(buf, 0);
			datestamp = extract_long(buf, 1);
			extract_token(displayname, buf, 2, '|', sizeof displayname);
			extract_token(nodename, buf, 3, '|', sizeof nodename);
			extract_token(inetaddr, buf, 4, '|', sizeof inetaddr);
			extract_token(subject, buf, 5, '|', sizeof subject);
			++nummsgs;

			if (with_headers) {
				if (nummsgs > num_summ_alloc) {
					num_summ_alloc *= 2;
					WC->summ = realloc(WC->summ, num_summ_alloc * sizeof(struct message_summary));
				}
				++WC->num_summ;

				memset(&WC->summ[nummsgs-1], 0, sizeof(struct message_summary));
				WC->summ[nummsgs-1].msgnum = WC->msgarr[nummsgs-1];
				safestrncpy(WC->summ[nummsgs-1].subj, _("(no subject)"), sizeof WC->summ[nummsgs-1].subj);
				if (strlen(displayname) > 0) {
					safestrncpy(WC->summ[nummsgs-1].from, displayname, sizeof WC->summ[nummsgs-1].from);
				}
				if (strlen(subject) > 0) {
				safestrncpy(WC->summ[nummsgs-1].subj, subject,
					sizeof WC->summ[nummsgs-1].subj);
				}
#ifdef HAVE_ICONV
				/* Handle subjects with RFC2047 encoding */
				utf8ify_rfc822_string(WC->summ[nummsgs-1].subj);
#endif
				if (strlen(WC->summ[nummsgs-1].subj) > 75) {
					strcpy(&WC->summ[nummsgs-1].subj[72], "...");
				}

				if (strlen(nodename) > 0) {
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
				/* Handle senders with RFC2047 encoding */
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

 
int summcmp_subj(const void *s1, const void *s2) {
	struct message_summary *summ1;
	struct message_summary *summ2;
	
	summ1 = (struct message_summary *)s1;
	summ2 = (struct message_summary *)s2;
	return strcasecmp(summ1->subj, summ2->subj);
}

int summcmp_rsubj(const void *s1, const void *s2) {
	struct message_summary *summ1;
	struct message_summary *summ2;
	
	summ1 = (struct message_summary *)s1;
	summ2 = (struct message_summary *)s2;
	return strcasecmp(summ2->subj, summ1->subj);
}

int summcmp_sender(const void *s1, const void *s2) {
	struct message_summary *summ1;
	struct message_summary *summ2;
	
	summ1 = (struct message_summary *)s1;
	summ2 = (struct message_summary *)s2;
	return strcasecmp(summ1->from, summ2->from);
}

int summcmp_rsender(const void *s1, const void *s2) {
	struct message_summary *summ1;
	struct message_summary *summ2;
	
	summ1 = (struct message_summary *)s1;
	summ2 = (struct message_summary *)s2;
	return strcasecmp(summ2->from, summ1->from);
}

int summcmp_date(const void *s1, const void *s2) {
	struct message_summary *summ1;
	struct message_summary *summ2;
	
	summ1 = (struct message_summary *)s1;
	summ2 = (struct message_summary *)s2;

	if (summ1->date < summ2->date) return -1;
	else if (summ1->date > summ2->date) return +1;
	else return 0;
}

int summcmp_rdate(const void *s1, const void *s2) {
	struct message_summary *summ1;
	struct message_summary *summ2;
	
	summ1 = (struct message_summary *)s1;
	summ2 = (struct message_summary *)s2;

	if (summ1->date < summ2->date) return +1;
	else if (summ1->date > summ2->date) return -1;
	else return 0;
}

/*
 * command loop for reading messages
 */
void readloop(char *oper)
{
	char cmd[SIZ];
	char buf[SIZ];
	char old_msgs[SIZ];
	int a, b;
	int nummsgs;
	long startmsg;
	int maxmsgs;
	int num_displayed = 0;
	int is_summary = 0;
	int is_addressbook = 0;
	int is_singlecard = 0;
	int is_calendar = 0;
	int is_tasks = 0;
	int is_notes = 0;
	int remaining_messages;
	int lo, hi;
	int lowest_displayed = (-1);
	int highest_displayed = 0;
	long pn_previous = 0L;
	long pn_current = 0L;
	long pn_next = 0L;
	int bg = 0;
	struct addrbookent *addrbook = NULL;
	int num_ab = 0;
	char *sortby = NULL;
	char sortpref_name[128];
	char sortpref_value[128];
	char *subjsort_button;
	char *sendsort_button;
	char *datesort_button;

	startmsg = atol(bstr("startmsg"));
	maxmsgs = atoi(bstr("maxmsgs"));
	is_summary = atoi(bstr("summary"));
	if (maxmsgs == 0) maxmsgs = DEFAULT_MAXMSGS;

	snprintf(sortpref_name, sizeof sortpref_name, "sort %s", WC->wc_roomname);
	get_preference(sortpref_name, sortpref_value, sizeof sortpref_value);

	sortby = bstr("sortby");
	if ( (strlen(sortby) > 0) && (strcasecmp(sortby, sortpref_value)) ) {
		set_preference(sortpref_name, sortby, 1);
	}
	if (strlen(sortby) == 0) sortby = sortpref_value;
	if (strlen(sortby) == 0) sortby = "msgid";

	output_headers(1, 1, 1, 0, 0, 0);

	/* When in summary mode, always show ALL messages instead of just
	 * new or old.  Otherwise, show what the user asked for.
	 */
	if (!strcmp(oper, "readnew")) {
		strcpy(cmd, "MSGS NEW");
	}
	else if (!strcmp(oper, "readold")) {
		strcpy(cmd, "MSGS OLD");
	}
	else {
		strcpy(cmd, "MSGS ALL");
	}

	if ((WC->wc_view == VIEW_MAILBOX) && (maxmsgs > 1)) {
		is_summary = 1;
		strcpy(cmd, "MSGS ALL");
	}

	if ((WC->wc_view == VIEW_ADDRESSBOOK) && (maxmsgs > 1)) {
		is_addressbook = 1;
		strcpy(cmd, "MSGS ALL");
		maxmsgs = 9999999;
	}

	if (is_summary) {
		strcpy(cmd, "MSGS ALL|||1");	/* fetch header summary */
		startmsg = 1;
		maxmsgs = 9999999;
	}

	/* Are we doing a summary view?  If so, we need to know old messages
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

	if (WC->wc_view == VIEW_CALENDAR) {		/* calendar */
		is_calendar = 1;
		strcpy(cmd, "MSGS ALL");
		maxmsgs = 32767;
	}
	if (WC->wc_view == VIEW_TASKS) {		/* tasks */
		is_tasks = 1;
		strcpy(cmd, "MSGS ALL");
		maxmsgs = 32767;
	}
	if (WC->wc_view == VIEW_NOTES) {		/* notes */
		is_notes = 1;
		strcpy(cmd, "MSGS ALL");
		maxmsgs = 32767;
	}

	nummsgs = load_msg_ptrs(cmd, is_summary);
	if (nummsgs == 0) {

		if ((!is_tasks) && (!is_calendar) && (!is_notes) && (!is_addressbook)) {
			wprintf("<em>");
			if (!strcmp(oper, "readnew")) {
				wprintf(_("No new messages."));
			} else if (!strcmp(oper, "readold")) {
				wprintf(_("No old messages."));
			} else {
				wprintf(_("No messages here."));
			}
			wprintf("</em>\n");
		}

		goto DONE;
	}

	if (is_summary) {
		for (a = 0; a < nummsgs; ++a) {
			/* Are you a new message, or an old message? */
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

	if (startmsg == 0L) startmsg = WC->msgarr[0];
	remaining_messages = 0;

	for (a = 0; a < nummsgs; ++a) {
		if (WC->msgarr[a] >= startmsg) {
			++remaining_messages;
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
		subjsort_button = "<a href=\"/readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=rsubject\"><img border=\"0\" src=\"/static/down_pointer.gif\" /></a>" ;
	}
	else if (!strcasecmp(sortby, "rsubject")) {
		subjsort_button = "<a href=\"/readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=subject\"><img border=\"0\" src=\"/static/up_pointer.gif\" /></a>" ;
	}
	else {
		subjsort_button = "<a href=\"/readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=subject\"><img border=\"0\" src=\"/static/sort_none.gif\" /></a>" ;
	}

	if (!strcasecmp(sortby, "sender")) {
		sendsort_button = "<a href=\"/readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=rsender\"><img border=\"0\" src=\"/static/down_pointer.gif\" /></a>" ;
	}
	else if (!strcasecmp(sortby, "rsender")) {
		sendsort_button = "<a href=\"/readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=sender\"><img border=\"0\" src=\"/static/up_pointer.gif\" /></a>" ;
	}
	else {
		sendsort_button = "<a href=\"/readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=sender\"><img border=\"0\" src=\"/static/sort_none.gif\" /></a>" ;
	}

	if (!strcasecmp(sortby, "date")) {
		datesort_button = "<a href=\"/readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=rdate\"><img border=\"0\" src=\"/static/down_pointer.gif\" /></a>" ;
	}
	else if (!strcasecmp(sortby, "rdate")) {
		datesort_button = "<a href=\"/readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=date\"><img border=\"0\" src=\"/static/up_pointer.gif\" /></a>" ;
	}
	else {
		datesort_button = "<a href=\"/readfwd?startmsg=1?maxmsgs=9999999?summary=1?sortby=date\"><img border=\"0\" src=\"/static/sort_none.gif\" /></a>" ;
	}

	if (is_summary) {
		wprintf("</div>");		/* end of 'content' div */

		wprintf("<div id=\"message_list\">"

			"<div id=\"fix_scrollbar_bug\">\n"

			"<form name=\"msgomatic\" "
			"method=\"POST\" action=\"/do_stuff_to_msgs\">\n"

			"<table border=0 cellspacing=0 "
			"cellpadding=0 width=100%%>\n"
			"<TR>"
			"<TD align=center><b><i>%s</i></b> %s</TD>"
			"<TD align=center><b><i>%s</i></b> %s</TD>"
			"<TD align=center><b><i>%s</i></b> %s</TD>"
			"<TD><INPUT TYPE=\"submit\" NAME=\"delete_button\" "
			"STYLE=\"font-family: Bitstream Vera Sans,Arial,Helvetica,sans-serif;"
			" font-size: 6pt;\" "
			"VALUE=\"%s\"></TD>"
			"</TR>\n"
			,
			_("Subject"),	subjsort_button,
			_("Sender"),	sendsort_button,
			_("Date"),	datesort_button,
			_("Delete")
		);
	}

	for (a = 0; a < nummsgs; ++a) {
		if ((WC->msgarr[a] >= startmsg) && (num_displayed < maxmsgs)) {

			/* Learn which msgs "Prev" & "Next" buttons go to */
			pn_current = WC->msgarr[a];
			if (a > 0) pn_previous = WC->msgarr[a-1];
			if (a < (nummsgs-1)) pn_next = WC->msgarr[a+1];

			/* If a tabular view, set up the line */
			if (is_summary) {
				bg = 1 - bg;
				wprintf("<tr bgcolor=\"#%s\" ",
					(bg ? "DDDDDD" : "FFFFFF")
				);

				wprintf("onClick=\" new Ajax.Updater('preview_pane', '/msg', { method: 'get', parameters: 'msgnum=%ld' } ); \" ", WC->summ[a].msgnum);

				wprintf(">");
			}

			/* Display the message */
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
				read_message(WC->msgarr[a], 0);
			}

			/* If a tabular view, finish the line */
			if (is_summary) {
				wprintf("</TR>\n");
			}

			if (lowest_displayed < 0) lowest_displayed = a;
			highest_displayed = a;

			++num_displayed;
			--remaining_messages;
		}
	}

	if (is_summary) {
		wprintf("</table></form>"
			"</div>\n");			/* end of 'fix_scrollbar_bug' div */
		wprintf("</div>");			/* end of 'message_list' div */

		wprintf("<div id=\"ml_slider\"></div>");	/* slider */

		wprintf("<div id=\"preview_pane\">");	/* The preview pane will initially be empty */
	}

	/* Bump these because although we're thinking in zero base, the user
	 * is a drooling idiot and is thinking in one base.
	 */
	++lowest_displayed;
	++highest_displayed;

	/* If we're only looking at one message, do a prev/next thing */
	if (num_displayed == 1) {
	   if ((!is_tasks) && (!is_calendar) && (!is_addressbook) && (!is_notes) && (!is_singlecard)) {

		wprintf("<div id=\"fix_scrollbar_bug\">"
			"<table border=0 width=100%% bgcolor=\"#dddddd\"><tr><td>");
		wprintf(_("Reading #%d of %d messages."), lowest_displayed, nummsgs);
		wprintf("</TD><TD ALIGN=RIGHT><FONT SIZE=+1>");

		if (pn_previous > 0L) {
			wprintf("<A HREF=\"/%s"
				"?startmsg=%ld"
				"?maxmsgs=1"
				"?summary=0\">"
				"%s</A> \n",
					oper,
					pn_previous,
					_("Previous"));
		}

		if (pn_next > 0L) {
			wprintf("<A HREF=\"/%s"
				"?startmsg=%ld"
				"?maxmsgs=1"
				"?summary=0\">"
				"%s</A> \n",
					oper,
					pn_next,
					_("Next"));
		}

		wprintf("<A HREF=\"/%s?startmsg=%ld"
			"?maxmsgs=%d?summary=1\">"
			"%s"
			"</A>",
			oper,
			WC->msgarr[0],
			DEFAULT_MAXMSGS,
			_("Summary")
		);

		wprintf("</td></tr></table></div>\n");
	    }
	}

	/*
	 * If we're not currently looking at ALL requested
	 * messages, then display the selector bar
	 */
	if (num_displayed > 1) {
	   if ((!is_tasks) && (!is_calendar) && (!is_addressbook)
	      && (!is_notes) && (!is_singlecard) && (!is_summary)) {

		wprintf("<form name=\"msgomatic\" "
			"method=\"POST\" action=\"/do_stuff_to_msgs\">\n");

		wprintf(_("Reading #"), lowest_displayed, highest_displayed);

		wprintf("<select name=\"whichones\" size=\"1\" "
			"OnChange=\"location.href=msgomatic.whichones.options"
			"[selectedIndex].value\">\n");

		for (b=0; b<nummsgs; b = b + maxmsgs) {
		lo = b+1;
		hi = b+maxmsgs;
		if (hi > nummsgs) hi = nummsgs;
			wprintf("<option %s value="
				"\"/%s"
				"?startmsg=%ld"
				"?maxmsgs=%d"
				"?summary=%d\">"
				"%d-%d</option> \n",
				((WC->msgarr[b] == startmsg) ? "selected" : ""),
				oper,
				WC->msgarr[b],
				maxmsgs,
				is_summary,
				lo, hi);
		}
		wprintf("<option value=\"/%s?startmsg=%ld"
			"?maxmsgs=9999999?summary=%d\">"
			"ALL"
			"</option> ",
			oper,
			WC->msgarr[0], is_summary);

		wprintf("</select> ");
		wprintf(_("of %d messages."), nummsgs);
		wprintf("</form>\n");
	    }
	}

DONE:
	if (is_tasks) {
		do_tasks_view();	/* Render the task list */
	}

	if (is_calendar) {
		do_calendar_view();	/* Render the calendar */
	}

	if (is_addressbook) {
		do_addrbook_view(addrbook, num_ab);	/* Render the address book */
	}

	/* Put the data transfer hidden iframe in a hidden div, to make it *really* hidden */
	wprintf("</div>"
		"<div display=\"hidden\">\n"
		"<iframe name=\"msgloader1\" id=\"msgloader1\" width=\"1\"></iframe>\n"
	);

	/* Note: wDumpContent() will output one additional </div> tag. */
	wDumpContent(1);
	if (addrbook != NULL) free(addrbook);

	/* free the summary */
	if (WC->num_summ != 0) {
		WC->num_summ = 0;
		free(WC->summ);
	}

	/* If we got here via a mailbox view and are reading a single
	 * message, mark it as "seen." We do this after rendering the web page
	 * so it doesn't keep the user waiting.
	 */
	if ( (maxmsgs == 1) && (WC->wc_view == VIEW_MAILBOX) ) {
		serv_printf("SEEN %ld|1", startmsg);
		serv_getln(buf, sizeof buf);
	}
}


/*
 * Back end for post_message() ... this is where the actual message
 * gets transmitted to the server.
 */
void post_mime_to_server(void) {
	char boundary[SIZ];
	int is_multipart = 0;
	static int seq = 0;
	struct wc_attachment *att;
	char *encoded;
	size_t encoded_length;

	/* RFC2045 requires this, and some clients look for it... */
	serv_puts("MIME-Version: 1.0");

	/* If there are attachments, we have to do multipart/mixed */
	if (WC->first_attachment != NULL) {
		is_multipart = 1;
	}

	if (is_multipart) {
		sprintf(boundary, "---Citadel-Multipart-%s-%04x%04x---",
			serv_info.serv_fqdn,
			getpid(),
			++seq
		);

		/* Remember, serv_printf() appends an extra newline */
		serv_printf("Content-type: multipart/mixed; "
			"boundary=\"%s\"\n", boundary);
		serv_printf("This is a multipart message in MIME format.\n");
		serv_printf("--%s", boundary);
	}

	serv_puts("Content-type: text/html; charset=utf-8");
	serv_puts("");
	serv_puts("<HTML><BODY>\n");
	text_to_server(bstr("msgtext"), 0);
	serv_puts("</BODY></HTML>\n");
	

	if (is_multipart) {

		/* Add in the attachments */
		for (att = WC->first_attachment; att!=NULL; att=att->next) {

			encoded_length = ((att->length * 150) / 100);
			encoded = malloc(encoded_length);
			if (encoded == NULL) break;
			CtdlEncodeBase64(encoded, att->data, att->length);

			serv_printf("--%s", boundary);
			serv_printf("Content-type: %s", att->content_type);
			serv_printf("Content-disposition: attachment; "
				"filename=\"%s\"", att->filename);
			serv_puts("Content-transfer-encoding: base64");
			serv_puts("");
			serv_write(encoded, strlen(encoded));
			serv_puts("");
			serv_puts("");
			free(encoded);
		}
		serv_printf("--%s--", boundary);
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
	char buf[SIZ];
	static long dont_post = (-1L);
	struct wc_attachment *att, *aptr;

	if (WC->upload_length > 0) {

		/* There's an attachment.  Save it to this struct... */
		att = malloc(sizeof(struct wc_attachment));
		memset(att, 0, sizeof(struct wc_attachment));
		att->length = WC->upload_length;
		strcpy(att->content_type, WC->upload_content_type);
		strcpy(att->filename, WC->upload_filename);
		att->next = NULL;

		/* And add it to the list. */
		if (WC->first_attachment == NULL) {
			WC->first_attachment = att;
		}
		else {
			aptr = WC->first_attachment;
			while (aptr->next != NULL) aptr = aptr->next;
			aptr->next = att;
		}

		/* Mozilla sends a simple filename, which is what we want,
		 * but Satan's Browser sends an entire pathname.  Reduce
		 * the path to just a filename if we need to.
		 */
		while (num_tokens(att->filename, '/') > 1) {
			remove_token(att->filename, 0, '/');
		}
		while (num_tokens(att->filename, '\\') > 1) {
			remove_token(att->filename, 0, '\\');
		}

		/* Transfer control of this memory from the upload struct
		 * to the attachment struct.
		 */
		att->data = WC->upload;
		WC->upload_length = 0;
		WC->upload = NULL;
		display_enter();
		return;
	}

	if (strlen(bstr("cancel_button")) > 0) {
		sprintf(WC->ImportantMessage, 
			_("Cancelled.  Message was not posted."));
	} else if (strlen(bstr("attach_button")) > 0) {
		display_enter();
		return;
	} else if (atol(bstr("postseq")) == dont_post) {
		sprintf(WC->ImportantMessage, 
			_("Automatically cancelled because you have already "
			"saved this message."));
	} else {
		sprintf(buf, "ENT0 1|%s|0|4|%s|||%s|%s",
			bstr("recp"),
			bstr("subject"),
			bstr("cc"),
			bstr("bcc")
		);
		serv_puts(buf);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '4') {
			post_mime_to_server();
			if ( (strlen(bstr("recp")) > 0)
			   || (strlen(bstr("cc")) > 0)
			   || (strlen(bstr("bcc")) > 0)
			) {
				sprintf(WC->ImportantMessage, _("Message has been sent.\n"));
			}
			else {
				sprintf(WC->ImportantMessage, _("Message has been posted.\n"));
			}
			dont_post = atol(bstr("postseq"));
		} else {
			sprintf(WC->ImportantMessage, "%s", &buf[4]);
			display_enter();
			return;
		}
	}

	free_attachments(WC);
	readloop("readnew");
}




/*
 * display the message entry screen
 */
void display_enter(void)
{
	char buf[SIZ];
	char ebuf[SIZ];
	long now;
	struct wc_attachment *att;
	int recipient_required = 0;
	int recipient_bad = 0;
	int i;

	if (strlen(bstr("force_room")) > 0) {
		gotoroom(bstr("force_room"));
	}

	/* Are we perhaps in an address book view?  If so, then an "enter
	 * message" command really means "add new entry."
	 */
	if (WC->wc_view == VIEW_ADDRESSBOOK) {
		do_edit_vcard(-1, "", "");
		return;
	}

#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	/* Are we perhaps in a calendar view?  If so, then an "enter
	 * message" command really means "add new calendar item."
	 */
	if (WC->wc_view == VIEW_CALENDAR) {
		display_edit_event();
		return;
	}

	/* Are we perhaps in a tasks view?  If so, then an "enter
	 * message" command really means "add new task."
	 */
	if (WC->wc_view == VIEW_TASKS) {
		display_edit_task();
		return;
	}
#endif

	/*
	 * Otherwise proceed normally.
	 * Do a custom room banner with no navbar...
	 */
	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	embed_room_banner(NULL, navbar_none);
	wprintf("</div>\n");
	wprintf("<div id=\"content\">\n"
		"<div id=\"fix_scrollbar_bug\">"
		"<table width=100%% border=0 bgcolor=\"#ffffff\"><tr><td>");

	/* First test to see whether this is a room that requires recipients to be entered */
	serv_puts("ENT0 0");
	serv_getln(buf, sizeof buf);
	if (!strncmp(buf, "570", 3)) {		/* 570 means that we need a recipient here */
		recipient_required = 1;
	}
	else if (buf[0] != '2') {		/* Any other error means that we cannot continue */
		wprintf("<EM>%s</EM><br />\n", &buf[4]);
		goto DONE;
	}

	/* Now check our actual recipients if there are any */
	if (recipient_required) {
		sprintf(buf, "ENT0 0|%s|0|0||||%s|%s", bstr("recp"), bstr("cc"), bstr("bcc"));
		serv_puts(buf);
		serv_getln(buf, sizeof buf);

		if (!strncmp(buf, "570", 3)) {	/* 570 means we have an invalid recipient listed */
			if (strlen(bstr("recp")) + strlen(bstr("cc")) + strlen(bstr("bcc")) > 0) {
				recipient_bad = 1;
			}
		}
		else if (buf[0] != '2') {	/* Any other error means that we cannot continue */
			wprintf("<EM>%s</EM><br />\n", &buf[4]);
			goto DONE;
		}
	}

	/* If we got this far, we can display the message entry screen. */

	now = time(NULL);
	fmt_date(buf, now, 0);
	strcat(&buf[strlen(buf)], _(" <I>from</I> "));
	stresc(&buf[strlen(buf)], WC->wc_username, 1, 1);

	/* Don't need this anymore, it's in the input box below
	if (strlen(bstr("recp")) > 0) {
		strcat(&buf[strlen(buf)], _(" <I>to</I> "));
		stresc(&buf[strlen(buf)], bstr("recp"), 1, 1);
	}
	*/

	strcat(&buf[strlen(buf)], _(" <I>in</I> "));
	stresc(&buf[strlen(buf)], WC->wc_roomname, 1, 1);

	/* begin message entry screen */
	wprintf("<form enctype=\"multipart/form-data\" "
		"method=\"POST\" action=\"/post\" "
		"name=\"enterform\""
		">\n");
	wprintf("<input type=\"hidden\" name=\"postseq\" value=\"%ld\">\n", now);

	wprintf("<img src=\"static/newmess3_24x.gif\" align=middle alt=\" \">");
	wprintf("%s<br>\n", buf);	/* header bar */

	wprintf("<table border=\"0\" width=\"100%%\">\n");
	if (recipient_required) {

		wprintf("<tr><td>");
		wprintf("<font size=-1>");
		wprintf(_("To:"));
		wprintf("</font>");
		wprintf("</td><td>"
			"<input autocomplete=\"off\" type=\"text\" name=\"recp\" id=\"recp_id\" value=\"");
		escputs(bstr("recp"));
		wprintf("\" size=50 maxlength=1000 />");
		wprintf("<div class=\"auto_complete\" id=\"recp_name_choices\"></div>");
		wprintf("</td><td></td></tr>\n");

		wprintf("<tr><td>");
		wprintf("<font size=-1>");
		wprintf(_("CC:"));
		wprintf("</font>");
		wprintf("</td><td>"
			"<input autocomplete=\"off\" type=\"text\" name=\"cc\" id=\"cc_id\" value=\"");
		escputs(bstr("cc"));
		wprintf("\" size=50 maxlength=1000 />");
		wprintf("<div class=\"auto_complete\" id=\"cc_name_choices\"></div>");
		wprintf("</td><td></td></tr>\n");

		wprintf("<tr><td>");
		wprintf("<font size=-1>");
		wprintf(_("BCC:"));
		wprintf("</font>");
		wprintf("</td><td>"
			"<input autocomplete=\"off\" type=\"text\" name=\"bcc\" id=\"bcc_id\" value=\"");
		escputs(bstr("bcc"));
		wprintf("\" size=50 maxlength=1000 />");
		wprintf("<div class=\"auto_complete\" id=\"bcc_name_choices\"></div>");
		wprintf("</td><td></td></tr>\n");

		/* Initialize the autocomplete ajax helpers (found in wclib.js) */
		wprintf("<script type=\"text/javascript\">	\n"
			" activate_entmsg_autocompleters();	\n"
			"</script>				\n"
		);
	}

	wprintf("<tr><td>");
	wprintf("<font size=-1>");
	wprintf(_("Subject (optional):"));
	wprintf("</font>");
	wprintf("</td><td>"
		"<input type=\"text\" name=\"subject\" value=\"");
	escputs(bstr("subject"));
	wprintf("\" size=50 maxlength=70></td><td>\n");

	wprintf("<input type=\"submit\" name=\"send_button\" value=\"");
	if (recipient_required) {
		wprintf(_("Send message"));
	} else {
		wprintf(_("Post message"));
	}
	wprintf("\">&nbsp;"
		"<input type=\"submit\" name=\"cancel_button\" value=\"%s\">\n", _("Cancel"));
	wprintf("</td></tr></table>\n");

	wprintf("<center>");

	wprintf("<textarea name=\"msgtext\" cols=\"80\" rows=\"15\">");

	/* If we're continuing from a previous edit, put our partially-composed message back... */
	msgescputs(bstr("msgtext"));

	/* If we're forwarding a message, insert it here... */
	if (atol(bstr("fwdquote")) > 0L) {
		wprintf("<br><div align=center><i>");
		wprintf(_("--- forwarded message ---"));
		wprintf("</i></div><br>");
		pullquote_message(atol(bstr("fwdquote")), 1);
	}

	/* If we're replying quoted, insert the quote here... */
	else if (atol(bstr("replyquote")) > 0L) {
		wprintf("<br>"
			"<blockquote>");
		pullquote_message(atol(bstr("replyquote")), 0);
		wprintf("</blockquote>");
	}

	/* Insert our signature if appropriate... */
	if ( (WC->is_mailbox) && (strcmp(bstr("sig_inserted"), "yes")) ) {
		get_preference("use_sig", buf, sizeof buf);
		if (!strcasecmp(buf, "yes")) {
			get_preference("signature", ebuf, sizeof ebuf);
			euid_unescapize(buf, ebuf);
			wprintf("<br>--<br>");
			for (i=0; i<strlen(buf); ++i) {
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
	wprintf("</center><br />\n");

	/*
	 * The following script embeds the TinyMCE richedit control, and automatically
	 * transforms the textarea into a richedit textarea.
	 */
	wprintf(
		"<script language=\"javascript\" type=\"text/javascript\" src=\"tiny_mce/tiny_mce.js\"></script>\n"
		"<script language=\"javascript\" type=\"text/javascript\">"
		"tinyMCE.init({"
		"	mode : \"textareas\", width : \"100%%\", browsers : \"msie,gecko\" "
		"});"
		"</script>\n"
	);

	/* Enumerate any attachments which are already in place... */
	wprintf("<img src=\"/static/diskette_24x.gif\" border=0 "
		"align=middle height=16 width=16> Attachments: ");
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

	/* Now offer the ability to attach additional files... */
	wprintf("&nbsp;&nbsp;&nbsp;"
		"Attach file: <input NAME=\"attachfile\" "
		"SIZE=16 TYPE=\"file\">\n&nbsp;&nbsp;"
		"<input type=\"submit\" name=\"attach_button\" value=\"%s\">\n", _("Add"));

	/* Seth asked for these to be at the top *and* bottom... */
	wprintf("<input type=\"submit\" name=\"send_button\" value=\"");
	if (recipient_required) {
		wprintf(_("Send message"));
	} else {
		wprintf(_("Post message"));
	}
	wprintf("\">&nbsp;"
		"<input type=\"submit\" name=\"cancel_button\" value=\"%s\">\n", _("Cancel"));

	/* Make sure we only insert our signature once */
	if (strcmp(bstr("sig_inserted"), "yes")) {
		wprintf("<INPUT TYPE=\"hidden\" NAME=\"sig_inserted\" VALUE=\"yes\">\n");
	}

	wprintf("</form>\n");

	wprintf("</td></tr></table></div>\n");
DONE:	wDumpContent(1);
}








void delete_msg(void)
{
	long msgid;
	char buf[SIZ];

	msgid = atol(bstr("msgid"));

	output_headers(1, 1, 1, 0, 0, 0);

	sprintf(buf, "DELE %ld", msgid);
	serv_puts(buf);
	serv_getln(buf, sizeof buf);
	wprintf("<EM>%s</EM><br />\n", &buf[4]);

	wDumpContent(1);
}




/*
 * Confirm move of a message
 */
void confirm_move_msg(void)
{
	long msgid;
	char buf[SIZ];
	char targ[SIZ];

	msgid = atol(bstr("msgid"));


	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">");
	wprintf(_("Confirm move of message"));
	wprintf("</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	wprintf("<CENTER>");

	wprintf(_("Move this message to:"));
	wprintf("<br />\n");

	wprintf("<form METHOD=\"POST\" ACTION=\"/move_msg\">\n");
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



void move_msg(void)
{
	long msgid;
	char buf[SIZ];

	msgid = atol(bstr("msgid"));

	if (strlen(bstr("move_button")) > 0) {
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
 * This gets called when a user selects multiple messages in a summary
 * list and then clicks to perform a transformation of some sort on them
 * (such as deleting them).
 */
void do_stuff_to_msgs(void) {
	char buf[256];

	struct stuff_t {
		struct stuff_t *next;
		long msgnum;
	};

	struct stuff_t *stuff = NULL;
	struct stuff_t *ptr;
	int delete_button_pressed = 0;


	serv_puts("MSGS ALL");
	serv_getln(buf, sizeof buf);

	if (buf[0] == '1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		ptr = malloc(sizeof(struct stuff_t));
		ptr->msgnum = atol(buf);
		ptr->next = stuff;
		stuff = ptr;
	}

	if (strlen(bstr("delete_button")) > 0) {
		delete_button_pressed = 1;
	}

	while (stuff != NULL) {

		sprintf(buf, "msg_%ld", stuff->msgnum);
		if (!strcasecmp(bstr(buf), "yes")) {

			if (delete_button_pressed) {
				serv_printf("DELE %ld", stuff->msgnum);
				serv_getln(buf, sizeof buf);
			}

		}

		ptr = stuff->next;
		free(stuff);
		stuff = ptr;
	}

	readloop("readfwd");
}
