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
 *
 * Set 'full' to nonzero to display the full card, otherwise it will only
 * show a summary line.
 */
void display_parsed_vcard(struct vCard *v, int full) {
	int i, j;
	char buf[SIZ];
	char *name;

	if (!full) {
		wprintf("<TD>");
		name = vcard_get_prop(v, "n", 1, 0, 0);
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
	if (v->numprops) for (i=0; i<(v->numprops); ++i) {
		if (!strcasecmp(v->prop[i].name, "n")) {
			wprintf("<TR BGCOLOR=#AAAAAA>"
			"<TD BGCOLOR=#FFFFFF>"
			"<IMG ALIGN=CENTER SRC=\"/static/vcard.gif\"></TD>"
			"<TD><FONT SIZE=+1><B>");
			escputs(v->prop[i].value);
			wprintf("</B></FONT></TD></TR>\n");
		}
		else if (!strcasecmp(v->prop[i].name, "email;internet")) {
			wprintf("<TR><TD>Internet e-mail:</TD>"
				"<TD><A HREF=\"mailto:");
			urlescputs(v->prop[i].value);
			wprintf("\">");
			escputs(v->prop[i].value);
			wprintf("</A></TD></TR>\n");
		}
		else if (!strcasecmp(v->prop[i].name, "adr")) {
			wprintf("<TR><TD>Address:</TD><TD>");
			for (j=0; j<num_tokens(v->prop[i].value, ';'); ++j) {
				extract_token(buf, v->prop[i].value, j, ';');
				if (strlen(buf) > 0) {
					escputs(buf);
					wprintf("<BR>");
				}
			}
			wprintf("</TD></TR>\n");
		}
		else if (!strncasecmp(v->prop[i].name, "tel;", 4)) {
			wprintf("<TR><TD>%s telephone:</TD><TD>",
				&v->prop[i].name[4]);
			for (j=0; j<num_tokens(v->prop[i].value, ';'); ++j) {
				extract_token(buf, v->prop[i].value, j, ';');
				if (strlen(buf) > 0) {
					escputs(buf);
					wprintf("<BR>");
				}
			}
			wprintf("</TD></TR>\n");
		}
		else {
			wprintf("<TR><TD>");
			escputs(v->prop[i].name);
			wprintf("</TD><TD>");
			escputs(v->prop[i].value);
			wprintf("</TD></TR>\n");
		}
	}
	wprintf("</TABLE>\n");
}



/*
 * Display a textual vCard
 * (Converts to a vCard object and then calls the actual display function)
 * Set 'full' to nonzero to display the whole card instead of a one-liner
 */
void display_vcard(char *vcard_source, char alpha, int full) {
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

	if ( (alpha == 0)
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
	char *mime_http = NULL;
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

	serv_printf("MSG4 %ld", msgnum);
	serv_gets(buf);
	if (buf[0] != '1') {
		wprintf("<STRONG>ERROR:</STRONG> %s<BR>\n", &buf[4]);
		return;
	}

	wprintf("<TABLE WIDTH=100%% BORDER=0 CELLSPACING=0 "
		"CELLPADDING=1 BGCOLOR=CCCCCC><TR><TD>\n");

	wprintf("<FONT ");
	wprintf("SIZE=+1 ");
	wprintf("COLOR=\"000000\"> ");
	strcpy(m_subject, "");

	while (serv_gets(buf), strcasecmp(buf, "text")) {
		if (!strcmp(buf, "000")) {
			wprintf("<I>unexpected end of message</I><BR><BR>\n");
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
				if (mime_http == NULL) {
					mime_http = malloc(512);
					strcpy(mime_http, "");
				}
				else {
					mime_http = realloc(mime_http,
						strlen(mime_http) + 512);
				}
				sprintf(&mime_http[strlen(mime_http)],
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
				if (mime_http == NULL) {
					mime_http = malloc(512);
					strcpy(mime_http, "");
				}
				else {
					mime_http = realloc(mime_http,
						strlen(mime_http) + 512);
				}
				sprintf(&mime_http[strlen(mime_http)],
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
		if (strlen(node) > 0) {
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

	wprintf("</FONT></TD>");

	wprintf("<TD ALIGN=RIGHT>\n"
		"<TABLE BORDER=0><TR>\n");

	wprintf("<TD BGCOLOR=\"AAAADD\">"
		"<A HREF=\"/readfwd?startmsg=%ld", msgnum);
	wprintf("&maxmsgs=1&summary=0\">Read</A>"
		"</TD>\n", msgnum);

	wprintf("<TD BGCOLOR=\"AAAADD\">"
		"<A HREF=\"/display_enter?recp=");
	urlescputs(reply_to);
	wprintf("\"><FONT SIZE=-1>Reply</FONT></A>"
		"</TD>\n", msgnum);

	if (WC->is_room_aide) {
		wprintf("<TD BGCOLOR=\"AAAADD\">"
			"<A HREF=\"/confirm_move_msg"
			"&msgid=%ld"
			"\"><FONT SIZE=-1>Move</FONT></A>"
			"</TD>\n", msgnum);

		wprintf("<TD BGCOLOR=\"AAAADD\">"
			"<A HREF=\"/delete_msg"
			"&msgid=%ld\""
			"onClick=\"return confirm('Delete this message?');\""
			"><FONT SIZE=-1>Del</FONT></A>"
			"</TD>\n", msgnum);
	}

	wprintf("</TR></TABLE>\n"
		"</TD>\n");

	if (strlen(m_subject) > 0) {
		wprintf("<TR><TD><FONT COLOR=\"0000FF\">"
			"Subject: %s</FONT>"
			"</TD><TD>&nbsp;</TD></TR>\n", m_subject);
	}

	wprintf("</TR></TABLE>\n");

	/* 
	 * Learn the content type
	 */
	strcpy(mime_content_type, "text/plain");
	while (serv_gets(buf), (strlen(buf) > 0)) {
		if (!strcmp(buf, "000")) {
			wprintf("<I>unexpected end of message</I><BR><BR>\n");
			return;
		}
		if (!strncasecmp(buf, "Content-type: ", 14)) {
			safestrncpy(mime_content_type, &buf[14],
				sizeof(mime_content_type));
		}
	}

	/* Messages in legacy Citadel variformat get handled thusly... */
	if (!strcasecmp(mime_content_type, "text/x-citadel-variformat")) {
		fmout(NULL);
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
				wprintf("<FONT COLOR=\"000044\"><I>");
				bq = 1;
			} else if ((bq == 1) &&
			   	(strncmp(buf, ">", 1)) && (strncmp(buf, " >", 2)) && (strncmp(buf, " :-)", 4))) {
				wprintf("</FONT></I>");
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
	if (mime_http != NULL) {
		wprintf("%s", mime_http);
		free(mime_http);
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
			display_vcard(part_source, 0, 1);
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

	sprintf(buf, "MSG0 %ld|1", msgnum);	/* ask for headers only */
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
			display_vcard(vcard_source, alpha, 0);

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



/* 
 * load message pointers from the server
 */
int load_msg_ptrs(char *servcmd)
{
	char buf[SIZ];
	int nummsgs;

	nummsgs = 0;
	serv_puts(servcmd);
	serv_gets(buf);
	if (buf[0] != '1') {
		wprintf("<EM>%s</EM><BR>\n", &buf[4]);
		return (nummsgs);
	}
	while (serv_gets(buf), strcmp(buf, "000")) {
		WC->msgarr[nummsgs] = atol(buf);
		/* FIXME check for overflow */
		++nummsgs;
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

	startmsg = atol(bstr("startmsg"));
	maxmsgs = atoi(bstr("maxmsgs"));
	is_summary = atoi(bstr("summary"));
	if (maxmsgs == 0) maxmsgs = 20;

	output_headers(1);

	if (!strcmp(oper, "readnew")) {
		strcpy(cmd, "MSGS NEW");
	} else if (!strcmp(oper, "readold")) {
		strcpy(cmd, "MSGS OLD");
	} else {
		strcpy(cmd, "MSGS ALL");
	}

	/* FIXME put in the correct constant #defs */
	if ((WC->wc_view == 1) && (maxmsgs > 1)) {
		is_summary = 1;
		strcpy(cmd, "MSGS ALL");
		maxmsgs = 32767;
	}
	if ((WC->wc_view == 2) && (maxmsgs > 1)) {
		is_addressbook = 1;
		strcpy(cmd, "MSGS ALL");
		maxmsgs = 32767;
		if (bstr("alpha") == NULL) {
			alpha = 'A';
		}
		else {
			strcpy(buf, bstr("alpha"));
			alpha = buf[0];
		}

		for (i='A'; i<='Z'; ++i) {
			if (i == alpha) wprintf("<FONT SIZE=+2>"
						"%c</FONT>\n", i);
			else {
				wprintf("<A HREF=\"/readfwd?alpha=%c\">"
					"%c</A>\n", i, i);
			}
			wprintf("&nbsp;");
		}
		if (!isalpha(alpha)) wprintf("<FONT SIZE=+2>(other)</FONT>\n");
		else wprintf("<A HREF=\"/readfwd?alpha=1\">(other)</A>\n");
		wprintf("<HR width=100%%>\n");
	}
	if (WC->wc_view == 3) {		/* calendar */
		is_calendar = 1;
		strcpy(cmd, "MSGS ALL");
		maxmsgs = 32767;
	}
	if (WC->wc_view == 4) {		/* tasks */
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

	if (is_addressbook) {
		wprintf("<TABLE border=0 cellspacing=0 "
			"cellpadding=0 width=100%%>\n"
		);
	}

	for (a = 0; a < nummsgs; ++a) {
		if ((WC->msgarr[a] >= startmsg) && (num_displayed < maxmsgs)) {

			/* Learn which msgs "Prev" & "Next" buttons go to */
			pn_current = WC->msgarr[a];
			if (a > 0) pn_previous = WC->msgarr[a-1];
			if (a < (nummsgs-1)) pn_next = WC->msgarr[a+1];

			/* If a tabular view, set up the line */
			if ( (is_summary) || (is_addressbook) ) {
				bg = 1 - bg;
				wprintf("<TR BGCOLOR=%s>",
					(bg ? "DDDDDD" : "FFFFFF")
				);
			}

			/* Display the message */
			if (is_summary) {
				summarize_message(WC->msgarr[a]);
			}
			else if (is_addressbook) {
				display_addressbook(WC->msgarr[a], alpha);
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
			if ( (is_summary) || (is_addressbook) ) {
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

	if (is_addressbook) {
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
	   if ((!is_tasks) && (!is_calendar)) {

		wprintf("<CENTER>"
			"<TABLE BORDER=0 WIDTH=100%% BGCOLOR=DDDDDD><TR><TD>"
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
	   if ((!is_tasks) && (!is_calendar)) {
		wprintf("<CENTER>"
			"<TABLE BORDER=0 WIDTH=100%% BGCOLOR=DDDDDD><TR><TD>"
			"Reading #%d-%d of %d messages.</TD>\n"
			"<TD ALIGN=RIGHT><FONT SIZE=+1>",
			lowest_displayed, highest_displayed, nummsgs);

		if (is_summary) {
			wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" "
				"VALUE=\"Delete selected\">\n");
		}


		for (b=0; b<nummsgs; b = b + maxmsgs) {
		lo = b+1;
		hi = b+maxmsgs+1;
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

	wDumpContent(1);
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

	output_headers(1);

	wprintf("<FONT FACE=\"Arial,Helvetica,sans-serif\">");
	strcpy(buf, bstr("sc"));
	if (strcasecmp(buf, "Save message")) {
		wprintf("Cancelled.  Message was not posted.<BR>\n");
	} else if (atol(bstr("postseq")) == dont_post) {
		wprintf("Automatically cancelled because you have already "
			"saved this message.<BR>\n");
	} else {
		sprintf(buf, "ENT0 1|%s|0|4|%s",
			bstr("recp"),
			bstr("subject") );
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] == '4') {
			serv_puts("Content-type: text/html");
			serv_puts("");
			text_to_server(bstr("msgtext"), 1);
			serv_puts("000");
			wprintf("Message has been posted.<BR>\n");
			dont_post = atol(bstr("postseq"));
		} else {
			wprintf("<EM>%s</EM><BR>\n", &buf[4]);
		}
	}

	wDumpContent(1);
}




/*
 * display the message entry screen
 */
void display_enter(void)
{
	char buf[SIZ];
	long now;
	struct tm *tm;

	output_headers(1);

	wprintf("<FONT FACE=\"Arial,Helvetica,sans-serif\">");

	sprintf(buf, "ENT0 0|%s|0|0", bstr("recp"));
	serv_puts(buf);
	serv_gets(buf);

	if (!strncmp(buf, "570", 3)) {
		if (strlen(bstr("recp")) > 0) {
			wprintf("<EM>%s</EM><BR>\n", &buf[4]);
		}
		do_template("prompt_for_recipient");
		goto DONE;
	}
	if (buf[0] != '2') {
		wprintf("<EM>%s</EM><BR>\n", &buf[4]);
		goto DONE;
	}

	now = time(NULL);
	tm = (struct tm *) localtime(&now);
	strcpy(buf, (char *) asctime(tm));
	buf[strlen(buf) - 1] = 0;
	strcpy(&buf[16], &buf[19]);
	wprintf("</CENTER><FONT COLOR=\"440000\">\n"
		"<IMG SRC=\"static/enter.gif\" ALIGN=MIDDLE ALT=\" \" "
		"onLoad=\"document.enterform.msgtext.focus();\" >");
	wprintf("<B> %s ", &buf[4]);
	wprintf("from %s ", WC->wc_username);
	if (strlen(bstr("recp")) > 0)
		wprintf("to %s ", bstr("recp"));
	wprintf("in %s&gt; ", WC->wc_roomname);
	wprintf("</B></FONT><BR><CENTER>\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/post\" "
		"NAME=\"enterform\">\n");
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"recp\" VALUE=\"%s\">\n",
		bstr("recp"));
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"postseq\" VALUE=\"%ld\">\n",
		now);
	wprintf("<FONT SIZE=-1>Subject (optional):</FONT>"
		"<INPUT TYPE=\"text\" NAME=\"subject\" MAXLENGTH=70>"
		"&nbsp;&nbsp;&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Save message\">"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\"><BR>\n");

	wprintf("<TEXTAREA NAME=\"msgtext\" wrap=soft ROWS=30 COLS=80 "
		"WIDTH=80></TEXTAREA><P>\n");

	wprintf("</FORM></CENTER>\n");
DONE:	wDumpContent(1);
	wprintf("</FONT>");
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

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=770000><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
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
