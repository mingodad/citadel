/* $Id$ */

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include "webcit.h"
#include "child.h"

struct whouser {
	struct whouser *next;
	int sessionnum;
	char username[256];
	char roomname[256];
	char hostname[256];
	char clientsoftware[256];
};

/*
 * who is on?
 */
void whobbs(void)
{
	struct whouser *wlist = NULL;
	struct whouser *wptr = NULL;
	char buf[256], sess, user[256], room[256], host[256];
	int foundit;

	printf("HTTP/1.0 200 OK\n");
	output_headers(1);

	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=007700><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"<B>Users currently on ");
	escputs(serv_info.serv_humannode);
	wprintf("</B></FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER>\n<TABLE BORDER=1>\n<TR>\n");
	wprintf("<TH><FONT FACE=\"Arial,Helvetica,sans-serif\">Session ID</FONT></TH>\n");
	wprintf("<TH><FONT FACE=\"Arial,Helvetica,sans-serif\">User Name</FONT></TH>\n");
	wprintf("<TH><FONT FACE=\"Arial,Helvetica,sans-serif\">Room</FONT></TH>");
	wprintf("<TH><FONT FACE=\"Arial,Helvetica,sans-serif\">From host</FONT></TH>\n</TR>\n");
	serv_puts("RWHO");
	serv_gets(buf);
	if (buf[0] == '1') {
		while (serv_gets(buf), strcmp(buf, "000")) {
			sess = extract_int(buf, 0);
			extract(user, buf, 1);
			extract(room, buf, 2);
			extract(host, buf, 3);

			foundit = 0;
			for (wptr = wlist; wptr != NULL; wptr = wptr->next) {
				if (wptr->sessionnum == sess) {
					foundit = 1;
					if (strcasecmp(user, wptr->username)) {
						sprintf(buf, "%cBR%c%s",
							LB, RB, user);
						strcat(wptr->username, buf);
					}
					if (strcasecmp(room, wptr->roomname)) {
						sprintf(buf, "%cBR%c%s",
							LB, RB, room);
						strcat(wptr->roomname, buf);
					}
					if (strcasecmp(host, wptr->hostname)) {
						sprintf(buf, "%cBR%c%s",
							LB, RB, host);
						strcat(wptr->hostname, buf);
					}
				}
			}

			if (foundit == 0) {
				wptr = (struct whouser *)
				    malloc(sizeof(struct whouser));
				wptr->next = wlist;
				wlist = wptr;
				strcpy(wlist->username, user);
				strcpy(wlist->roomname, room);
				strcpy(wlist->hostname, host);
				wlist->sessionnum = sess;
			}
		}

		while (wlist != NULL) {
			wprintf("<TR>\n\t<TD ALIGN=center><FONT FACE=\"Arial,Helvetica,sans-serif\">%d", wlist->sessionnum);
			if ((is_aide) &&
			    (wlist->sessionnum != serv_info.serv_pid)) {
				wprintf(" <A HREF=\"/terminate_session&which_session=%d&session_owner=", wlist->sessionnum);
				urlescputs(wlist->username);
				wprintf("\">(kill)</A>");
			}
			if (wlist->sessionnum == serv_info.serv_pid) {
				wprintf(" <A HREF=\"/edit_me\">(edit)</A>");
			}
			/* username */
			wprintf("</FONT></TD>\n\t<TD><FONT FACE=\"Arial,Helvetica,sans-serif\"><A HREF=\"/showuser?who=");
			urlescputs(wlist->username);
			wprintf("\" onMouseOver=\"window.status='View profile for ");
			escputs(wlist->username);
			wprintf("'; return true\">");
			escputs(wlist->username);
			wprintf("</A>");
			/* room */
			wprintf("</FONT></TD>\n\t<TD><FONT FACE=\"Arial,Helvetica,sans-serif\">");
			/* handle chat */
			if (strstr(wlist->roomname, "chat") != NULL) {
				wprintf("<A HREF=\"/chat\" onMouseOver=\"window.status='Chat'; return true\">&lt;chat&gt;</A>");
			} else {
				wprintf("<A HREF=\"/dotgoto&room=");
				urlescputs(wlist->roomname);
				wprintf("\" onMouseOver=\"window.status='Go to room ");
				escputs(wlist->roomname);
				wprintf("'; return true\">");
				escputs(wlist->roomname);
				wprintf("</A>");
			}
			wprintf("</FONT></TD><TD><FONT FACE=\"Arial,Helvetica,sans-serif\">");
			/* hostname */
			escputs(wlist->hostname);
			wprintf("</FONT></TD>\n</TR>");
			wptr = wlist->next;
			free(wlist);
			wlist = wptr;
		}
	}
	wprintf("</TABLE>\n<BR><BR>\n");
	wprintf("<TABLE BORDER=0 BGCOLOR=\"#003399\">\n<TR><TD ALIGN=center VALIGN=center CELLPADING=20>\n");
	wprintf("<FONT FACE=\"Arial,Helvetica,sans-serif\" SIZE=+2><B><A HREF=\"/whobbs\"><FONT COLOR=\"#FF0000\">Refresh</FONT></A></B></FONT>\n");
	wprintf("</TD></TR>\n</TABLE>\n</CENTER>");
	wDumpContent(1);
}


void terminate_session(void)
{
	char buf[256];

	if (!strcasecmp(bstr("confirm"), "Yes")) {
		serv_printf("TERM %s", bstr("which_session"));
		serv_gets(buf);
		if (buf[0] == '2') {
			whobbs();
		} else {
			display_error(&buf[4]);
		}
	} else {
		printf("HTTP/1.0 200 OK\n");
		output_headers(1);
		wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=770000><TR><TD>");
		wprintf("<FONT FACE=\"Arial,Helvetica,sans-serif\" SIZE=+1 COLOR=\"FFFFFF\"<B>Confirm session termination");
		wprintf("</B></FONT></TD></TR></TABLE>\n");

		wprintf("<FONT FACE=\"Arial,Helvetica,sans-serif\">Are you sure you want to terminate session %s",
			bstr("which_session"));
		if (strlen(bstr("session_owner")) > 0) {
			wprintf(" (");
			escputs(bstr("session_owner"));
			wprintf(")");
		}
		wprintf("?<BR><BR>\n");

		wprintf("<A HREF=\"/terminate_session&which_session=%s&confirm=yes\">",
			bstr("which_session"));
		wprintf("Yes</A>&nbsp;&nbsp;&nbsp;");
		wprintf("<A HREF=\"/whobbs\">No</A></FONT>");
		wDumpContent(1);
	}

}



/*
 * Change your session info (fake roomname and hostname)
 */
void edit_me(void)
{
	char buf[256];

	printf("HTTP/1.0 200 OK\n");
	output_headers(1);

	if (!strcasecmp(bstr("sc"), "Change room name")) {
		serv_printf("RCHG %s", bstr("fake_roomname"));
		serv_gets(buf);
		whobbs();
	} else if (!strcasecmp(bstr("sc"), "Change host name")) {
		serv_printf("HCHG %s", bstr("fake_hostname"));
		serv_gets(buf);
		whobbs();
	} else if (!strcasecmp(bstr("sc"), "Change user name")) {
		serv_printf("UCHG %s", bstr("fake_username"));
		serv_gets(buf);
		whobbs();
	} else if (!strcasecmp(bstr("sc"), "Cancel")) {
		whobbs();
	} else {

		wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=000077><TR><TD>");
		wprintf("<FONT FACE=\"Arial,Helvetica,sans-serif\" SIZE=+1 COLOR=\"FFFFFF\"><B>");
		wprintf("Edit your session display");
		wprintf("</B></FONT></TD></TR></TABLE>\n");
		wprintf("<FONT FACE=\"Arial,Helvetica,sans-serif\">");
		wprintf("This screen allows you to change the way your\n");
		wprintf("session appears in the 'Who is online' listing.\n");
		wprintf("To turn off any 'fake' name you've previously\n");
		wprintf("set, simply click the appropriate 'change' button\n");
		wprintf("without typing anything in the corresponding box.\n");
		wprintf("<BR>\n</FONT>\n");

		wprintf("<FORM METHOD=\"POST\" ACTION=\"/edit_me\">\n");

		wprintf("<TABLE border=0 width=100%>\n");

		wprintf("<TR><TD><FONT FACE=\"Arial,Helvetica,sans-serif\"><B>Room name:</B></FONT></TD>\n<TD>");
		wprintf("<FONT FACE=\"Arial,Helvetica,sans-serif\"><INPUT TYPE=\"text\" NAME=\"fake_roomname\" MAXLENGTH=\"64\">\n");
		wprintf("</FONT></TD>\n<TD ALIGN=center>");
		wprintf("<FONT FACE=\"Arial,Helvetica,sans-serif\"><INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Change room name\">");
		wprintf("</FONT></TD>\n</TR>\n");

		wprintf("<TR><TD><FONT FACE=\"Arial,Helvetica,sans-serif\"><B>Host name:</B></FONT></TD><TD>");
		wprintf("<FONT FACE=\"Arial,Helvetica,sans-serif\"><INPUT TYPE=\"text\" NAME=\"fake_hostname\" MAXLENGTH=\"64\">\n");
		wprintf("</FONT></TD>\n<TD ALIGN=center>");
		wprintf("<FONT FACE=\"Arial,Helvetica,sans-serif\"><INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Change host name\">");
		wprintf("</FONT></TD>\n</TR>\n");

		if (is_aide) {
			wprintf("<TR><TD><FONT FACE=\"Arial,Helvetica,sans-serif\"><B>User name:</B></FONT></TD><TD>");
			wprintf("<FONT FACE=\"Arial,Helvetica,sans-serif\"><INPUT TYPE=\"text\" NAME=\"fake_username\" MAXLENGTH=\"64\">\n");
			wprintf("</FONT></TD>\n<TD ALIGN=center>");
			wprintf("<FONT FACE=\"Arial,Helvetica,sans-serif\"><INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Change user name\">");
			wprintf("</FONT></TD>\n</TR>\n");
		}
		wprintf("<TR><TD>&nbsp;</TD><TD>&nbsp;</TD><TD ALIGN=center>");
		wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
		wprintf("</TD></TR></TABLE>\n");

		wprintf("</FORM></CENTER>\n");
		wDumpContent(1);
	}
}
