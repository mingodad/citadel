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
	char buf[128];

	time(&now);
	localtime_r(&now, &tm);

	wc_strftime(buf, 32, "%A, %x", &tm);
	wprintf("%s", buf);
}




/**
 * \brief Dummy section
 */
void dummy_section(void) {
	svput("BOXTITLE", WCS_STRING, "(dummy&nbsp;section)");
	do_template("beginbox");
	wprintf(_("(nothing)"));
	do_template("endbox");
}


/**
 * \brief New messages section
 */
void new_messages_section(void) {
	char buf[SIZ];
	char room[SIZ];
	int i;
	int number_of_rooms_to_check;
	char *rooms_to_check = "Mail|Lobby";


	number_of_rooms_to_check = num_tokens(rooms_to_check, '|');
	if (number_of_rooms_to_check == 0) return;

	wprintf("<table border=0 width=100%%>\n");
	for (i=0; i<number_of_rooms_to_check; ++i) {
		extract_token(room, rooms_to_check, i, '|', sizeof room);

		serv_printf("GOTO %s", room);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			extract_token(room, &buf[4], 0, '|', sizeof room);
			wprintf("<tr><td><a href=\"dotgoto?room=");
			urlescputs(room);
			wprintf("\">");
			escputs(room);
			wprintf("</a></td><td>%d/%d</td></tr>\n",
				extract_int(&buf[4], 1),
				extract_int(&buf[4], 2)
			);
		}
	}
	wprintf("</table>\n");

}


/**
 * \brief Task list section
 */
void tasks_section(void) {
	int num_msgs = 0;
	int i;

	gotoroom("_TASKS_");
	if (WC->wc_view != VIEW_TASKS) {
		num_msgs = 0;
	}
	else {
		num_msgs = load_msg_ptrs("MSGS ALL", 0);
	}

	if (num_msgs < 1) {
		wprintf("<i>");
		wprintf(_("(None)"));
		wprintf("</i><br />\n");
	}
	else {
		for (i=0; i<num_msgs; ++i) {
			display_task(WC->msgarr[i], 0);
		}
	}

	calendar_summary_view();
}


/**
 * \brief Calendar section
 */
void calendar_section(void) {
	int num_msgs = 0;
	int i;

	gotoroom("_CALENDAR_");
	if ( (WC->wc_view != VIEW_CALENDAR) && (WC->wc_view != VIEW_CALBRIEF) ) {
		num_msgs = 0;
	}
	else {
		num_msgs = load_msg_ptrs("MSGS ALL", 0);
	}

	if (num_msgs < 1) {
		wprintf("<i>");
		wprintf(_("(Nothing)"));
		wprintf("</i><br />\n");
	}
	else {
		for (i=0; i<num_msgs; ++i) {
			display_calendar(WC->msgarr[i], 0);
		}
		calendar_summary_view();
	}
}

/**
 * \brief Server info section (fluff, really)
 */
void server_info_section(void) {
	char message[512];

	snprintf(message, sizeof message,
		_("You are connected to %s, running %s with %s, server build %s and located in %s.  Your system administrator is %s."),
		serv_info.serv_humannode,
		serv_info.serv_software,
		PACKAGE_STRING,
		serv_info.serv_svn_revision,
		serv_info.serv_bbs_city,
		serv_info.serv_sysadm);
	escputs(message);
}

/**
 * \brief summary of inner div????
 */



void summary_inner_div(void) {
	/**
	 * Now let's do three columns of crap.  All portals and all groupware
	 * clients seem to want to do three columns, so we'll do three
	 * columns too.  Conformity is not inherently a virtue, but there are
	 * a lot of really shallow people out there, and even though they're
	 * not people I consider worthwhile, I still want them to use WebCit.
	 */

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table width=\"100%%\" cellspacing=\"10px\" cellpadding=\"0\">"
		"<tr valign=top>");

	/**
	 * Column One
	 */
	wprintf("<td width=33%%>");
	wprintf("<div class=\"box\">");	
	wprintf("<div class=\"boxlabel\">");	
	wprintf(_("Messages"));
	wprintf("</div><div class=\"boxcontent\">");	
	wprintf("<div id=\"msg_inner\">");	
	new_messages_section();
	wprintf("</div></div></div>");
	wprintf("</td>");

	/**
	 * Column Two 
	 */
	wprintf("<td width=33%%>");
	wprintf("<div class=\"box\">");	
	wprintf("<div class=\"boxlabel\">");	
	wprintf(_("Tasks"));
	wprintf("</div><div class=\"boxcontent\">");	
	wprintf("<div id=\"tasks_inner\">");	
	tasks_section();
	wprintf("</div></div></div>");
	wprintf("</td>");

	/**
	 * Column Three
	 */
	wprintf("<td width=33%%>");
	wprintf("<div class=\"box\">");	
	wprintf("<div class=\"boxlabel\">");	
	wprintf(_("Today&nbsp;on&nbsp;your&nbsp;calendar"));
	wprintf("</div><div class=\"boxcontent\">");	
	wprintf("<div id=\"calendar_inner\">");	
	calendar_section();
	wprintf("</div></div></div>");
	wprintf("</td>");

	wprintf("</tr><tr valign=top>");

	/**
	 * Row Two - Column One
	 */
	wprintf("<td colspan=2>");
	wprintf("<div class=\"box\">");	
	wprintf("<div class=\"boxlabel\">");	
	wprintf(_("Who's&nbsp;online&nbsp;now"));
	wprintf("</div><div class=\"boxcontent\">");	
	wprintf("<div id=\"who_inner\">");	
	who_inner_div(); 
	wprintf("</div></div></div>");
	wprintf("</td>");

	/**
	 * Row Two - Column Two
	 */
	wprintf("<td width=33%%>");
	wprintf("<div class=\"box\">");	
	wprintf("<div class=\"boxlabel\">");	
	wprintf(_("About&nbsp;this&nbsp;server"));
	wprintf("</div><div class=\"boxcontent\">");	
	wprintf("<div id=\"info_inner\">");	
	server_info_section();
	wprintf("</div></div></div>");
	wprintf("</td>");


	/**
	 * End of columns
	 */
	wprintf("</tr></table>");
}


/**
 * \brief Display this user's summary page
 */
void summary(void) {
	char title[256];

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<div class=\"room_banner\">");
        wprintf("<img src=\"static/summscreen_48x.gif\">");
        wprintf("<h1>");
        snprintf(title, sizeof title, _("Summary page for %s"), WC->wc_fullname);
        escputs(title);
        wprintf("</h1><h2>");
        output_date();
        wprintf("</h2></div>");
	wprintf("<ul class=\"room_actions\">\n");
	wprintf("<li class=\"start_page\">");
	offer_start_page();
        wprintf("</li></ul>");
        wprintf("</div>");

	/**
	 * You guessed it ... we're going to refresh using ajax.
	 * In the future we might consider updating individual sections of the summary
	 * instead of the whole thing.
	 */
	wprintf("<div id=\"content\" class=\"service\">\n");
	summary_inner_div();
	wprintf("</div>\n");

	wprintf(
		"<script type=\"text/javascript\">					"
		" new Ajax.PeriodicalUpdater('msg_inner', 'new_messages_html',		"
		"                            { method: 'get', frequency: 60 }  );	"
		" new Ajax.PeriodicalUpdater('tasks_inner', 'tasks_inner_html',		"
		"                            { method: 'get', frequency: 120 }  );	"
		" new Ajax.PeriodicalUpdater('calendar_inner', 'calendar_inner_html',		"
		"                            { method: 'get', frequency: 90 }  );	"
		" new Ajax.PeriodicalUpdater('who_inner', 'who_inner_html',		"
		"                            { method: 'get', frequency: 30 }  );	"
		"</script>							 	\n"
	);

	wDumpContent(1);
}


/*@}*/
