/*
 * $Id$
 *
 * Displays and customizes the iconbar.
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
	int ib_summary = 1;	/* Summary page icon */
	int ib_inbox = 1;	/* Inbox icon */
	int ib_calendar = 1;	/* Calendar icon */
	int ib_contacts = 1;	/* Contacts icon */
	int ib_notes = 1;	/* Notes icon */
	int ib_tasks = 1;	/* Tasks icon */
	int ib_rooms = 1;	/* Rooms icon */
	int ib_users = 1;	/* Users icon */
	int ib_chat = 1;	/* Chat icon */
	int ib_advanced = 1;	/* Advanced Options icon */
	int ib_citadel = 1;	/* 'Powered by Citadel' logo */
	/*
	 */

	get_preference("iconbar", iconbar, sizeof iconbar);
	for (i=0; i<num_tokens(iconbar, ','); ++i) {
		extract_token(buf, iconbar, i, ',', sizeof buf);
		extract_token(key, buf, 0, '=', sizeof key);
		extract_token(value, buf, 1, '=', sizeof value);

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
		if (!strcasecmp(key, "ib_citadel")) ib_citadel = atoi(value);
	}

	wprintf("<div id=\"button\">\n"
		"<ul>\n"
	);

	if (ib_logo) {
		wprintf("<li>");
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" "
				"HEIGHT=\"32\" SRC=\"/image&name=hello\" ALT=\"&nbsp;\">\n"
			);
		}
		wprintf("</li>\n");
	}

	if (ib_citadel) if (ib_displayas != IB_TEXTONLY) wprintf(
		"<li><div align=\"center\">"
		"<A HREF=\"http://www.citadel.org\" "
		"title=\"Find out more about Citadel\" target=\"aboutcit\">"
		"<img border=\"0\" width=\"48\" height=\"48\" "
		"SRC=\"/static/citadel-logo.gif\" ALT=\"CITADEL\">"
		"CITADEL</A>"
		"</div></li>\n"
	);


	if (ib_summary) {
		wprintf("<li><A HREF=\"/summary\" "
			"TITLE=\"Your summary page\" "
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
				"SRC=\"/static/summscreen_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Summary");
		}
		wprintf("</A></li>\n");
	}

	if (ib_inbox) {
		wprintf("<li>"
			"<A HREF=\"/dotgoto?room=_MAIL_\" "
			"TITLE=\"Go to your e-mail inbox\" "
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
				"SRC=\"/static/privatemess_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Mail");
		}
		wprintf("</A></li>\n");
	}

	if (ib_calendar) {
		wprintf("<li>"
			"<A HREF=\"/dotgoto?room=Calendar\" "
			"TITLE=\"Go to your personal calendar\" "
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/calarea_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Calendar");
		}
		wprintf("</A></li>\n");
	}

	if (ib_contacts) {
		wprintf("<li>"
			"<A HREF=\"/dotgoto?room=Contacts\" "
			"TITLE=\"Go to your personal address book\" "
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/viewcontacts_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Contacts");
		}
		wprintf("</A></li>\n");
	}

	if (ib_notes) {
		wprintf("<li>"
			"<A HREF=\"/dotgoto?room=Notes\" "
			"TITLE=\"Go to your personal notes\" "
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/storenotes_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Notes");
		}
		wprintf("</A></li>\n");
	}

	if (ib_tasks)  {
		wprintf("<li>"
			"<A HREF=\"/dotgoto?room=Tasks\" "
			"TITLE=\"Go to your personal task list\" "
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/taskmanag_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Tasks");
		}
		wprintf("</A></li>\n");
	}

	if (ib_rooms) {
		wprintf("<li>"
			"<A HREF=\"/knrooms\" TITLE=\"List all of your "
			"accessible rooms\" >"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/chatrooms_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Rooms");
		}
		wprintf("</A></li>\n");
	}

	if (ib_users) {
		wprintf("<li>"
			"<A HREF=\"/whobbs\" TITLE=\"See who is online right now\" "
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/usermanag_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Who is online?");
		}
		wprintf("</A></li>\n");
	}

	if (ib_chat) {
		wprintf("<li>"
			"<A HREF=\"#\" onClick=\"window.open('/chat', "
			"'ctdl_chat_window', "
			"'toolbar=no,location=no,directories=no,copyhistory=no,"
			"status=no,scrollbars=yes,resizable=yes');\""
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/citadelchat_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Chat");
		}
		wprintf("</A></li>\n");
	}

	if (ib_advanced) {
		wprintf("<li>"
			"<A HREF=\"/display_main_menu\" "
			"TITLE=\"Advanced Options Menu: Advanced Room commands, "
			"Account Info, and Chat\" "
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/advanpage2_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Advanced");
		}
		wprintf("</A></li>\n");
	}

	if ((WC->axlevel >= 6) || (WC->is_room_aide)) {
		wprintf("<li>"
			"<A HREF=\"/display_aide_menu\" "
			"TITLE=\"Room and system administration functions\" "
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/advanpage2_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Administration");
		}
		wprintf("</A></li>\n");
	}

	if (1) {
		wprintf("<li>"
			"<A HREF=\"/termquit\" TITLE=\"Log off\" "
			"onClick=\"return confirm('Log off now?');\">"
		);
		if (ib_displayas != IB_TEXTONLY) {
		wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"SRC=\"/static/logoff_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf("Log off");
		}
		wprintf("</A></li>\n");
	}

	wprintf(
		"<li><div align=\"center\">"
		"<A HREF=\"/display_customize_iconbar\" "
		"TITLE=\"Customize this menu\" "
		">customize this menu"
		"</A></div></li>\n"
	);

	wprintf("</ul>\n"
		"</div>\n");
}



void display_customize_iconbar(void) {
	char iconbar[SIZ];
	char buf[SIZ];
	char key[SIZ], value[SIZ];
	int i;
	int bar = 0;

	/* The initialized values of these variables also happen to
	 * specify the default values for users who haven't customized
	 * their iconbars.  These should probably be set in a master
	 * configuration somewhere.
	 */
	int ib_displayas = IB_PICTEXT;	/* pictures and text, pictures, text */
	int ib_logo = 0;	/* Site logo */
	int ib_summary = 1;	/* Summary page icon */
	int ib_inbox = 1;	/* Inbox icon */
	int ib_calendar = 1;	/* Calendar icon */
	int ib_contacts = 1;	/* Contacts icon */
	int ib_notes = 1;	/* Notes icon */
	int ib_tasks = 1;	/* Tasks icon */
	int ib_rooms = 1;	/* Rooms icon */
	int ib_users = 1;	/* Users icon */
	int ib_chat = 1;	/* Chat icon */
	int ib_advanced = 1;	/* Advanced Options icon */
	int ib_citadel = 1;	/* 'Powered by Citadel' logo */
	/*
	 */

	get_preference("iconbar", iconbar, sizeof iconbar);
	for (i=0; i<num_tokens(iconbar, ','); ++i) {
		extract_token(buf, iconbar, i, ',', sizeof buf);
		extract_token(key, buf, 0, '=', sizeof key);
		extract_token(value, buf, 1, '=', sizeof value);

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
		if (!strcasecmp(key, "ib_citadel")) ib_citadel = atoi(value);
	}

	output_headers(1, 1, 2, 0, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">Customize the icon bar</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<div id=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#ffffff\"><tr><td>");

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

	wprintf("<TR BGCOLOR=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_logo\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/image&name=hello\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Site logo</B><br />"
		"An icon describing this site"
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_logo ? "CHECKED" : "")
	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_summary\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/summscreen_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Summary</B><br />"
		"Your summary page"
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_summary ? "CHECKED" : "")
	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_inbox\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/privatemess_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Mail (inbox)</B><br />"
		"A shortcut to your e-mail Inbox."
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_inbox ? "CHECKED" : "")
	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_contacts\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/viewcontacts_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Contacts</B><br />"
		"Your personal address book."
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_contacts ? "CHECKED" : "")
	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_notes\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/storenotes_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Notes</B><br />"
		"Your personal notes."
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_notes ? "CHECKED" : "")
	);

#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_calendar\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/calarea_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Calendar</B><br />"
		"A shortcut to your personal calendar."
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_calendar ? "CHECKED" : "")
	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_tasks\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/taskmanag_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Tasks</B><br />"
		"A shortcut to your personal task list."
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_tasks ? "CHECKED" : "")
	);
#endif /* WEBCIT_WITH_CALENDAR_SERVICE */

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_rooms\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/chatrooms_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Rooms</B><br />"
		"Clicking this icon displays a list of all accesible "
		"rooms (or folders) available."
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_rooms ? "CHECKED" : "")
	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_users\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/usermanag_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Who is online?</B><br />"
		"Clicking this icon displays a list of all users "
		"currently logged in."
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_users ? "CHECKED" : "")
	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_chat\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/citadelchat_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Chat</B><br />"
		"Clicking this icon enters real-time chat mode "
		"with other users in the same room."
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_chat ? "CHECKED" : "")
	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_advanced\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/advanpage2_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Advanced options</B><br />"
		"Access to the complete menu of Citadel functions."
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_advanced ? "CHECKED" : "")
	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_citadel\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"SRC=\"/static/citadel-logo.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>Citadel logo</B><br />"
		"Displays the &quot;Powered by Citadel&quot; icon"
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_citadel ? "CHECKED" : "")
	);

	wprintf("</TABLE><br />\n"
		"<CENTER>"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">"
		"&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">"
		"</CENTER></FORM>\n"
	);

	wprintf("</td></tr></table></div>\n");
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

	set_preference("iconbar", iconbar, 1);

	output_headers(1, 1, 0, 0, 0, 0, 0);
	wprintf(
		"<center><table border=1 bgcolor=\"#ffffff\"><tr><td>"
		"<IMG SRC=\"/static/advanpage2_48x.gif\">"
		"&nbsp;"
		"Your icon bar has been updated.  Please select any of its "
		"choices to continue."
		"</td></tr></table>\n"
	);
	wDumpContent(2);
}
