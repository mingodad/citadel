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
#include <time.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"


void do_iconbar(void) {
	char iconbar[SIZ];
	char buf[SIZ];
	char key[SIZ], value[SIZ];
	int i;

	/* The initialized values of these variables also happen to
	 * specify the default values for users who haven't customized
	 * their iconbars.  These should probably be set in a master
	 * configuration somewhere.
	 */
	int ib_logo = 1;	/* Site logo */
	int ib_inbox = 0;	/* Inbox icon */
	int ib_calendar = 0;	/* Calendar icon */
	int ib_tasks = 0;	/* Tasks icon */
	int ib_rooms = 1;	/* Rooms icon */
	int ib_users = 1;	/* Users icon */
	int ib_advanced = 1;	/* Advanced Options icon */
	int ib_logoff = 1;	/* Logoff button */
	int ib_citadel = 1;	/* 'Powered by Citadel' logo */
	/*
	 */

	get_preference("iconbar", iconbar);
	for (i=0; i<num_tokens(iconbar, ','); ++i) {
		extract_token(buf, iconbar, i, ',');
		extract_token(key, buf, 0, '=');
		extract_token(value, buf, 1, '=');

		if (!strcasecmp(key, "ib_logo")) ib_logo = atoi(value);
		if (!strcasecmp(key, "ib_inbox")) ib_inbox = atoi(value);
		if (!strcasecmp(key, "ib_calendar")) ib_calendar = atoi(value);
		if (!strcasecmp(key, "ib_tasks")) ib_tasks = atoi(value);
		if (!strcasecmp(key, "ib_rooms")) ib_rooms = atoi(value);
		if (!strcasecmp(key, "ib_users")) ib_users = atoi(value);
		if (!strcasecmp(key, "ib_advanced")) ib_advanced = atoi(value);
		if (!strcasecmp(key, "ib_logoff")) ib_logoff = atoi(value);
		if (!strcasecmp(key, "ib_citadel")) ib_citadel = atoi(value);

	}


	output_headers(11);
	do_template("beginbox_nt");

	wprintf("<center>\n");

	if (ib_logo) wprintf("\"<IMG BORDER=\"0\" WIDTH=\"48\" "
		"HEIGHT=\"48\" SRC=\"/image&name=hello\" ALT=\"&nbsp;\">"
		"<BR>\n"
	);

	if (ib_inbox) wprintf(
		"<SPAN CLASS=\"iconbar_link\">"
		"<A HREF=\"/dotgoto?room=_MAIL_\" "
		"TITLE=\"Go to your e-mail inbox\" "
		"TARGET=\"workspace\">"
		"<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
		"SRC=\"/static/mail.gif\">"
		"<BR>Mail</A></SPAN><BR>\n"
	);

	if (ib_calendar) wprintf(
		"<SPAN CLASS=\"iconbar_link\">"
		"<A HREF=\"/dotgoto?room=Calendar\" "
		"TITLE=\"Go to your personal calendar\" "
		"TARGET=\"workspace\">"
		"<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
		"SRC=\"/static/vcalendar.gif\">"
		"<BR>Calendar</A></SPAN><BR>\n"
	);

	if (ib_tasks) wprintf(
		"<SPAN CLASS=\"iconbar_link\">"
		"<A HREF=\"/dotgoto?room=Tasks\" "
		"TITLE=\"Go to your personal task list\" "
		"TARGET=\"workspace\">"
		"<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
		"SRC=\"/static/vcalendar.gif\">"
		"<BR>Tasks</A></SPAN><BR>\n"
	);

	if (ib_rooms) wprintf(
		"<SPAN CLASS=\"iconbar_link\">"
		"<A HREF=\"/knrooms\" TITLE=\"Shows a list of all "
		"Rooms that you have access to\" TARGET=\"workspace\">"
		"<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
		"SRC=\"/static/rooms-icon.gif\">"
		"<BR>Rooms</A></SPAN><BR>\n"
	);

	if (ib_users) wprintf(
		"<SPAN CLASS=\"iconbar_link\">"
		"<A HREF=\"/whobbs\" TITLE=\"See who is online right now\" "
		"TARGET=\"workspace\">"
		"<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
		"SRC=\"/static/users-icon.gif\">"
		"<BR>Users</A></SPAN><BR>\n"
	);

	if (ib_advanced) wprintf(
		"<SPAN CLASS=\"iconbar_link\">"
		"<A HREF=\"/display_main_menu\" "
		"TITLE=\"Advanced Options Menu: Advanced Room commands, "
		"Account Info, and Chat\" "
		"TARGET=\"workspace\">"
		"<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
		"SRC=\"/static/advanced-icon.gif\">"
		"<BR>Advanced options</A></SPAN><BR>\n"
	);

	if (ib_logoff) wprintf(
		"<SPAN CLASS=\"iconbar_link\">"
		"<A HREF=\"/termquit\" TITLE=\"Log off\" TARGET=\"_top\" "
		"onClick=\"return confirm('Log off now?');\">"
		"<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
		"SRC=\"/static/exit-icon.gif\">"
		"<BR>Log off</A></SPAN><BR>\n"
	);

	wprintf(
		"<SPAN CLASS=\"customize\">"
		"<A HREF=\"/display_customize_iconbar\" "
		"TITLE=\"Customize this menu\" "
		"TARGET=\"workspace\">customize this menu</A>"
		"</SPAN><BR>\n"
	);

	if (ib_citadel) wprintf(
		"<SPAN CLASS=\"powered_by\">"
		"<A HREF=\"http://uncensored.citadel.org/citadel\" "
		"TITLE=\"Find out more about Citadel/UX\" TARGET=\"aboutcit\" "
		"onMouseOver=\"window.status='Find out more about "
		"Citadel/UX'; return true;\">powered by<BR><IMG "
		"BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/citadel-logo.jpg\" ALT=\"CITADEL/UX\"></A>"
		"</SPAN>\n"
	);

	wprintf("</CENTER>\n");
	do_template("endbox");
	wDumpContent(2);
}



void display_customize_iconbar(void) {
	char iconbar[SIZ];
	char buf[SIZ];
	char key[SIZ], value[SIZ];
	int i;

	/* The initialized values of these variables also happen to
	 * specify the default values for users who haven't customized
	 * their iconbars.  These should probably be set in a master
	 * configuration somewhere.
	 */
	int ib_logo = 1;	/* Site logo */
	int ib_inbox = 0;	/* Inbox icon */
	int ib_calendar = 0;	/* Calendar icon */
	int ib_tasks = 0;	/* Tasks icon */
	int ib_rooms = 1;	/* Rooms icon */
	int ib_users = 1;	/* Users icon */
	int ib_advanced = 1;	/* Advanced Options icon */
	int ib_logoff = 1;	/* Logoff button */
	int ib_citadel = 1;	/* 'Powered by Citadel' logo */
	/*
	 */

	get_preference("iconbar", iconbar);
	for (i=0; i<num_tokens(iconbar, ','); ++i) {
		extract_token(buf, iconbar, i, ',');
		extract_token(key, buf, 0, '=');
		extract_token(value, buf, 1, '=');

		if (!strcasecmp(key, "ib_logo")) ib_logo = atoi(value);
		if (!strcasecmp(key, "ib_inbox")) ib_inbox = atoi(value);
		if (!strcasecmp(key, "ib_calendar")) ib_calendar = atoi(value);
		if (!strcasecmp(key, "ib_tasks")) ib_tasks = atoi(value);
		if (!strcasecmp(key, "ib_rooms")) ib_rooms = atoi(value);
		if (!strcasecmp(key, "ib_users")) ib_users = atoi(value);
		if (!strcasecmp(key, "ib_advanced")) ib_advanced = atoi(value);
		if (!strcasecmp(key, "ib_logoff")) ib_logoff = atoi(value);
		if (!strcasecmp(key, "ib_citadel")) ib_citadel = atoi(value);

	}

	output_headers(3);
	svprintf("BOXTITLE", WCS_STRING, "Customize the icon bar");
	do_template("beginbox");

	wprintf("<CENTER>Select the icons you would like to see displayed "
		"in the &quot;icon bar&quot; menu on the left side of the "
		"screen.</CENTER><BR>\n"
	);

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/commit_iconbar\">\n");
	wprintf("<TABLE border=0 cellspacing=0 cellpadding=3 width=100%%>\n");

	wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_logo\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/image&name=hello\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Site logo</B><BR>"
		"A graphic describing this site"
		"</TD></TR>\n",
		(ib_logo ? "CHECKED" : "")
	);

	wprintf("<TR><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_inbox\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/mail.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Mail (inbox)</B><BR>"
		"A shortcut to your e-mail Inbox."
		"</TD></TR>\n",
		(ib_inbox ? "CHECKED" : "")
	);

#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_calendar\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/vcalendar.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Calendar</B><BR>"
		"A shortcut to your personal calendar."
		"</TD></TR>\n",
		(ib_calendar ? "CHECKED" : "")
	);

	wprintf("<TR><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_tasks\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/vcalendar.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Tasks</B><BR>"
		"A shortcut to your personal task list."
		"</TD></TR>\n",
		(ib_tasks ? "CHECKED" : "")
	);
#endif /* WEBCIT_WITH_CALENDAR_SERVICE */

	wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_rooms\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/rooms-icon.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Rooms</B><BR>"
		"Clicking this icon displays a list of all accesible "
		"rooms (or folders) available."
		"</TD></TR>\n",
		(ib_rooms ? "CHECKED" : "")
	);

	wprintf("<TR><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_users\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/users-icon.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Users</B><BR>"
		"Clicking this icon displays a list of all users "
		"currently logged in."
		"</TD></TR>\n",
		(ib_users ? "CHECKED" : "")
	);

	wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_advanced\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/advanced-icon.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Advanced options</B><BR>"
		"Access to the complete menu of Citadel functions."
		"</TD></TR>\n",
		(ib_advanced ? "CHECKED" : "")
	);

	wprintf("<TR><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_logoff\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/exit-icon.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Log off</B><BR>"
		"Exit from the Citadel system.  If you remove this icon "
		"then you will have no way out!"
		"</TD></TR>\n",
		(ib_logoff ? "CHECKED" : "")
	);
	wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_citadel\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/citadel-logo.jpg\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Citadel logo</B><BR>"
		"Displays the &quot;Powered by Citadel&quot; graphic"
		"</TD></TR>\n",
		(ib_citadel ? "CHECKED" : "")
	);

	wprintf("</TABLE><BR>\n"
		"<CENTER>"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">"
		"&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">"
		"</CENTER></FORM>\n"
	);

	do_template("endbox");
	wDumpContent(2);
}


void commit_iconbar(void) {
	char iconbar[SIZ];
	int i;

	char *boxen[] = {
		"ib_logo",
		"ib_inbox",
		"ib_calendar",
		"ib_tasks",
		"ib_rooms",
		"ib_users",
		"ib_advanced",
		"ib_logoff",
		"ib_citadel"
	};

	if (strcmp(bstr("sc"), "OK")) {
		display_main_menu();
		return;
	}

	strcpy(iconbar, "");

	for (i=0; i<(sizeof(boxen)/sizeof(char *)); ++i) {
		if (i > 0) {
			sprintf(&iconbar[strlen(iconbar)], ",");
		}
		sprintf(&iconbar[strlen(iconbar)], "%s=", boxen[i]);
		if (!strcasecmp(bstr(boxen[i]), "yes")) {
			sprintf(&iconbar[strlen(iconbar)], "1");
		}
		else {
			sprintf(&iconbar[strlen(iconbar)], "0");
		}
	}

	set_preference("iconbar", iconbar);

	output_headers(3);
	wprintf("FIXME");
	wDumpContent(2);
}
