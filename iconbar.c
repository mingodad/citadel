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


/* Values for ib_displayas */
#define IB_PICTEXT	0
#define IB_PICONLY	1
#define IB_TEXTONLY	2

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
	int ib_displayas = 0;	/* pictures and text, pictures, text */
	int ib_logo = 0;	/* Site logo */
	int ib_summary = 0;	/* Summary page icon */
	int ib_inbox = 0;	/* Inbox icon */
	int ib_calendar = 0;	/* Calendar icon */
	int ib_contacts = 0;	/* Contacts icon */
	int ib_notes = 0;	/* Notes icon */
	int ib_tasks = 0;	/* Tasks icon */
	int ib_rooms = 1;	/* Rooms icon */
	int ib_users = 1;	/* Users icon */
	int ib_chat = 0;	/* Chat icon */
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

		if (!strcasecmp(key, "ib_displayas")) ib_displayas = atoi(value);
		if (!strcasecmp(key, "ib_logo")) ib_logo = atoi(value);
		if (!strcasecmp(key, "ib_summary")) ib_summary = atoi(value);
		if (!strcasecmp(key, "ib_inbox")) ib_inbox = atoi(value);
		if (!strcasecmp(key, "ib_calendar")) ib_calendar = atoi(value);
		if (!strcasecmp(key, "ib_contacts")) ib_contacts = atoi(value);
		if (!strcasecmp(key, "ib_notes")) ib_notes = atoi(value);
		if (!strcasecmp(key, "ib_tasks")) ib_tasks = atoi(value);
		if (!strcasecmp(key, "ib_rooms")) ib_rooms = atoi(value);
		if (!strcasecmp(key, "ib_users")) ib_users = atoi(value);
		if (!strcasecmp(key, "ib_chat")) ib_chat = atoi(value);
		if (!strcasecmp(key, "ib_advanced")) ib_advanced = atoi(value);
		if (!strcasecmp(key, "ib_logoff")) ib_logoff = atoi(value);
		if (!strcasecmp(key, "ib_citadel")) ib_citadel = atoi(value);
	}

	wprintf("<center>\n");

	if (ib_logo) if (ib_displayas != IB_TEXTONLY) wprintf(
		"<IMG BORDER=\"0\" WIDTH=\"48\" "
			"HEIGHT=\"48\" SRC=\"/image&name=hello\" ALT=\"&nbsp;\">"
			"<br />\n"
	);

	if (ib_summary) {
		wprintf("<SPAN CLASS=\"iconbar_link\">"
			"<A HREF=\"/summary\" "
			"TITLE=\"Your summary page\" "
			"><P>"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
				"SRC=\"/static/summary.gif\"><br />");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Summary<br />");
		}
		wprintf("</A></P></SPAN>\n");
	}

	if (ib_inbox) {
		wprintf("<SPAN CLASS=\"iconbar_link\"><P>"
			"<A HREF=\"/dotgoto?room=_MAIL_\" "
			"TITLE=\"Go to your e-mail inbox\" "
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
				"SRC=\"/static/mail.gif\"><br />");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Mail<br />");
		}
		wprintf("</A></P></SPAN>\n");
	}

	if (ib_calendar) {
		wprintf("<SPAN CLASS=\"iconbar_link\"><P>"
			"<A HREF=\"/dotgoto?room=Calendar\" "
			"TITLE=\"Go to your personal calendar\" "
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/vcalendar.gif\"><br />");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Calendar<br />");
		}
		wprintf("</A></P></SPAN>\n");
	}

	if (ib_contacts) {
		wprintf("<SPAN CLASS=\"iconbar_link\"><P>"
			"<A HREF=\"/dotgoto?room=Contacts\" "
			"TITLE=\"Go to your personal address book\" "
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/vcard.gif\"><br />");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Contacts<br />");
		}
		wprintf("</A></P></SPAN>\n");
	}

	if (ib_notes) {
		wprintf("<SPAN CLASS=\"iconbar_link\"><P>"
			"<A HREF=\"/dotgoto?room=Notes\" "
			"TITLE=\"Go to your personal notes\" "
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/note.gif\"><br />");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Notes<br />");
		}
		wprintf("</A></P></SPAN>\n");
	}

	if (ib_tasks)  {
		wprintf("<SPAN CLASS=\"iconbar_link\"><P>"
			"<A HREF=\"/dotgoto?room=Tasks\" "
			"TITLE=\"Go to your personal task list\" "
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/vcalendar.gif\"><br />");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Tasks<br />");
		}
		wprintf("</A></P></SPAN>\n");
	}

	if (ib_rooms) {
		wprintf("<SPAN CLASS=\"iconbar_link\"><P>"
			"<A HREF=\"/knrooms\" TITLE=\"List all of your "
			"accessible rooms\" >"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/rooms-icon.gif\"><br />");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Rooms<br />");
		}
		wprintf("</A></P></SPAN>\n");
	}

	if (ib_users) {
		wprintf("<SPAN CLASS=\"iconbar_link\"><P>"
			"<A HREF=\"/whobbs\" TITLE=\"See who is online right now\" "
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/users-icon.gif\"><br />");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Users<br />");
		}
		wprintf("</A></P></SPAN>\n");
	}

	if (ib_chat) {
		wprintf("<SPAN CLASS=\"iconbar_link\"><P>"
			"<A HREF=\"#\" onClick=\"window.open('/chat', "
			"'ctdl_chat_window', "
			"'toolbar=no,location=no,directories=no,copyhistory=no,"
			"status=no,scrollbars=yes,resizable=yes');\""
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/chat-icon.gif\"><br />");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Chat<br />");
		}
		wprintf("</A></P></SPAN>\n");
	}

	if (ib_advanced) {
		wprintf("<SPAN CLASS=\"iconbar_link\"><P>"
			"<A HREF=\"/display_main_menu\" "
			"TITLE=\"Advanced Options Menu: Advanced Room commands, "
			"Account Info, and Chat\" "
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/advanced-icon.gif\"><br />");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Advanced options<br />");
		}
		wprintf("</A></P></SPAN>\n");
	}

	if (ib_logoff) {
		wprintf("<SPAN CLASS=\"iconbar_link\"><P>"
			"<A HREF=\"/termquit\" TITLE=\"Log off\" "
			"onClick=\"return confirm('Log off now?');\">"
		);
		if (ib_displayas != IB_TEXTONLY) {
		wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/exit-icon.gif\"><br />");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Log off<br />");
		}
		wprintf("</A></P></SPAN>\n");
	}

	wprintf(
		"<SPAN CLASS=\"customize\"><P>"
		"<A HREF=\"/display_customize_iconbar\" "
		"TITLE=\"Customize this menu\" "
		">customize this menu</A>"
		"</P></SPAN>\n"
	);

	if (ib_citadel) if (ib_displayas != IB_TEXTONLY) wprintf(
		"<SPAN CLASS=\"powered_by\"><P>"
		"<A HREF=\"http://www.citadel.org\" "
		"title=\"Find out more about Citadel\" target=\"aboutcit\" "
		"onMouseOver=\"window.status='Find out more about "
		"Citadel'; return true;\">powered by<br /><IMG "
		"BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/citadel-logo.gif\" ALT=\"CITADEL\">"
		"<br />CITADEL</A>"
		"</P></SPAN>\n"
	);

	wprintf("</CENTER>\n");
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
	int ib_displayas = IB_PICTEXT;	/* pictures and text, pictures, text */
	int ib_logo = 0;	/* Site logo */
	int ib_summary = 0;	/* Summary page icon */
	int ib_inbox = 0;	/* Inbox icon */
	int ib_calendar = 0;	/* Calendar icon */
	int ib_contacts = 0;	/* Contacts icon */
	int ib_notes = 0;	/* Notes icon */
	int ib_tasks = 0;	/* Tasks icon */
	int ib_rooms = 1;	/* Rooms icon */
	int ib_users = 1;	/* Users icon */
	int ib_chat = 0;	/* Chat icon */
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

		if (!strcasecmp(key, "ib_displayas")) ib_displayas = atoi(value);
		if (!strcasecmp(key, "ib_logo")) ib_logo = atoi(value);
		if (!strcasecmp(key, "ib_summary")) ib_summary = atoi(value);
		if (!strcasecmp(key, "ib_inbox")) ib_inbox = atoi(value);
		if (!strcasecmp(key, "ib_calendar")) ib_calendar = atoi(value);
		if (!strcasecmp(key, "ib_contacts")) ib_contacts = atoi(value);
		if (!strcasecmp(key, "ib_notes")) ib_notes = atoi(value);
		if (!strcasecmp(key, "ib_tasks")) ib_tasks = atoi(value);
		if (!strcasecmp(key, "ib_rooms")) ib_rooms = atoi(value);
		if (!strcasecmp(key, "ib_users")) ib_users = atoi(value);
		if (!strcasecmp(key, "ib_chat")) ib_chat = atoi(value);
		if (!strcasecmp(key, "ib_advanced")) ib_advanced = atoi(value);
		if (!strcasecmp(key, "ib_logoff")) ib_logoff = atoi(value);
		if (!strcasecmp(key, "ib_citadel")) ib_citadel = atoi(value);
	}

	output_headers(1, 1, 0, 0, 0, 0, 0);
	svprintf("BOXTITLE", WCS_STRING, "Customize the icon bar");
	do_template("beginbox");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/commit_iconbar\">\n");

	wprintf("<CENTER>"
		"Display icons as: ");
	for (i=0; i<=2; ++i) {
		wprintf("<INPUT TYPE=\"radio\" NAME=\"ib_displayas\" VALUE=\"%d\"", i);
		if (ib_displayas == i) wprintf(" CHECKED");
		wprintf(">");
		if (i == IB_PICTEXT) wprintf("pictures and text");
		if (i == IB_PICONLY) wprintf("pictures only");
		if (i == IB_TEXTONLY) wprintf("text only");
		wprintf("\n");
	}
	wprintf("<br /><br />\n");

	wprintf("Select the icons you would like to see displayed "
		"in the &quot;icon bar&quot; menu on the left side of the "
		"screen.</CENTER><br />\n"
	);

	wprintf("<TABLE border=0 cellspacing=0 cellpadding=3 width=100%%>\n");

	wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_logo\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/image&name=hello\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Site logo</B><br />"
		"A graphic describing this site"
		"</TD></TR>\n",
		(ib_logo ? "CHECKED" : "")
	);

	wprintf("<TR><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_summary\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/summary.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Summary</B><br />"
		"Your summary page"
		"</TD></TR>\n",
		(ib_summary ? "CHECKED" : "")
	);

	wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_inbox\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/mail.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Mail (inbox)</B><br />"
		"A shortcut to your e-mail Inbox."
		"</TD></TR>\n",
		(ib_inbox ? "CHECKED" : "")
	);

	wprintf("<TR><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_contacts\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/vcard.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Contacts</B><br />"
		"Your personal address book."
		"</TD></TR>\n",
		(ib_contacts ? "CHECKED" : "")
	);

	wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_notes\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/note.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Notes</B><br />"
		"Your personal notes."
		"</TD></TR>\n",
		(ib_notes ? "CHECKED" : "")
	);

#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	wprintf("<TR><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_calendar\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/vcalendar.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Calendar</B><br />"
		"A shortcut to your personal calendar."
		"</TD></TR>\n",
		(ib_calendar ? "CHECKED" : "")
	);

	wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_tasks\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/vcalendar.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Tasks</B><br />"
		"A shortcut to your personal task list."
		"</TD></TR>\n",
		(ib_tasks ? "CHECKED" : "")
	);
#endif /* WEBCIT_WITH_CALENDAR_SERVICE */

	wprintf("<TR><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_rooms\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/rooms-icon.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Rooms</B><br />"
		"Clicking this icon displays a list of all accesible "
		"rooms (or folders) available."
		"</TD></TR>\n",
		(ib_rooms ? "CHECKED" : "")
	);

	wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_users\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/users-icon.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Users</B><br />"
		"Clicking this icon displays a list of all users "
		"currently logged in."
		"</TD></TR>\n",
		(ib_users ? "CHECKED" : "")
	);

	wprintf("<TR><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_chat\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/chat-icon.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Chat</B><br />"
		"Clicking this icon enters real-time chat mode "
		"with other users in the same room."
		"</TD></TR>\n",
		(ib_chat ? "CHECKED" : "")
	);

	wprintf("<TR BGCOLOR=\"#CCCCCC\"><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_advanced\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/advanced-icon.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Advanced options</B><br />"
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
		"<B>Log off</B><br />"
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
		"SRC=\"/static/citadel-logo.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Citadel logo</B><br />"
		"Displays the &quot;Powered by Citadel&quot; graphic"
		"</TD></TR>\n",
		(ib_citadel ? "CHECKED" : "")
	);

	wprintf("</TABLE><br />\n"
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
		"ib_summary",
		"ib_inbox",
		"ib_calendar",
		"ib_contacts",
		"ib_notes",
		"ib_tasks",
		"ib_rooms",
		"ib_users",
		"ib_chat",
		"ib_advanced",
		"ib_logoff",
		"ib_citadel"
	};

	if (strcmp(bstr("sc"), "OK")) {
		display_main_menu();
		return;
	}

	sprintf(iconbar, "ib_displayas=%d", atoi(bstr("ib_displayas")));

	for (i=0; i<(sizeof(boxen)/sizeof(char *)); ++i) {
		sprintf(&iconbar[strlen(iconbar)], ",%s=", boxen[i]);
		if (!strcasecmp(bstr(boxen[i]), "yes")) {
			sprintf(&iconbar[strlen(iconbar)], "1");
		}
		else {
			sprintf(&iconbar[strlen(iconbar)], "0");
		}
	}

	set_preference("iconbar", iconbar);

	output_headers(1, 1, 0, 0, 0, 0, 0);
	do_template("beginbox_nt");
	wprintf(
		"<IMG SRC=\"/static/advanced-icon.gif\">"
		"&nbsp;"
		"Your icon bar has been updated.  Please select any of its "
		"choices to continue.\n"
	);
	do_template("endbox");
	wDumpContent(2);
}
