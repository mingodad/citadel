/*
 * $Id$
 *
 * Displays the "Summary Page"
 */

#include "webcit.h"

/*
 * Display today's date in a friendly format
 */
void output_date(void) {
	struct tm tm;
	time_t now;

	static char *wdays[] = {
		"Sunday", "Monday", "Tuesday", "Wednesday",
		"Thursday", "Friday", "Saturday"
	};
	static char *months[] = {
		"January", "February", "March", "April", "May", "June", "July",
		"August", "September", "October", "November", "December"
	};

	time(&now);
	localtime_r(&now, &tm);

	wprintf("%s, %s %d, %d",
		wdays[tm.tm_wday],
		months[tm.tm_mon],
		tm.tm_mday,
		tm.tm_year + 1900
	);
}




/*
 * Dummy section
 */
void dummy_section(void) {
	svprintf("BOXTITLE", WCS_STRING, "(dummy&nbsp;section)");
	do_template("beginbox");
	wprintf("(nothing)");
	do_template("endbox");
}


/*
 * New messages section
 */
void new_messages_section(void) {
	char buf[SIZ];
	char room[SIZ];
	int i;
	int number_of_rooms_to_check;
	char *rooms_to_check = "Mail|Lobby";

	svprintf("BOXTITLE", WCS_STRING, "Messages");
	do_template("beginbox");

	number_of_rooms_to_check = num_tokens(rooms_to_check, '|');
	if (number_of_rooms_to_check == 0) return;

	wprintf("<TABLE BORDER=0 WIDTH=100%%>\n");
	for (i=0; i<number_of_rooms_to_check; ++i) {
		extract_token(room, rooms_to_check, i, '|', sizeof room);

		serv_printf("GOTO %s", room);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			extract_token(room, &buf[4], 0, '|', sizeof room);
			wprintf("<TR><TD><A HREF=\"/dotgoto?room=");
			urlescputs(room);
			wprintf("\">");
			escputs(room);
			wprintf("</A></TD><TD>%d/%d</TD></TR>\n",
				extract_int(&buf[4], 1),
				extract_int(&buf[4], 2)
			);
		}
	}
	wprintf("</TABLE>\n");
	do_template("endbox");

}


/*
 * Wholist section
 */
void wholist_section(void) {
	char buf[SIZ];
	char user[SIZ];

	svprintf("BOXTITLE", WCS_STRING, "Who's&nbsp;online&nbsp;now");
	do_template("beginbox");
	serv_puts("RWHO");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') while(serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		extract_token(user, buf, 1, '|', sizeof user);
		escputs(user);
		wprintf("<br />\n");
	}
	do_template("endbox");
}


/*
 * Task list section
 */
void tasks_section(void) {
#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	int num_msgs = 0;
	int i;
#endif

	svprintf("BOXTITLE", WCS_STRING, "Tasks");
	do_template("beginbox");
#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	gotoroom("_TASKS_");
	if (WC->wc_view != VIEW_TASKS) {
		num_msgs = 0;
	}
	else {
		num_msgs = load_msg_ptrs("MSGS ALL", 0);
	}

	if (num_msgs < 1) {
		wprintf("<i>(None)</i><br />\n");
	}
	else {
		for (i=0; i<num_msgs; ++i) {
			display_task(WC->msgarr[i]);
		}
	}

	calendar_summary_view();

#else /* WEBCIT_WITH_CALENDAR_SERVICE */
	wprintf("<I>(This server does not support task lists)</I>\n");
#endif /* WEBCIT_WITH_CALENDAR_SERVICE */
	do_template("endbox");
}


/*
 * Calendar section
 */
void calendar_section(void) {
#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	int num_msgs = 0;
	int i;
#endif

	svprintf("BOXTITLE", WCS_STRING, "Today&nbsp;on&nbsp;your&nbsp;calendar");
	do_template("beginbox");
#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	gotoroom("_CALENDAR_");
	if (WC->wc_view != VIEW_CALENDAR) {
		num_msgs = 0;
	}
	else {
		num_msgs = load_msg_ptrs("MSGS ALL", 0);
	}

	if (num_msgs < 1) {
		wprintf("<i>(Nothing)</i><br />\n");
	}
	else {
		for (i=0; i<num_msgs; ++i) {
			display_calendar(WC->msgarr[i]);
		}
		calendar_summary_view();
	}

#else /* WEBCIT_WITH_CALENDAR_SERVICE */
	wprintf("<I>(This server does not support calendars)</I>\n");
#endif /* WEBCIT_WITH_CALENDAR_SERVICE */
	do_template("endbox");
}


/*
 * Server info section (fluff, really)
 */
void server_info_section(void) {
	svprintf("BOXTITLE", WCS_STRING, "About&nbsp;this&nbsp;server");
	do_template("beginbox");
	wprintf("You are connected to ");
	escputs(serv_info.serv_humannode);
	wprintf(", running ");
	escputs(serv_info.serv_software);
	wprintf(" with ");
	escputs(SERVER);
	wprintf(", and located in ");
	escputs(serv_info.serv_bbs_city);
	wprintf(".<br />\nYour system administrator is ");
	escputs(serv_info.serv_sysadm);
	wprintf(".\n");
	do_template("endbox");
}


/*
 * Display this user's summary page
 */
void summary(void) {

	output_headers(1, 1, 2, 0, 1, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=#444455><TR>"
		"<TD><IMG SRC=\"/static/summscreen_48x.gif\"></TD><TD>"
		"<SPAN CLASS=\"titlebar\">"
		"Summary page for ");
	escputs(WC->wc_username);
	wprintf("</SPAN></TD><TD>\n");
	wprintf("</TD><TD ALIGN=RIGHT><SPAN CLASS=\"titlebar\">");
	output_date();
	wprintf("</SPAN><br />");
	offer_start_page();
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	/*
	 * Now let's do three columns of crap.  All portals and all groupware
	 * clients seem to want to do three columns, so we'll do three
	 * columns too.  Conformity is not inherently a virtue, but there are
	 * a lot of really shallow people out there, and even though they're
	 * not people I consider worthwhile, I still want them to use WebCit.
	 */

	wprintf("<div id=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%%><tr valign=top>");

	/*
	 * Column One
	 */
	wprintf("<td width=33%%>");
	wholist_section();

	/*
	 * Column Two
	 */
	wprintf("</td><td width=33%%>");
	server_info_section();
	wprintf("<br />");
	tasks_section();

	/*
	 * Column Three
	 */
	wprintf("</td><td width=33%%>");
	new_messages_section();
	wprintf("<br />");
	calendar_section();

	/*
	 * End of columns
	 */
	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);
}
