
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
	char urlbuf[256];
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


void read_message(long msgnum, int is_summary) {
	char buf[256];
	char m_subject[256];
	char from[256];
	char node[256];
	char rfca[256];
	char reply_to[512];
	char now[256];
	int format_type = 0;
	int nhdr = 0;
	int bq = 0;

	strcpy(from, "");
	strcpy(node, "");
	strcpy(rfca, "");
	strcpy(reply_to, "");

	sprintf(buf, "MSG0 %ld", msgnum);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '1') {
		wprintf("<STRONG>ERROR:</STRONG> %s<BR>\n", &buf[4]);
		return;
	}
	wprintf("<TABLE WIDTH=100% BORDER=0 CELLSPACING=0 CELLPADDING=1 BGCOLOR=000077><TR><TD>\n");
	wprintf("<FONT ");
	if (!is_summary) wprintf("SIZE=+1 ");
	wprintf("COLOR=\"FFFF00\"> ");
	strcpy(m_subject, "");

	while (serv_gets(buf), strncasecmp(buf, "text", 4)) {
		if (!strncasecmp(buf, "nhdr=yes", 8))
			nhdr = 1;
		if (nhdr == 1)
			buf[0] = '_';
		if (!strncasecmp(buf, "type=", 5))
			format_type = atoi(&buf[5]);
		if (!strncasecmp(buf, "from=", 5)) {
			strcpy(from, &buf[5]);
			wprintf("from ");
			escputs(from);
			wprintf(" ");
		}
		if (!strncasecmp(buf, "subj=", 5))
			strcpy(m_subject, &buf[5]);
		if ((!strncasecmp(buf, "hnod=", 5))
		    && (strcasecmp(&buf[5], serv_info.serv_humannode)))
			wprintf("(%s) ", &buf[5]);
		if ((!strncasecmp(buf, "room=", 5))
		    && (strcasecmp(&buf[5], WC->wc_roomname)))
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

	if (nhdr == 1)
		wprintf("****");
	wprintf("</FONT></TD>");

	/* begin right-hand toolbar */
	wprintf("<TD ALIGN=RIGHT>\n"
		"<TABLE BORDER=0><TR>\n");

	if (is_summary) {
		wprintf("<TD BGCOLOR=\"AAAADD\">"
			"<A HREF=\"/readfwd?startmsg=%ld", msgnum);
		wprintf("&maxmsgs=1&summary=0\">Read</A>"
			"</TD>\n", msgnum);
	}

	wprintf("<TD BGCOLOR=\"AAAADD\">"
		"<A HREF=\"/display_enter?recp=");
	urlescputs(reply_to);
	wprintf("\">Reply</A>"
		"</TD>\n", msgnum);

	if (WC->is_room_aide) {
		wprintf("<TD BGCOLOR=\"AAAADD\">"
			"<A HREF=\"/confirm_move_msg"
			"&msgid=%ld"
			"\">Move</A>"
			"</TD>\n", msgnum);

		wprintf("<TD BGCOLOR=\"AAAADD\">"
			"<A HREF=\"/confirm_delete_msg"
			"&msgid=%ld"
			"\">Del</A>"
			"</TD>\n", msgnum);

	}

	wprintf("</TR></TABLE>\n"
		"</TD>\n");

	/* end right-hand toolbar */


	if (strlen(m_subject) > 0) {
		wprintf("<TR><TD><FONT COLOR=\"FFFFFF\">"
			"Subject: %s</FONT>"
			"</TD><TD>&nbsp;</TD></TR>\n", m_subject);
	}

	wprintf("</TR></TABLE>\n");

	if (is_summary) {
		while (serv_gets(buf), strcmp(buf, "000")) ;
		return;
	}

	if (format_type == 0) {
		fmout(NULL);
	} else {
		while (serv_gets(buf), strcmp(buf, "000")) {
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
	}
	wprintf("</I><BR>");
}



/* 
 * load message pointers from the server
 */
int load_msg_ptrs(servcmd)
char *servcmd;
{
	char buf[256];
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
		++nummsgs;
	}
	return (nummsgs);
}


/*
 * command loop for reading messages
 */
void readloop(char *oper)
{
	char cmd[256];
	int a;
	int nummsgs;
	long startmsg;
	int maxmsgs;
	int num_displayed = 0;
	int is_summary = 0;
	int remaining_messages;

	startmsg = atol(bstr("startmsg"));
	maxmsgs = atoi(bstr("maxmsgs"));
	is_summary = atoi(bstr("summary"));
	if (maxmsgs == 0) maxmsgs = 20;

	output_headers(1);

	/* wprintf("<CENTER><B>%s - ",
		WC->wc_roomname); */
	if (!strcmp(oper, "readnew")) {
		strcpy(cmd, "MSGS NEW");
		/* wprintf("new messages"); */
	} else if (!strcmp(oper, "readold")) {
		strcpy(cmd, "MSGS OLD");
		/* wprintf("old messages"); */
	} else {
		strcpy(cmd, "MSGS ALL");
		/* wprintf("all messages"); */
	}
	/* wprintf("</B></CENTER><BR>\n"); */

	nummsgs = load_msg_ptrs(cmd);
	if (nummsgs == 0) {
		if (!strcmp(oper, "readnew")) {
			wprintf("<EM>No new messages in this room.</EM>\n");
		} else if (!strcmp(oper, "readold")) {
			wprintf("<EM>No old messages in this room.</EM>\n");
		} else {
			wprintf("<EM>This room is empty.</EM>\n");
		}
		goto DONE;
	}

	remaining_messages = 0;
	for (a = 0; a < nummsgs; ++a) {
		if (WC->msgarr[a] >= startmsg) {
			++remaining_messages;
		}
	}



	for (a = 0; a < nummsgs; ++a) {
		if (WC->msgarr[a] >= startmsg) {

			read_message(WC->msgarr[a], is_summary);
			if (is_summary) wprintf("<BR>");

			++num_displayed;
			--remaining_messages;

			if ( (num_displayed >= maxmsgs) && (a < nummsgs) ) {
				wprintf("<CENTER><FONT SIZE=+1>"
					"There are %d more messages here."
					"&nbsp;&nbsp;&nbsp;</FONT>",
					remaining_messages);
				wprintf("<A HREF=\"/readfwd?startmsg=%ld"
					"&maxmsgs=999999&summary=%d\">"
					"Read them ALL"
					"</A>&nbsp;&nbsp;&nbsp;",
					WC->msgarr[a+1], is_summary);
				wprintf("<A HREF=\"/readfwd?startmsg=%ld"
					"&maxmsgs=%d&summary=%d\">"
					"Read next %d"
					"</A>",
					WC->msgarr[a+1], maxmsgs,
					is_summary, maxmsgs);
				wprintf("</CENTER><HR>\n");
				goto DONE;
			}
		}
	}

DONE:	wDumpContent(1);
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
	char buf[256];
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
		sprintf(buf, "ENT0 1|%s|0|0", bstr("recp"));
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] == '4') {
			text_to_server(bstr("msgtext"));
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
 * prompt for a recipient (to be called from display_enter() only)
 */
void prompt_for_recipient()
{

	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=007700><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Send private e-mail</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER>");
	wprintf("<FORM METHOD=\"POST\" ACTION=\"/display_enter\">\n");
	wprintf("Enter recipient: ");
	wprintf("<INPUT TYPE=\"text\" NAME=\"recp\" MAXLENGTH=\"64\"><BR>\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Enter message\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
	wprintf("</FORM></CENTER>\n");
}



/*
 * display the message entry screen
 */
void display_enter(void)
{
	char buf[256];
	long now;
	struct tm *tm;

	output_headers(1);

	wprintf("<FACE=\"Arial,Helvetica,sans-serif\">");

	sprintf(buf, "ENT0 0|%s|0|0", bstr("recp"));
	serv_puts(buf);
	serv_gets(buf);

	if (!strncmp(buf, "570", 3)) {
		if (strlen(bstr("recp")) > 0) {
			wprintf("<EM>%s</EM><BR>\n", &buf[4]);
		}
		prompt_for_recipient();
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
	wprintf("</CENTER><FONT COLOR=\"440000\"><B> %s ", &buf[4]);
	wprintf("from %s ", WC->wc_username);
	if (strlen(bstr("recp")) > 0)
		wprintf("to %s ", bstr("recp"));
	wprintf("in %s&gt; ", WC->wc_roomname);
	wprintf("</B></FONT><BR><CENTER>\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/post\">\n");
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"recp\" VALUE=\"%s\">\n",
		bstr("recp"));
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"postseq\" VALUE=\"%ld\">\n",
		now);
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Save message\">"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\"><BR>\n");

	wprintf("<TEXTAREA NAME=\"msgtext\" wrap=soft ROWS=30 COLS=80 "
		"WIDTH=80></TEXTAREA><P>\n");

	wprintf("</FORM></CENTER>\n");
DONE:	wDumpContent(1);
	wprintf("</FONT>");
}







/*
 * Confirm deletion of a message
 */
void confirm_delete_msg(void)
{
	long msgid;

	msgid = atol(bstr("msgid"));

	output_headers(1);

	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=770000><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Confirm deletion of message</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER>");

	wprintf("Are you sure you want to delete this message? <BR>\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/delete_msg\">\n");
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"msgid\" VALUE=\"%s\">\n",
		bstr("msgid"));
	wprintf("<INPUT TYPE=\"submit\" NAME=\"yesno\" VALUE=\"Yes\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"yesno\" VALUE=\"No\">");
	wprintf("</FORM></CENTER>\n");

	wprintf("</CENTER>\n");
	wDumpContent(1);
}



void delete_msg(void)
{
	long msgid;
	char buf[256];

	msgid = atol(bstr("msgid"));

	output_headers(1);

	if (!strcasecmp(bstr("yesno"), "Yes")) {
		sprintf(buf, "DELE %ld", msgid);
		serv_puts(buf);
		serv_gets(buf);
		wprintf("<EM>%s</EM><BR>\n", &buf[4]);
	} else {
		wprintf("<EM>Message not deleted.</EM><BR>\n");
	}

	wDumpContent(1);
}




/*
 * Confirm move of a message
 */
void confirm_move_msg(void)
{
	long msgid;
	char buf[256];
	char targ[256];

	msgid = atol(bstr("msgid"));

	output_headers(1);

	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=770000><TR><TD>");
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
	char buf[256];

	msgid = atol(bstr("msgid"));

	output_headers(1);

	if (!strcasecmp(bstr("yesno"), "Move")) {
		sprintf(buf, "MOVE %ld|%s", msgid, bstr("target_room"));
		serv_puts(buf);
		serv_gets(buf);
		wprintf("<EM>%s</EM><BR>\n", &buf[4]);
	} else {
		wprintf("<EM>Message not deleted.</EM><BR>\n");
	}

	wDumpContent(1);
}



