/* $Id$ */




#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
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

	output_headers(7);


	wprintf("<SCRIPT LANGUAGE=\"JavaScript\">\n"
		"function ConfirmKill() { \n"
		"return confirm('Do you really want to kill this session?');\n"
		"}\n"
		"</SCRIPT>\n"
	);

	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=007700><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"<B>Users currently on ");
	escputs(serv_info.serv_humannode);
	wprintf("</B></FONT></TD></TR></TABLE>\n");

	wprintf("<FONT SIZE=-2>\n");
	wprintf("<CENTER>\n<TABLE BORDER=1 WIDTH=100%>\n<TR>\n");
	wprintf("<TH>Session ID</TH>\n");
	wprintf("<TH>User Name</TH>\n");
	wprintf("<TH>Room</TH>");
	wprintf("<TH>From host</TH>\n</TR>\n");
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
			wprintf("<TR>\n\t<TD ALIGN=center>%d", wlist->sessionnum);
			if ((WC->is_aide) &&
			    (wlist->sessionnum != serv_info.serv_pid)) {
				wprintf(" <A HREF=\"/terminate_session&which_session=%d&session_owner=", wlist->sessionnum);
				urlescputs(wlist->username);
				wprintf("\" onClick=\"return ConfirmKill();\" "
				">(kill)</A>");
			}
			if (wlist->sessionnum == serv_info.serv_pid) {
				wprintf(" <A HREF=\"/edit_me\" "
					">(edit)</A>");
			}
			/* username */
			wprintf("</TD>\n\t<TD>");
			escputs(wlist->username);
			/* room */
			wprintf("</TD>\n\t<TD>");
			escputs(wlist->roomname);
			wprintf("</TD>\n\t<TD>");
			/* hostname */
			escputs(wlist->hostname);
			wprintf("</TD>\n</TR>");
			wptr = wlist->next;
			free(wlist);
			wlist = wptr;
		}
	}
	wprintf("</TABLE>\n<BR><BR>\n");
	wprintf("<TABLE BORDER=0 BGCOLOR=\"#003399\">\n<TR><TD ALIGN=center VALIGN=center CELLPADING=20>\n");
	wprintf("<B><A HREF=\"javascript:window.close();\">Close window</A></B>\n");
	wprintf("</TD></TR>\n</TABLE></FONT>\n</CENTER>");

	wDumpContent(1);
}


void terminate_session(void)
{
	char buf[256];

	serv_printf("TERM %s", bstr("which_session"));
	serv_gets(buf);
	whobbs();
}


/*
 * Change your session info (fake roomname and hostname)
 */
void edit_me(void)
{
	char buf[256];

	if (!strcasecmp(bstr("sc"), "Change room name")) {
		serv_printf("RCHG %s", bstr("fake_roomname"));
		serv_gets(buf);
		http_redirect("/whobbs");
	} else if (!strcasecmp(bstr("sc"), "Change host name")) {
		serv_printf("HCHG %s", bstr("fake_hostname"));
		serv_gets(buf);
		http_redirect("/whobbs");
	} else if (!strcasecmp(bstr("sc"), "Change user name")) {
		serv_printf("UCHG %s", bstr("fake_username"));
		serv_gets(buf);
		http_redirect("/whobbs");
	} else if (!strcasecmp(bstr("sc"), "Cancel")) {
		http_redirect("/whobbs");
	} else {

		output_headers(3);

		wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=000077><TR><TD>");
		wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"><B>");
		wprintf("Edit your session display");
		wprintf("</B></FONT></TD></TR></TABLE>\n");
		wprintf("This screen allows you to change the way your\n");
		wprintf("session appears in the 'Who is online' listing.\n");
		wprintf("To turn off any 'fake' name you've previously\n");
		wprintf("set, simply click the appropriate 'change' button\n");
		wprintf("without typing anything in the corresponding box.\n");
		wprintf("<BR>\n");

		wprintf("<FORM METHOD=\"POST\" ACTION=\"/edit_me\">\n");

		wprintf("<TABLE border=0 width=100%>\n");

		wprintf("<TR><TD><B>Room name:</B></TD>\n<TD>");
		wprintf("<INPUT TYPE=\"text\" NAME=\"fake_roomname\" MAXLENGTH=\"64\">\n");
		wprintf("</TD>\n<TD ALIGN=center>");
		wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Change room name\">");
		wprintf("</TD>\n</TR>\n");

		wprintf("<TR><TD><B>Host name:</B></TD><TD>");
		wprintf("<INPUT TYPE=\"text\" NAME=\"fake_hostname\" MAXLENGTH=\"64\">\n");
		wprintf("</TD>\n<TD ALIGN=center>");
		wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Change host name\">");
		wprintf("</TD>\n</TR>\n");

		if (WC->is_aide) {
			wprintf("<TR><TD><B>User name:</B></TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"fake_username\" MAXLENGTH=\"64\">\n");
			wprintf("</TD>\n<TD ALIGN=center>");
			wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Change user name\">");
			wprintf("</TD>\n</TR>\n");
		}
		wprintf("<TR><TD>&nbsp;</TD><TD>&nbsp;</TD><TD ALIGN=center>");
		wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
		wprintf("</TD></TR></TABLE>\n");

		wprintf("</FORM></CENTER>\n");
		wDumpContent(1);
	}
}
