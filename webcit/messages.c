#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "webcit.h"
#include "child.h"

char reply_to[512];
long msgarr[1024];

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
	char outbuf[256];

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
	strcpy(buf, outbuf);
}


void read_message(msgnum, oper)
long msgnum;
char *oper;
{
	char buf[256];
	char m_subject[256];
	char from[256];
	long now;
	struct tm *tm;
	int format_type = 0;
	int nhdr = 0;
	int bq = 0;

	sprintf(buf, "MSG0 %ld", msgnum);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '1') {
		wprintf("<STRONG>ERROR:</STRONG> %s<BR>\n", &buf[4]);
		return;
	}
	wprintf("<TABLE WIDTH=100% BORDER=0 CELLSPACING=0 CELLPADDING=0 BGCOLOR=000077><TR><TD>\n");
	wprintf("<FONT FACE=\"Arial,Helvetica,sans-serif\" SIZE=+1 COLOR=\"FFFF00\"> ");
	strcpy(m_subject, "");

	strcpy(reply_to, "nobody...xxxxx");
	while (serv_gets(buf), strncasecmp(buf, "text", 4)) {
		if (!strncasecmp(buf, "nhdr=yes", 8))
			nhdr = 1;
		if (nhdr == 1)
			buf[0] = '_';
		if (!strncasecmp(buf, "type=", 5))
			format_type = atoi(&buf[5]);
		if (!strncasecmp(buf, "from=", 5)) {
			wprintf("from %s ", &buf[5]);
			strcpy(from, &buf[5]);
		}
		if (!strncasecmp(buf, "path=", 5))
			strcpy(reply_to, &buf[5]);
		if (!strncasecmp(buf, "subj=", 5))
			strcpy(m_subject, &buf[5]);
		if ((!strncasecmp(buf, "hnod=", 5))
		    && (strcasecmp(&buf[5], serv_info.serv_humannode)))
			wprintf("(%s) ", &buf[5]);
		if ((!strncasecmp(buf, "room=", 5))
		    && (strcasecmp(&buf[5], wc_roomname)))
			wprintf("in %s> ", &buf[5]);

		if (!strncasecmp(buf, "node=", 5)) {
			if ((room_flags & QR_NETWORK)
			|| ((strcasecmp(&buf[5], serv_info.serv_nodename)
			&& (strcasecmp(&buf[5], serv_info.serv_fqdn))))) {
				wprintf("@%s ", &buf[5]);
			}
			if ((!strcasecmp(&buf[5], serv_info.serv_nodename))
			|| (!strcasecmp(&buf[5], serv_info.serv_fqdn))) {
				strcpy(reply_to, from);
			} else if (haschar(&buf[5], '.') == 0) {
				sprintf(reply_to, "%s @ %s", from, &buf[5]);
			}
		}
		if (!strncasecmp(buf, "rcpt=", 5))
			wprintf("to %s ", &buf[5]);
		if (!strncasecmp(buf, "time=", 5)) {
			now = atol(&buf[5]);
			tm = (struct tm *) localtime(&now);
			strcpy(buf, (char *) asctime(tm));
			buf[strlen(buf) - 1] = 0;
			strcpy(&buf[16], &buf[19]);
			wprintf("%s ", &buf[4]);
		}
	}

	if (nhdr == 1)
		wprintf("****");
	wprintf("</FONT></TD>");

	if (is_room_aide) {
		wprintf("<TD ALIGN=RIGHT NOWRAP><FONT FACE=\"Arial,Helvetica,sans-serif\" COLOR=\"FFFF00\"><B>");

		wprintf("<A HREF=\"/confirm_move_msg");
		wprintf("&msgid=%ld", msgnum);
		wprintf("\">Move</A>");

		wprintf("&nbsp;&nbsp;");

		wprintf("<A HREF=\"/confirm_delete_msg");
		wprintf("&msgid=%ld", msgnum);
		wprintf("\">Del</A>");

		wprintf("</B></FONT></TD>");
	}
	wprintf("</TR></TABLE>\n");

	if (strlen(m_subject) > 0) {
		wprintf("Subject: %s<BR>\n", m_subject);
	}
	if (format_type == 0) {
		fmout(NULL);
	} else {
		while (serv_gets(buf), strcmp(buf, "000")) {
			while ((strlen(buf) > 0) && (isspace(buf[strlen(buf) - 1])))
				buf[strlen(buf) - 1] = 0;
			if ((bq == 0) &&
			    ((!strncmp(buf, ">", 1)) || (!strncmp(buf, " >", 2)) || (!strncmp(buf, " :-)", 4)))) {
				wprintf("<FONT FACE=\"Arial,Helvetica,sans-serif\" COLOR=\"000044\"><I>");
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
		msgarr[nummsgs] = atol(buf);
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

	printf("HTTP/1.0 200 OK\n");
	output_headers(1);

	wprintf("<FONT FACE=\"Arial,Helvetica,sans-serif\"><CENTER><B>%s - ", wc_roomname);
	if (!strcmp(oper, "readnew")) {
		strcpy(cmd, "MSGS NEW");
		wprintf("new messages");
	} else if (!strcmp(oper, "readold")) {
		strcpy(cmd, "MSGS OLD");
		wprintf("old messages");
	} else {
		strcpy(cmd, "MSGS ALL");
		wprintf("all messages");
	}
	wprintf("</B></CENTER><BR>\n");

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
	for (a = 0; a < nummsgs; ++a) {
		read_message(msgarr[a], oper);
	}

      DONE:wDumpContent(1);
}




/*
 * post message (or don't post message)
 */
void post_message(void)
{
	char buf[256];

	printf("HTTP/1.0 200 OK\n");
	output_headers(1);

	wprintf("<FONT FACE=\"Arial,Helvetica,sans-serif\">");
	strcpy(buf, bstr("sc"));
	if (strcasecmp(buf, "Save message")) {
		wprintf("Cancelled.  Message was not posted.<BR>\n");
	} else {
		sprintf(buf, "ENT0 1|%s|0|0", bstr("recp"));
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] == '4') {
			text_to_server(bstr("msgtext"));
			serv_puts("000");
			wprintf("Message has been posted.<BR>\n");
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

	printf("HTTP/1.0 200 OK\n");
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
	wprintf("<CENTER>Enter message below.  Messages are formatted to\n");
	wprintf("the <EM>reader's</EM> screen width.  To defeat the\n");
	wprintf("formatting, indent a line at least one space.  \n");
	wprintf("<BR>");

	time(&now);
	tm = (struct tm *) localtime(&now);
	strcpy(buf, (char *) asctime(tm));
	buf[strlen(buf) - 1] = 0;
	strcpy(&buf[16], &buf[19]);
	wprintf("</CENTER><FONT COLOR=\"440000\"><B> %s ", &buf[4]);
	wprintf("from %s ", wc_username);
	if (strlen(bstr("recp")) > 0)
		wprintf("to %s ", bstr("recp"));
	wprintf("in %s&gt; ", wc_roomname);
	wprintf("</B></FONT><BR><CENTER>\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/post\">\n");
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"recp\" VALUE=\"%s\">\n",
		bstr("recp"));
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Save message\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\"><BR>\n");

	wprintf("<TEXTAREA NAME=\"msgtext\" wrap=soft ROWS=30 COLS=80 WIDTH=80></TEXTAREA><P>\n");

	wprintf("</FORM></CENTER>\n");
      DONE:wDumpContent(1);
	wprintf("</FONT>");
}








/*
 * Confirm deletion of a message
 */
void confirm_delete_msg(void)
{
	long msgid;

	msgid = atol(bstr("msgid"));

	printf("HTTP/1.0 200 OK\n");
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

	printf("HTTP/1.0 200 OK\n");
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

	printf("HTTP/1.0 200 OK\n");
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

	printf("HTTP/1.0 200 OK\n");
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
