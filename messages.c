/*
 * $Id$
 *
 * Functions which deal with the fetching and displaying of messages.
 *
 */

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"
#include "vcard.h"
#include "webserver.h"


/* Address book entry (keep it short and sweet, it's just a quickie lookup
 * which we can use to get to the real meat and bones later)
 */
struct addrbookent {
	char ab_name[64];
	long ab_msgnum;
};


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


/* display_vcard() calls this after parsing the textual vCard into
 * our 'struct vCard' data object.
 * This gets called instead of display_parsed_vcard() if we are only looking
 * to extract the person's name instead of displaying the card.
 */
void fetchname_parsed_vcard(struct vCard *v, char *storename) {
	int i;

	strcpy(storename, "");
	if (v->numprops) for (i=0; i<(v->numprops); ++i) {
		if (!strcasecmp(v->prop[i].name, "n")) {
			strcpy(storename, v->prop[i].value);
		}
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
	char phone[SIZ];
	char mailto[SIZ];

	strcpy(displayname, "");
	strcpy(phone, "");
	strcpy(mailto, "");

	if (!full) {
		wprintf("<TD>");
		name = vcard_get_prop(v, "fn", 1, 0, 0);
		if (name == NULL) name = vcard_get_prop(v, "n", 1, 0, 0);
		if (name != NULL) {
			strcpy(buf, name);
			escputs(buf);
		}
		else {
			wprintf("&nbsp;");
		}
		wprintf("</TD>");
		return;
	}

	wprintf("<TABLE bgcolor=#888888>");
	for (pass=1; pass<=2; ++pass) {

		if (v->numprops) for (i=0; i<(v->numprops); ++i) {

			thisname = strdup(v->prop[i].name);
			extract_token(firsttoken, thisname, 0, ';');
	
			for (j=0; j<num_tokens(thisname, ';'); ++j) {
				extract_token(buf, thisname, j, ';');
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
				}
			}
	
			/* FN (full name) is a true 'display name' field */
			else if (!strcasecmp(firsttoken, "fn")) {
				strcpy(displayname, thisvalue);
			}
	
			else if (!strcasecmp(firsttoken, "email")) {
				if (strlen(mailto) > 0) strcat(mailto, "<BR>");
				strcat(mailto,
					"<A HREF=\"/display_enter"
					"?force_room=_MAIL_&recp=");
				urlesc(&mailto[strlen(mailto)], thisvalue);
				strcat(mailto, "\">");
				urlesc(&mailto[strlen(mailto)], thisvalue);
				strcat(mailto, "</A>");
			}
			else if (!strcasecmp(firsttoken, "tel")) {
				if (strlen(phone) > 0) strcat(phone, "<BR>");
				strcat(phone, thisvalue);
				for (j=0; j<num_tokens(thisname, ';'); ++j) {
					extract_token(buf, thisname, j, ';');
					if (!strcasecmp(buf, "tel"))
						strcat(phone, "");
					else if (!strcasecmp(buf, "work"))
						strcat(phone, " (work)");
					else if (!strcasecmp(buf, "home"))
						strcat(phone, " (home)");
					else if (!strcasecmp(buf, "cell"))
						strcat(phone, " (cell)");
					else {
						strcat(phone, " (");
						strcat(phone, buf);
						strcat(phone, ")");
					}
				}
			}
			else if (!strcasecmp(firsttoken, "adr")) {
				if (pass == 2) {
					wprintf("<TR><TD>Address:</TD><TD>");
					for (j=0; j<num_tokens(thisvalue, ';'); ++j) {
						extract_token(buf, thisvalue, j, ';');
						if (strlen(buf) > 0) {
							escputs(buf);
							wprintf("<BR>");
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
				if (pass == 2) {
					wprintf("<TR><TD>");
					escputs(thisname);
					wprintf("</TD><TD>");
					escputs(thisvalue);
					wprintf("</TD></TR>\n");
				}
			}
	
			free(thisname);
			free(thisvalue);
		}
	
		if (pass == 1) {
			wprintf("<TR BGCOLOR=\"#AAAAAA\">"
			"<TD COLSPAN=2 BGCOLOR=\"#FFFFFF\">"
			"<IMG ALIGN=CENTER SRC=\"/static/vcard.gif\">"
			"<FONT SIZE=+1><B>");
			escputs(displayname);
			wprintf("</B></FONT></TD></TR>\n");
		
			if (strlen(phone) > 0)
				wprintf("<TR><TD>Telephone:</TD><TD>%s</TD></TR>\n", phone);
			if (strlen(mailto) > 0)
				wprintf("<TR><TD>E-mail:</TD><TD>%s</TD></TR>\n", mailto);
		}

	}

	wprintf("</TABLE>\n");
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
	else if ( (alpha == 0)
	   || ((isalpha(alpha)) && (tolower(alpha) == tolower(this_alpha)) )
	   || ((!isalpha(alpha)) && (!isalpha(this_alpha))) ) {
		display_parsed_vcard(v, full);
	}

	vcard_free(v);
}




/*
 * I wanna SEE that message!
 */
void read_message(long msgnum) {
	char buf[SIZ];
	char mime_partnum[SIZ];
	char mime_filename[SIZ];
	char mime_content_type[SIZ];
	char mime_disposition[SIZ];
	int mime_length;
	char mime_http[SIZ];
	char m_subject[SIZ];
	char from[SIZ];
	char node[SIZ];
	char rfca[SIZ];
	char reply_to[512];
	char now[SIZ];
	int format_type = 0;
	int nhdr = 0;
	int bq = 0;
	char vcard_partnum[SIZ];
	char cal_partnum[SIZ];
	char *part_source = NULL;

	strcpy(from, "");
	strcpy(node, "");
	strcpy(rfca, "");
	strcpy(reply_to, "");
	strcpy(vcard_partnum, "");
	strcpy(cal_partnum, "");
	strcpy(mime_http, "");

	serv_printf("MSG4 %ld", msgnum);
	serv_gets(buf);
	if (buf[0] != '1') {
		wprintf("<STRONG>ERROR:</STRONG> %s<BR>\n", &buf[4]);
		return;
	}

	/* begin everythingamundo table */
	wprintf("<table width=100% border=1 cellspacing=0 "
		"cellpadding=0><TR><TD>\n");

	/* begin message header table */
	wprintf("<TABLE WIDTH=100%% BORDER=0 CELLSPACING=0 "
		"CELLPADDING=1 BGCOLOR=\"#CCCCCC\"><TR><TD>\n");

	wprintf("<SPAN CLASS=\"message_header\">");
	strcpy(m_subject, "");

	while (serv_gets(buf), strcasecmp(buf, "text")) {
		if (!strcmp(buf, "000")) {
			wprintf("<I>unexpected end of message</I><BR><BR>\n");
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
			wprintf("from <A HREF=\"/showuser&who=");
			urlescputs(from);
			wprintf("\">");
			escputs(from);
			wprintf("</A> ");
		}
		if (!strncasecmp(buf, "subj=", 5))
			strcpy(m_subject, &buf[5]);
		if ((!strncasecmp(buf, "hnod=", 5))
		    && (strcasecmp(&buf[5], serv_info.serv_humannode)))
			wprintf("(%s) ", &buf[5]);
		if ((!strncasecmp(buf, "room=", 5))
		    && (strcasecmp(&buf[5], WC->wc_roomname))
		    && (strlen(&buf[5])>0) )
			wprintf("in %s> ", &buf[5]);
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
		if (!strncasecmp(buf, "rcpt=", 5))
			wprintf("to %s ", &buf[5]);
		if (!strncasecmp(buf, "time=", 5)) {
			fmt_date(now, atol(&buf[5]));
			wprintf("%s ", now);
		}

		if (!strncasecmp(buf, "part=", 5)) {
			extract(mime_filename, &buf[5], 1);
			extract(mime_partnum, &buf[5], 2);
			extract(mime_disposition, &buf[5], 3);
			extract(mime_content_type, &buf[5], 4);
			mime_length = extract_int(&buf[5], 5);

			if (!strcasecmp(mime_disposition, "attachment")) {
				snprintf(&mime_http[strlen(mime_http)],
					(sizeof(mime_http) - strlen(mime_http) - 1),
					"<A HREF=\"/output_mimepart?"
					"msgnum=%ld&partnum=%s\" "
					"TARGET=\"wc.%ld.%s\">"
					"<IMG SRC=\"/static/attachment.gif\" "
					"BORDER=0 ALIGN=MIDDLE>\n"
					"Part %s: %s (%s, %d bytes)</A><BR>\n",
					msgnum, mime_partnum,
					msgnum, mime_partnum,
					mime_partnum, mime_filename,
					mime_content_type, mime_length);
			}

			if ((!strcasecmp(mime_disposition, "inline"))
			   && (!strncasecmp(mime_content_type, "image/", 6)) ){
				snprintf(&mime_http[strlen(mime_http)],
					(sizeof(mime_http) - strlen(mime_http) - 1),
					"<IMG SRC=\"/output_mimepart?"
					"msgnum=%ld&partnum=%s\">",
					msgnum, mime_partnum);
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

	wprintf("</SPAN></TD>");

	wprintf("<TD ALIGN=RIGHT>\n"
		"<TABLE BORDER=0><TR>\n");

	wprintf("<TD BGCOLOR=\"#AAAADD\">"
		"<A HREF=\"/display_enter?recp=");
	urlescputs(reply_to);
	if (!strncasecmp(m_subject, "Re:", 2)) {
		wprintf("&subject=");
		escputs(m_subject);
	}
	else if (strlen(m_subject) > 0) {
		wprintf("&subject=Re:%%20");
		escputs(m_subject);
	}
	wprintf("\"><FONT SIZE=-1>Reply</FONT></A></TD>\n", msgnum);

	if (WC->is_room_aide) {
		wprintf("<TD BGCOLOR=\"#AAAADD\">"
			"<A HREF=\"/confirm_move_msg"
			"&msgid=%ld"
			"\"><FONT SIZE=-1>Move</FONT></A>"
			"</TD>\n", msgnum);

		wprintf("<TD BGCOLOR=\"#AAAADD\">"
			"<A HREF=\"/delete_msg"
			"&msgid=%ld\""
			"onClick=\"return confirm('Delete this message?');\""
			"><FONT SIZE=-1>Del</FONT></A>"
			"</TD>\n", msgnum);
	}

	wprintf("</TR></TABLE>\n"
		"</TD>\n");

	if (strlen(m_subject) > 0) {
		wprintf("<TR><TD>"
			"<SPAN CLASS=\"message_subject\">"
			"Subject: %s"
			"</SPAN>"
			"</TD><TD>&nbsp;</TD></TR>\n", m_subject);
	}

	wprintf("</TR></TABLE>\n");

	/* Begin body */
	wprintf("<TABLE BORDER=0 WIDTH=100%% BGCOLOR=#FFFFFF "
		"CELLPADDING=1 CELLSPACING=0><TR><TD>");

	/* 
	 * Learn the content type
	 */
	strcpy(mime_content_type, "text/plain");
	while (serv_gets(buf), (strlen(buf) > 0)) {
		if (!strcmp(buf, "000")) {
			wprintf("<I>unexpected end of message</I><BR><BR>\n");
			goto ENDBODY;
		}
		if (!strncasecmp(buf, "Content-type: ", 14)) {
			safestrncpy(mime_content_type, &buf[14],
				sizeof(mime_content_type));
		}
	}

	/* Messages in legacy Citadel variformat get handled thusly... */
	if (!strcasecmp(mime_content_type, "text/x-citadel-variformat")) {
		fmout(NULL, "JUSTIFY");
	}

	/* Boring old 80-column fixed format text gets handled this way... */
	else if (!strcasecmp(mime_content_type, "text/plain")) {
		while (serv_gets(buf), strcmp(buf, "000")) {
			if (buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = 0;
			if (buf[strlen(buf)-1] == '\r') buf[strlen(buf)-1] = 0;
			while ((strlen(buf) > 0) && (isspace(buf[strlen(buf) - 1])))
				buf[strlen(buf) - 1] = 0;
			if ((bq == 0) &&
		    	((!strncmp(buf, ">", 1)) || (!strncmp(buf, " >", 2)) || (!strncmp(buf, " :-)", 4)))) {
				wprintf("<SPAN CLASS=\"pull_quote\">");
				bq = 1;
			} else if ((bq == 1) &&
			   	(strncmp(buf, ">", 1)) && (strncmp(buf, " >", 2)) && (strncmp(buf, " :-)", 4))) {
				wprintf("</SPAN>");
				bq = 0;
			}
			wprintf("<TT>");
			url(buf);
			escputs(buf);
			wprintf("</TT><BR>\n");
		}
		wprintf("</I><BR>");
	}

	else /* HTML is fun, but we've got to strip it first */
	if (!strcasecmp(mime_content_type, "text/html")) {
		output_html();
	}

	/* Unknown weirdness */
	else {
		wprintf("I don't know how to display %s<BR>\n",
			mime_content_type);
		while (serv_gets(buf), strcmp(buf, "000")) { }
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
			if ( (!strcasecmp(WC->wc_roomname, USERCONFIGROOM))
			   || (!strcasecmp(&WC->wc_roomname[11], USERCONFIGROOM))) {
				wprintf("<A HREF=\"/edit_vcard?"
					"msgnum=%ld&partnum=%s\">",
					msgnum, vcard_partnum);
				wprintf("(edit)</A>");
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
	wprintf("</TD></TR></TABLE><BR>\n");
}


void summarize_message(long msgnum) {
	char buf[SIZ];

	struct {
		char date[SIZ];
		char from[SIZ];
		char to[SIZ];
		char subj[SIZ];
		int hasattachments;
	} summ;

	memset(&summ, 0, sizeof(summ));
	strcpy(summ.subj, "(no subject)");

	sprintf(buf, "MSG0 %ld|3", msgnum);	/* ask for headers only with no MIME */
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '1') return;

	while (serv_gets(buf), strcmp(buf, "000")) {
		if (!strncasecmp(buf, "from=", 5)) {
			strcpy(summ.from, &buf[5]);
		}
		if (!strncasecmp(buf, "subj=", 5)) {
			strcpy(summ.subj, &buf[5]);
		}
		if (!strncasecmp(buf, "rfca=", 5)) {
			strcat(summ.from, " <");
			strcat(summ.from, &buf[5]);
			strcat(summ.from, ">");
		}

		if (!strncasecmp(buf, "node=", 5)) {
			if ( ((WC->room_flags & QR_NETWORK)
			|| ((strcasecmp(&buf[5], serv_info.serv_nodename)
			&& (strcasecmp(&buf[5], serv_info.serv_fqdn)))))
			) {
				strcat(summ.from, " @ ");
				strcat(summ.from, &buf[5]);
			}
		}

		if (!strncasecmp(buf, "rcpt=", 5)) {
			strcpy(summ.to, &buf[5]);
		}

		if (!strncasecmp(buf, "time=", 5)) {
			fmt_date(summ.date, atol(&buf[5]));
		}
	}

	wprintf("<TD><A HREF=\"/readfwd?startmsg=%ld"
		"&maxmsgs=1&summary=0\">", 
		msgnum);
	escputs(summ.subj);
	wprintf("</A></TD><TD>");
	escputs(summ.from);
	wprintf(" </TD><TD>");
	escputs(summ.date);
	wprintf(" </TD>");
	wprintf("<TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"msg_%ld\" VALUE=\"yes\">"
		"</TD>\n");

	return;
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

	struct {
		char date[SIZ];
		char from[SIZ];
		char to[SIZ];
		char subj[SIZ];
		int hasattachments;
	} summ;

	memset(&summ, 0, sizeof(summ));
	strcpy(summ.subj, "(no subject)");

	sprintf(buf, "MSG0 %ld|1", msgnum);	/* ask for headers only */
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '1') return;

	while (serv_gets(buf), strcmp(buf, "000")) {
		if (!strncasecmp(buf, "part=", 5)) {
			extract(mime_filename, &buf[5], 1);
			extract(mime_partnum, &buf[5], 2);
			extract(mime_disposition, &buf[5], 3);
			extract(mime_content_type, &buf[5], 4);
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
			if ( (!strcasecmp(WC->wc_roomname, USERCONFIGROOM))
			   || (!strcasecmp(&WC->wc_roomname[11], USERCONFIGROOM))) {
				wprintf("<A HREF=\"/edit_vcard?"
					"msgnum=%ld&partnum=%s\">",
					msgnum, vcard_partnum);
				wprintf("(edit)</A>");
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

	extract_token(lastname, namebuf, i-1, ' ');
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

	struct {
		char date[SIZ];
		char from[SIZ];
		char to[SIZ];
		char subj[SIZ];
		int hasattachments;
	} summ;

	if (namebuf == NULL) return;
	strcpy(namebuf, "");

	memset(&summ, 0, sizeof(summ));
	strcpy(summ.subj, "(no subject)");

	sprintf(buf, "MSG0 %ld|1", msgnum);	/* ask for headers only */
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '1') return;

	while (serv_gets(buf), strcmp(buf, "000")) {
		if (!strncasecmp(buf, "part=", 5)) {
			extract(mime_filename, &buf[5], 1);
			extract(mime_partnum, &buf[5], 2);
			extract(mime_disposition, &buf[5], 3);
			extract(mime_content_type, &buf[5], 4);
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
 * Render the address book using info we gathered during the scan
 */
void do_addrbook_view(struct addrbookent *addrbook, int num_ab) {
	int i = 0;
	int bg = 0;

	if (num_ab > 1) {
		qsort(addrbook, num_ab, sizeof(struct addrbookent), abcmp);
	}

	wprintf("<TABLE border=0 cellspacing=0 "
		"cellpadding=3 width=100%%>\n"
	);

	for (i=0; i<num_ab; ++i) {

		if ((i % 4) == 0) {
			if (i > 0) {
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
		wprintf("&maxmsgs=1&summary=0&alpha=%s\">", bstr("alpha"));
		escputs(addrbook[i].ab_name);
		wprintf("</A></TD>\n");
	}

	wprintf("</TR></TABLE>\n");
}



/* 
 * load message pointers from the server
 */
int load_msg_ptrs(char *servcmd)
{
	char buf[SIZ];
	int nummsgs;
	int maxload = 0;

	nummsgs = 0;
	maxload = sizeof(WC->msgarr) / sizeof(long) ;
	serv_puts(servcmd);
	serv_gets(buf);
	if (buf[0] != '1') {
		wprintf("<EM>%s</EM><BR>\n", &buf[4]);
		return (nummsgs);
	}
	while (serv_gets(buf), strcmp(buf, "000")) {
		if (nummsgs < maxload) {
			WC->msgarr[nummsgs] = atol(buf);
			++nummsgs;
		}
	}
	return (nummsgs);
}


/*
 * command loop for reading messages
 */
void readloop(char *oper)
{
	char cmd[SIZ];
	char buf[SIZ];
	int a, b, i;
	int nummsgs;
	long startmsg;
	int maxmsgs;
	int num_displayed = 0;
	int is_summary = 0;
	int is_addressbook = 0;
	int is_singlecard = 0;
	int is_calendar = 0;
	int is_tasks = 0;
	int remaining_messages;
	int lo, hi;
	int lowest_displayed = (-1);
	int highest_displayed = 0;
	long pn_previous = 0L;
	long pn_current = 0L;
	long pn_next = 0L;
	int bg = 0;
	char alpha = 0;
	char ab_alpha = 0;
	struct addrbookent *addrbook = NULL;
	int num_ab = 0;

	startmsg = atol(bstr("startmsg"));
	maxmsgs = atoi(bstr("maxmsgs"));
	is_summary = atoi(bstr("summary"));
	if (maxmsgs == 0) maxmsgs = DEFAULT_MAXMSGS;

	output_headers(1);

	if (!strcmp(oper, "readnew")) {
		strcpy(cmd, "MSGS NEW");
	} else if (!strcmp(oper, "readold")) {
		strcpy(cmd, "MSGS OLD");
	} else {
		strcpy(cmd, "MSGS ALL");
	}

	if ((WC->wc_view == VIEW_MAILBOX) && (maxmsgs > 1)) {
		is_summary = 1;
		strcpy(cmd, "MSGS ALL");
		/* maxmsgs = 32767; */
	}

	if ((WC->wc_view == VIEW_ADDRESSBOOK) && (maxmsgs > 1)) {
		is_addressbook = 1;
		strcpy(cmd, "MSGS ALL");
		maxmsgs = 32767;
	}

	is_singlecard = atoi(bstr("is_singlecard"));

	/* Display the letter indices across the top */
	if ((is_addressbook) || (is_singlecard)) {
		if (strlen(bstr("alpha")) == 0) {
			alpha = 'a';
		}
		else {
			strcpy(buf, bstr("alpha"));
			alpha = buf[0];
		}

		for (i='1'; i<='z'; ++i) if ((i=='1')||(islower(i))) {
			if ((i != alpha) || (is_singlecard)) {
				wprintf("<A HREF=\"/readfwd?alpha=%c\">", i);
			}
			if (i == alpha) wprintf("<FONT SIZE=+2>");
			if (isalpha(i)) {
				wprintf("%c", toupper(i));
			}
			else {
				wprintf("(other)");
			}
			if (i == alpha) wprintf("</FONT>");
			if ((i != alpha) || (is_singlecard)) {
				wprintf("</A>\n");
			}
			wprintf("&nbsp;");
		}

		wprintf("<HR width=100%%>\n");
	}

	if (WC->wc_view == VIEW_CALENDAR) {		/* calendar */
		is_calendar = 1;
		strcpy(cmd, "MSGS ALL");
		maxmsgs = 32767;
	}
	if (WC->wc_view == VIEW_TASKS) {		/* tasks */
		is_tasks = 1;
		strcpy(cmd, "MSGS ALL");
		maxmsgs = 32767;
		wprintf("<UL>");
	}

	nummsgs = load_msg_ptrs(cmd);
	if (nummsgs == 0) {

		if ((!is_tasks) && (!is_calendar)) {
			if (!strcmp(oper, "readnew")) {
				wprintf("<EM>No new messages in this room.</EM>\n");
			} else if (!strcmp(oper, "readold")) {
				wprintf("<EM>No old messages in this room.</EM>\n");
			} else {
				wprintf("<EM>This room is empty.</EM>\n");
			}
		}

		goto DONE;
	}

	if (startmsg == 0L) startmsg = WC->msgarr[0];
	remaining_messages = 0;

	for (a = 0; a < nummsgs; ++a) {
		if (WC->msgarr[a] >= startmsg) {
			++remaining_messages;
		}
	}

	if (is_summary) {
		wprintf("<FORM METHOD=\"POST\" ACTION=\"/do_stuff_to_msgs\">\n"
			"<TABLE border=0 cellspacing=0 "
			"cellpadding=0 width=100%%>\n"
			"<TR>"
			"<TD><I>Subject</I></TD>"
			"<TD><I>Sender</I></TD>"
			"<TD><I>Date</I></TD>"
			"<TD></TD>"
			"</TR>\n"
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
				wprintf("<TR BGCOLOR=\"#%s\">",
					(bg ? "DDDDDD" : "FFFFFF")
				);
			}

			/* Display the message */
			if (is_summary) {
				summarize_message(WC->msgarr[a]);
			}
			else if (is_addressbook) {
				fetch_ab_name(WC->msgarr[a], buf);
				if ((strlen(buf) > 0) && (isalpha(buf[0]))) {
					ab_alpha = tolower(buf[0]);
				}
				else {
					ab_alpha = '1';
				}
				if (alpha == ab_alpha) {
					++num_ab;
					addrbook = realloc(addrbook,
						(sizeof(struct addrbookent) * num_ab) );
					safestrncpy(addrbook[num_ab-1].ab_name, buf,
						sizeof(addrbook[num_ab-1].ab_name));
					addrbook[num_ab-1].ab_msgnum = WC->msgarr[a];
				}
			}
			else if (is_calendar) {
				display_calendar(WC->msgarr[a]);
			}
			else if (is_tasks) {
				display_task(WC->msgarr[a]);
			}
			else {
				read_message(WC->msgarr[a]);
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
		wprintf("</TABLE>\n");
	}

	if (is_tasks) {
		wprintf("</UL>\n");
	}

	/* Bump these because although we're thinking in zero base, the user
	 * is a drooling idiot and is thinking in one base.
	 */
	++lowest_displayed;
	++highest_displayed;

	/* If we're only looking at one message, do a prev/next thing */
	if (num_displayed == 1) {
	   if ((!is_tasks) && (!is_calendar) && (!is_addressbook) && (!is_singlecard)) {

		wprintf("<CENTER>"
			"<TABLE BORDER=0 WIDTH=100%% BGCOLOR=\"#DDDDDD\"><TR><TD>"
			"Reading #%d of %d messages.</TD>\n"
			"<TD ALIGN=RIGHT><FONT SIZE=+1>",
			lowest_displayed, nummsgs);

		if (is_summary) {
			wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" "
				"VALUE=\"Delete selected\">\n");
		}

		if (pn_previous > 0L) {
			wprintf("<A HREF=\"/%s"
				"?startmsg=%ld"
				"&maxmsgs=1"
				"&summary=0\">"
				"Previous</A> \n",
					oper,
					pn_previous );
		}

		if (pn_next > 0L) {
			wprintf("<A HREF=\"/%s"
				"?startmsg=%ld"
				"&maxmsgs=1"
				"&summary=0\">"
				"Next</A> \n",
					oper,
					pn_next );
		}

		wprintf("<A HREF=\"/%s?startmsg=%ld"
			"&maxmsgs=999999&summary=1\">"
			"Summary"
			"</A>",
			oper,
			WC->msgarr[0]);

		wprintf("</TD></TR></TABLE></CENTER>\n");
	    }
	}

	/*
	 * If we're not currently looking at ALL requested
	 * messages, then display the selector bar
	 */
	if (num_displayed > 1) {
	   if ((!is_tasks) && (!is_calendar) && (!is_addressbook) && (!is_singlecard)) {
		wprintf("<CENTER>"
			"<TABLE BORDER=0 WIDTH=100%% BGCOLOR=\"#DDDDDD\"><TR><TD>"
			"Reading #%d-%d of %d messages.</TD>\n"
			"<TD ALIGN=RIGHT><FONT SIZE=+1>",
			lowest_displayed, highest_displayed, nummsgs);

		if (is_summary) {
			wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" "
				"VALUE=\"Delete selected\">\n");
		}


		for (b=0; b<nummsgs; b = b + maxmsgs) {
		lo = b+1;
		hi = b+maxmsgs;
		if (hi > nummsgs) hi = nummsgs;
			if (WC->msgarr[b] != startmsg) {
				wprintf("<A HREF=\"/%s"
					"?startmsg=%ld"
					"&maxmsgs=%d"
					"&summary=%d\">"
					"%d-%d</A> \n",
						oper,
						WC->msgarr[b],
						maxmsgs,
						is_summary,
						lo, hi);
			}
			else {
				wprintf("%d-%d \n", lo, hi);
			}

		}
		wprintf("<A HREF=\"/%s?startmsg=%ld"
			"&maxmsgs=999999&summary=%d\">"
			"ALL"
			"</A> ",
			oper,
			WC->msgarr[0], is_summary);

		wprintf("<A HREF=\"/%s?startmsg=%ld"
			"&maxmsgs=999999&summary=1\">"
			"Summary"
			"</A>",
			oper,
			WC->msgarr[0]);

		wprintf("</TD></TR></TABLE></CENTER>\n");
	    }
	}
	if (is_summary) wprintf("</FORM>\n");

DONE:
	if (is_tasks) {
		wprintf("<A HREF=\"/display_edit_task?msgnum=0\">"
			"Add new task</A>\n"
		);
	}

	if (is_calendar) {
		do_calendar_view();	/* Render the calendar */
	}

	if (is_addressbook) {
		do_addrbook_view(addrbook, num_ab);	/* Render the address book */
	}

	wDumpContent(1);
	if (addrbook != NULL) free(addrbook);
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

	serv_puts("Content-type: text/html");
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

		/* Netscape sends a simple filename, which is what we want,
		 * but Satan's browser sends an entire pathname.  Reduce
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

	if (!strcasecmp(bstr("sc"), "Cancel")) {
		sprintf(WC->ImportantMessage, 
			"Cancelled.  Message was not posted.");
	} else if (!strcasecmp(bstr("attach"), "Add")) {
		display_enter();
		return;
	} else if (atol(bstr("postseq")) == dont_post) {
		sprintf(WC->ImportantMessage, 
			"Automatically cancelled because you have already "
			"saved this message.");
	} else {
		sprintf(buf, "ENT0 1|%s|0|4|%s",
			bstr("recp"),
			bstr("subject") );
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] == '4') {
			post_mime_to_server();
			sprintf(WC->ImportantMessage, 
				"Message has been posted.\n");
			dont_post = atol(bstr("postseq"));
		} else {
			sprintf(WC->ImportantMessage, 
				"%s", &buf[4]);
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
	long now;
	struct wc_attachment *att;

	if (strlen(bstr("force_room")) > 0) {
		gotoroom(bstr("force_room"), 0);
	}

	/* Are we perhaps in an address book view?  If so, then an "enter
	 * message" command really means "add new entry."
	 */
	if (WC->wc_view == 2) {
		do_edit_vcard(-1, "", "");
		return;
	}

	/* Otherwise proceed normally */
	output_headers(1);
	sprintf(buf, "ENT0 0|%s|0|0", bstr("recp"));
	serv_puts(buf);
	serv_gets(buf);

	if (!strncmp(buf, "570", 3)) {
		if (strlen(bstr("recp")) > 0) {
			svprintf("RECPERROR", WCS_STRING,
				"<SPAN CLASS=\"errormsg\">%s</SPAN><BR>\n",
				&buf[4]
			);
		}
		do_template("prompt_for_recipient");
		goto DONE;
	}
	if (buf[0] != '2') {
		wprintf("<EM>%s</EM><BR>\n", &buf[4]);
		goto DONE;
	}

	now = time(NULL);
	fmt_date(buf, now);
	strcat(&buf[strlen(buf)], " <I>from</I> ");
	stresc(&buf[strlen(buf)], WC->wc_username, 1, 1);
	if (strlen(bstr("recp")) > 0) {
		strcat(&buf[strlen(buf)], " <I>to</I> ");
		stresc(&buf[strlen(buf)], bstr("recp"), 1, 1);
	}
	strcat(&buf[strlen(buf)], " <I>in</I> ");
	stresc(&buf[strlen(buf)], WC->wc_roomname, 1, 1);
	svprintf("BOXTITLE", WCS_STRING, buf);
	do_template("beginbox");

	wprintf("<CENTER>\n");

	wprintf("<FORM ENCTYPE=\"multipart/form-data\" "
		"METHOD=\"POST\" ACTION=\"/post\" "
		"NAME=\"enterform\""
		"onSubmit=\"return submitForm();\""
		">\n");
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"recp\" VALUE=\"%s\">\n",
		bstr("recp"));
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"postseq\" VALUE=\"%ld\">\n",
		now);
	wprintf("<IMG SRC=\"static/enter.gif\" ALIGN=MIDDLE ALT=\" \">");
		/* "onLoad=\"document.enterform.msgtext.focus();\" " */
	wprintf("<FONT SIZE=-1>Subject (optional):</FONT>"
		"<INPUT TYPE=\"text\" NAME=\"subject\" VALUE=\"");
	escputs(bstr("subject"));
	wprintf("\" MAXLENGTH=70>"
		"&nbsp;"
	);

	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Save message\">"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\"><BR>\n");

	wprintf("<SCRIPT language=\"JavaScript\" type=\"text/javascript\" "
		"src=\"static/richtext_compressed.js\"></SCRIPT>\n"
		"<SCRIPT language=\"JavaScript\" type=\"text/javascript\">\n"
		"function submitForm() { \n"
		"  updateRTE('msgtext'); \n"
		"  return true; \n"
		"} \n"
		"  \n"
		"initRTE(\"static/\", \"static/\", \"\"); \n"
		"</script> \n"
		"<noscript>JAVASCRIPT MUST BE ENABLED.</noscript> \n"
		"<SCRIPT language=\"javascript\" type=\"text/javascript\"> \n"
		"writeRichText('msgtext', '");
	msgescputs(bstr("msgtext"));
	wprintf("', '100%%', 200, true, false); \n"
		"</script> \n");

/*
 * Before we had the richedit widget, we did it this way...
 *
	wprintf("<TEXTAREA NAME=\"msgtext\" wrap=soft ROWS=25 COLS=80 "
		"WIDTH=80>");
	escputs(bstr("msgtext"));
	wprintf("</TEXTAREA><BR>\n");
 */

	/* Enumerate any attachments which are already in place... */
	for (att = WC->first_attachment; att != NULL; att = att->next) {
		wprintf("<IMG SRC=\"/static/attachment.gif\" "
			"BORDER=0 ALIGN=MIDDLE> Attachment: ");
		escputs(att->filename);
		wprintf(" (%s, %d bytes)<BR>\n",
			att->content_type, att->length);
	}

	/* Now offer the ability to attach additional files... */
	wprintf("&nbsp;&nbsp;&nbsp;"
		"Attach file: <input NAME=\"attachfile\" "
		"SIZE=48 TYPE=\"file\">\n&nbsp;&nbsp;"
		"<input type=\"submit\" name=\"attach\" value=\"Add\">\n");

	wprintf("</FORM></CENTER>\n");
	do_template("endbox");
DONE:	wDumpContent(1);
}








void delete_msg(void)
{
	long msgid;
	char buf[SIZ];

	msgid = atol(bstr("msgid"));

	output_headers(1);

	sprintf(buf, "DELE %ld", msgid);
	serv_puts(buf);
	serv_gets(buf);
	wprintf("<EM>%s</EM><BR>\n", &buf[4]);

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

	output_headers(1);

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"#FFFFFF\"");
	wprintf("<B>Confirm move of message</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER>");

	wprintf("Please select the room to which you would like this message moved:<BR>\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/move_msg\">\n");
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"msgid\" VALUE=\"%s\">\n",
		bstr("msgid"));


	wprintf("<SELECT NAME=\"target_room\" SIZE=5>\n");
	serv_puts("LKRA");
	serv_gets(buf);
	if (buf[0] == '1') {
		while (serv_gets(buf), strcmp(buf, "000")) {
			extract(targ, buf, 0);
			wprintf("<OPTION>");
			escputs(targ);
			wprintf("\n");
		}
	}
	wprintf("</SELECT>\n");
	wprintf("<BR>\n");

	wprintf("<INPUT TYPE=\"submit\" NAME=\"yesno\" VALUE=\"Move\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"yesno\" VALUE=\"Cancel\">");
	wprintf("</FORM></CENTER>\n");

	wprintf("</CENTER>\n");
	wDumpContent(1);
}



void move_msg(void)
{
	long msgid;
	char buf[SIZ];

	msgid = atol(bstr("msgid"));

	output_headers(1);

	if (!strcasecmp(bstr("yesno"), "Move")) {
		sprintf(buf, "MOVE %ld|%s", msgid, bstr("target_room"));
		serv_puts(buf);
		serv_gets(buf);
		wprintf("<EM>%s</EM><BR>\n", &buf[4]);
	} else {
		wprintf("<EM>Message not moved.</EM><BR>\n");
	}

	wDumpContent(1);
}



void do_stuff_to_msgs(void) {
	char buf[SIZ];
	char sc[SIZ];

	struct stuff_t {
		struct stuff_t *next;
		long msgnum;
	};

	struct stuff_t *stuff = NULL;
	struct stuff_t *ptr;


	serv_puts("MSGS ALL");
	serv_gets(buf);

	if (buf[0] == '1') while (serv_gets(buf), strcmp(buf, "000")) {
		ptr = malloc(sizeof(struct stuff_t));
		ptr->msgnum = atol(buf);
		ptr->next = stuff;
		stuff = ptr;
	}

	strcpy(sc, bstr("sc"));

	while (stuff != NULL) {

		sprintf(buf, "msg_%ld", stuff->msgnum);
		if (!strcasecmp(bstr(buf), "yes")) {

			if (!strcasecmp(sc, "Delete selected")) {
				serv_printf("DELE %ld", stuff->msgnum);
				serv_gets(buf);
			}

		}

		ptr = stuff->next;
		free(stuff);
		stuff = ptr;
	}

	readloop("readfwd");
}
